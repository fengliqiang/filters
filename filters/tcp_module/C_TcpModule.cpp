/* C_ssl_protocol.cpp */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#include "C_TcpModule.h"
C_TcpModule::C_TcpModule(void)
:m_isRun(false), m_nMaxEnableConn(64 * 16), m_pReadArray(0), m_pWriteArray(0), m_pErrArray(0), 
m_pHandler(0), m_stoped(false)
{
}

C_TcpModule::~C_TcpModule(void)
{
	if ( this->m_pErrArray ) delete[] (char*)this->m_pErrArray;
	if ( this->m_pReadArray ) delete[] (char*)this->m_pReadArray;
	if ( this->m_pWriteArray ) delete[] (char*)this->m_pWriteArray;
}
inline void setReuse(SOCKET sConn, bool reuse = true)
{
	DWORD bReUse = reuse ? 1: 0;
	setsockopt(sConn, SOL_SOCKET, SO_REUSEADDR, (char*)&bReUse, sizeof(bReUse));

}

inline SOCKET asyncConnect(DWORD dwTargetIp, WORD wTargetPort, DWORD localIp, WORD wLocalPort)
{

	SOCKET sConn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if ( sConn == INVALID_SOCKET ) return 0;

	setReuse(sConn, true);
	sockblock(sConn, false);

	if ( wLocalPort || localIp ) {

		SOCKADDR_IN localAddr = {0};
		localAddr.sin_family = AF_INET;
		localAddr.sin_port = htons(wLocalPort);
		localAddr.sin_addr.s_addr = htonl(localIp);

		bind(sConn, (sockaddr*)&localAddr, sizeof(localAddr));
	}

	SOCKADDR_IN remoteAddr = {0};
	remoteAddr.sin_addr.s_addr = htonl(dwTargetIp);
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = htons(wTargetPort);

	if ( connect(sConn, (sockaddr*)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR ) {

		if ( GetLastError() == 10035 ) return sConn;
		closesocket(sConn); return 0;
	}
	return sConn;
}
HANDLE C_TcpModule::i_listen(WORD port)
{

	SOCKET sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); if ( sListen == INVALID_SOCKET ) return 0;

	setReuse(sListen, false);
	sockblock(sListen, false);

	SOCKADDR_IN addrLocal = {0};
	addrLocal.sin_family = AF_INET;
	addrLocal.sin_port = htons(port);

	if ( bind(sListen, (sockaddr*)&addrLocal, sizeof(addrLocal)) == SOCKET_ERROR ) {

		closesocket(sListen); return 0;
	}

	if ( listen(sListen, 32) == SOCKET_ERROR ) {

		closesocket(sListen); return 0;
	}

	return this->_insert(sListen, 0, true, 'l');

}

HANDLE C_TcpModule::i_connect(IN DWORD remoteIp,IN WORD remotePort, IN DWORD localIp, IN WORD localPort, IN void *ptr, IN long useTimeMsec)
{

	if ( this->m_connSet.size() >= (size_t)this->m_nMaxEnableConn ) return 0;

	if ( ! this->m_isRun ) return 0;
	SOCKET s = asyncConnect(remoteIp, remotePort, localIp, localPort);
    if ( (s == 0) || (s == INVALID_SOCKET) ) {
        return 0;
    }

	_ConnContext *pConnInfo =  new _ConnContext(s, ptr, (DWORD)useTimeMsec);
    if ( pConnInfo == 0 ) {
        return 0;
    }

	sockblock(s, false);
	this->m_connSet.insert(pConnInfo);
	this->m_fdMap[s] = pConnInfo;

	return (HANDLE) pConnInfo;
}
HANDLE C_TcpModule::i_bind(IN SOCKET s,IN void *ptr,IN bool enableNagle)
{
	return this->_insert(s, ptr, enableNagle, 'c');
}

