/* C_ssl_protocol.cpp */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#pragma once
#include <set>
#include <map>



#ifdef WIN32
#define FD_SETSIZE 1024 * 10
#include <winsock2.h>
#pragma comment(lib, "ws2_32")
#include <Windows.h>
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "i_tcpmodule.h"
typedef int socklen_t;
static void sockblock(SOCKET s, bool isBlock)
{

	unsigned long lArgp = ! isBlock;

	ioctlsocket(s, FIONBIO, &lArgp);

}
#define strcasecmp stricmp
#define strncasecmp strnicmp

#else

#endif//WIN32

class C_AutoNetwork {
public:
	C_AutoNetwork() {
#ifdef WIN32
		WSAData wsaData = {0};
		WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
	}
	~C_AutoNetwork() {
#ifdef WIN32
		WSACleanup();
#endif
	}
};

using namespace std;

#pragma warning(disable: 4200)
class C_TcpModule :public I_tcpModule
{
public:
	C_TcpModule(void);
	virtual ~C_TcpModule(void);
private:
	I_dataHandler *m_pHandler;
private:
	enum _CONN_EVENT {
		_SOCK_UDEF = 0,
#define _SOCK_UDEF _SOCK_UDEF
		_SOCK_IN = 0x01,
#define _SOCK_IN _SOCK_IN
		_SOCK_OUT = 0x02,
#define _SOCK_OUT _SOCK_OUT
		_SOCK_ERR = 0x04,
#define _SOCK_ERR _SOCK_ERR
	};
	struct _SendBuf {

		_SendBuf(const char* buf, long len):bufLen(len), next(0) {

			this->buffer = new char[len];
			memcpy(this->buffer, buf, len);
			this->head = this->buffer;
		}

		~_SendBuf() {delete[] this->buffer;}

		void _insert(_SendBuf *bufNode) {

			_SendBuf *last = this;
			while ( last->next ) last = last->next;
			last->next = bufNode;
		}

		_SendBuf *next;
		char *head;
		char *buffer;
		long bufLen;
	};

	struct _ConnContext {

		_ConnContext(SOCKET s, void *ptr, char type):ptr(ptr), pSendList(0), s(s), type(type), state('f'), pBClosed(0), conn_evt(_SOCK_UDEF), cache_size(0){}
		_ConnContext(SOCKET s, void *ptr, DWORD connTimeOut)
			:ptr(ptr), pSendList(0), s(s), type('i'), state('f'), pBClosed(0), start_time(GetTickCount()), timeOut(connTimeOut), conn_evt(_SOCK_UDEF), cache_size(0){}

		~_ConnContext() {

			for ( _SendBuf *p; p = this->pSendList; delete p) this->pSendList = p->next; 
		}

		bool _insert(const char *buffer, int len) {
			_SendBuf *pbuf = new _SendBuf(buffer, len); if ( pbuf == 0 ) return false;
			if ( this->pSendList )this->pSendList->_insert(pbuf);
			else this->pSendList = pbuf;
			cache_size += len;
			return true;
		}

		bool _send(I_dataHandler *pdataHandler = 0) {

			bool bSend = false;

			for (_SendBuf *pHead ; pHead = this->pSendList; ) {

				int len = ::send(this->s, pHead->head, pHead->bufLen - (pHead->head - pHead->buffer), 0);

				if ( len == 0 ) {

					state = 'e'; return false;
				}
				if ( len < 0 ) {
					bool retval;
					retval = GetLastError() == WSAEWOULDBLOCK;
					if ( ! retval ) state = 'e';
					return retval;
				}

				bSend = true;
				pHead->head += len;
				cache_size -= len;
				if ( pHead->bufLen > pHead->head - pHead->buffer ) break;

				this->pSendList = pHead->next;
				delete pHead;

				if ( pdataHandler ) {

					bool bClosed = false;
					this->pBClosed = &bClosed;
					pdataHandler->i_onSended((HANDLE)this, this->ptr);
					if ( bClosed ) break;
					this->pBClosed = 0;
				}
			}
			return bSend;
		}


		SOCKET s;
		void *ptr;
		_SendBuf *pSendList;
		bool *pBClosed;
		unsigned char conn_evt;
		char state;//fine / error; 'f' , 'e'
		char type;//listen socket /connect / connecting; /// 'l', 'c', 'i'
		DWORD start_time;//for i
		DWORD timeOut;//for i
		DWORD cache_size;
	};
	typedef set<_ConnContext *> C_SetInfo;

	typedef map<SOCKET, _ConnContext *> _SOCK_MAP;
	C_AutoNetwork m_autoSocket;

	bool m_isRun;//运行状态
	bool m_stoped;//

	C_SetInfo m_connSet;//连接集合

	int m_nMaxEnableConn;//最大允许连接

private:
	struct _WEP_fd_set {

		unsigned int fd_count;
		SOCKET  fd_array[0];

		void clear() { this->fd_count = 0; }

		void insert(SOCKET fd) {

			this->fd_array[this->fd_count] = fd;
			this->fd_count++;
		}
		SOCKET max_fd() {

			SOCKET retval = 0;

			for ( unsigned int i = 0; i < this->fd_count; i++ ) {

				if ( this->fd_array[i] > retval ) retval = fd_array[i];
			}
			return retval;
		}

		void erase(SOCKET fd) {

			for ( unsigned int i = 0; i < this->fd_count; i++ ) {

				if ( this->fd_array[i] == fd ) {

					this->fd_count--;
					this->fd_array[i] = this->fd_array[this->fd_count];
					break;
				}
			}
		}

		bool exist(SOCKET fd) {

			for ( unsigned int i = 0; i < this->fd_count; i++ ) {

				if ( this->fd_array[i] == fd ) return true;
			}
			return false;
		}
	};
	_SOCK_MAP m_fdMap;
	_WEP_fd_set *m_pReadArray;
	_WEP_fd_set *m_pWriteArray;
	_WEP_fd_set *m_pErrArray;

public:
	void i_init(I_dataHandler *pHandler) {this->m_pHandler = pHandler;}
private:
	//interface
	virtual HANDLE i_listen(IN WORD port);
	virtual HANDLE i_bind(IN SOCKET s,IN void *ptr,IN bool enableNagle = true);
	virtual HANDLE i_connect(IN DWORD remoteIp,IN WORD remotePort, IN DWORD localIp, IN WORD localPort, IN void *ptr, IN long useTimeMsec);
	virtual void i_close(IN HANDLE hConn);
	virtual bool i_send(IN HANDLE hConn, IN const char *buf, IN int len);
	virtual bool i_start(IN bool no_thread = true);
	virtual bool i_process();
	virtual UINT64 i_param(IN HANDLE hConn, const char *_req_name);
private:
	HANDLE _insert(SOCKET s, void *ptr, bool enableNagle, char sockettype);
private:
	void _fill_fd();
	int _wait(DWORD timeOut);

	void _handle_event();
	void _handle_event(_ConnContext *pConnInfo);
	void _accept_handle(_ConnContext *pConnInfo);
	void _connect_handle(_ConnContext *pConnInfo);
	void _conn_handle(_ConnContext *pConnInfo);
	void _handle_conn_timeOut();
private:
	static void * fnWorkerThread(void *pParam);
	virtual void main();

};

//#pragma pack(pop)
