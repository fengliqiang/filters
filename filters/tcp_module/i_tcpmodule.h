/* C_ssl_protocol.cpp */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#pragma once

class I_dataHandler {
public:
	virtual ~I_dataHandler(){}
public:
	virtual void i_onAccept(IN HANDLE hListen, IN SOCKET s, IN DWORD remoteIp, IN WORD remotePort, IN DWORD localIp, IN WORD localPort) = 0;
	
	virtual void i_onConnected(IN SOCKET s, IN HANDLE hConnect, IN void *ptr) = 0;

	virtual bool i_onData(IN HANDLE hConn, IN const char *buf, IN int len, IN void *ptr) = 0;
	
	virtual void i_onBreak(IN HANDLE hConn, IN void *ptr) = 0;

	virtual void i_onSended(IN HANDLE hConn, IN void *ptr) = 0;

	virtual void i_onLoop() = 0;
};

class I_tcpModule {
public:

	virtual ~I_tcpModule(){}
	virtual HANDLE i_listen(IN WORD port) = 0;

	virtual HANDLE i_bind(IN SOCKET s,IN void *ptr,IN bool enableNagle = true) = 0;//加入一个连接，将建立好的Tcp连接的交给库管理，设定该连接上的三个回调函数和绑定该连接对应的指针，设定是否允许Nagle算法

	virtual HANDLE i_connect(IN DWORD remoteIp,IN WORD remotePort, IN DWORD localIp, IN WORD localPort, IN void *ptr, IN long useTimeMsec) = 0;

	virtual void i_close(IN HANDLE hConn) = 0;

	virtual bool i_send(IN HANDLE hConn, IN const char *buf, IN int len) = 0;//发送数据

	virtual bool i_start(IN bool no_thread = true) = 0;

	virtual bool i_process() = 0;

	virtual UINT64 i_param(IN HANDLE hConn, const char *_req_name) = 0;

};
I_tcpModule *createTcpModule(IN I_dataHandler *dataHandler);
void destroyTcpModule(I_tcpModule *instance);
