/*	$NetBSD: conf.c,v 1.56.4.4 2002/08/01 02:43:59 nathanw Exp $	*/

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	@(#)conf.c	7.18 (Berkeley) 5/9/91
 */

#include "opt_systrace.h"
#include "opt_cputype.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <machine/cpu.h>

#include "hp.h" /* 0 */
bdev_decl(hp);

#include "ht.h"
bdev_decl(ht);

#include "rk.h"
bdev_decl(rk);

bdev_decl(sw);

#include "te.h"
bdev_decl(tm);

#include "mt.h"
bdev_decl(mt);

#include "ts.h"
bdev_decl(ts);

#include "mu.h"
bdev_decl(mu);

#if VAX750 || VAXANY
#define NCTU	1
#else
#define NCTU	0
#endif
bdev_decl(ctu);

#include "ra.h"
bdev_decl(ra);
bdev_decl(rx);

#include "up.h"
bdev_decl(up);

#include "tj.h"
bdev_decl(ut);

#include "rb.h"
bdev_decl(idc);

#include "uu.h"
bdev_decl(uu);

#include "rl.h"
bdev_decl(rl);

#include "ccd.h"
bdev_decl(ccd);

#include "raid.h"
bdev_decl(raid);

#include "vnd.h"
bdev_decl(vnd);

#include "ry.h"
bdev_decl(rd);
bdev_decl(ry);

#include "sd.h"
bdev_decl(sd);

#include "st.h"
bdev_decl(st);

#include "cd.h"
bdev_decl(cd);

#include "md.h"
bdev_decl(md);

struct bdevsw	bdevsw[] =
{
	bdev_disk_init(NHP,hp),		/* 0: RP0?/RM0? */
	bdev_tape_init(NHT,ht),		/* 1: TU77 w/ TM03 */
	bdev_disk_init(NUP,up),		/* 2: SC-21/SC-31 */
	bdev_disk_init(NRK,rk),		/* 3: RK06/07 */
	bdev_swap_init(1,sw),		/* 4: swap pseudo-device */
	bdev_tape_init(NTE,tm),		/* 5: TM11/TE10 */
	bdev_tape_init(NTS,ts),		/* 6: TS11 */
	bdev_tape_init(NMU,mu),		/* 7: TU78 */
	bdev_tape_init(NCTU,ctu),	/* 8: Console TU58 on 730/750 */
	bdev_disk_init(NRA,ra),		/* 9: MSCP disk */
	bdev_tape_init(NTJ,ut),		/* 10: TU45 */
	bdev_disk_init(NRB,idc),	/* 11: IDC (RB730) */
	bdev_disk_init(NRX,rx),		/* 12: RX?? on MSCP */
	bdev_disk_init(NUU,uu),		/* 13: TU58 on DL11 */
	bdev_disk_init(NRL,rl),		/* 14: RL01/02 */
	bdev_tape_init(NMT,mt),		/* 15: MSCP tape */
	bdev_notdef(),			/* 16: was: KDB50/RA?? */
	bdev_disk_init(NCCD,ccd),	/* 17: concatenated disk driver */
	bdev_disk_init(NVND,vnd),	/* 18: vnode disk driver */
	bdev_disk_init(NRD,rd),		/* 19: VS3100 ST506 disk */
	bdev_disk_init(NSD,sd),		/* 20: SCSI disk */
	bdev_tape_init(NST,st),		/* 21: SCSI tape */
	bdev_disk_init(NCD,cd),		/* 22: SCSI CD-ROM */
	bdev_disk_init(NMD,md),		/* 23: memory disk driver */
	bdev_disk_init(NRY,ry),		/* 24: VS3100 floppy */
	bdev_disk_init(NRAID,raid),	/* 25: RAIDframe disk driver */
};
int	nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

/*
 * Console routines for VAX console.
 */
#include <dev/cons.h>

#include "smg.h"
#include "wskbd.h"
#if NSMG > 0
#if NWSKBD > 0
#define smgcngetc wskbd_cngetc
#else
static int
smgcngetc(dev_t dev)
{
	return 0;
}
#endif

#define smgcnputc wsdisplay_cnputc
#define	smgcnpollc nullcnpollc
#endif

cons_decl(gen);
cons_decl(dz);
cons_decl(qd);
cons_decl(smg);
#include "qv.h"
#include "qd.h"

struct	consdev constab[]={
#if VAX8600 || VAX8200 || VAX780 || VAX750 || VAX650 || VAX630 || VAX660 || \
	VAX670 || VAX680 || VAX8800 || VAXANY
#define NGEN	1
	cons_init(gen), /* Generic console type; mtpr/mfpr */
#else
#define NGEN	0
#endif
#if VAX410 || VAX43 || VAX46 || VAX48 || VAX49 || VAX53 || VAXANY
	cons_init(dz),	/* DZ11-like serial console on VAXstations */
#endif
#if VAX650 || VAX630 || VAXANY
#if NQV
	cons_init(qv),	/* QVSS/QDSS bit-mapped console driver */
#endif
#if NQD
	cons_init(qd),
#endif
#endif
#if NSMG
	cons_init(smg),
#endif

#ifdef notyet
/* We may not always use builtin console, sometimes RD */
	{ rdcnprobe, rdcninit, rdcngetc, rdcnputc },
#endif
	{ 0 }
};

/* Special for console storage */
#define dev_type_rw(n)	int n __P((dev_t, int, int, struct proc *))

/* plotters */
/* open, close, write, ioctl, poll */
#define cdev_plotter_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_noimpl(read,enodev), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_noimpl(stop,enodev), \
	0, dev_init(c,n,poll), dev_noimpl(mmap,enodev) }