HANDLE C_TcpModule::_insert(SOCKET s, void *ptr, bool enableNagle, char sockettype)
{

	if ( this->m_connSet.size() >= (size_t)this->m_nMaxEnableConn ) return 0;

	if ( ! this->m_isRun ) return 0;

	if ( s == INVALID_SOCKET || s == 0 ) return 0;

	_ConnContext *pConnInfo = new _ConnContext(s, ptr, sockettype);
	if ( pConnInfo == 0 ) return 0;

	sockblock(s, false);

	if ( pConnInfo->type == 'c' ) {

		char chOpt = enableNagle? 0: 1;
		setsockopt( s, IPPROTO_TCP, TCP_NODELAY, &chOpt, sizeof(chOpt) ); //是否允许Nagle算法
	}
	this->m_connSet.insert(pConnInfo);
	this->m_fdMap[s] = pConnInfo;

	return (HANDLE) pConnInfo;

}

void C_TcpModule::i_close(HANDLE hConn)
{

	_ConnContext *pConnInfo = (_ConnContext*)hConn;

	if ( this->m_connSet.find(pConnInfo) != this->m_connSet.end() ) {

		this->m_connSet.erase(pConnInfo);

		if ( pConnInfo->pBClosed ) *(pConnInfo->pBClosed) = true;

		if ( pConnInfo->s ) closesocket(pConnInfo->s);

		this->m_fdMap.erase(pConnInfo->s);

		delete pConnInfo;

	}

}


bool C_TcpModule::i_send(HANDLE hConn, const char *buf, int len)
{

	if ( this->m_connSet.find((_ConnContext *)hConn) != this->m_connSet.end() ) {

		_ConnContext *pConnInfo = (_ConnContext*)hConn;

		if ( pConnInfo->s ) {

			if ( ! pConnInfo->_insert(buf, len) ) return false;

            return pConnInfo->_send();
		}

	}
	return false;
}

UINT64 C_TcpModule::i_param(IN HANDLE hConn, const char *_req_name)
{
	if ( strcmp(_req_name, "send_cache") == 0 ) {

		if ( this->m_connSet.find((_ConnContext *)hConn) != this->m_connSet.end() ) {

			_ConnContext *pConnInfo = (_ConnContext*)hConn;

			return pConnInfo->s ? pConnInfo->cache_size: 0;
		}
	}
	return -1;
}

bool C_TcpModule::i_start(IN bool no_thread)
{
	this->m_pReadArray = (_WEP_fd_set*) new char[sizeof(SOCKET) * this->m_nMaxEnableConn + sizeof(_WEP_fd_set)];
	if ( this->m_pReadArray == 0 ) return false;
	this->m_pWriteArray = (_WEP_fd_set*) new char[sizeof(SOCKET) * this->m_nMaxEnableConn + sizeof(_WEP_fd_set)];
	if ( this->m_pWriteArray == 0 ) {
		delete this->m_pReadArray; this->m_pReadArray = 0; return false;
	}
	this->m_pErrArray = (_WEP_fd_set*) new char[sizeof(SOCKET) * this->m_nMaxEnableConn + sizeof(_WEP_fd_set)];
	if ( this->m_pErrArray == 0 ) {
		delete this->m_pReadArray; this->m_pReadArray = 0;
		delete this->m_pWriteArray; this->m_pWriteArray = 0;
		return false;
	}
	m_isRun = true;

	if ( ! no_thread ) {
		//创建线程
		m_stoped = false;
	}
	else m_stoped = true;
	return true;
}

void C_TcpModule::_fill_fd()
{
	this->m_pReadArray->clear();
	this->m_pWriteArray->clear();
	this->m_pErrArray->clear();

	for ( C_SetInfo::iterator it = this->m_connSet.begin(); it != this->m_connSet.end(); it++ ) {
		_ConnContext *pConnInfo = *it;
		if ( pConnInfo->s ) {

			switch ( pConnInfo->type ) {
				case 'c':
					this->m_pReadArray->insert(pConnInfo->s);
					this->m_pErrArray->insert(pConnInfo->s);
					break;
				case 'l':
					this->m_pReadArray->insert(pConnInfo->s);
					break;
				case 'i':
					this->m_pWriteArray->insert(pConnInfo->s);
					this->m_pErrArray->insert(pConnInfo->s);
					break;
				default:break;
			}
		}
	}

}

