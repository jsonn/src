/*	$NetBSD: cpu_cons.c,v 1.20.2.2 1998/11/24 06:20:57 cgd Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: cons.c 1.1 90/07/09
 *
 *	@(#)cons.c	8.2 (Berkeley) 1/11/94
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: cpu_cons.c,v 1.20.2.2 1998/11/24 06:20:57 cgd Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <dev/cons.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>

#include <pmax/stand/dec_prom.h>

#include <pmax/pmax/kn01.h>
#include <pmax/pmax/kn02.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/turbochannel.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/dev/rconsvar.h>

#include <machine/pmioctl.h>

#include <machine/fbio.h>
#include <machine/fbvar.h>

#include <pmax/dev/fbreg.h>

#include <machine/autoconf.h>
#include <pmax/dev/lk201var.h>
#include <dev/tc/tcvar.h>

#include "pm.h"
#include "cfb.h"
#include "mfb.h"
#include "xcfb.h"
#include "sfb.h"
#include "dc_ds.h"
#include "dc_ioasic.h"
#include "dtop.h"
#include "scc.h"
#include "asc.h"
#include "tc.h"
#include  "rasterconsole.h"

#if (NDC_DS > 0) || (NDC_IOASIC > 0)
#include <machine/dc7085cons.h>
#include <pmax/dev/dc_cons.h>
#include <pmax/dev/dc_ds_cons.h>
#include <pmax/dev/dc_ioasic_cons.h>
#endif

#if NDTOP > 0
#include <pmax/dev/dtopvar.h>
#endif

#if NSCC > 0
#include <pmax/tc/sccvar.h>
#include <pmax/pmax/asic.h>		/* scc serial console addresses */
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#endif

#if NPM > 0
#include <pmax/dev/pmvar.h>
#endif

#if NXCFB > 0
#include <pmax/dev/xcfbvar.h>
#endif


static int romgetc __P ((dev_t));
static void romputc __P ((dev_t, int));
static void rompollc __P((dev_t, int));


extern int	pmax_boardtype;		/* Mother board type */

/*
 * Major device numbers for possible console devices. XXX
 */
#define	DTOPDEV		15
#define	DCDEV		16
#define	SCCDEV		17
#define RCONSDEV	85

/*
 * Console I/O is redirected to the appropriate device, either a screen and
 * keyboard, a serial port, or the "virtual" console.
 */

struct consdev cd = {
	(void (*)(struct consdev *))0,		/* probe */
	(void (*)(struct consdev *))0,		/* init */
	(int  (*)(dev_t))     romgetc,		/* getc */
	(void (*)(dev_t, int))romputc,		/* putc */
	(void (*)(dev_t, int))rompollc,		/* pollc */
	makedev (0, 0),
	CN_DEAD,
};

/*
 * Should be locals of consinit, but that's split in two until
 * new-style config is finished
 */

/*
 * Forward declarations
 */

void consinit __P((void));
void xconsinit __P((void));


extern struct tc_cpu_desc *  cpu_tcdesc __P ((int cputype));


int kbd;
int pending_remcons = 0;

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
	int crt;
	register char *oscon;
	int screen = 0;

#ifdef RCONS_HACK
	extern void (*v_putc) __P ((dev_t, int));
#endif
	cn_tab = &cd;


	/*
	 * First get the "osconsole" environment variable.
	 */
	oscon = (*callv->_getenv)("osconsole");
	crt = kbd = -1;
	if (oscon && *oscon >= '0' && *oscon <= '9') {
		kbd = *oscon - '0';
		/*cn_tab.cn_pri = CN_DEAD;*/
		screen = 0;
		while (*++oscon) {
			if (*oscon == ',')
				/*cn_tab.cn_pri = CN_INTERNAL;*/
				screen = 1;
			else if (screen &&
			    *oscon >= '0' && *oscon <= '9') {
				crt = kbd;
				kbd = *oscon - '0';
				break;
			}
		}
	}

	/*
	 * We can't do anything until auto-configuration
	 * has run, and that requires kmalloc(), which
	 * hasn't been initialized yet.  Just keep using
	 * whatever the PROM vector gave us.
	 */