/* console mass storage */
/* open, close, read, write */
#define cdev_cnstore_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_noimpl(ioctl,enodev), \
	dev_noimpl(stop,enodev), 0, seltrue, dev_noimpl(mmap,enodev) }

/* open, close, write */
#define cdev_lp_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_noimpl(read,enodev), \
	dev_init(c,n,write), dev_noimpl(ioctl,enodev), \
	dev_noimpl(stop,enodev), 0, seltrue, dev_noimpl(mmap,enodev) }

/* graphic display adapters */
/* open, close, read, write, ioctl, stop, poll */
#define cdev_graph_init(c,n)	cdev__ocrwisp_init(c,n)

/* Ingres */
/* open, close, (read), (write), ioctl */
#define cdev_ingres_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_noimpl(read,nullop), \
	dev_noimpl(write,nullop), dev_init(c,n,ioctl), \
	dev_noimpl(stop,enodev), 0, seltrue, dev_noimpl(mmap,enodev) }



cdev_decl(cn);
cdev_decl(ctty);
#define mmread	mmrw
#define mmwrite mmrw
cdev_decl(mm);
cdev_decl(sw);
#include "pty.h"
#define ptstty		ptytty
#define ptsioctl	ptyioctl
cdev_decl(pts);
#define ptctty		ptytty
#define ptcioctl	ptyioctl
cdev_decl(ptc);
cdev_decl(log);

cdev_decl(hp);
cdev_decl(rk);
cdev_decl(tm);
cdev_decl(mt);
cdev_decl(ts);
cdev_decl(mu);
cdev_decl(ra);
cdev_decl(up);
cdev_decl(ut);
cdev_decl(idc);
cdev_decl(fd);
cdev_decl(gencn);
cdev_decl(rx);
cdev_decl(rl);
cdev_decl(ccd);
cdev_decl(raid);
cdev_decl(rd);
cdev_decl(ry);
cdev_decl(sd);
cdev_decl(st);

#include "ct.h"
cdev_decl(ct);
#include "dh.h"
cdev_decl(dh);
#include "dmf.h"
cdev_decl(dmf);