int C_TcpModule::_wait(DWORD __timeout)
{

	if ( (m_pErrArray->fd_count == 0) && (m_pReadArray->fd_count == 0) && (m_pWriteArray->fd_count == 0 ) ) return 0;
	unsigned int max_err_fd = m_pErrArray->max_fd();
	unsigned int max_read_fd = m_pReadArray->max_fd();
	unsigned int max_write_fd = m_pWriteArray->max_fd();
	unsigned int max_fd = max(max_err_fd, max_read_fd);
	max_fd = max(max_fd, max_write_fd);
	timeval timeout = { __timeout / 1000, (__timeout % 1000) * 1000 };
	return select(max_fd + 1, m_pReadArray->fd_count?(fd_set *)m_pReadArray:0, m_pWriteArray->fd_count?(fd_set*)m_pWriteArray:0, m_pErrArray->fd_count?(fd_set*)m_pErrArray:0, &timeout);
}

bool C_TcpModule::i_process()
{

	this->m_pHandler->i_onLoop();

	bool bSend = false;
    DWORD tick1 = GetTickCount();
	for (C_SetInfo::iterator it = this->m_connSet.begin(); it != this->m_connSet.end(); ) {
		_ConnContext *pConnInfo = *it; it++;
		if ( pConnInfo->_send(this->m_pHandler) ) bSend = true;
	}
	this->_fill_fd();
	int evt_count = this->_wait(1);
	if ( evt_count <= 0 ) return bSend;
	this->_handle_event();
	this->_handle_conn_timeOut();
	return true;
}
void C_TcpModule::_handle_event()
{
	C_SetInfo tmp;
	for ( unsigned int i = 0; i < this->m_pReadArray->fd_count; i++ ) {
		
		_SOCK_MAP::iterator it = this->m_fdMap.find(this->m_pReadArray->fd_array[i]);

		if ( it != this->m_fdMap.end() ) {
			it->second->conn_evt = _SOCK_IN; tmp.insert(it->second);
		}
	}
	for ( unsigned int i = 0; i < this->m_pWriteArray->fd_count; i++ ) {

		_SOCK_MAP::iterator it = this->m_fdMap.find(this->m_pWriteArray->fd_array[i]);

		if ( it != this->m_fdMap.end() ) {
			it->second->conn_evt |= _SOCK_OUT; tmp.insert(it->second);
		}
	}
	for ( unsigned int i = 0; i < this->m_pErrArray->fd_count; i++ ) {

		_SOCK_MAP::iterator it = this->m_fdMap.find(this->m_pErrArray->fd_array[i]);

		if ( it != this->m_fdMap.end() ) {
			it->second->conn_evt |= _SOCK_ERR; tmp.insert(it->second);
		}
	}

	for ( C_SetInfo::iterator it = tmp.begin(); it != tmp.end(); it++ ) this->_handle_event(*it);

}
void C_TcpModule::_handle_event(_ConnContext *pConnInfo)
{
	C_SetInfo::iterator it = this->m_connSet.find(pConnInfo);
	if ( it == this->m_connSet.end() ) return;

	switch ( pConnInfo->type ) {
		case 'c':
			this->_conn_handle(pConnInfo);
			break;
		case 'l':
			this->_accept_handle(pConnInfo);
			break;
		case 'i':
			this->_connect_handle(pConnInfo);
			break;
		default:
			break;
	}
}

