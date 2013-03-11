/* C_ssl_protocol.cpp */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#include <winsock2.h>
#include <Windows.h>
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include "../tcp_module/i_tcpmodule.h"
#include "../ssl_protocol/protocol_ssl.h"

#ifndef _SERVICE_ADDR_DEF
#define _SERVICE_ADDR_DEF
struct _SERVICE_ADDR {
	_SERVICE_ADDR():ip(0), port(0){}
	_SERVICE_ADDR(DWORD _ip, WORD _port):ip(_ip), port(_port){}
	DWORD ip;
	WORD port;
	bool operator == (const _SERVICE_ADDR &_right) const {
		return (ip == _right.ip) && (port == _right.port);
	}
	bool operator != (const _SERVICE_ADDR &_right) const {
		return (ip != _right.ip) || (port != _right.port);
	}
	bool operator < (const _SERVICE_ADDR &_right) const {
		if ( ip == _right.ip ) return port < _right.port;
		return ip < _right.ip;
	}
	char *to_string(char *_buffer) const {
		in_addr addr; addr.s_addr = htonl(ip);
		sprintf(_buffer, "%s:%d", inet_ntoa(addr), port);
		return _buffer;
	}
};
#endif


static bool getHost(const char *url, std::string &host) {
	const char *protocol_type = "http://";
	if ( strstr(url, "https://") == url) protocol_type = "https://";
	if ( strstr(url, protocol_type) == url ) url += strlen(protocol_type);
	char url_buf[256]; strcpy(url_buf, url);
	char *end_token = strstr(url_buf, "/");
	if ( end_token ) {
		*end_token = '\0';
		end_token = strrchr(url_buf, ':');
		if ( end_token ) *end_token = '\0';
	}
	host = url_buf;
	return true;
}

static bool getAddr(const char *host, DWORD &ip) 
{
	hostent * pHost = gethostbyname(host);
	if ( pHost == NULL ) return false;
	ip = ntohl(*((DWORD *)&pHost->h_addr_list[0][0]));
	return true;

}

class C_https_client 
	:public I_dataHandler
	, public frames::filter::I_pin
	, public frames::filter::I_pout 
{
	std::string run_state;
	std::string m_url;
	I_tcpModule *tcp_instance;
	HANDLE conn;
	ssl_protocol::I_ssl_filter *m_filter;

private:
	virtual void i_onAccept(IN HANDLE hListen, IN SOCKET s, IN DWORD remoteIp, IN WORD remotePort, IN DWORD localIp, IN WORD localPort) {}
	virtual void i_onConnected(IN SOCKET s, IN HANDLE hConnect, IN void *ptr) 
	{
		tcp_instance->i_close(hConnect);
		conn = 0;
		if ( s == 0 ) {
			run_state = "closed"; return;
		}
		if ( (conn = tcp_instance->i_bind(s, 0)) == 0 ) {

			run_state = "closed"; return;
		}
		m_connected = true;

		m_filter->loop();

		char buffer[1024];
		std::string host_name;
		getHost(m_url.c_str(), host_name);
		typedef std::map<std::string, std::string> _STRING_MAP;
		_STRING_MAP parameters;
		parameters["Accept"] = "*/*";
		parameters["Accept-Language"] = "zh-cn";
		parameters["Accept-Encoding"] = "gzip, deflate";
		parameters["User-Agent"] = "HTTP-TOOL";
		parameters["Content-Length"] = "0";
		parameters["Connection"] = "Close";


		char *pRequest = (char*)strstr(this->m_url.c_str(), host_name.c_str()) + host_name.size();
		if ( *pRequest == '\0' ) pRequest = "/";
		size_t ret_val = 0;
		ret_val += sprintf(buffer + ret_val, "GET %s HTTP/1.1\r\n", pRequest);

		for (_STRING_MAP::iterator it = parameters.begin(); it != parameters.end(); it++ ) {
			ret_val += sprintf(buffer + ret_val, "%s: %s\r\n", it->first.c_str(), it->second.c_str());
		}
		ret_val += sprintf(buffer + ret_val, "\r\n");

		printf("顶层应用，向第一个filter写入\n");
		m_filter->write(buffer, ret_val);
	}

	virtual bool i_onData(IN HANDLE hConn, IN const char *buf, IN int len, IN void *ptr) 
	{
		m_filter->on_data(buf, len); return true;
	}
	virtual void i_onBreak(IN HANDLE hConn, IN void *ptr) 
	{
		tcp_instance->i_close(conn);
		conn = 0;
		m_filter->destroy(); m_filter = 0;
		run_state = "closed";
	}
	virtual void i_onSended(IN HANDLE hConn, IN void *ptr) {}
	virtual void i_onLoop() 
	{
		if ( m_connected && m_filter ) m_filter->loop();
	}
	bool m_connected;
private:
	virtual void write(const char *data, int len) 
	{
		printf("最底层I_pout，调用网络框架接口发送数据\n");
		bool sended = tcp_instance->i_send(conn, data, len);
	}
	virtual void on_data(const char *data, int len) 
	{
		char *buffer = new char[len + 1];
		memcpy(buffer, data, len);
		buffer[len] = '\0';
		printf("%s", buffer);
		delete [] buffer;
	}
public:
	C_https_client(ssl_protocol::I_ctx *ctx)
		:tcp_instance(0), conn(0), m_filter(0), m_connected(false)
	{
		if ( ctx ) {

			m_filter = ctx->create();
			m_filter->connect(this);
			m_filter->set_pin(this);
		}
	}
	~C_https_client()
	{
		if ( conn ) tcp_instance->i_close(conn);
		if ( m_filter ) m_filter->destroy();
	}
	const char *state() const { return run_state.c_str();}

	bool start(const char *url, I_tcpModule *inst) 
	{

		m_url = url;
		std::string host;
		getHost(m_url.c_str(), host);
		_SERVICE_ADDR addr;
		
		if ( ! getAddr(host.c_str(), addr.ip) ) return false;
		if ( strstr(m_url.c_str(), "https") == m_url.c_str() ) addr.port = 443;
		else addr.port = 80;

		tcp_instance = inst;
		conn = tcp_instance->i_connect(addr.ip, addr.port, 0, 0, 0, 4 * 1000);
		return conn != 0;
	}
};
class C_tcp_inst {
	I_tcpModule *tcp_instance;
public:
	C_tcp_inst(I_dataHandler *handler){
		if ( tcp_instance = createTcpModule(handler) ) tcp_instance->i_start();
	}
	~C_tcp_inst(){destroyTcpModule(tcp_instance);}
	I_tcpModule *inst() { return tcp_instance; }
};
class C_client_ctx {
	ssl_protocol::I_ctx *ctx;
public:
	C_client_ctx(){ctx = ssl_protocol::create_client();}
	~C_client_ctx(){if ( ctx ) ctx->destroy();}
	ssl_protocol::I_ctx *instance() { return ctx; }
};
int main(int argc, char* argv[])
{

	if ( argc == 1 ) return 0;

	char *url = argv[1];

	if ( strstr(url, "https://") != url ) {
		printf("not a https request\n"); return 0;
	}

	C_client_ctx auto_ctx;
	C_https_client client(auto_ctx.instance());
	C_tcp_inst inst(&client);

	if ( ! client.start(url, inst.inst()) ) {

		printf("can't find ip address\n"); return 0;
	}

	while ( strcmp(client.state(), "closed") ) {

		if ( ! inst.inst()->i_process() ) Sleep(1);
	}
	return 0;
}