#if VAX8600 || VAXANY
#define NCRL 1
#else
#define NCRL 0
#endif
#define crlread crlrw
#define crlwrite crlrw
cdev_decl(crl);

#if VAX8200 || VAXANY
#define NCRX 1
#else
#define NCRX 0
#endif
#define crxread crxrw
#define crxwrite crxrw
cdev_decl(crx);

#if VAX780 || VAXANY
#define NCFL 1
#else
#define NCFL 0
#endif
#define cflread cflrw
#define cflwrite cflrw
cdev_decl(cfl);

#include "dz.h"
cdev_decl(dz);

#include "vp.h"
cdev_decl(vp);

#include "lp.h"
cdev_decl(lp);

#include "va.h"
cdev_decl(va);

#include "lpa.h"
cdev_decl(lpa);

#include "dn.h"
cdev_decl(dn);

#include "ik.h"
cdev_decl(ik);

#include "ps.h"
cdev_decl(ps);

#include "ad.h"
cdev_decl(ad);

#include "dhu.h"
cdev_decl(dhu);

#include "dmz.h"
cdev_decl(dmz);

cdev_decl(qv);
cdev_decl(qd);

#include "ipfilter.h"
cdev_decl(ipl);

#include "dl.h"
cdev_decl(dl);

#if defined(INGRES)
#define NII 1
#else
#define NII 0
#endif
cdev_decl(ii);

cdev_decl(vnd);

#include "bpfilter.h"
cdev_decl(bpf);

#include "tun.h" 
cdev_decl(tun);
cdev_decl(cd);
#include "ch.h"
cdev_decl(ch);
cdev_decl(md);
#include "ss.h"
cdev_decl(ss);
#include "uk.h"
cdev_decl(uk);

#include "rnd.h"

dev_decl(filedesc,open);

#include "wsdisplay.h"
cdev_decl(wsdisplay);
#include "wskbd.h"
cdev_decl(wskbd);
#include "wsmouse.h"
cdev_decl(wsmouse);
#include "wsmux.h"
cdev_decl(wsmux);

#include "scsibus.h"
cdev_decl(scsibus);