#if NRASTERCONSOLE > 0
	if (pmax_boardtype == DS_PMAX && kbd == 1)
		screen = 1;
	/*
	 * The boot program uses PMAX ROM entrypoints so the ROM sets
	 * osconsole to '1' like the PMAX.
	 */
	if (pmax_boardtype == DS_3MAX && crt == -1 && kbd == 1) {
		/* Try to use pmax onboard framebuffer */
		screen = 1;
		crt = 0;
		kbd = 7;
	}

	/*
	 * First try the keyboard/crt cases then fall through to the
	 * remote serial lines.
	 */
	if (screen) {
	    switch (pmax_boardtype) {
	    case DS_PMAX:
#if NDC_DS > 0 && NPM > 0
		if (pminit(0, 0, 1)) {
			cd.cn_pri = CN_INTERNAL;
			cd.cn_dev = makedev(DCDEV, DCKBD_PORT);
			cd.cn_getc = LKgetc;
			lk_divert(dcGetc, makedev(DCDEV, DCKBD_PORT));
			cd.cn_dev = makedev(RCONSDEV, 0);
			cd.cn_putc = rcons_vputc;	/*XXX*/
			return;
		}
#endif /* NDC_DS and NPM */
		goto remcons;

	    case DS_MAXINE:
#if NDTOP > 0
		if (kbd == 3) {
			cd.cn_dev = makedev(RCONSDEV, 0);
			cd.cn_getc = dtopKBDGetc;
		} else
#endif /* NDTOP */
			goto remcons;
#if NXCFB > 0
		if (crt == 3 && xcfbinit(NULL, NULL, 0, 0)) {
			cd.cn_pri = CN_INTERNAL;
			cd.cn_putc = rcons_vputc;	/*XXX*/
			return;
		}
#endif /* XCFB */
		break;

	    case DS_3MAX:
#if NDC_IOASIC > 0
		if (kbd == 7) {
			cd.cn_dev = makedev(DCDEV, DCKBD_PORT);
			cd.cn_getc = LKgetc;
			lk_divert(dcGetc, makedev(DCDEV, DCKBD_PORT));
		} else
#endif /* NDC_IOASIC */
			goto remcons;
		break;

	    case DS_3MIN:
	    case DS_3MAXPLUS:
#if NSCC > 0
		if (kbd == 3) {
			/*cd.cn_dev = makedev (RCONSDEV, 0);*/
			cd.cn_dev =  makedev(SCCDEV, SCCKBD_PORT);
			lk_divert(sccGetc, makedev(SCCDEV, SCCKBD_PORT));
			cd.cn_getc = LKgetc;
		} else
#endif /* NSCC */
			goto remcons;
		break;

	    /*
	     * No configured baseboard video found, or 
	     * no configured baseboard keyboard found. Fallback to serial.
	     */
	    default:
		goto remcons;
	    };


	    /*
	     * Check for a suitable turbochannel frame buffer.
	     */
#if NTC>0
	    if (tc_findconsole(crt)) {

			cd.cn_pri = CN_NORMAL;
			/*
			 * send console output to rcons output, with
			 * input from cn_dev as computed above.
			 */
	      		rcons_indev(&cd);
			return;
	    } else
#endif
	    {
		printf("No supported console device in slot %d. ", crt);
	    	printf("Switching over to serial console!\n");
	    }
	}


remcons:

