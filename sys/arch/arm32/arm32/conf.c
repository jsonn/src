/*	$NetBSD: conf.c,v 1.18.2.1 1997/01/30 05:23:58 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * conf.c
 *
 * Character and Block Device configuration
 * Console configuration
 *
 * Defines the structures cdevsw and constab
 *
 * Created      : 17/09/94
 */
 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "wdc.h"
#include "fdc.h"
#include "md.h"
#include "sd.h"
#include "st.h"
#include "cd.h"
#include "vnd.h"
#include "ccd.h"

/* Block devices */

struct bdevsw bdevsw[] = {
	bdev_lkm_dummy(),		/*  0: */
	bdev_swap_init(1, sw),		/*  1: swap pseudo-device */
	bdev_lkm_dummy(),		/*  2: */
	bdev_lkm_dummy(),		/*  3: */
	bdev_lkm_dummy(),		/*  4: */
	bdev_lkm_dummy(),		/*  5: */
	bdev_lkm_dummy(),		/*  6: */
	bdev_lkm_dummy(),		/*  7: */
	bdev_lkm_dummy(),		/*  8: */
	bdev_lkm_dummy(),		/*  9: */
	bdev_lkm_dummy(),		/* 10: */
	bdev_lkm_dummy(),		/* 11: */
	bdev_lkm_dummy(),		/* 12: */
	bdev_lkm_dummy(),		/* 13: */
	bdev_lkm_dummy(),		/* 14: */
	bdev_lkm_dummy(),		/* 15: */
	bdev_disk_init(NWDC, wd),	/* 16: Internal IDE disk */
	bdev_disk_init(NFDC, fd),	/* 17: floppy diskette */
	bdev_disk_init(NMD, md),	/* 18: memory disk */
	bdev_disk_init(NVND,vnd),	/* 19: vnode disk driver */
	bdev_lkm_dummy(),		/* 20: */
 	bdev_disk_init(NCCD,ccd),	/* 21: concatenated disk driver */
	bdev_lkm_dummy(),		/* 22: */
	bdev_lkm_dummy(),		/* 23: */
	bdev_disk_init(NSD,sd),		/* 24: SCSI disk */
	bdev_tape_init(NST,st),		/* 25: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 26: SCSI cdrom */
	bdev_lkm_dummy(),		/* 27: */
	bdev_lkm_dummy(),		/* 28: */
	bdev_lkm_dummy(),		/* 29: */
	bdev_lkm_dummy(),		/* 30: */
	bdev_lkm_dummy(),		/* 31: */
	bdev_lkm_dummy(),		/* 32: */
	bdev_lkm_dummy(),		/* 33: */
	bdev_lkm_dummy(),		/* 34: */
	bdev_lkm_dummy(),		/* 35: */
	bdev_lkm_dummy(),		/* 36: */
	bdev_lkm_dummy(),		/* 37: */
	bdev_lkm_dummy(),		/* 38: */
	bdev_lkm_dummy(),		/* 39: */
	bdev_lkm_dummy(),		/* 40: */
	bdev_lkm_dummy(),		/* 41: */
	bdev_lkm_dummy(),		/* 42: */
	bdev_lkm_dummy(),		/* 43: */
	bdev_lkm_dummy(),		/* 44: */
	bdev_lkm_dummy(),		/* 45: */
	bdev_lkm_dummy(),		/* 46: */
	bdev_lkm_dummy(),		/* 47: */
	bdev_lkm_dummy(),		/* 48: */
	bdev_lkm_dummy(),		/* 49: */
};

int nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

#include "vt.h"                                 
#include "pty.h"
#define ptstty          ptytty
#define ptsioctl        ptyioctl
#define ptctty          ptytty
#define ptcioctl        ptyioctl
#include "com.h"
#include "lpt.h"
#include "bpfilter.h"
#include "ch.h"
#include "uk.h"
#include "ss.h"
#include "tun.h"
#include "quadmouse.h"
#include "pms.h"
#include "beep.h"
#include "kbd.h"
#include "audio.h"
#include "cpu.h"
#include "iic.h"
#include "rtc.h"
#include "ipfilter.h"

/* Character devices */

