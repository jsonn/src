/*	$NetBSD: disk.h,v 1.22.2.2 2004/09/18 14:56:30 skrll Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Header: disk.h,v 1.5 92/11/19 04:33:03 torek Exp  (LBL)
 *
 *	@(#)disk.h	8.2 (Berkeley) 1/9/95
 */

#ifndef _SYS_DISK_H_
#define _SYS_DISK_H_

/*
 * Disk device structures.
 */

#include <sys/time.h>
#include <sys/queue.h>

struct buf;
struct disklabel;
struct cpu_disklabel;

struct disk {
	TAILQ_ENTRY(disk) dk_link;	/* link in global disklist */
	char		*dk_name;	/* disk name */
	int		dk_bopenmask;	/* block devices open */
	int		dk_copenmask;	/* character devices open */
	int		dk_openmask;	/* composite (bopen|copen) */
	int		dk_state;	/* label state   ### */
	int		dk_blkshift;	/* shift to convert DEV_BSIZE to blks */
	int		dk_byteshift;	/* shift to convert bytes to blks */

	/*
	 * Metrics data; note that some metrics may have no meaning
	 * on certain types of disks.
	 */
	int		dk_busy;	/* busy counter */
	u_int64_t	dk_rxfer;	/* total number of read transfers */
	u_int64_t	dk_wxfer;	/* total number of write transfers */
	u_int64_t	dk_seek;	/* total independent seek operations */
	u_int64_t	dk_rbytes;	/* total bytes read */
	u_int64_t	dk_wbytes;	/* total bytes written */
	struct timeval	dk_attachtime;	/* time disk was attached */
	struct timeval	dk_timestamp;	/* timestamp of last unbusy */
	struct timeval	dk_time;	/* total time spent busy */

	struct	dkdriver *dk_driver;	/* pointer to driver */

	/*
	 * Disk label information.  Storage for the in-core disk label
	 * must be dynamically allocated, otherwise the size of this
	 * structure becomes machine-dependent.
	 */
	daddr_t		dk_labelsector;		/* sector containing label */
	struct disklabel *dk_label;	/* label */
	struct cpu_disklabel *dk_cpulabel;
};

#define	DK_DISKNAMELEN	16

/* The following structure is 64-bit alignment safe */
struct disk_sysctl {
	char		dk_name[DK_DISKNAMELEN];
	int32_t		dk_busy;
	int32_t		pad;
	u_int64_t	dk_xfer;
	u_int64_t	dk_seek;
	u_int64_t	dk_bytes;
	u_int32_t	dk_attachtime_sec;
	u_int32_t	dk_attachtime_usec;
	u_int32_t	dk_timestamp_sec;
	u_int32_t	dk_timestamp_usec;
	u_int32_t	dk_time_sec;
	u_int32_t	dk_time_usec;
	/* New separate read/write stats */
	u_int64_t	dk_rxfer;
	u_int64_t	dk_rbytes;
	u_int64_t	dk_wxfer;
	u_int64_t	dk_wbytes;
};

struct dkdriver {
	void	(*d_strategy) __P((struct buf *));
#ifdef notyet
	int	(*d_open) __P((dev_t, int, int, struct proc *));
	int	(*d_close) __P((dev_t, int, int, struct proc *));
	int	(*d_ioctl) __P((dev_t, u_long, caddr_t, int, struct proc *));
	int	(*d_dump) __P((dev_t));
	void	(*d_start) __P((struct buf *, daddr_t));
	int	(*d_mklabel) __P((struct disk *));
#endif
};

/* states */
#define	DK_CLOSED	0		/* drive is closed */
#define	DK_WANTOPEN	1		/* drive being opened */
#define	DK_WANTOPENRAW	2		/* drive being opened */
#define	DK_RDLABEL	3		/* label being read */
#define	DK_OPEN		4		/* label read, drive open */
#define	DK_OPENRAW	5		/* open without label */

/*
 * disklist_head is defined here so that user-land has access to it.
 */
TAILQ_HEAD(disklist_head, disk);	/* the disklist is a TAILQ */

/*
 * Bad sector lists per fixed disk
 */
struct disk_badsectors {
	SLIST_ENTRY(disk_badsectors)	dbs_next;
	daddr_t		dbs_min;	/* min. sector number */
	daddr_t		dbs_max;	/* max. sector number */
	struct timeval	dbs_failedat;	/* first failure at */
};

struct disk_badsecinfo {
	u_int32_t	dbsi_bufsize;	/* size of region pointed to */
	u_int32_t	dbsi_skip;	/* how many to skip past */
	u_int32_t	dbsi_copied;	/* how many got copied back */
	u_int32_t	dbsi_left;	/* remaining to copy */
	caddr_t		dbsi_buffer;	/* region to copy disk_badsectors to */
};

#ifdef _KERNEL
extern	int disk_count;			/* number of disks in global disklist */

void	disk_init __P((void));
void	disk_attach __P((struct disk *));
void	disk_detach __P((struct disk *));
void	disk_busy __P((struct disk *));
void	disk_unbusy __P((struct disk *, long, int));
void	disk_resetstat __P((struct disk *));
struct	disk *disk_find __P((char *));
#endif

#endif /* _SYS_DISK_H_ */