#include "clockctl.h"
cdev_decl(clockctl);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_tty_init(NDZ,dz),		/* 1: DZ11 */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
	cdev_disk_init(NHP,hp),		/* 4: Massbuss disk */
	cdev_notdef(),			/* 5 */
	cdev_plotter_init(NVP,vp),	/* 6: Versatec plotter */
	cdev_swap_init(1,sw),		/* 7 */
	cdev_cnstore_init(NCFL,cfl),	/* 8: 11/780 console floppy */
	cdev_disk_init(NRA,ra),		/* 9: MSCP disk interface */
	cdev_plotter_init(NVA,va),	/* 10: Benson-Varian plotter */
	cdev_disk_init(NRK,rk),		/* 11: RK06/07 */
	cdev_tty_init(NDH,dh),		/* 12: DH-11/DM-11 */
	cdev_disk_init(NUP,up),		/* 13: SC-21/SC-31 */
	cdev_tape_init(NTE,tm),		/* 14: TM11/TE10 */
	cdev_lp_init(NLP,lp),		/* 15: LP-11 line printer */
	cdev_tape_init(NTS,ts),		/* 16: TS11 */
	cdev_tape_init(NTJ,ut),		/* 17: TU45 */
	cdev_lp_init(NCT,ct),		/* 18: phototypesetter interface */
	cdev_tape_init(NMU,mu),		/* 19: TU78 */
	cdev_tty_init(NPTY,pts),	/* 20: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 21: pseudo-tty master */
	cdev_tty_init(NDMF,dmf),	/* 22: DMF32 */
	cdev_disk_init(NRB,idc),	/* 23: IDC (RB730) */
	cdev_lp_init(NDN,dn),		/* 24: DN-11 autocall unit */
	cdev_tty_init(NGEN,gencn),	/* 25: Generic console (mtpr...) */
	cdev_audio_init(NLPA,lpa),	/* 26 ??? */
	cdev_graph_init(NPS,ps),	/* 27: E/S graphics device */
	cdev_lkm_init(NLKM,lkm),	/* 28: loadable module driver */
	cdev_ch_init(NAD,ad),		/* 29: DT A/D converter */
	cdev_disk_init(NRX,rx),		/* 30: RX?? on MSCP */
	cdev_graph_init(NIK,ik),	/* 31: Ikonas frame buffer */
	cdev_disk_init(NRL,rl),		/* 32: RL01/02 on unibus */
	cdev_log_init(1,log),		/* 33: /dev/klog */
	cdev_tty_init(NDHU,dhu),	/* 34: DHU-11 */
	cdev_cnstore_init(NCRL,crl),	/* 35: Console RL02 on 8600 */
	cdev_notdef(),			/* 36 */
	cdev_tty_init(NDMZ,dmz),	/* 37: DMZ32 */
	cdev_tape_init(NMT,mt),		/* 38: MSCP tape */
	cdev_notdef(),			/* 39  was NP Intelligent Board */
	cdev_graph_init(NQV,qv),	/* 40: QVSS graphic display */
	cdev_graph_init(NQD,qd),	/* 41: QDSS graphic display */
	cdev_ipf_init(NIPFILTER,ipl),	/* 42: Packet filter */
	cdev_ingres_init(NII,ii),	/* 43: Ingres device */
	cdev_notdef(),			/* 44  was Datakit */
	cdev_notdef(),			/* 45  was Datakit */
	cdev_notdef(),			/* 46  was Datakit */
	cdev_notdef(),			/* 47 */
	cdev_notdef(),			/* 48 */
	cdev_notdef(),			/* 49 */
	cdev_notdef(),			/* 50 */
	cdev_cnstore_init(NCRX,crx),	/* 51: Console RX50 at 8200 */
	cdev_notdef(),			/* 52: was: KDB50/RA?? */
	cdev_fd_init(1,filedesc),	/* 53: file descriptor pseudo-device */
	cdev_disk_init(NCCD,ccd),	/* 54: concatenated disk driver */
	cdev_disk_init(NVND,vnd),	/* 55: vnode disk driver */
	cdev_bpftun_init(NBPFILTER,bpf),/* 56: berkeley packet filter */
	cdev_bpftun_init(NTUN,tun),	/* 57: tunnel filter */
	cdev_disk_init(NRD,rd),		/* 58: HDC9224/RD?? */
	cdev_disk_init(NSD,sd),		/* 59: SCSI disk */
	cdev_tape_init(NST,st),		/* 60: SCSI tape */
	cdev_disk_init(NCD,cd),		/* 61: SCSI CD-ROM */
	cdev_disk_init(NMD,md),		/* 62: memory disk driver */
	cdev_ch_init(NCH,ch),		/* 63: SCSI autochanger */
	cdev_scanner_init(NSS,ss),	/* 64: SCSI scanner */
	cdev_uk_init(NUK,uk),		/* 65: SCSI unknown */
	cdev_tty_init(NDL,dl),		/* 66: DL11 */
	cdev_rnd_init(NRND,rnd),	/* 67: random source pseudo-device */
	cdev_wsdisplay_init(NWSDISPLAY,
			wsdisplay),	/* 68: frame buffers, etc. */

	cdev_mouse_init(NWSKBD, wskbd),	/* 69: keyboards */
	cdev_mouse_init(NWSMOUSE,
			wsmouse),	/* 70: mice */
	cdev_disk_init(NRY,ry),		/* 71: VS floppy */
	cdev_scsibus_init(NSCSIBUS,scsibus), /* 72: SCSI bus */
	cdev_disk_init(NRAID,raid),	/* 73: RAIDframe disk driver */
	cdev_mouse_init(NWSMUX, wsmux),	/* 74: ws multiplexor */
	cdev_clockctl_init(NCLOCKCTL, clockctl),/* 75: clockctl pseudo device */