struct cdevsw cdevsw[] = {
	cdev_mm_init(1, mm),            /*  0: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1, sw),          /*  1: /dev/drum (swap pseudo-device) */
	cdev_cn_init(1, cn),            /*  2: virtual console */
	cdev_ctty_init(1,ctty),         /*  3: controlling terminal */
	cdev_physcon_init(NVT, physcon),  /*  4: RPC console */
        cdev_log_init(1,log),           /*  5: /dev/klog */
	cdev_ptc_init(NPTY,ptc),        /*  6: pseudo-tty master */
	cdev_tty_init(NPTY,pts),        /*  7: pseudo-tty slave */
	cdev_lpt_init(NLPT,lpt),        /*  8: parallel printer */
	cdev_mouse_init(NQUADMOUSE,quadmouse),       /* 9: quadmouse driver */
	cdev_beep_init(NBEEP,beep),	/* 10: simple beep device */
	cdev_kbd_init(NKBD,kbd),	/* 11: kbd device */
	cdev_tty_init(NCOM,com),        /* 12: serial port */
	cdev_lkm_dummy(),		/* 13: */
	cdev_lkm_dummy(),		/* 14: */
	cdev_lkm_dummy(),		/* 15: */
	cdev_disk_init(NWDC, wd),       /* 16: ST506/ESDI/IDE disk */
	cdev_disk_init(NFDC, fd),       /* 17: floppy diskette */
	cdev_disk_init(NMD, md),        /* 18: memory disk driver */
	cdev_disk_init(NVND,vnd),       /* 19: vnode disk driver */
	cdev_lkm_dummy(),		/* 20: */
 	cdev_disk_init(NCCD,ccd),	/* 21: concatenated disk driver */
	cdev_lkm_dummy(),		/* 22: */
	cdev_lkm_dummy(),		/* 23: */
	cdev_disk_init(NSD,sd),	    	/* 24: SCSI disk */
	cdev_tape_init(NST,st),	   	/* 25: SCSI tape */
	cdev_disk_init(NCD,cd),	    	/* 26: SCSI CD-ROM */
	cdev_ch_init(NCH,ch),	 	/* 27: SCSI autochanger */
	cdev_uk_init(NUK,uk),	 	/* 28: SCSI unknown */
	cdev_scanner_init(NSS,ss),	/* 29: SCSI scanner */
	cdev_lkm_dummy(),		/* 30: */
	cdev_lkm_dummy(),		/* 31: */
	cdev_bpftun_init(NBPFILTER,bpf),/* 32: Berkeley packet filter */
        cdev_bpftun_init(NTUN,tun),     /* 33: network tunnel */
        cdev_fd_init(1,filedesc),       /* 34: file descriptor pseudo-device */
	cdev_lkm_init(NLKM,lkm),        /* 35: loadable module driver */
	cdev_audio_init(NAUDIO,audio),	/* 36: generic audio I/O */
	cdev_vidcvid_init(1,vidcvideo),	/* 37: vidcvideo device */
	cdev_cpu_init(NCPU,cpu),	/* 38: cpu device */
	cdev_lkm_dummy(),		/* 39: */
	cdev_mouse_init(NPMS,pms),      /* 40: PS2 mouse driver */
	cdev_lkm_dummy(),		/* 41: */
	cdev_iic_init(NIIC, iic),	/* 42: IIC bus driver */
	cdev_rtc_init(NRTC, rtc),	/* 43: RTC driver */
	cdev_lkm_dummy(),		/* 44: */
	cdev_lkm_dummy(),		/* 45: */
	cdev_ipf_init(NIPFILTER,ipl),	/* 46: ip-filter device */
	cdev_lkm_dummy(),		/* 47: */
	cdev_lkm_dummy(),		/* 48: */
	cdev_lkm_dummy(),		/* 49: */
};

int nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int mem_no = 0; 	/* major device number of memory special file */

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
	return (major(dev) == mem_no && minor(dev) == 3);
}


static int chrtoblktbl[] = {
/* XXXX This needs to be dynamic for LKMs. */
    /*VCHR*/        /*VBLK*/
    /*  0 */        NODEV,
    /*  1 */        1,
    /*  2 */        NODEV,
    /*  3 */        NODEV,
    /*  4 */        NODEV,
    /*  5 */        NODEV,
    /*  6 */        NODEV,
    /*  7 */        NODEV,
    /*  8 */        NODEV,
    /*  9 */        NODEV,
    /* 10 */        NODEV,
    /* 11 */        NODEV,
    /* 12 */        NODEV,
    /* 13 */        NODEV,
    /* 14 */        NODEV,
    /* 15 */        NODEV,
    /* 16 */        16,
    /* 17 */        17,
    /* 18 */        18,
    /* 19 */        19,
    /* 20 */        NODEV,
    /* 21 */        21,
    /* 22 */        NODEV,
    /* 23 */        NODEV,
    /* 24 */        24,
    /* 25 */        25,
    /* 26 */        26,
    /* 27 */        NODEV,
    /* 28 */        NODEV,
    /* 29 */        NODEV,
    /* 30 */        NODEV,
    /* 31 */        NODEV,
    /* 32 */        NODEV,
    /* 33 */        NODEV,
    /* 34 */        NODEV,
    /* 35 */        NODEV,
    /* 36 */        NODEV,
    /* 37 */        NODEV,
    /* 38 */        NODEV,
    /* 39 */        NODEV,
    /* 40 */        NODEV,
    /* 41 */        NODEV,
    /* 42 */        NODEV,
    /* 43 */        NODEV,
    /* 44 */        NODEV,
    /* 45 */        NODEV,
    /* 46 */        NODEV,
    /* 47 */        NODEV,
    /* 48 */        NODEV,
    /* 49 */        NODEV,
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

cons_decl(rpcconsole);
cons_decl(com);   
       
struct consdev constab[] = {
#if (NVT + NRPC > 0)
	cons_init(rpcconsole),
#endif

#ifdef notyet
#if (NCOM > 0)
	cons_init(com),
#endif
#endif
	{ 0 },
};
                           
/* End of conf.c */
