/*	$NetBSD: disk.h,v 1.1.6.2 2009/05/13 17:18:54 jym Exp $	*/
/*	$OpenBSD: disk.h,v 1.1 2005/05/24 20:38:20 uwe Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _STAND_DISK_H
#define _STAND_DISK_H

#include <sys/disklabel.h>
#include <sys/queue.h>

#define	MAXDEVNAME	16

/* XXX snatched from <i386/biosdev.h> */
#if 1
/* Info about disk from the bios, plus the mapping from
 * BIOS numbers to BSD major (driver?) number.
 *
 * Also, do not bother with BIOSN*() macros, just parcel
 * the info out, and use it like this.  This makes for less
 * of a dependance on BIOSN*() macros having to be the same
 * across /boot, /bsd, and userland.
 */
#define	BOOTARG_DISKINFO 1
typedef struct _bios_diskinfo {
	/* BIOS section */
	int bios_number;	/* BIOS number of drive (or -1) */

	/* Misc. flags */
	uint32_t flags;
#define BDI_INVALID	0x00000001	/* I/O error during checksumming */
#define BDI_GOODLABEL	0x00000002	/* Had SCSI or ST506/ESDI disklabel */
#define BDI_BADLABEL	0x00000004	/* Had another disklabel */
#define BDI_EL_TORITO	0x00000008	/* 2,048-byte sectors */
#define BDI_PICKED	0x80000000	/* kernel-only: cksum matched */
} bios_diskinfo_t;

#define	BOOTARG_CKSUMLEN 3		/* uint32_t */
#endif /* 1 */

/* All the info on a disk we've found */
struct diskinfo {
	bios_diskinfo_t bios_info;
	struct disklabel disklabel;
	char devname[MAXDEVNAME];

	TAILQ_ENTRY(diskinfo) list;
};
TAILQ_HEAD(disklist_lh, diskinfo);

/* diskprobe.c */
void diskprobe(char *buf, size_t bufsiz);
struct diskinfo *dkdevice(const char *, uint);
int bios_devname(int, char *, int);
void bios_devpath(int, int, char *);
char *bios_getdiskinfo(int, bios_diskinfo_t *);
int bios_getdospart(bios_diskinfo_t *);
char *bios_getdisklabel(bios_diskinfo_t *, struct disklabel *);
void dump_diskinfo(void);

#endif /* _STAND_DISK_H */
