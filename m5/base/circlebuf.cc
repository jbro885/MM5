/*
 * Copyright (c) 2002, 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

#include <algorithm>
#include <string>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "base/circlebuf.hh"
#include "base/cprintf.hh"
#include "base/intmath.hh"

using namespace std;

CircleBuf::CircleBuf(int l)
    : _rollover(false), _buflen(l), _size(0), _start(0), _stop(0)
{
    _buf = new char[_buflen];
}

CircleBuf::~CircleBuf()
{
    if (_buf)
	delete [] _buf;
}

void
CircleBuf::dump()
{
    cprintf("start = %10d, stop = %10d, buflen = %10d\n",
	    _start, _stop, _buflen);
    fflush(stdout);
    ::write(STDOUT_FILENO, _buf, _buflen);
    ::write(STDOUT_FILENO, "<\n", 2);
}

void
CircleBuf::flush()
{
    _start = 0;
    _stop = 0;
    _rollover = false;
}

void
CircleBuf::read(char *b, int len)
{
    _size -= len;
    if (_size < 0)
	_size = 0;

    if (_stop > _start) {
	len = min(len, _stop - _start);
	memcpy(b, _buf + _start, len);
	_start += len;
    }
    else {
	int endlen = _buflen - _start;
	if (endlen > len) {
	    memcpy(b, _buf + _start, len);
	    _start += len;
	}
	else {
	    memcpy(b, _buf + _start, endlen);
	    _start = min(len - endlen, _stop);
	    memcpy(b + endlen, _buf, _start);
	}
    }
}

void
CircleBuf::read(int fd, int len)
{
    _size -= len;
    if (_size < 0)
	_size = 0;

    if (_stop > _start) {
	len = min(len, _stop - _start);
	::write(fd, _buf + _start, len);
	_start += len;
    }
    else {
	int endlen = _buflen - _start;
	if (endlen > len) {
	    ::write(fd, _buf + _start, len);
	    _start += len;
	}
	else {
	    ::write(fd, _buf + _start, endlen);
	    _start = min(len - endlen, _stop);
	    ::write(fd, _buf, _start);
	}
    }
}

void
CircleBuf::read(int fd)
{
    _size = 0;

    if (_stop > _start) {
	::write(fd, _buf + _start, _stop - _start);
    }
    else {
	::write(fd, _buf + _start, _buflen - _start);
	::write(fd, _buf, _stop);
    }

    _start = _stop;
}

void
CircleBuf::read(ostream &out)
{
    _size = 0;

    if (_stop > _start) {
	out.write(_buf + _start, _stop - _start);
    }
    else {
	out.write(_buf + _start, _buflen - _start);
	out.write(_buf, _stop);
    }

    _start = _stop;
}

void
CircleBuf::readall(int fd)
{
    if (_rollover)
	::write(fd, _buf + _stop, _buflen - _stop);

    ::write(fd, _buf, _stop);
    _start = _stop;
}

void
CircleBuf::write(char b)
{
    write(&b, 1);
}

void
CircleBuf::write(const char *b)
{
    write(b, strlen(b));
}

void
CircleBuf::write(const char *b, int len)
{
    if (len <= 0)
	return;

    _size += len;
    if (_size > _buflen)
	_size = _buflen;

    int old_start = _start;
    int old_stop = _stop;

    if (len >= _buflen) {
	_start = 0;
	_stop = _buflen;
	_rollover = true;
	memcpy(_buf, b + (len - _buflen), _buflen);
	return;
    }

    if (_stop + len <= _buflen) {
	memcpy(_buf + _stop, b, len);
	_stop += len;
    } else {
	int end_len = _buflen - old_stop;
	_stop = len - end_len;
	memcpy(_buf + old_stop, b, end_len);
	memcpy(_buf, b + end_len, _stop);
	_rollover = true;
    }

    if (old_start > old_stop && old_start < _stop ||
	old_start < old_stop && _stop < old_stop)
	_start = _stop + 1;
}
