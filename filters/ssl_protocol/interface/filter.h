/* interface/filter.h */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#pragma once
namespace frames {
	namespace filter {
		class I_pin {
		public:
			virtual ~I_pin(){}
			virtual void on_data(const char *data, int len) = 0;
		};
		class I_pout {
			I_pin *_pin;
		public:
			I_pout():_pin(0){}
			virtual ~I_pout(){}
			I_pin *pin() const { return _pin; }
			void set_pin(I_pin *p) {_pin = p; }
			virtual void write(const char *data, int len) = 0;
		};
		class I_filter :public I_pin, public I_pout {
			I_pout *_next;
		public:
			I_pout *next() { return _next; }
		public:
			I_filter():_next(0){}
			virtual ~I_filter(){}
			void connect(I_pout *out) { 
				if ( _next = out ) out->set_pin(this); 
			}
		};
	}
}