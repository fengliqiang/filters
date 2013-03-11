/* bio_proxy.cpp */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#include "bio_proxy.h"
#include <openssl/bio.h>
#ifdef _MSC_VER
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "ssleay32.lib")
#endif
static int proxy_create(BIO *bio)
{
	bio->init = 0;
	bio->num = 0;
	bio->ptr = 0;
	bio->flags = 0;
	return 1;
}
static int proxy_destroy(BIO *bio)
{
	if ( bio == 0 ) return 0;
	if ( bio->shutdown ) {
		bio->init = 0;
		bio->flags = 0;
	}
	return 1;
}

static int proxy_read(BIO *bio, char *buf, int len)
{
	if ( len == 0 ) return 0;
	bio_proxy::I_io *sink = (bio_proxy::I_io*)bio->ptr;

	int ret = sink->proxy_read(buf, len);
	BIO_clear_retry_flags(bio);
	if ( ret <= 0 ) BIO_set_retry_read(bio);
	return ret;
}

static int proxy_write(BIO *bio, const char *buf, int len)
{
	if ( len == 0 ) return 0;
	bio_proxy::I_io *sink = (bio_proxy::I_io*)bio->ptr;

	printf("BIO/filter，调用下层I_pout发送数据\n");

	int ret = sink->proxy_write(buf, len);

	BIO_clear_retry_flags(bio);
	if ( ret <= 0 ) BIO_set_retry_read(bio);
	return ret;
}

static long proxy_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
	switch (cmd) {
	case BIO_C_SET_FD:
		proxy_destroy(bio);
		bio->num = 0;
		bio->ptr = ptr;
		bio->shutdown = (int)num;
		bio->init = 1;
		return 1;
	case BIO_C_GET_FD:
		if ( bio->init ) {
			if ( ptr ) *( (int *)ptr ) = bio->num;
			return bio->num;
		}
		else return -1;
	case BIO_CTRL_GET_CLOSE:
		return bio->shutdown;
	case BIO_CTRL_SET_CLOSE:
		bio->shutdown = (int)num;
		return 1;
	case BIO_CTRL_DUP:
	case BIO_CTRL_FLUSH:
		return 1;
	default:
		break;
	}
	return 0;
}

static int proxy_puts(BIO *bp, const char *str)
{
	return proxy_write(bp, str, strlen(str));
}

static BIO_METHOD proxy_methods =
{
	BIO_TYPE_SOURCE_SINK | 0x80,
	"asio_proxy",
	proxy_write,
	proxy_read,
	proxy_puts,
	0,//proxy_gets,
	proxy_ctrl,
	proxy_create,
	proxy_destroy,
	0,
};

bool bio_proxy::proxy_init(SSL *s, bio_proxy::I_io *proxy)
{
	BIO *bio = BIO_new(&proxy_methods);
	if ( bio == 0 ) return false;
	proxy_ctrl(bio, BIO_C_SET_FD, BIO_NOCLOSE, (void*)proxy);
	SSL_set_bio(s,bio,bio);
	return true;
}
