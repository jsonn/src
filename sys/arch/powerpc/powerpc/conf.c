/*	$NetBSD: conf.c,v 1.3.4.1 1997/10/14 10:18:12 thorpej Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include "ofdisk.h"
bdev_decl(ofd);
bdev_decl(sw);

struct bdevsw bdevsw[] = {
	bdev_disk_init(NOFDISK,ofd),	/* 0: Openfirmware disk */
	bdev_swap_init(1,sw),		/* 1: swap pseudo device */
};
int nblkdev = sizeof bdevsw / sizeof bdevsw[0];

cdev_decl(cn);
cdev_decl(ctty);
#define mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);
#include "pty.h"
#define	ptstty		ptytty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptctty		ptytty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);
cdev_decl(log);
cdev_decl(sw);
#include "ofcons.h"
cdev_decl(ofc);
cdev_decl(ofd);
#include "ofrtc.h"
cdev_decl(ofrtc);
#include "bpfilter.h"
cdev_decl(bpf);
#include "rnd.h"

#define	cdev_rtc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), \
	dev_init(c,n,read), dev_init(c,n,write), \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

struct cdevsw cdevsw[] = {
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: control tty */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_tty_init(NPTY,pts),	/* 3: pseudo tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 4: pseudo tty master */
	cdev_log_init(1,log),		/* 5: /dev/klog */
	cdev_swap_init(1,sw),		/* 6: /dev/drum pseudo device */
	cdev_tty_init(NOFCONS,ofc),	/* 7: Openfirmware console */
	cdev_disk_init(NOFDISK,ofd),	/* 8: Openfirmware disk */
	cdev_rtc_init(NOFRTC,ofrtc),	/* 9: Openfirmware RTC */
	cdev_bpftun_init(NBPFILTER,bpf),/* 10: Berkeley packet filter */
	cdev_rnd_init(NRND,rnd),	/* 11: random source pseudo-device */
};
int nchrdev = sizeof cdevsw / sizeof cdevsw[0];

int mem_no = 2;				/* major number of /dev/mem */

/*
 * Swapdev is a fake device implemented in sw.c.
 * It is used only internally to get to swstrategy.
 */
dev_t swapdev = makedev(1, 0);

/*
 * Check whether dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{
	return major(dev) == mem_no && minor(dev) < 2;
}

/*
 * Check whether dev is /dev/zero.
 */
int
iszerodev(dev)
	dev_t dev;
{
	return major(dev) == mem_no && minor(dev) == 12;
}

static int chrtoblktbl[] = {
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	0,
	/*  9 */	NODEV,
	/* 10 */	NODEV,
	/* 11 */	NODEV,
};

/*
 * Return accompanying block dev for a char dev.
 */
int
chrtoblk(dev)
	dev_t dev;
{
	int major;
	
	if ((major = major(dev)) >= nchrdev)
		return NODEV;
	if ((major = chrtoblktbl[major]) == NODEV)
		return NODEV;
	return makedev(major, minor(dev));
}

#include <dev/cons.h>

cons_decl(ofc);

struct consdev constab[] = {
#if NOFCONS > 0
	cons_init(ofc),
#endif
	{ 0 },
};
