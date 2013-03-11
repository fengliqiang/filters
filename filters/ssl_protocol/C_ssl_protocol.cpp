/* C_ssl_protocol.cpp */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#include "C_ssl_protocol.h"
#include <openssl/rand.h>
#include <openssl/err.h>

#include <string>

C_ssl_protocol::C_ssl_protocol(SSL_CTX *ctx, bool b_server)
:_connected(false), _is_server(b_server)
{
	_ssl = SSL_new(ctx);

	if ( _ssl ) {

		if ( ! bio_proxy::proxy_init(_ssl, this) ) {
			printf("init error !\n");
			SSL_free(_ssl);
			_ssl = 0;
		}
	}
}

C_ssl_protocol::~C_ssl_protocol(void)
{
	if ( _ssl ) {
		SSL_shutdown(_ssl); 
		SSL_free(_ssl); 
	}
}

void C_ssl_protocol::loop()
{
	on_data(0, 0);
	write(0, 0);
}

void C_ssl_protocol::write(const char *data, int len)
{
	if ( _connected ) {

		while (_out_cache.size() ) {
			char buffer[1024 * 10];
			int size = _out_cache.pop(buffer, sizeof(buffer));
			int write_size = SSL_write(_ssl, buffer, size);
			assert(write_size = size);
		}
		if ( len ) {
			int size = SSL_write(_ssl, data, len);
			assert(size == len);
		}
	}
	else {
		_out_cache.push(data, len);
	}
}

void C_ssl_protocol::on_data(const char *data, int len)
{
	_in_data = data;
	_in_size = len;
	if ( ! _connected ) {
		_connected = (_is_server? SSL_accept(_ssl): SSL_connect(_ssl)) > 0;
	}

	if ( _connected && ( _in_size || _in_cache.size() ) ) {

		while ( true ) {
			char buffer[1024 * 10];
			int size = SSL_read(_ssl, buffer, sizeof(buffer));
			if ( size <= 0 ) break;
			if ( pin() ) pin()->on_data(buffer, size);
		}
	}
	if ( _in_size ) {
		_in_cache.push(_in_data, _in_size);
		_in_size = 0;
	}
}

int C_ssl_protocol::proxy_read(char *buf, int size)
{
	int ret_size = 0;
	if ( _in_cache.size() ) ret_size += _in_cache.pop(buf, size);
	if ( ret_size < size ) {
		int min_read = (std::min)(size - ret_size, _in_size);
		if ( min_read ) memcpy(buf + ret_size, _in_data, min_read);
		_in_size -= min_read;
		_in_data += min_read;
		ret_size += min_read;
	}
	return ret_size ? ret_size: -1;
}

int C_ssl_protocol::proxy_write(const char *buf, int size)
{
	if ( next() ) next()->write(buf, size);
	return size;
}
C_ssl_client_ctx::C_ssl_client_ctx()
{
	SSL_load_error_strings();
	SSL_library_init();
	_ctx = SSL_CTX_new(SSLv23_client_method());
	srand( (unsigned)time( NULL ) );
	int seed_int[100];
	for( int i = 0; i < 100;i++ )
		seed_int[i] = rand();
	RAND_seed(seed_int, sizeof(seed_int));
	while ( ! RAND_status() ) RAND_poll();

}
C_ssl_client_ctx::~C_ssl_client_ctx()
{
	SSL_CTX_free(_ctx);
	ERR_free_strings();
}

ssl_protocol::I_ctx *ssl_protocol::create_client()
{
	return new C_ssl_client_ctx();
}