void C_TcpModule::_accept_handle(_ConnContext *pConnInfo)
{
	if ( pConnInfo->s ) {

		SOCKADDR_IN acceptAddr = {0};
		socklen_t acceptAddrLen = sizeof(acceptAddr);
		for ( SOCKET s = accept(pConnInfo->s, (sockaddr*)&acceptAddr, &acceptAddrLen); (s != INVALID_SOCKET) && (s != 0); s = accept(pConnInfo->s, (sockaddr*)&acceptAddr, &acceptAddrLen) ) {

			if ( this->m_pHandler ) {

				SOCKADDR_IN addr = {0}; socklen_t namelen = sizeof(addr); getsockname(s, (sockaddr*)&addr, &namelen);
				this->m_pHandler->i_onAccept((HANDLE)pConnInfo, s, ntohl(acceptAddr.sin_addr.s_addr), ntohs(acceptAddr.sin_port),ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
			}
			else closesocket(s);
		}
	}
}

void C_TcpModule::_connect_handle(_ConnContext *pConnInfo)
{

	SOCKET s = 0;
	void *ptr = pConnInfo->ptr;
	if ( (pConnInfo->conn_evt & _SOCK_ERR) == 0 ) s = pConnInfo->s;
	else closesocket(pConnInfo->s);
	this->m_fdMap.erase(pConnInfo->s);
	this->m_connSet.erase(pConnInfo);
	delete pConnInfo;

    if ( this->m_pHandler ) {
        this->m_pHandler->i_onConnected(s, (HANDLE)pConnInfo, ptr);
    }
	else if ( s ) closesocket(s);

}

void C_TcpModule::_conn_handle(_ConnContext *pConnInfo)
{
	bool bBreak = false;
	if (pConnInfo->conn_evt & _SOCK_IN ) {

		char buffer[102400];
        for ( int i = 0; (i < 3) && (! bBreak); i++ ) {
            int nRecv = recv(pConnInfo->s, buffer, sizeof(buffer), 0);
            if ( nRecv == 0 ) {
                bBreak = true; break;
            }
            if ( nRecv < 0 ) {
                DWORD error_code = GetLastError();
                if ( error_code != WSAEWOULDBLOCK ) bBreak = true;
                break;
            }

            bool bHandle = this->m_pHandler->i_onData((HANDLE)pConnInfo, buffer, nRecv, pConnInfo->ptr);
            if ( ! bHandle ) {
                bBreak = true; break;
            }
            if ( this->m_connSet.find(pConnInfo) == this->m_connSet.end() ) return;
            if ( nRecv < sizeof(buffer) ) break;

        }
	}

	if ( bBreak || ( (pConnInfo->conn_evt & _SOCK_ERR) != 0 ) ) {

        if ( this->m_connSet.find(pConnInfo) != this->m_connSet.end() ) {

            this->m_pHandler->i_onBreak((HANDLE)pConnInfo, pConnInfo->ptr);
        }
	}
}

void C_TcpModule::_handle_conn_timeOut()
{
	DWORD currTime = GetTickCount();
	for ( C_SetInfo::iterator it = this->m_connSet.begin(); it != this->m_connSet.end(); ) {
		_ConnContext *pConnInfo = *it; it++;
		if ( pConnInfo->type == 'i' ) {
			if ( currTime - pConnInfo->start_time >= pConnInfo->timeOut ) {

				void *ptr = pConnInfo->ptr;
				closesocket(pConnInfo->s);
				this->m_fdMap.erase(pConnInfo->s);
				this->m_connSet.erase(pConnInfo);
				delete pConnInfo;
				this->m_pHandler->i_onConnected(0, (HANDLE)pConnInfo, ptr);
			}
		}
	}
}

void C_TcpModule::main()
{

	while ( m_isRun ) {

		if ( ! i_process() ) Sleep(1);
	}
	m_stoped = true;
}

I_tcpModule *createTcpModule(IN I_dataHandler *dataHandler)
{
	C_TcpModule *instance = new C_TcpModule();
	instance->i_init(dataHandler);
	return instance;
}

void destroyTcpModule(I_tcpModule *instance)
{
	delete instance;
}
