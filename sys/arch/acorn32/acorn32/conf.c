/*	$NetBSD: conf.c,v 1.1.6.4 2002/06/23 17:33:53 jdolecek Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
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

#include "opt_footbridge.h"
 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#include <machine/conf.h>

#include "wd.h"
#include "fdc.h"
#include "md.h"
#include "sd.h"
#include "st.h"
#include "cd.h"
#include "vnd.h"
#include "ccd.h"
#include "raid.h"

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
	bdev_disk_init(NWD, wd),	/* 16: Internal IDE disk */
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
	bdev_lkm_dummy(),		/* 50: */
	bdev_lkm_dummy(),		/* 51: */
	bdev_lkm_dummy(),		/* 52: */
	bdev_lkm_dummy(),		/* 53: */
	bdev_lkm_dummy(),		/* 54: */
	bdev_lkm_dummy(),		/* 55: */
	bdev_lkm_dummy(),		/* 56: */
	bdev_lkm_dummy(),		/* 57: */
	bdev_lkm_dummy(),		/* 58: */
	bdev_lkm_dummy(),		/* 59: */
	bdev_lkm_dummy(),		/* 60: */
	bdev_lkm_dummy(),		/* 61: */
	bdev_lkm_dummy(),		/* 62: */
	bdev_lkm_dummy(),		/* 63: */
	bdev_lkm_dummy(),		/* 64: */
	bdev_lkm_dummy(),		/* 65: */
	bdev_lkm_dummy(),		/* 66: */
	bdev_lkm_dummy(),		/* 67: */
	bdev_lkm_dummy(),		/* 68: */
	bdev_lkm_dummy(),		/* 69: */
	bdev_lkm_dummy(),		/* 70: */
	bdev_disk_init(NRAID,raid),	/* 71: RAIDframe disk driver */
	bdev_lkm_dummy(),		/* 72: */
	bdev_lkm_dummy(),		/* 73: */
	bdev_lkm_dummy(),		/* 74: */
	bdev_lkm_dummy(),		/* 75: */
	bdev_lkm_dummy(),		/* 76: */
	bdev_lkm_dummy(),		/* 77: */
	bdev_lkm_dummy(),		/* 78: */
	bdev_lkm_dummy(),		/* 79: */
	bdev_lkm_dummy(),		/* 80: */
	bdev_lkm_dummy(),		/* 81: */
	bdev_lkm_dummy(),		/* 82: */
	bdev_lkm_dummy(),		/* 83: */
	bdev_lkm_dummy(),		/* 84: */
};

int nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

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

#include "vt.h"
#include "vidcconsole.h"                                 
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
#include "qms.h"
#include "opms.h"
#include "beep.h"
#include "kbd.h"
#include "audio.h"
#include "midi.h"
#include "sequencer.h"
#include "iic.h"
#include "rtc.h"
#include "vidcconsole.h"
#include "ipfilter.h"
#include "rnd.h"

#include "vcoda.h"			/* coda file system */
#include "wsdisplay.h"
#include "wskbd.h"
#include "wsmouse.h"
#include "wsmux.h"
#include "scsibus.h"
#include "clockctl.h"
cdev_decl(clockctl);

/* Character devices */

