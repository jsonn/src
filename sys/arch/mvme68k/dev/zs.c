/*	$NetBSD: zs.c,v 1.12.12.1 1997/11/04 23:06:59 mellon Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Zilog Z8530 Dual UART driver (machine-dependent part)
 *
 * Runs two serial lines per chip using slave drivers.
 * Plain tty/async lines use the zs_async slave.
 *
 * Modified for NetBSD/mvme68k by Jason R. Thorpe <thorpej@NetBSD.ORG>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <dev/cons.h>
#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#include <machine/cpu.h>

#include <mvme68k/dev/zsvar.h>

/*
 * Some warts needed by z8530tty.c -
 * The default parity REALLY needs to be the same as the PROM uses,
 * or you can not see messages done with printf during boot-up...
 */
int zs_def_cflag = (CREAD | CS8 | HUPCL);
/* XXX Shouldn't hardcode the minor number... */
int zs_major = 12;

static u_long zs_sir;	/* software interrupt cookie */

/* Flags from zscnprobe() */
static int zs_hwflags[NZSC][2];

/* Default speed for each channel */
static int zs_defspeed[NZSC][2] = {
	{ 9600, 	/* port 1 */
	  9600 },	/* port 2 */
	{ 9600, 	/* port 3 */
	  9600 },	/* port 4 */
};

static struct zs_chanstate zs_conschan_store;
static struct zs_chanstate *zs_conschan;

u_char zs_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	0,	/* 1: No interrupts yet. */
	0x18 + ZSHARD_PRI,	/* IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE,
	0,	/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	14,	/*12: BAUDLO (default=9600) */
	0,	/*13: BAUDHI (default=9600) */
	ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK,
	ZSWR15_BREAK_IE | ZSWR15_DCD_IE,
};


/****************************************************************
 * Autoconfig
 ****************************************************************/

/* Definition of the driver for autoconfig. */
static int	zsc_print __P((void *, const char *name));

struct cfdriver zsc_cd = {
	NULL, "zsc", DV_DULL
};

static int zs_get_speed __P((struct zs_chanstate *));


/*
 * Configure children of an SCC.
 */
void
zs_config(zsc, chan_addr)
	struct zsc_softc *zsc;
	struct zschan *(*chan_addr) __P((int, int));
{
	struct zsc_attach_args zsc_args;
	volatile struct zschan *zc;
	struct zs_chanstate *cs;
	int zsc_unit, channel, s;

	zsc_unit = zsc->zsc_dev.dv_unit;
	printf(": Zilog 8530 SCC\n");

	/*
	 * Initialize software state for each channel.
	 */
	for (channel = 0; channel < 2; channel++) {
		zsc_args.channel = channel;
		zsc_args.hwflags = zs_hwflags[zsc_unit][channel];
		cs = &zsc->zsc_cs_store[channel];
		zsc->zsc_cs[channel] = cs;

		/*
		 * If we're the console, copy the channel state, and
		 * adjust the console channel pointer.
		 */
		if (zsc_args.hwflags & ZS_HWFLAG_CONSOLE) {
			bcopy(zs_conschan, cs, sizeof(struct zs_chanstate));
			zs_conschan = cs;
		} else {
			zc = (*chan_addr)(zsc_unit, channel);
			cs->cs_reg_csr  = &zc->zc_csr;
			cs->cs_reg_data = &zc->zc_data;
			bcopy(zs_init_reg, cs->cs_creg, 16);
			bcopy(zs_init_reg, cs->cs_preg, 16);
			cs->cs_defspeed = zs_defspeed[zsc_unit][channel];
		}
		cs->cs_defcflag = zs_def_cflag;

		/* Make these correspond to cs_defcflag (-crtscts) */
		cs->cs_rr0_dcd = ZSRR0_DCD;
		cs->cs_rr0_cts = 0;
		cs->cs_wr5_dtr = ZSWR5_DTR | ZSWR5_RTS;
		cs->cs_wr5_rts = 0;

		cs->cs_channel = channel;
		cs->cs_private = NULL;
		cs->cs_ops = &zsops_null;
		cs->cs_brg_clk = PCLK / 16;

		/*
		 * Clear the master interrupt enable.
		 * The INTENA is common to both channels,
		 * so just do it on the A channel.
		 */
		if (channel == 0) {
			zs_write_reg(cs, 9, 0);
		}

		/*
		 * Look for a child driver for this channel.
		 * The child attach will setup the hardware.
		 */
		if (!config_found(&zsc->zsc_dev, (void *)&zsc_args, zsc_print)) {
			/* No sub-driver.  Just reset it. */
			u_char reset = (channel == 0) ?
				ZSWR9_A_RESET : ZSWR9_B_RESET;
			s = splzs();
			zs_write_reg(cs,  9, reset);
			splx(s);
		}
	}

