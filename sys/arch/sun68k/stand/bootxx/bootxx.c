/*	$NetBSD: bootxx.c,v 1.10.78.2 2009/05/04 08:12:01 yamt Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a generic "first-stage" boot program.
 *
 * Note that this program has absolutely no filesystem knowledge!
 *
 * Instead, this uses a table of disk block numbers that are
 * filled in by the installboot program such that this program
 * can load the "second-stage" boot program.
 */

#include <sys/param.h>
#include <sys/bootblock.h>
#include <machine/mon.h>

#include <stand.h>
#include "libsa.h"

int copyboot(struct open_file *, char *);

/*
 * This is the address where we load the second-stage boot loader.
 */
#define LOADADDR	0x4000

/*
 * The contents of the sun68k_bbinfo below are set by installboot(8)
 * to hold the filesystem data of the second-stage boot program
 * (typically `/ufsboot'): filesystem block size, # of filesystem
 * blocks and the block numbers themselves.
 */
struct shared_bbinfo bbinfo = {
	{ SUN68K_BBINFO_MAGIC },
	0,
	SHARED_BBINFO_MAXBLOCKS,
	{ 0 }
};

int 
main(void)
{
	struct open_file	f;
	void	*entry;
	char	*addr;
	int error;

#ifdef DEBUG
	printf("bootxx: open...\n");
#endif
	f.f_flags = F_RAW;
	if (devopen(&f, 0, &addr)) {
		putstr("bootxx: devopen failed\n");
		return 1;
	}

	addr = (char*)LOADADDR;
	error = copyboot(&f, addr);
	f.f_dev->dv_close(&f);
	if (!error) {
#ifdef DEBUG
		printf("bootxx: start 0x%x\n", (long)addr);
#endif
		entry = addr;
		chain_to(entry);
	}
	/* copyboot had a problem... */
	return 0;
}

int 
copyboot(struct open_file *fp, char *addr)
{
	size_t n;
	int i, blknum;
	char *buf;

	/* Need to use a buffer that can be mapped into DVMA space. */
	buf = alloc(bbinfo.bbi_block_size);
	if (!buf)
		panic("bootxx: alloc failed");

	for (i = 0; i < bbinfo.bbi_block_count; i++) {

		if ((blknum = bbinfo.bbi_block_table[i]) == 0)
			break;

#ifdef DEBUG
		printf("bootxx: block # %d = %d\n", i, blknum);
#endif
		if ((fp->f_dev->dv_strategy)(fp->f_devdata, F_READ, blknum,
					   bbinfo.bbi_block_size, buf, &n))
		{
			putstr("bootxx: read failed\n");
			return -1;
		}
		if (n != bbinfo.bbi_block_size) {
			putstr("bootxx: short read\n");
			return -1;
		}
		memcpy(addr, buf, bbinfo.bbi_block_size);
		addr += bbinfo.bbi_block_size;
	}

	return 0;
}
