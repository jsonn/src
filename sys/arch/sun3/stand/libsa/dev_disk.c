/*	$NetBSD: dev_disk.c,v 1.5.14.1 1998/01/27 02:35:32 gwr Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <stand.h>

#include "libsa.h"
#include "dvma.h"
#include "saio.h"

/* #include "dev_disk.h" XXX - stdarg woes */

#define RETRY_COUNT 5

int disk_opencount;
struct saioreq disk_ioreq;

int
disk_open(f, devname)
	struct open_file *f;
	char *devname;		/* Device part of file name (or NULL). */
{
	struct bootparam *bp;
	struct saioreq *si;
	int	error;

#ifdef DEBUG_PROM
	if (debug)
		printf("disk_open: %s\n", devname);
#endif

	si = &disk_ioreq;
	if (disk_opencount == 0) {
		/*
		 * Setup our part of the saioreq.
		 * (determines what gets opened)
		 */
		bp = *romVectorPtr->bootParam;
		si->si_boottab = bp->bootDevice;
		si->si_ctlr = bp->ctlrNum;
		si->si_unit = bp->unitNum;
		si->si_boff = bp->partNum;
		if ((error = prom_iopen(si)) != 0)
			return (error);
	}
	disk_opencount++;

	f->f_devdata = si;
	return 0;
}

int
disk_close(f)
	struct open_file *f;
{
	struct saioreq *si;

#ifdef DEBUG_PROM
	if (debug)
		printf("disk_close: ocnt=%d\n", disk_opencount);
#endif

	si = f->f_devdata;
	f->f_devdata = NULL;
	if (disk_opencount <= 0)
		return 0;
	if (--disk_opencount == 0)
		prom_iclose(si);
	return 0;
}

int
disk_strategy(devdata, flag, dblk, size, buf, rsize)
	void	*devdata;
	int	flag;
	daddr_t	dblk;
	u_int	size;
	char	*buf;
	u_int	*rsize;
{
	struct saioreq *si;
	struct boottab *ops;
	char *dmabuf;
	int retry, si_flag, xcnt;

	si = devdata;
	ops = si->si_boottab;

#ifdef DEBUG_PROM
	if (debug > 1)
		printf("disk_strategy: size=%d dblk=%d\n", size, dblk);
#endif

	dmabuf = dvma_mapin(buf, size);
	si_flag = (flag == F_READ) ? SAIO_F_READ : SAIO_F_WRITE;
	
	/*
	 * The PROM strategy will occasionally return -1 and expect
	 * us to try again.  From mouse@Collatz.McRCIM.McGill.EDU
	 */
	retry = RETRY_COUNT;
	do {
		si->si_bn = dblk;
		si->si_ma = dmabuf;
		si->si_cc =	size;
		xcnt = (*ops->b_strategy)(si, si_flag);
	} while ((xcnt <= 0) && (--retry > 0));

	dvma_mapout(dmabuf, size);

#ifdef DEBUG_PROM
	if (debug > 1)
		printf("disk_strategy: xcnt = %x retries=%d\n",
		   xcnt, RETRY_COUNT - retry);
#endif

	if (xcnt <= 0)
		return (EIO);

	*rsize = xcnt;
	return (0);
}

int
disk_ioctl()
{
	return EIO;
}