#ifdef SYSTRACE
	cdev_clonemisc_init(1, systrace),/* 76: system call tracing */
#else
	cdev_notdef(),			/* 76: system call tracing */
#endif
};
int	nchrdev = sizeof(cdevsw) / sizeof(cdevsw[0]);

int	mem_no = 3;	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(4, 0);

int	chrtoblktbl[] = {
	NODEV,	/* 0 */
	NODEV,	/* 1 */
	NODEV,	/* 2 */
	NODEV,	/* 3 */
	0,	/* 4 */
	1,	/* 5 */
	NODEV,	/* 6 */
	NODEV,	/* 7 */
	NODEV,	/* 8 */
	9,	/* 9 */
	NODEV,	/* 10 */
	3,	/* 11 */
	NODEV,	/* 12 */
	2,	/* 13 */
	5,	/* 14 */
	NODEV,	/* 15 */
	6,	/* 16 */
	10,	/* 17 */
	NODEV,	/* 18 */
	7,	/* 19 */
	NODEV,	/* 20 */
	NODEV,	/* 21 */
	NODEV,	/* 22 */
	11,	/* 23 */
	NODEV,	/* 24 */
	NODEV,	/* 25 */
	NODEV,	/* 26 */
	NODEV,	/* 27 */
	NODEV,	/* 28 */
	NODEV,	/* 29 */
	12,	/* 30 */
	NODEV,	/* 31 */
	14,	/* 32 */
	NODEV,	/* 33 */
	NODEV,	/* 34 */
	NODEV,	/* 35 */
	NODEV,	/* 36 */
	NODEV,	/* 37 */
	15,	/* 38 */
	NODEV,	/* 39 */
	NODEV,	/* 40 */
	NODEV,	/* 41 */
	NODEV,	/* 42 */
	NODEV,	/* 43 */
	NODEV,	/* 44 */
	NODEV,	/* 45 */
	NODEV,	/* 46 */
	NODEV,	/* 47 */
	NODEV,	/* 48 */
	NODEV,	/* 49 */
	NODEV,	/* 50 */
	NODEV,	/* 51 */
	16,	/* 52 */
	NODEV,	/* 53 */
	17,	/* 54 */
	18,	/* 55 */
	NODEV,	/* 56 */
	NODEV,	/* 57 */
	19,	/* 58 */
	20,	/* 59 */
	21,	/* 60 */
	22,	/* 61 */
	23,	/* 62 */
	NODEV,	/* 63 */
	NODEV,	/* 64 */
	NODEV,	/* 65 */
	NODEV,	/* 66 */
	NODEV,	/* 67 */
	NODEV,	/* 68 */
	NODEV,	/* 69 */
	NODEV,	/* 70 */
	NODEV,	/* 71 */
	NODEV,	/* 72 */
	25,	/* 73 */
	NODEV,	/* 74 */
	NODEV,	/* 75 */
	NODEV,	/* 76 */
};

dev_t
chrtoblk(dev)
	dev_t dev;
{
	if(major(dev)>=nchrdev) return(NODEV);
	return chrtoblktbl[major(dev)]==NODEV?NODEV:
		makedev(chrtoblktbl[major(dev)],minor(dev));
}

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == 3 && minor(dev) < 2);
}

/*
 * Returns true if dev is /dev/zero.
 * ?? Shall I use 12 as /dev/zero?
 */
int
iszerodev(dev)
	dev_t dev;
{

	return (major(dev) == 3 && minor(dev) == 12);
}

int
getmajor(void *ptr)
{
	int i;

	for (i = 0; i < nchrdev; i++)
		if (cdevsw[i].d_open == ptr)
			return i;
	panic("getmajor");
}
