/* buffer/cache_buffer.h */
/* Copyright (C) 2013 fengliqiang (mr.fengliqiang@gmail.com)
 * All rights reserved.
 *
 */
#pragma once
#include <memory.h>
#include <assert.h>

template <int block_size = 1024>
class C_cache_buffer
{
	template <class T> 
	static const T &min(const T &a, const T &b) { return a < b ? a: b; }
private:
	struct Buffer_node {

		Buffer_node *next_node;
		unsigned int used;
		unsigned int startPos;
		char buffer[block_size];

		Buffer_node(): next_node(0), startPos(0), used(0) { }
		Buffer_node(const char *buf, unsigned int len):startPos(0), next_node(0) {

			this->used = min(sizeof(this->buffer), len);
			if ( this->used ) memcpy( this->buffer, buf, this->used );
			if ( this->used != len ) {
				buf += used; len -= used;
				Buffer_node *p = this;
				while ( len ) {
					unsigned int size = min(used, len);
					p->next_node = new Buffer_node(buf, size);
					buf += size; len -= size;
					p = p->next_node;
				}
			}
		}

		Buffer_node *push(const char *buf, unsigned int len) {

			if ( this->next_node ) return this->next_node->push(buffer, len);
			if ( this->used + len <= sizeof(this->buffer) ) {
				//挪移数据到最前
				if ( this->startPos ) {
					for ( unsigned int i = 0; i < this->used; i++ ) this->buffer[i] = this->buffer[i+this->startPos];
				}
				this->startPos = 0;
				//复制新数据
				memcpy(this->buffer + this->used, buf, len); this->used += len;
				return this;
			}
			else {

				this->next_node = new Buffer_node(buf, len);
				return this->next_node->lastNode();
			}
		}

		unsigned int pop(char *buf, unsigned int len) {

			unsigned int popLen = min(this->used, len);
			if ( popLen ) memcpy(buf, this->buffer + this->startPos, popLen);
			this->used -= popLen;
			if ( this->used ) this->startPos += popLen;
			else this->startPos = 0;
			return popLen;
		}
		unsigned int pop(unsigned int len) {

			unsigned int popLen = min(this->used, len);
			this->used -= popLen;
			if ( this->used ) this->startPos += popLen;
			else this->startPos = 0;
			return popLen;
		}

		unsigned int peek(char *buf, unsigned int len) const {

			unsigned int peekLen = min(this->used, len);
			if ( peekLen ) memcpy(buf, this->buffer + this->startPos, peekLen);
			return peekLen;
		}

		unsigned int peek(unsigned int _offset, char *_buffer, unsigned int _len) const {

			unsigned int ret_size = min(used - _offset, _len);
			unsigned int copied = 0;
			unsigned int copy_begin = startPos + _offset;
			if ( copy_begin < sizeof(buffer) ) {

				copied = min(sizeof(buffer) - copy_begin, ret_size);
				memcpy(_buffer, buffer + copy_begin, copied);
			}
			if ( copied < ret_size ) memcpy(_buffer + copied, buffer, ret_size - copied);
			return ret_size;
		}
		unsigned int replace(unsigned int _offset, const char *_buffer, unsigned int _len) {

			unsigned int ret_size = min(used - _offset, _len);
			unsigned int copied = 0;
			unsigned int copy_begin = startPos + _offset;
			if ( copy_begin < sizeof(buffer) ) {

				copied = min(sizeof(buffer) - copy_begin, ret_size);
				memcpy(buffer + copy_begin, _buffer, copied);
			}
			if ( copied < ret_size ) memcpy(buffer, _buffer + copied, ret_size - copied);
			return ret_size;
		}

