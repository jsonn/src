/*	$NetBSD: dev_tape.c,v 1.3.78.2 2009/05/04 08:12:02 yamt Exp $	*/

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
 * This module implements a "raw device" interface suitable for
 * use by the stand-alone I/O library UFS file-system code, and
 * possibly for direct access (i.e. boot from tape).
 *
 * The implementation is deceptively simple because it uses the
 * drivers provided by the Sun PROM monitor.  Note that only the
 * PROM driver used to load the boot program is available here.
 */

#include <sys/types.h>
#include <machine/mon.h>
#include <machine/stdarg.h>
#include <stand.h>

#include "libsa.h"
#include "dvma.h"
#include "saio.h"
#include "dev_tape.h"

extern int debug;

struct saioreq tape_ioreq;

/*
 * This is a special version of devopen() for tape boot.
 * In this version, the file name is a numeric string of
 * one digit, which is passed to the device open so it
 * can open the appropriate tape segment.
 */
int 
devopen(struct open_file *f, const char *fname,	char **file)
{
	struct devsw *dp;

	*file = (char *)fname;
	dp = &devsw[0];
	f->f_dev = dp;

	/* The following will call tape_open() */
	return (dp->dv_open(f, fname));
}

int
tape_open(struct open_file *f, ...)
{
	struct bootparam *bp;
	struct saioreq *si;
	int error, part;
	char *fname;		/* partition number, i.e. "1" */
	va_list ap;

	va_start(ap, f);
	fname = va_arg(ap, char *);
#ifdef DEBUG
	printf("tape_open: part=%s\n", fname);
#endif
	va_end(ap);

	/*
	 * Set the tape segment number to the one indicated
	 * by the single digit fname passed in above.
	 */
	if ((fname[0] < '0') && (fname[0] > '9')) {
		return ENOENT;
	}
	part = fname[0] - '0';

	/*
	 * Setup our part of the saioreq.
	 * (determines what gets opened)
	 */
	si = &tape_ioreq;
	memset(si, 0, sizeof(*si));

	bp = *romVectorPtr->bootParam;
	si->si_boottab = bp->bootDevice;
	si->si_ctlr = bp->ctlrNum;
	si->si_unit = bp->unitNum;
	si->si_boff = part; 	/* default = bp->partNum + 1; */

	error = prom_iopen(si);

#ifdef DEBUG
	printf("tape_open: prom_iopen returned 0x%x\n", error);
#endif

	if (!error)
		f->f_devdata = si;

	return (error);
}

int 
tape_close(struct open_file *f)
{
	struct saioreq *si;

#ifdef DEBUG
	printf("tape_close: calling prom_iclose\n");
#endif

	si = f->f_devdata;
	prom_iclose(si);
	f->f_devdata = NULL;
	return 0;
}

int 
tape_strategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf,
    size_t *rsize)
{
	struct saioreq *si;
	struct boottab *ops;
	char *dmabuf;
	int	si_flag, xcnt;

	si = devdata;
	ops = si->si_boottab;

#ifdef DEBUG
	if (debug > 1)
		printf("tape_strategy: size=%d dblk=%d\n", size, dblk);
#endif

	dmabuf = dvma_mapin(buf, size);
	
	si->si_bn = dblk;
	si->si_ma = dmabuf;
	si->si_cc = size;

	si_flag = (flag == F_READ) ? SAIO_F_READ : SAIO_F_WRITE;
	xcnt = (*ops->b_strategy)(si, si_flag);
	dvma_mapout(dmabuf, size);

#ifdef DEBUG
	if (debug > 1)
		printf("tape_strategy: xcnt = %x\n", xcnt);
#endif

	/* At end of tape, xcnt == 0 (not an error) */
	if (xcnt < 0)
		return (EIO);

	*rsize = xcnt;
	return (0);
}

int 
tape_ioctl(struct open_file *f, u_long cmd, void *data)
{
	return EIO;
}

