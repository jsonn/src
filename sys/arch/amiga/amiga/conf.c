/*	$NetBSD: conf.c,v 1.62.2.2 2002/06/23 17:34:21 jdolecek Exp $	*/

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

#include "opt_compat_svr4.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: conf.c,v 1.62.2.2 2002/06/23 17:34:21 jdolecek Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <dev/cons.h>

#include <machine/conf.h>

#ifdef BANKEDDEVPAGER
#include <sys/bankeddev.h>
#endif

#include "vnd.h"
#include "sd.h"
#include "cd.h"
#include "st.h"
#include "fd.h"
#include "ccd.h"
#include "raid.h"
#include "wd.h"
#include "ss.h"
#include "ch.h"
#include "uk.h"
#include "md.h"

bdev_decl(wd);

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),			/* 0 */
	bdev_notdef(),			/* 1 */
	bdev_disk_init(NFD,fd),		/* 2: floppy disk */
	bdev_swap_init(1,sw),		/* 3: swap pseudo-device */
	bdev_disk_init(NSD,sd),		/* 4: SCSI disk */
	bdev_tape_init(NST,st),		/* 5: SCSI tape */
	bdev_disk_init(NVND,vnd),	/* 6: vnode disk driver */
	bdev_disk_init(NCD,cd),		/* 7: SCSI CD-ROM */
	bdev_disk_init(NCCD,ccd),	/* 8: concatenated disk driver */
	bdev_lkm_dummy(),		/* 9 */
	bdev_lkm_dummy(),		/* 10 */
	bdev_lkm_dummy(),		/* 11 */
	bdev_lkm_dummy(),		/* 12 */
	bdev_lkm_dummy(),		/* 13 */
	bdev_lkm_dummy(),		/* 14 */
	bdev_disk_init(NMD,md),		/* 15: memory disk */
	bdev_disk_init(NRAID,raid),	/* 16: RAIDframe disk driver */
	bdev_disk_init(NWD,wd),		/* 17: IDE disk */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

#include "pty.h"
#include "grf.h"
#include "par.h"
#include "ser.h"
#include "msc.h"
#include "ite.h"
#include "kbd.h"
#include "ms.h"
#include "view.h"
#include "mfcs.h"
#include "com.h"
#include "lpt.h"
#include "audio.h"
cdev_decl(audio);
dev_decl(filedesc,open);
#include "bpfilter.h"
#include "tun.h"
#include "ipfilter.h"
#include "rnd.h"
#include "scsibus.h"

#include "wsdisplay.h"
#include "amidisplaycc.h"
#include "wskbd.h"
cdev_decl(wsdisplay);
cdev_decl(wskbd);

cdev_decl(wd);

#include "isdn.h"
#include "isdnctl.h"
#include "isdntrc.h"
#include "isdnbchan.h"
#include "isdntel.h"
cdev_decl(isdn);
cdev_decl(isdnctl);
cdev_decl(isdntrc);
cdev_decl(isdnbchan);
cdev_decl(isdntel);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_disk_init(NCCD,ccd),	/* 7: concatenated disk driver */
	cdev_disk_init(NSD,sd),		/* 8: SCSI disk */
	cdev_disk_init(NCD,cd),		/* 9: SCSI CD-ROM */
	cdev_grf_init(NGRF,grf),	/* 10: frame buffer */
	cdev_par_init(NPAR,par),	/* 11: parallel interface */
	cdev_tty_init(NSER,ser),	/* 12: built-in single-port serial */
	cdev_tty_init(NITE,ite),	/* 13: console terminal emulator */
	cdev_mouse_init(NKBD,kbd),	/* 14: /dev/kbd */
	cdev_mouse_init(NMS,ms),	/* 15: /dev/mouse0 /dev/mouse1 */
	cdev_view_init(NVIEW,view),	/* 16: /dev/view00 /dev/view01 ... */
	cdev_tty_init(NMFCS,mfcs),	/* 17: MultiFaceCard III serial */
	cdev_disk_init(NFD,fd),		/* 18: floppy disk */
	cdev_disk_init(NVND,vnd),	/* 19: vnode disk driver */
	cdev_tape_init(NST,st),		/* 20: SCSI tape */
	cdev_fd_init(1,filedesc),	/* 21: file descriptor pseudo-dev */
	cdev_bpftun_init(NBPFILTER,bpf),/* 22: Berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 23: network tunnel */
	cdev_lkm_init(NLKM,lkm),	/* 24: loadable module driver */
	cdev_lkm_dummy(),		/* 25 */
	cdev_lkm_dummy(),		/* 26 */
	cdev_lkm_dummy(),		/* 27 */
	cdev_lkm_dummy(),		/* 28 */
	cdev_lkm_dummy(),		/* 29 */
	cdev_lkm_dummy(),		/* 30 */
 	cdev_tty_init(NMSC,msc),	/* 31: A2232 MSC Multiport serial */
	cdev_tty_init(NCOM,com),	/* 32: com ports */
	cdev_lpt_init(NLPT,lpt),	/* 33: lpt-style parallel ports */
	cdev_lkm_dummy(),		/* 34 */
	cdev_lkm_dummy(),		/* 35 */
	cdev_lkm_dummy(),		/* 36 */
	cdev_scanner_init(NSS,ss),	/* 37: SCSI scanner */
	cdev_ch_init(NCH,ch),		/* 38: SCSI autochanger */
	cdev_uk_init(NUK,uk),		/* 39: SCSI unknown */
	cdev_ipf_init(NIPFILTER,ipl),	/* 40: ip-filter device */
	cdev_audio_init(NAUDIO,audio),	/* 41: cc audio interface */
	cdev_rnd_init(NRND,rnd),	/* 42: random source pseudo-device */
	cdev_disk_init(NMD,md),		/* 43: memory disk */
	cdev_scsibus_init(NSCSIBUS,scsibus), /* 44: SCSI bus */
	cdev_isdn_init(NISDN, isdn),		/* 45: isdn main device */
	cdev_isdnctl_init(NISDNCTL, isdnctl),	/* 46: isdn control device */
	cdev_isdnbchan_init(NISDNBCHAN, isdnbchan),	/* 47: isdn raw b-channel */
	cdev_isdntrc_init(NISDNTRC, isdntrc),	/* 48: isdn trace device */
	cdev_isdntel_init(NISDNTEL, isdntel),	/* 49: isdn phone device */
	cdev_disk_init(NRAID,raid),	/* 50: RAIDframe disk driver */
	cdev_svr4_net_init(NSVR4_NET,svr4_net), /* 51: svr4 net pseudo-device */
	cdev_disk_init(NWD,wd),		/* 52: IDE disk */
	cdev_wsdisplay_init(NWSDISPLAY,
			    wsdisplay), /* 53: display */

	cdev_mouse_init(NWSKBD,wskbd),  /* 54: keyboard */
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

