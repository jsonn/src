/*	$NetBSD: bootxx.c,v 1.9.12.1 2001/03/12 13:29:37 bouyer Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <machine/mon.h>

#include <stand.h>
#include "libsa.h"

/*
 * This is the address where we load the second-stage boot loader.
 */
#define LOADADDR	0x4000

/* This determines the largest boot program we can load. */
#define MAXBLOCKNUM	64

/*
 * These three names are known by installboot.
 * The block_table contains starting block numbers,
 * in terms of 512-byte blocks.  Each non-zero value
 * will result in a read of block_size bytes.
 */
int     	block_size = 512;	/* default */
int     	block_count = MAXBLOCKNUM;	/* length of table */
daddr_t 	block_table[MAXBLOCKNUM] = { 0 };


void
main()
{
	struct open_file	f;
	void	*entry;
	char	*addr;
	int n, error;

#ifdef DEBUG
	printf("bootxx: open...\n");
#endif
	f.f_flags = F_RAW;
	if (devopen(&f, 0, &addr)) {
		printf("bootxx: devopen failed\n");
		return;
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
	return;
}

int
copyboot(fp, addr)
	struct open_file	*fp;
	char			*addr;
{
	int	n, i, blknum;
	char *buf;

	/* Need to use a buffer that can be mapped into DVMA space. */
	buf = alloc(block_size);
	if (!buf)
		panic("bootxx: alloc failed");

	for (i = 0; i < block_count; i++) {

		if ((blknum = block_table[i]) == 0)
			break;

#ifdef DEBUG
		printf("bootxx: block # %d = %d\n", i, blknum);
#endif
		if ((fp->f_dev->dv_strategy)(fp->f_devdata, F_READ,
					   blknum, block_size, buf, &n))
		{
			printf("bootxx: read failed\n");
			return -1;
		}
		if (n != block_size) {
			printf("bootxx: short read\n");
			return -1;
		}
		bcopy(buf, addr, block_size);
		addr += block_size;
	}

	return 0;
}