	/*
	 * Allocate a software interrupt cookie.  Note that the argument
	 * "zsc" is never actually used in the software interrupt
	 * handler.
	 */
	if (zs_sir == 0)
		zs_sir = allocate_sir(zssoft, zsc);
}

static int
zsc_print(aux, name)
	void *aux;
	const char *name;
{
	struct zsc_attach_args *args = aux;

	if (name != NULL)
		printf("%s: ", name);

	if (args->channel != -1)
		printf(" channel %d", args->channel);

	return UNCONF;
}

static int zssoftpending;

/*
 * Our ZS chips all share a common, autovectored interrupt,
 * so we have to look at all of them on each interrupt.
 */
int
zshard(arg)
	void *arg;
{
	register struct zsc_softc *zsc;
	register int unit, rval;

	rval = 0;
	for (unit = 0; unit < zsc_cd.cd_ndevs; unit++) {
		zsc = zsc_cd.cd_devs[unit];
		if (zsc == NULL)
			continue;
		rval |= zsc_intr_hard(zsc);
		if ((zsc->zsc_cs[0]->cs_softreq) ||
			(zsc->zsc_cs[1]->cs_softreq))
		{
			/* zsc_req_softint(zsc); */
			/* We are at splzs here, so no need to lock. */
			if (zssoftpending == 0) {
				zssoftpending = zs_sir;
				setsoftint(zs_sir);
			}
		}
	}
	return (rval);
}

/*
 * Similar scheme as for zshard (look at all of them)
 */
int
zssoft(arg)
	void *arg;
{
	register struct zsc_softc *zsc;
	register int unit;

	/* This is not the only ISR on this IPL. */
	if (zssoftpending == 0)
		return (0);

	/*
	 * The soft intr. bit will be set by zshard only if
	 * the variable zssoftpending is zero.
	 */
	zssoftpending = 0;

	for (unit = 0; unit < zsc_cd.cd_ndevs; ++unit) {
		zsc = zsc_cd.cd_devs[unit];
		if (zsc == NULL)
			continue;
		(void) zsc_intr_soft(zsc);
	}
	return (1);
}


/*
 * Compute the current baud rate given a ZSCC channel.
 */
static int
zs_get_speed(cs)
	struct zs_chanstate *cs;
{
	int tconst;

	tconst = zs_read_reg(cs, 12);
	tconst |= zs_read_reg(cs, 13) << 8;
	return (TCONST_TO_BPS(cs->cs_brg_clk, tconst));
}

/*
 * MD functions for setting the baud rate and control modes.
 */
int
zs_set_speed(cs, bps)
	struct zs_chanstate *cs;
	int bps;	/* bits per second */
{
	int tconst, real_bps;

	if (bps == 0)
		return (0);

#ifdef	DIAGNOSTIC
	if (cs->cs_brg_clk == 0)
		panic("zs_set_speed");
#endif

	tconst = BPS_TO_TCONST(cs->cs_brg_clk, bps);
	if (tconst < 0)
		return (EINVAL);

	/* Convert back to make sure we can do it. */
	real_bps = TCONST_TO_BPS(cs->cs_brg_clk, tconst);

	/* XXX - Allow some tolerance here? */
	if (real_bps != bps)
		return (EINVAL);

	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;

	/* Caller will stuff the pending registers. */
	return (0);
}

int
zs_set_modes(cs, cflag)
	struct zs_chanstate *cs;
	int cflag;	/* bits per second */
{
	int s;

	/*
	 * Output hardware flow control on the chip is horrendous:
	 * if carrier detect drops, the receiver is disabled, and if
	 * CTS drops, the transmitter is stoped IN MID CHARACTER!
	 * Therefore, NEVER set the HFC bit, and instead use the
	 * status interrupt to detect CTS changes.
	 */
	s = splzs();
	if ((cflag & (CLOCAL | MDMBUF)) != 0)
		cs->cs_rr0_dcd = 0;
	else
		cs->cs_rr0_dcd = ZSRR0_DCD;
	if ((cflag & CRTSCTS) != 0) {
		cs->cs_wr5_dtr = ZSWR5_DTR;
		cs->cs_wr5_rts = ZSWR5_RTS;
		cs->cs_rr0_cts = ZSRR0_CTS;
	} else if ((cflag & MDMBUF) != 0) {
		cs->cs_wr5_dtr = 0;
		cs->cs_wr5_rts = ZSWR5_DTR;
		cs->cs_rr0_cts = ZSRR0_DCD;
	} else {
		cs->cs_wr5_dtr = ZSWR5_DTR | ZSWR5_RTS;
		cs->cs_wr5_rts = 0;
		cs->cs_rr0_cts = 0;
	}
	splx(s);

	/* Caller will stuff the pending registers. */
	return (0);
}


