/*	$NetBSD: stand_user.c,v 1.2.46.3 2004/09/21 13:17:11 skrll Exp $	*/

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <lib/libsa/stand.h>

#include "sanamespace.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <machine/sysarch.h>
#include <err.h>

/*
 * Harness for test of standalone code in user space.
 * XXX Requires silly namespace games.
 */

#ifndef HEAPSIZE
#define HEAPSIZE (128*1024)
#endif

int samain __P((void));

int
main()
{
	char *h = malloc(HEAPSIZE);
	setheap(h, h + HEAPSIZE);

	return (samain());
}

void
_rtt()
{
	warnx("_rtt called");
	_exit(1);
}

int
getsecs()
{
	struct timeval t;
	gettimeofday(&t, 0);
	return (t.tv_sec);
}

void
delay(t)
	int t;
{
	struct timeval to;
	to.tv_sec = 0;
	to.tv_usec = t;
	select(0, 0, 0, 0, &to);
}

/* make output appear unbuffered */
void
saputchar(c)
	int c;
{
	putchar(c);
	fflush(stdout);
}

/*
 * some functions to get access to the hardware
 */

static int memfd, memcnt;

caddr_t
mapmem(offset, len)
	int offset, len;
{
	caddr_t base;

	if (memcnt == 0)
		memfd = open("/dev/mem", O_RDWR, 0);
	if (memfd < 0) {
		warn("open /dev/mem");
		return (0);
	}
	base = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED,
		    memfd, offset);
	if (base == (caddr_t)-1) {
		warn("mmap %x-%x", offset, offset + len - 1);
		return (0);
	}
	memcnt++;
	return (base);
}

void
unmapmem(addr, len)
	caddr_t addr;
	int len;
{
	munmap(addr, len);
	memcnt--;
	if (memcnt == 0)
		close(memfd);
}

int
mapio()
{
	int res;

	res = i386_iopl(1);
	if (res)
		warn("i386_iopl");
	return (res);
}

int ourseg = 12345;
