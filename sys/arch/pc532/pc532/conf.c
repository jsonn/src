/*	$NetBSD: conf.c,v 1.38.24.2 2002/09/06 08:38:42 jdolecek Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *      @(#)conf.c	7.9 (Berkeley) 5/28/91
 */

#include "opt_systrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "ccd.h"
#include "cd.h"
#include "md.h"
#include "raid.h"
#include "sd.h"
#include "st.h"
#include "vnd.h"

#include "bpfilter.h"
#include "ch.h"
#include "ipfilter.h"
#include "lpt.h"
#include "pty.h"
#include "rnd.h"
#include "scn.h"
#include "se.h"
#include "ss.h"
#include "tun.h"
#include "uk.h"
#include "scsibus.h"
#include "clockctl.h"
cdev_decl(clockctl);

struct bdevsw	bdevsw[] =
{
	bdev_disk_init(NSD,sd),		/* 0: SCSI disk */
	bdev_swap_init(1,sw),		/* 1: swap pseudo-device */
	bdev_tape_init(NST,st),		/* 2: SCSI tape */
	bdev_disk_init(NMD,md),		/* 3: memory disk */
	bdev_disk_init(NCD,cd),		/* 4: SCSI CD-ROM */
	bdev_disk_init(NVND,vnd),	/* 5: vnode disk driver */
	bdev_disk_init(NCCD,ccd),	/* 6: concatenated disk driver */
	bdev_lkm_dummy(),		/* 7 */
	bdev_lkm_dummy(),		/* 8 */
	bdev_lkm_dummy(),		/* 9 */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_disk_init(NRAID,raid),	/* 13: RAIDframe disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_disk_init(NSD,sd),		/* 3: SCSI disk */
	cdev_swap_init(1,sw),		/* 4: /dev/drum (swap device) */
	cdev_tty_init(NPTY,pts),	/* 5: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 6: pseudo-tty master */
	cdev_log_init(1,log),		/* 7: /dev/klog */
	cdev_tty_init(NSCN,scn),	/* 8: serial ports */
	cdev_disk_init(NMD,md),		/* 9: memory disk */
	cdev_tape_init(NST,st),		/* 10: SCSI tape */
	cdev_fd_init(1,filedesc),	/* 11: file descriptor pseudo-device */
	cdev_disk_init(NCD,cd),		/* 12: SCSI CD-ROM */
	cdev_disk_init(NVND,vnd),	/* 13: vnode disk driver */
	cdev_bpftun_init(NBPFILTER,bpf),/* 14: Berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 15: network tunnel */
	cdev_ch_init(NCH,ch),		/* 16: SCSI autochanger */
	cdev_lpt_init(NLPT, lpt),	/* 17: Centronics */
	cdev_disk_init(NCCD,ccd),	/* 18: concatenated disk driver */
	cdev_scanner_init(NSS,ss),	/* 19: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 20: SCSI unknown */
	cdev_lkm_init(NLKM,lkm),	/* 21: loadable module driver */
	cdev_se_init(NSE, se),		/* 22: SCSI ethernet */
	cdev_lkm_dummy(),		/* 23 */
	cdev_lkm_dummy(),		/* 24 */
	cdev_lkm_dummy(),		/* 25 */
	cdev_lkm_dummy(),		/* 26 */
	cdev_lkm_dummy(),		/* 27 */
	cdev_ipf_init(NIPFILTER,ipl),	/* 28: ip-filter device */
	cdev_rnd_init(NRND,rnd),	/* 29: random source pseudo-device */
	cdev_scsibus_init(NSCSIBUS,scsibus), /* 30: SCSI bus */
	cdev_disk_init(NRAID,raid),	/* 31: RAIDframe disk driver */
	cdev_clockctl_init(NCLOCKCTL, clockctl),/* 32: clockctl pseudo device */
#ifdef SYSTRACE
	cdev_clonemisc_init(1, systrace),/* 33: system call tracing */
#else
	cdev_notdef(),			/* 33: system call tracing */
#endif
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 2; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(1, 0);

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t dev;
{

	return (major(dev) == mem_no && minor(dev) == 12);
}

static int chrtoblktbl[] = {
	/* XXXX This needs to be dynamic for LKMs. */
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	0,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	NODEV,
	/*  9 */	3,
	/* 10 */	2,
	/* 11 */	NODEV,
	/* 12 */	4,
	/* 13 */	5,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	6,
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	NODEV,
	/* 27 */	NODEV,
	/* 28 */	NODEV,
	/* 29 */	NODEV,
	/* 30 */	NODEV,
	/* 31 */	13,
	/* 32 */	NODEV,
	/* 33 */	NODEV,
};

/*
 * Convert a character device number to a block device number.
 */
dev_t
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	if (major(dev) >= nchrdev)
		return (NODEV);
	blkmaj = chrtoblktbl[major(dev)];
	if (blkmaj == NODEV)
		return (NODEV);
	return (makedev(blkmaj, minor(dev)));
}

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
#include <dev/cons.h>

cons_decl(scn);

struct	consdev constab[] = {
#if NSCN > 0
	cons_init(scn),
#endif
	{ 0 },
};