		Buffer_node *lastNode() const {

			if ( ! this ) return 0;
			Buffer_node *pLast = (Buffer_node *)this;
			while ( pLast->next_node ) pLast = pLast->next_node;
			return pLast;
		}

	};
private:
	unsigned int buf_size;//数据长度
	Buffer_node *first_node;//首节点
	Buffer_node *last_node;//尾节点
public:
	C_cache_buffer & operator = (const C_cache_buffer &_right) {

		clear();
		if ( _right.size() ) {
			for ( Buffer_node *last = _right.m_first; last; last = last->next_node ) {
				char buffer[block_size];
				int size = last->peek(buffer, sizeof(buffer));
				push(buffer, size);
			}
		}
	}
	C_cache_buffer(const C_cache_buffer &_right):buf_size(0), first_node(0), last_node(0) {
		clear();
		if ( _right.size() ) {
			for ( Buffer_node *last = _right.m_first; last; last = last->next_node ) {
				char buffer[block_size];
				int size = last->peek(buffer, sizeof(buffer));
				push(buffer, size);
			}
		}
	}
public:
	C_cache_buffer(void):buf_size(0), first_node(0), last_node(0) {}

public:
	~C_cache_buffer(void){

		for ( Buffer_node *pNextBuf = this->first_node; pNextBuf;) {

			Buffer_node *pLast = pNextBuf->next_node;
			delete pNextBuf; pNextBuf = pLast;
		}
	}

public:
	bool push(const char *buffer, unsigned int len) {//将数据推入缓冲区

		if ( len == 0 ) return true;

		if ( this->last_node ) {

			this->last_node = this->last_node->push(buffer, len); this->buf_size += len;
		}
		else {

			this->first_node = new Buffer_node(buffer, len);
			this->last_node = this->first_node->lastNode(); this->buf_size = len;
		}
		return true;
	}

public:
	unsigned int pop(char *buffer, unsigned int len) {//取数据，返回取到的数据长度，对象内部将取走部分数据删除

		if ( len == 0 ) return 0;
		unsigned int popLen = min(this->buf_size, len);
		Buffer_node *last = this->first_node;

		for ( unsigned int right = 0 ; (right < popLen) && last; ) {

			right += last->pop(buffer + right, popLen - right);

			if ( ! last->used ) {

				Buffer_node *pTmp = last; last = last->next_node;
				delete pTmp;
			}
		}
		this->first_node = last;
		this->last_node = this->first_node ? this->last_node: this->first_node;
		this->buf_size -= popLen;
		return popLen;
	}

	unsigned int pop(unsigned int len) {//取数据，返回取到的数据长度，对象内部将取走部分数据删除

		if ( len == 0 ) return 0;
		unsigned int popLen = min(this->buf_size, len);
		Buffer_node *last = this->first_node;

		for ( unsigned int right = 0 ; (right < popLen) && last; ) {

			right += last->pop(popLen - right);

			if ( ! last->used ) {

				Buffer_node *pTmp = last; last = last->next_node;
				delete pTmp;
			}
		}
		this->first_node = last;
		this->last_node = this->first_node ? this->last_node: this->first_node;
		this->buf_size -= popLen;
		return popLen;
	}

public:
	unsigned int peek(char *buffer, unsigned int len) const {//查看数据，返回取到数据的长度，对象内部的数据不动

		if ( len == 0 ) return 0;
		unsigned int peekLen = min(this->buf_size, len);
		Buffer_node *last = this->first_node;

		for ( unsigned int right = 0 ; (right < peekLen) && last; ) {

			unsigned int nodLen = last->peek(buffer + right, peekLen - right);
			right += nodLen;
			if ( last->used == nodLen ) last = last->next_node;
		}
		return peekLen;
	}

	unsigned int peek(unsigned int _start, char *_buffer, unsigned int _len) const {
		//
		if ( _len == 0 ) return 0;
		if ( buf_size <= _start ) return 0;
		unsigned int ret_size = min(buf_size - _start, _len);
		int offset = 0;
		Buffer_node *curr_block = seek(_start, offset);
		unsigned int copied = 0;
		for ( ; (copied < ret_size) && curr_block; curr_block = curr_block->next_node ) {

			copied += curr_block->peek(offset, _buffer + copied, ret_size - copied);
			offset = 0;
		}
		assert(copied == ret_size);
		return copied;
	}

	unsigned int replace(unsigned int _start, const char *_buffer, unsigned int _len) const {
		//
		if ( _len == 0 ) return 0;
		if ( buf_size <= _start ) return 0;
		unsigned int ret_size = min(buf_size - _start, _len);
		int offset = 0;
		Buffer_node *curr_block = seek(_start, offset);
		unsigned int copied = 0;
		for ( ; (copied < ret_size) && curr_block; curr_block = curr_block->next_node ) {

			copied += curr_block->replace(offset, _buffer + copied, ret_size - copied);
			offset = 0;
		}
		assert(copied == ret_size);
		return copied;
	}

private:

	Buffer_node *seek(int _start, int &_offset) const {

		int seek_pos = 0;

		Buffer_node *next = first_node;

		for (  ; next ; seek_pos += next->used, next = next->next_node ) {

			if ( seek_pos + next->used > (unsigned int)_start ) break;
		}

		if ( next ) {

			_offset = _start - seek_pos;
		}
		return next;
	}

public:
	unsigned int size() const { return this->buf_size; }

public:
	void clear() {

		for ( Buffer_node *last = this->first_node; last ; last = this->first_node ) {

			this->first_node = this->first_node->next_node; delete last;
		}
		this->last_node = 0;
		this->buf_size = 0;
	}

};

