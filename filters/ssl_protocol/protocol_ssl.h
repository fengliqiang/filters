/* C_ssl_protocol.cpp */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#pragma once
#include "interface/filter.h"
namespace ssl_protocol {
	class I_ssl_filter :public frames::filter::I_filter {
	public:
		virtual ~I_ssl_filter(){}
		virtual void destroy() = 0;
		virtual void loop() = 0;
	};
	class I_ctx {
	public:
		virtual ~I_ctx(){}
		virtual void destroy() = 0;
		virtual I_ssl_filter *create() = 0;
	};
	I_ctx *create_client();
	I_ctx *create_server(const char *cert_path);
}