/*
 * Read or write the chip with suitable delays.
 */

u_char
zs_read_reg(cs, reg)
	struct zs_chanstate *cs;
	u_char reg;
{
	u_char val;

	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	val = *cs->cs_reg_csr;
	ZS_DELAY();
	return val;
}

void
zs_write_reg(cs, reg, val)
	struct zs_chanstate *cs;
	u_char reg, val;
{
	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

u_char zs_read_csr(cs)
	struct zs_chanstate *cs;
{
	register u_char val;

	val = *cs->cs_reg_csr;
	ZS_DELAY();
	return val;
}

void  zs_write_csr(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

u_char zs_read_data(cs)
	struct zs_chanstate *cs;
{
	register u_char val;

	val = *cs->cs_reg_data;
	ZS_DELAY();
	return val;
}

void  zs_write_data(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	*cs->cs_reg_data = val;
	ZS_DELAY();
}

/****************************************************************
 * Console support functions (MVME specific!)
 ****************************************************************/

/*
 * Polled input char.
 */
int
zs_getc(arg)
	void *arg;
{
	register struct zs_chanstate *cs = arg;
	register int s, c, rr0, stat;

	s = splhigh();
 top:
	/* Wait for a character to arrive. */
	do {
		rr0 = *cs->cs_reg_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_RX_READY) == 0);

	/* Read error register. */
	stat = zs_read_reg(cs, 1) & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE);
	if (stat) {
		zs_write_csr(cs, ZSM_RESET_ERR);
		goto top;
	}

	/* Read character. */
	c = *cs->cs_reg_data;
	ZS_DELAY();
	splx(s);

	return (c);
}

/*
 * Polled output char.
 */
void
zs_putc(arg, c)
	void *arg;
	int c;
{
	register struct zs_chanstate *cs = arg;
	register int s, rr0;

	s = splhigh();
	/* Wait for transmitter to become ready. */
	do {
		rr0 = *cs->cs_reg_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_TX_READY) == 0);

	*cs->cs_reg_data = c;
	ZS_DELAY();
	splx(s);
}

/*
 * Common parts of console init.
 */
void
zs_cnconfig(zsc_unit, channel, zc)
	int zsc_unit, channel;
	struct zschan *zc;
{
	struct zs_chanstate *cs;

	/*
	 * Pointer to channel state.  Later, the console channel
	 * state is copied into the softc, and the console channel
	 * pointer adjusted to point to the new copy.
	 */
	zs_conschan = cs = &zs_conschan_store;
	zs_hwflags[zsc_unit][channel] = ZS_HWFLAG_CONSOLE;

	/* Setup temporary chanstate. */
	cs->cs_reg_csr  = &zc->zc_csr;
	cs->cs_reg_data = &zc->zc_data;

	/* Initialize the pending registers. */
	bcopy(zs_init_reg, cs->cs_preg, 16);
	cs->cs_preg[5] |= (ZSWR5_DTR | ZSWR5_RTS);

	/* XXX: Preserve BAUD rate from boot loader. */
	/* XXX: Also, why reset the chip here? -gwr */
	/* cs->cs_defspeed = zs_get_speed(cs); */
	cs->cs_defspeed = 9600;	/* XXX */

	/* Clear the master interrupt enable. */
	zs_write_reg(cs, 9, 0);

	/* Reset the whole SCC chip. */
	zs_write_reg(cs, 9, ZSWR9_HARD_RESET);

	/* Copy "pending" to "current" and H/W. */
	zs_loadchannelregs(cs);
}

/*
 * Polled console input putchar.
 */
int
zscngetc(dev)
	dev_t dev;
{
	register struct zs_chanstate *cs = zs_conschan;
	register int c;

	c = zs_getc(cs);
	return (c);
}

/*
 * Polled console output putchar.
 */
void
zscnputc(dev, c)
	dev_t dev;
	int c;
{
	register struct zs_chanstate *cs = zs_conschan;

	zs_putc(cs, c);
}

/*
 * Handle user request to enter kernel debugger.
 */
void
zs_abort(cs)
	struct zs_chanstate *cs;
{
	int rr0;

	/* Wait for end of break to avoid PROM abort. */
	/* XXX - Limit the wait? */
	do {
		rr0 = *cs->cs_reg_csr;
		ZS_DELAY();
	} while (rr0 & ZSRR0_BREAK);

	mvme68k_abort("SERIAL LINE ABORT");
}
