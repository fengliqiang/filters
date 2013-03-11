/* C_ssl_protocol.h */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#pragma once
#include "buffer/cache_buffer.h"
#include "protocol_ssl.h"
#include "bio_proxy.h"
class C_ssl_protocol :public ssl_protocol::I_ssl_filter, public bio_proxy::I_io
{
	C_cache_buffer<> _in_cache;
	C_cache_buffer<> _out_cache;
	bool _connected;
	SSL *_ssl;
private:
	const char *_in_data;
	int _in_size;
	bool _is_server;
private:
	virtual int proxy_read(char *buf, int size);
	virtual int proxy_write(const char *buf, int size);

public:
	C_ssl_protocol(SSL_CTX *ctx, bool b_server = false);
	virtual ~C_ssl_protocol(void);
	virtual void write(const char *data, int len);
	virtual void on_data(const char *data, int len);
	virtual void destroy() { delete this; }
	virtual void loop();
	bool fine() const{ return _ssl != 0; }
};

class C_ssl_client_ctx :public ssl_protocol::I_ctx {
	SSL_CTX *_ctx;
public:
	C_ssl_client_ctx();
	virtual ~C_ssl_client_ctx();
	virtual void destroy() { delete this; }
	virtual ssl_protocol::I_ssl_filter *create() { 

		C_ssl_protocol *filter = new C_ssl_protocol(_ctx);
		if ( filter->fine() ) return filter;
		delete filter;
		return 0;
	}
};
