/* $NetBSD: conf.c,v 1.23.2.2 1997/09/04 00:52:20 thorpej Exp $ */

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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: conf.c,v 1.23.2.2 1997/09/04 00:52:20 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include "fdc.h"
bdev_decl(fd);
bdev_decl(sw);
#include "st.h"
bdev_decl(st);
#include "cd.h"
bdev_decl(cd);
#include "wdc.h"
bdev_decl(wd);
#include "sd.h"
bdev_decl(sd);
#include "vnd.h"
bdev_decl(vnd);
#include "ccd.h"
bdev_decl(ccd);
#include "md.h"
bdev_decl(md);

struct bdevsw	bdevsw[] =
{
	bdev_disk_init(NFDC,fd),	/* 0: PC-ish floppy disk driver */
	bdev_swap_init(1,sw),		/* 1: swap pseudo-device */
	bdev_tape_init(NST,st),		/* 2: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 3: SCSI CD-ROM */
	bdev_disk_init(NWDC,wd),	/* 4: IDE disk driver */
	bdev_notdef(),			/* 5 */
	bdev_disk_init(NMD,md),		/* 6: memory disk driver */
	bdev_disk_init(NCCD,ccd),	/* 7: concatenated disk driver */
	bdev_disk_init(NSD,sd),		/* 8: SCSI disk driver */
	bdev_disk_init(NVND,vnd),	/* 9: vnode disk driver */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_lkm_dummy(),		/* 13 */
	bdev_lkm_dummy(),		/* 14 */
	bdev_lkm_dummy(),		/* 15 */
};
int	nblkdev = sizeof (bdevsw) / sizeof (bdevsw[0]);

/* open, close, read, write, ioctl, tty, mmap */
#define cdev_wscons_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	dev_init(c,n,tty), ttpoll, dev_init(c,n,mmap), D_TTY }

/* open, close, write, ioctl */
#define cdev_lpt_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, read, ioctl, poll */
#define cdev_satlink_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	(dev_type_mmap((*))) enodev }

cdev_decl(cn);
cdev_decl(ctty);
#define	mmread  mmrw
#define	mmwrite mmrw
cdev_decl(mm);
cdev_decl(sw);
#include "pty.h"
#define	ptstty		ptytty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptctty		ptytty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);
cdev_decl(log);
#include "tun.h"
cdev_decl(tun);
cdev_decl(sd);
cdev_decl(vnd);
cdev_decl(ccd);
dev_type_open(filedescopen);
#include "bpfilter.h"
cdev_decl(bpf);
cdev_decl(st);
cdev_decl(cd);
#include "ch.h"
cdev_decl(ch);
#include "scc.h"
cdev_decl(scc);
#include "audio.h"
cdev_decl(audio);
#include "wscons.h"
cdev_decl(wscons);
#include "com.h"
cdev_decl(com);
cdev_decl(kbd);
cdev_decl(ms);
#include "lpt.h"
cdev_decl(lpt);
cdev_decl(md);
#include "ss.h"
cdev_decl(ss);
#include "uk.h"
cdev_decl(uk);
cdev_decl(fd);
#include "ipfilter.h"
cdev_decl(ipl);
cdev_decl(wd);
#include "satlink.h"
cdev_decl(satlink);

cdev_decl(prom);			/* XXX XXX XXX */

#include "se.h"

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_bpftun_init(NTUN,tun),	/* 7: network tunnel */
	cdev_disk_init(NSD,sd),		/* 8: SCSI disk */
	cdev_disk_init(NVND,vnd),	/* 9: vnode disk driver */
	cdev_fd_init(1,filedesc),	/* 10: file descriptor pseudo-dev */
	cdev_bpftun_init(NBPFILTER,bpf),/* 11: Berkeley packet filter */
	cdev_tape_init(NST,st),		/* 12: SCSI tape */
	cdev_disk_init(NCD,cd),		/* 13: SCSI CD-ROM */
	cdev_ch_init(NCH,ch),		/* 14: SCSI autochanger */
	cdev_tty_init(NSCC,scc),	/* 15: scc 8530 serial interface */
	cdev_lkm_init(NLKM,lkm),	/* 16: loadable module driver */
	cdev_lkm_dummy(),		/* 17 */
	cdev_lkm_dummy(),		/* 18 */
	cdev_lkm_dummy(),		/* 19 */
	cdev_lkm_dummy(),		/* 20 */
	cdev_lkm_dummy(),		/* 21 */
	cdev_lkm_dummy(),		/* 22 */
	cdev_tty_init(1,prom),          /* 23: XXX prom console */
	cdev_audio_init(NAUDIO,audio),	/* 24: generic audio I/O */
	cdev_wscons_init(NWSCONS,wscons), /* 25: workstation console */
	cdev_tty_init(NCOM,com),	/* 26: ns16550 UART */
	cdev_disk_init(NCCD,ccd),	/* 27: concatenated disk driver */
	cdev_disk_init(NMD,md),		/* 28: memory disk driver */
	cdev_mouse_init(NWSCONS,kbd),	/* 29: /dev/kbd XXX */
	cdev_mouse_init(NWSCONS,ms),	/* 30: /dev/mouse XXX */
	cdev_lpt_init(NLPT,lpt),	/* 31: parallel printer */
	cdev_scanner_init(NSS,ss),	/* 32: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 33: SCSI unknown */
	cdev_disk_init(NFDC,fd),	/* 34: PC-ish floppy disk driver */
	cdev_ipf_init(NIPFILTER,ipl),	/* 35: ip-filter device */
	cdev_disk_init(NWDC,wd),	/* 36: IDE disk driver */
	cdev_se_init(NSE,se),		/* 37: Cabletron SCSI<->Ethernet */
	cdev_satlink_init(NSATLINK,satlink), /* 38: planetconnect satlink */
};
int	nchrdev = sizeof (cdevsw) / sizeof (cdevsw[0]);

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
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	NODEV,
	/*  8 */	8,		/* sd */
	/*  9 */	9,		/* vnd */
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	2,		/* st */
	/* 13 */	3,		/* cd */
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	NODEV,
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	NODEV,
	/* 27 */	7,		/* ccd */
	/* 28 */	6,		/* md */
	/* 29 */	NODEV,
	/* 30 */	NODEV,
	/* 31 */	NODEV,
	/* 32 */	NODEV,
	/* 33 */	NODEV,
	/* 34 */	0,		/* fd */
	/* 35 */	NODEV,
	/* 36 */	4,		/* wd */
	/* 37 */	NODEV,
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