#ifdef BANKEDDEVPAGER
extern int grfbanked_get(int, int, int);
extern int grfbanked_set(int, int);
extern int grfbanked_cur(int);

struct bankeddevsw bankeddevsw[sizeof (cdevsw) / sizeof (cdevsw[0])] = {
  { 0, 0, 0 },						/* 0 */
  { 0, 0, 0 },						/* 1 */
  { 0, 0, 0 },						/* 2 */
  { 0, 0, 0 },						/* 3 */
  { 0, 0, 0 },						/* 4 */
  { 0, 0, 0 },						/* 5 */
  { 0, 0, 0 },						/* 6 */
  { 0, 0, 0 },						/* 7 */
  { 0, 0, 0 },						/* 8 */
  { 0, 0, 0 },						/* 9 */
  { grfbanked_get, grfbanked_cur, grfbanked_set },	/* 10 */
  /* rest { 0, 0, 0 } */
};
#endif

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
dev_t	swapdev = makedev(3, 0);

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

static int chrtoblktab[] = {
	/* XXXX This needs to be dynamic for LKMs. */
	/*VCHR*/	/*VBLK*/
	/*  0 */	NODEV,
	/*  1 */	NODEV,
	/*  2 */	NODEV,
	/*  3 */	NODEV,
	/*  4 */	NODEV,
	/*  5 */	NODEV,
	/*  6 */	NODEV,
	/*  7 */	8,		/* ccd */
	/*  8 */	4,		/* sd */
	/*  9 */	7,		/* cd */
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	2,		/* fd */
	/* 19 */	6,		/* vnd */
	/* 20 */	5,		/* st */
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
 	/* 31 */	NODEV,
 	/* 32 */	NODEV,
 	/* 33 */	NODEV,
 	/* 34 */	NODEV,
 	/* 35 */	NODEV,
 	/* 36 */	NODEV,
 	/* 37 */	NODEV,
 	/* 38 */	NODEV,
 	/* 39 */	NODEV,
 	/* 40 */	NODEV,
 	/* 41 */	NODEV,
 	/* 42 */	NODEV,
	/* 43 */	15,		/* md */
	/* 44 */	NODEV,
	/* 45 */	NODEV,
	/* 46 */	NODEV,
	/* 47 */	NODEV,
	/* 48 */	NODEV,
	/* 49 */	NODEV,
	/* 50 */	16,
	/* 51 */	NODEV,
	/* 52 */	17,
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
		return(NODEV);
	blkmaj = chrtoblktab[major(dev)];
	if (blkmaj == NODEV)
		return(NODEV);
	return (makedev(blkmaj, minor(dev)));
}

/*
 * This entire table could be autoconfig()ed but that would mean that
 * the kernel's idea of the console would be out of sync with that of
 * the standalone boot.  I think it best that they both use the same
 * known algorithm unless we see a pressing need otherwise.
 */
cons_decl(ser);
cons_decl(ite);
cons_decl(amidisplaycc_);

struct	consdev constab[] = {
#if NSER > 0
	cons_init(ser),
#endif
#if NAMIDISPLAYCC>0
	cons_init(amidisplaycc_),
#endif
#if NITE > 0
	cons_init(ite),
#endif
	{ 0 },
};
