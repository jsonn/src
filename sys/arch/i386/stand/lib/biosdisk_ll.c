/*	$NetBSD: biosdisk_ll.c,v 1.18.2.3 2004/09/21 13:17:10 skrll Exp $	 */

/*
 * Copyright (c) 1996
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996
 * 	Perry E. Metzger.  All rights reserved.
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
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * shared by bootsector startup (bootsectmain) and biosdisk.c
 * needs lowlevel parts from bios_disk.S
 */

#include <lib/libsa/stand.h>

#include "biosdisk_ll.h"
#include "diskbuf.h"
#include "libi386.h"

static int do_read(struct biosdisk_ll *, daddr_t, int, char *);

/*
 * we get from get_diskinfo():
 * xxxx  %ch  %cl  %dh (registers after int13/8), ie
 * xxxx cccc Csss hhhh
 */
#define	SPT(di)		(((di)>>8)&0x3f)
#define	HEADS(di)	(((di)&0xff)+1)
#define CYL(di)		(((((di)>>16)&0xff)|(((di)>>6)&0x300))+1)

#ifndef BIOSDISK_RETRIES
#define BIOSDISK_RETRIES 5
#endif

int 
set_geometry(struct biosdisk_ll *d, struct biosdisk_ext13info *ed)
{
	int diskinfo;
	char buf[512];

	diskinfo = get_diskinfo(d->dev);
	d->sec = SPT(diskinfo);
	d->head = HEADS(diskinfo);
	d->cyl = CYL(diskinfo);
	d->chs_sectors = d->sec * d->head * d->cyl;

	d->flags = 0;
	if ((d->dev & 0x80) && int13_extension(d->dev)) {
		d->flags |= BIOSDISK_EXT13;
		if (ed != NULL) {
			ed->size = sizeof *ed;
			int13_getextinfo(d->dev, ed);
		}
	}

	/*
	 * If the drive is 2.88 floppy drive, check that we can actually
	 * read sector >= 18. If not, assume 1.44 floppy disk.
	 */
	if (d->dev == 0 && SPT(diskinfo) == 36) {
		if (biosread(d->dev, 0, 0, 18, 1, buf)) {
			d->sec = 18;
			d->chs_sectors /= 2;
		}			
	}

	/*
	 * get_diskinfo assumes floppy if BIOS call fails. Check at least
	 * "valid" geometry.
	 */
	return (!d->sec || !d->head);
}

/*
 * Global shared "diskbuf" is used as read ahead buffer.  For reading from
 * floppies, the bootstrap has to be loaded on a 64K boundary to ensure that
 * this buffer doesn't cross a 64K DMA boundary.
 */
#define RA_SECTORS      (DISKBUFSIZE / BIOSDISK_SECSIZE)
static int      ra_dev;
static int      ra_end;
static int      ra_first;

/*
 * Because some older BIOSes have bugs in their int13 extensions, we
 * only try to use the extended read if the I/O request can't be addressed
 * using CHS.
 *
 * Of course, some BIOSes have bugs in ths CHS read, such as failing to
 * function properly if the MBR table has a different geometry than the
 * BIOS would generate internally for the device in question, and so we
 * provide a way to force the extended on hard disks via a compile-time
 * option.
 */
#if defined(FORCE_INT13EXT)
#define	NEED_INT13EXT(d, dblk, num)				\
	(((d)->dev & 0x80) != 0)
#else
#define	NEED_INT13EXT(d, dblk, num)				\
	(((d)->dev & 0x80) != 0 && ((dblk) + (num)) >= (d)->chs_sectors)
#endif

static int
do_read(struct biosdisk_ll *d, daddr_t dblk, int num, char *buf)
{
	int		cyl, head, sec, nsec, spc, dblk32;
	struct {
		int8_t	size;
		int8_t	resvd;
		int16_t	cnt;
		int16_t	off;
		int16_t	seg;
		int64_t	sec;
	}		ext;

	if (NEED_INT13EXT(d, dblk, num)) {
		if (!(d->flags & BIOSDISK_EXT13))
			return -1;
		ext.size = sizeof(ext);
		ext.resvd = 0;
		ext.cnt = num;
		/* seg:off of physical address */
		ext.off = (int)buf & 0xf;
		ext.seg = vtophys(buf) >> 4;
		ext.sec = dblk;

		if (biosextread(d->dev, &ext)) {
			(void)biosdiskreset(d->dev);
			return -1;
		}

		return ext.cnt;
	} else {
		dblk32 = (int)dblk;
		spc = d->head * d->sec;
		cyl = dblk32 / spc;
		head = (dblk32 % spc) / d->sec;
		sec = dblk32 % d->sec;
		nsec = d->sec - sec;

		if (nsec > num)
			nsec = num;

		if (biosread(d->dev, cyl, head, sec, nsec, buf)) {
			(void)biosdiskreset(d->dev);
			return -1;
		}

		return nsec;
	}
}

/* NB if 'cold' is set below not all of the program is loaded, so
 * musn't use data segment, bss, call library functions or do
 * read-ahead.
 */

int 
readsects(struct biosdisk_ll *d, daddr_t dblk, int num, char *buf, int cold)
{
#ifdef BOOTXX
#define cold 1		/* collapse out references to diskbufp */
#endif
	while (num) {
		int             nsec;

		/* check for usable data in read-ahead buffer */
		if (cold || diskbuf_user != &ra_dev || d->dev != ra_dev
		    || dblk < ra_first || dblk >= ra_end) {

			/* no, read from disk */
			char           *trbuf;
			int maxsecs;
			int retries = BIOSDISK_RETRIES;

			if (cold) {
				/* transfer directly to buffer */
				trbuf = buf;
				maxsecs = num;
			} else {
				/* fill read-ahead buffer */
				trbuf = alloc_diskbuf(0); /* no data yet */
				maxsecs = RA_SECTORS;
			}

			while ((nsec = do_read(d, dblk, maxsecs, trbuf)) < 0) {
#ifdef DISK_DEBUG
				if (!cold)
					printf("read error dblk %d-%d\n", dblk,
					       dblk + maxsecs - 1);
#endif
				if (--retries >= 0)
					continue;
				return (-1);	/* XXX cannot output here if
						 * (cold) */
			}
			if (!cold) {
				ra_dev = d->dev;
				ra_first = dblk;
				ra_end = dblk + nsec;
				diskbuf_user = &ra_dev;
			}
		} else		/* can take blocks from end of read-ahead
				 * buffer */
			nsec = ra_end - dblk;

		if (!cold) {
			/* copy data from read-ahead to user buffer */
			if (nsec > num)
				nsec = num;
			memcpy(buf,
			       diskbufp + (dblk - ra_first) * BIOSDISK_SECSIZE,
			       nsec * BIOSDISK_SECSIZE);
		}
		buf += nsec * BIOSDISK_SECSIZE;
		num -= nsec;
		dblk += nsec;
	}

	return (0);
}
