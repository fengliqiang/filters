/* bio_proxy.h */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#pragma once
#include <openssl/ssl.h>
namespace bio_proxy {
	class I_io {
	public:
		virtual ~I_io(){}
		virtual int proxy_read(char *buf, int size) = 0;
		virtual int proxy_write(const char *buf, int size) = 0;
	};
	bool proxy_init(SSL *ssl, I_io *proxy);
}