#endif	/* NRASTERCONSOLE > 0 */

	/*
	 * Configure a serial port as a remote console.
	 */
	switch (pmax_boardtype) {
	case DS_PMAX:
#if NDC_DS > 0
		if (kbd == 4)
			cd.cn_dev = makedev(DCDEV, DCCOMM_PORT);
		else
			cd.cn_dev = makedev(DCDEV, DCPRINTER_PORT);
		dc_ds_consinit(cd.cn_dev);
		return;
#endif /* NDC_DS */
		break;

	case DS_3MAX:
#if (NDC_IOASIC > 0)
		cd.cn_dev = makedev(DCDEV, DCPRINTER_PORT);
		dc_ioasic_consinit(cd.cn_dev);
		return;
#endif /* NDC_IOASIC */
		break;


#if (NSCC > 0)
#define TC_KV(x) ((tc_addr_t)MIPS_PHYS_TO_KSEG1(x))
	case DS_MAXINE:
		cd.cn_dev = makedev(SCCDEV, SCCCOMM2_PORT);
		scc_consinit(cd.cn_dev,
		    (void*)(TC_KV(XINE_SYS_ASIC) + IOASIC_SLOT_4_START));
		return;

	case DS_3MIN:
		cd.cn_dev = makedev(SCCDEV, SCCCOMM3_PORT);
		scc_consinit(cd.cn_dev,
		    (void*)(TC_KV(KMIN_SYS_ASIC) + IOASIC_SLOT_6_START));
		return;

	case DS_3MAXPLUS:
		cd.cn_dev = makedev(SCCDEV, SCCCOMM3_PORT);
		scc_consinit(cd.cn_dev,
		    (void*)(TC_KV(KN03_SYS_ASIC) + IOASIC_SLOT_6_START));
		return;
#endif /* NSCC */
	}

	/*
	 * XXX
	 * scc serial drivers need to be rewritten to handle init this early.
	 * Defer switching to non-PROM driver until after serial device
	 * is configured normally.
	 */
	pending_remcons = 1;
	printf("Using PROM serial output until serial drivers initialized\n");

	/* We never changed output; go back to using PROM input */
	cd.cn_dev = makedev (0, 0);
	cd.cn_getc = /*(int (*)(dev_t)) */ romgetc;
}


/*
 * Configure a serial port as a remote console.
 * Called by configure() to switch from PROM I/O when the serial
 * device-driver cannot be set up as a serial console until
 * autoconfiguration is done.  (i.e., only needed for scc serial driver.)
 */
void
xconsinit()
{
	if (!pending_remcons)
		return;

	pending_remcons = 0;
	switch (pmax_boardtype) {
	case DS_3MIN:
	case DS_3MAXPLUS:
#if NSCC > 0
		cd.cn_dev = makedev(SCCDEV, SCCCOMM3_PORT);
		cd.cn_getc = sccGetc;
		cd.cn_putc = sccPutc;
		cd.cn_pri = CN_REMOTE;
#endif /* NSCC */
		break;

	case DS_MAXINE:
#if NSCC > 0
		cd.cn_dev = makedev(SCCDEV, SCCCOMM2_PORT);
		cd.cn_getc = sccGetc;
		cd.cn_putc = sccPutc;
		cd.cn_pri = CN_REMOTE;
#endif /* NSCC */
		break;
	};
	if (cd.cn_dev == NODEV)
		printf("Can't configure console!\n");
}



/*
 * Get character from ROM console.
 */
static int
romgetc(dev)
	dev_t dev;
{
	int s = splhigh ();
	int chr;
	chr = (*callv->_getchar)();
	splx (s);
	return chr;
}

/*
 * Print a character on ROM console.
 */
static void
romputc (dev, c)
	dev_t dev;
	register int c;
{
	int s;
	s = splhigh();
	(*callv->_printf)("%c", c);
	splx(s);
}

static void
rompollc (dev, c)
	dev_t dev;
	register int c;
{
	return;
}


#ifdef notanymore
/*
 * select() on a possibly-redirected console.
 */

extern struct	tty *constty;		/* virtual console output device */
extern struct	consdev *cn_tab;	/* physical console device info */
extern struct	vnode *cn_devvp;	/* vnode for underlying device. */

/*ARGSUSED*/
int
pmax_cnselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{

	/*
	 * Redirect the ioctl, if that's appropriate.
	 * I don't want to think of the possible side effects
	 * of console redirection here.
	 */
	if (constty != NULL && (cn_tab == NULL || cn_tab->cn_pri != CN_REMOTE))
		dev = constty->t_dev;
	else if (cn_tab == NULL)
		return ENXIO;
	else
		dev = cn_tab->cn_dev;
#ifdef RCONS
	if (cn_tab -> cn_dev == makedev (85, 0))
		return rconsselect (cn_tab -> cn_dev, rw, p);
#endif
	return (ttselect(cn_tab->cn_dev, rw, p));
}
#endif /* notanymore */