struct cdevsw cdevsw[] = {
	cdev_mm_init(1, mm),            /*  0: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1, sw),          /*  1: /dev/drum (swap pseudo-device) */
	cdev_cn_init(1, cn),            /*  2: virtual console */
	cdev_ctty_init(1,ctty),         /*  3: controlling terminal */
#if	NVIDCCONSOLE>0
	cdev_physcon_init(NVT, physcon),/*  4: RPC console */
#else
	cdev_notdef(),			/* 4: */
#endif
	cdev_log_init(1,log),           /*  5: /dev/klog */
	cdev_ptc_init(NPTY,ptc),        /*  6: pseudo-tty master */
	cdev_tty_init(NPTY,pts),        /*  7: pseudo-tty slave */
	cdev_lpt_init(NLPT,lpt),        /*  8: parallel printer */
	cdev_mouse_init(NQMS,qms),	/*  9: qms driver */
	cdev_beep_init(NBEEP,beep),	/* 10: simple beep device */
	cdev_kbd_init(NKBD,kbd),	/* 11: kbd device */
	cdev_tty_init(NCOM,com),        /* 12: serial port */
	cdev_lkm_dummy(),		/* 13: */
	cdev_lkm_dummy(),		/* 14: */
	cdev_lkm_dummy(),		/* 15: */
	cdev_disk_init(NWD, wd),        /* 16: ST506/ESDI/IDE disk */
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
	cdev_vidcvid_init(NVIDCCONSOLE,vidcconsole),	/* 37: vidcconsole device */
	cdev_lkm_dummy(),		/* 38: removed cpu device */
	cdev_lkm_dummy(),		/* 39: reserved */
	cdev_mouse_init(NOPMS,opms),    /* 40: PS2 mouse driver */
	cdev_lkm_dummy(),		/* 41: reserved */
	cdev_iic_init(NIIC, iic),	/* 42: IIC bus driver */
	cdev_rtc_init(NRTC, rtc),	/* 43: RTC driver */
	cdev_lkm_dummy(),		/* 44: reserved */
	cdev_lkm_dummy(),		/* 45: reserved */
	cdev_ipf_init(NIPFILTER,ipl),	/* 46: ip-filter device */
	cdev_lkm_dummy(),		/* 47: reserved */
	cdev_lkm_dummy(),		/* 48: reserved */
	cdev_notdef(),			/* 49: ofrom */
	cdev_notdef(),		        /* 50: Smart card reader  */
	cdev_notdef(),			/* 51: reserved */
	cdev_rnd_init(NRND,rnd),	/* 52: random source pseudo-device */
	cdev_notdef(),			/* 53: fiq Profiler; not for acorn32 */
	cdev_notdef(),			/* 54: FOOTBRIDGE console */
	cdev_lkm_dummy(),		/* 55: Reserved for bypass device */	
	cdev_notdef(),			/* 56: ISA joystick */
	cdev_midi_init(NMIDI,midi),	/* 57: MIDI I/O */
	cdev_midi_init(NSEQUENCER,sequencer),	/* 58: sequencer I/O */
	cdev_vc_nb_init(NVCODA,vc_nb_),	/* 59: coda file system psdev */
	cdev_wsdisplay_init(NWSDISPLAY, wsdisplay), /* 60: frame buffers, etc. */
	cdev_mouse_init(NWSKBD, wskbd), /* 61: keyboards */
	cdev_mouse_init(NWSMOUSE, wsmouse), /* 62: mice */
	cdev_lkm_dummy(),		/* 63: reserved */
	cdev_notdef(),		        /* 64: USB controller */
	cdev_notdef(),			/* 65: USB generic HID */
	cdev_notdef(),			/* 66: USB printer */
	cdev_lkm_dummy(),		/* 67: reserved */
	cdev_lkm_dummy(),		/* 68: reserved */
	cdev_lkm_dummy(),		/* 69: reserved */
	cdev_scsibus_init(NSCSIBUS,scsibus), /* 70: SCSI bus */
	cdev_disk_init(NRAID,raid),    	/* 71: RAIDframe disk driver */
	cdev_notdef(),			/* 72: USB generic driver */
	cdev_mouse_init(NWSMUX,wsmux),	/* 73: ws multiplexor */
	cdev_notdef(),			/* 74: USB tty */
	cdev_notdef(),			/* 75: USB Diamond Rio 500 */
	cdev_notdef(),			/* 76: USB scanner */
	cdev_notdef(),			/* 77: */
        cdev_notdef(),			/* 78: bicons pseudo-dev */
	cdev_isdn_init(NISDN, isdn),		/* 79: isdn main device */
	cdev_isdnctl_init(NISDNCTL, isdnctl),	/* 80: isdn control device */
	cdev_isdnbchan_init(NISDNBCHAN, isdnbchan),	/* 81: isdn raw b-channel access */
	cdev_isdntrc_init(NISDNTRC, isdntrc),	/* 82: isdn trace device */
	cdev_isdntel_init(NISDNTEL, isdntel),	/* 83: isdn phone device */
	cdev_clockctl_init(NCLOCKCTL, clockctl),/* 84: clockctl pseudo device */
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
    /* 50 */        NODEV,
    /* 51 */        NODEV,
    /* 52 */        NODEV,
    /* 53 */        NODEV,
    /* 54 */        NODEV,
    /* 55 */        NODEV,
    /* 56 */	    NODEV,
    /* 57 */	    NODEV,
    /* 58 */	    NODEV,
    /* 59 */        NODEV,
    /* 60 */        NODEV,
    /* 61 */        NODEV,
    /* 62 */        NODEV,
    /* 63 */        NODEV,
    /* 64 */        NODEV,
    /* 65 */        NODEV,
    /* 66 */	    NODEV,
    /* 67 */	    NODEV,
    /* 68 */	    NODEV,
    /* 69 */	    NODEV,
    /* 70 */	    NODEV,
    /* 71 */	    71,
    /* 72 */	    NODEV,
    /* 73 */	    NODEV,
    /* 74 */	    NODEV,
    /* 75 */	    NODEV,
    /* 76 */	    NODEV,
    /* 77 */	    NODEV,
    /* 78 */	    NODEV,
    /* 79 */	    NODEV,
    /* 80 */	    NODEV,
    /* 81 */	    NODEV,
    /* 82 */	    NODEV,
    /* 83 */	    NODEV,
    /* 84 */	    NODEV,
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
cons_decl(ofcons_);
cons_decl(pc);

struct consdev constab[] = {
#if (NCOM > 0)
	cons_init(com),
#endif
#if (NVT + NRPC > 0)
	cons_init(rpcconsole),
#elif (NPC > 0)
	cons_init(pc),
#elif (NOFCONS > 0)			/* XXX should work together */
	cons_init(ofcons_),
#endif
	{ 0 },
};


/* End of conf.c */
