/* $NetBSD: nvt.c,v 1.4.2.2 2007/10/27 11:28:25 yamt Exp $ */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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

#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>

#include "globals.h"

/*
 * - reverse endian access every CSR.
 * - no vtophys() translation, vaddr_t == paddr_t. 
 * - PIPT writeback cache aware.
 */
#define CSR_WRITE_1(l, r, v)	*(volatile uint8_t *)((l)->csr+(r)) = (v)
#define CSR_READ_1(l, r)	*(volatile uint8_t *)((l)->csr+(r))
#define CSR_WRITE_2(l, r, v)	out16rb((l)->csr+(r), (v))
#define CSR_READ_2(l, r)	in16rb((l)->csr+(r))
#define CSR_WRITE_4(l, r, v)	out32rb((l)->csr+(r), (v))
#define CSR_READ_4(l, r)	in32rb((l)->csr+(r))
#define VTOPHYS(va)		(uint32_t)(va)
#define wbinv(adr, siz)		_wbinv(VTOPHYS(adr), (uint32_t)(siz))
#define inv(adr, siz)		_inv(VTOPHYS(adr), (uint32_t)(siz))
#define DELAY(n)		delay(n)
#define ALLOC(T,A)	(T *)((unsigned)alloc(sizeof(T) + (A)) &~ ((A) - 1))

void *nvt_init(void *);
int nvt_send(void *, char *, unsigned);
int nvt_recv(void *, char *, unsigned, unsigned);

#define R0_OWN          (1U << 31)      /* 1: empty for HW to load anew */
#define R0_FLMASK       0x7fff0000      /* frame length */
#define R0_RXOK         (1U << 15)
#define R0_MAR          (1U << 13)      /* multicast frame */
#define R0_BAR          (1U << 12)      /* broadcast frame */
#define R0_PHY          (1U << 11)      /* unicast frame */
#define R0_CHN          (1U << 10)      /* 1 for chained buffer */
#define R0_STP          (1U << 9)       /* first frame segment */
#define R0_EDP          (1U << 8)       /* last frame segment */
#define R0_BUFF         (1U << 7)       /* segment chain was broken */
#define R0_RUNT         (1U << 5)       /* runt frame received */
#define R0_LONG         (1U << 4)       /* frame too long */
#define R0_FOV          (1U << 3)       /* Rx FIFO overflow */
#define R0_FAE          (1U << 2)       /* frame alignment error */
#define R0_CRCE         (1U << 1)       /* CRC error */
#define R0_RERR         (1U << 0)       /* Rx error summary */
#define R1_FLMASK       0x00007ffc      /* Rx segment buffer length */

#define T0_OWN          (1U << 31)      /* 1: loaded for HW to send */
#define T0_FLMASK       0x7fff0000      /* segment length */
#define T0_TERR         (1U << 15)      /* Tx error; ABT|CBH */
#define T0_UDF          (1U << 11)      /* FIFO underflow */
#define T0_CRS          (1U << 10)      /* found carrier sense lost */
#define T0_OWC          (1U << 9)       /* found out of window collision */
#define T0_ABT          (1U << 8)       /* excess collision Tx abort */
#define T0_CBH          (1U << 7)       /* heartbeat check failure */
#define T0_COLS         (1U << 4)       /* collision detected */
#define T0_NCRMASK      0x3             /* number of collision retries */
#define T1_IC           (1U << 23)      /* post Tx done interrupt */
#define T1_STP          (1U << 22)      /* first frame segment */
#define T1_EDP          (1U << 21)      /* last frame segment */
#define T1_CRC          (1U << 16)      /* _disable_ CRC generation */ 
#define T1_CHN          (1U << 15)      /* chain structure mark */
#define T1_FLMASK       0x00007fff      /* Tx segment length */

struct desc {
	uint32_t xd0, xd1, xd2, xd3;
};

#define VR_PAR0		0x00		/* SA [0] */
#define VR_PAR1		0x01		/* SA [1] */
#define VR_PAR2		0x02		/* SA [2] */
#define VR_PAR3		0x03		/* SA [3] */
#define VR_PAR4		0x04		/* SA [4] */
#define VR_PAR5		0x05		/* SA [5] */
#define VR_RCR		0x06		/* Rx control */
#define	 RCR_PROM	(1U << 4)	/* accept any frame */
#define	 RCR_AB		(1U << 3)	/* accept broadcast frame */
#define	 RCR_AM		(1U << 2)	/* use multicast filter */
#define VR_TCR		0x07		/* Tx control */
#define VR_CTL0		0x08		/* control #0 */
#define	 CTL0_TXON	(1U << 4)
#define	 CTL0_RXON	(1U << 3)
#define	 CTL0_STOP	(1U << 2)
#define	 CTL0_START	(1U << 1)
#define VR_CTL1		0x09		/* control #1 */
#define	 CTL1_RESET	(1U << 8)
#define	 CTL1_FDX	(1U << 2)
#define VR_ISR		0x0c		/* interrupt status */
#define VR_IEN		0x0e		/* interrupt enable */
#define VR_RDBA		0x18		/* Rx descriptor list base */
#define VR_TDBA		0x1c		/* Tx descriptor list base */
#define VR_MIICFG	0x6c		/* 4:0 PHY number */
#define VR_MIISR	0x6d		/* MII status */
#define VR_MIICR	0x70		/* MII control */
#define	 MIICR_RCMD	(1U << 6)	/* MII read operation */
#define	 MIICR_WCMD	(1U << 5)	/* MII write operation */
#define VR_MIIADR	0x71		/* MII indirect */
#define VR_MIIDATA	0x72		/* MII read/write */
#define VR_CTLA		0x78		/* control A */
#define	 CTLA_LED	0x03		/* LED selection, same as MII 16[6:5] */
#define VR_CTLB		0x79		/* control B */
#define VR_CTLC		0x7a		/* control C */
#define VR_CTLD		0x7b		/* control D */
#define VR_RXC		0x7e		/* Rx feature control */
#define VR_TXC		0x7f		/* Tx feature control */
#define VR_MCR0		0x80		/* misc control #0 */
#define	 MCR0_RFDXFLC	(1U << 3)	/* FCR1? */
#define	 MCR0_HDXFLC	(1U << 2)	/* FCR2? */
#define VR_MCR1		0x81		/* misc control #1 */
#define VR_FCR0		0x90		/* flow control #0 */
#define VR_FCR1		0x91		/* flow control #1 */
#define	 FCR1_3XFLC	(1U << 3)	/* 802.3x PAUSE flow control */
#define	 FCR1_TPAUSE	(1U << 2)	/* handle PAUSE on transmit side */
#define	 FCR1_RPAUSE	(1U << 1)	/* handle PAUSE on receive side */
#define	 FCR1_HDXFLC	(1U << 0)	/* HDX jabber flow control */

#define FRAMESIZE	1536

struct local {
	struct desc TxD;
	struct desc RxD[2];
	uint8_t rxstore[2][FRAMESIZE];
	unsigned csr, rx;
	unsigned phy, bmsr, anlpar;
	unsigned rcr, ctl0;
};

static int nvt_mii_read(struct local *, int, int);
static void nvt_mii_write(struct local *, int, int, int);
static void mii_initphy(struct local *);

void *
nvt_init(void *cookie)
{
	unsigned tag, val;
	struct local *l;
	struct desc *TxD, *RxD;
	uint8_t *en;

	if (pcifinddev(0x1106, 0x3106, &tag) != 0
	    || pcifinddev(0x1106, 0x3053, &tag) != 0) {
		printf("nvt NIC not found\n");
		return NULL;
	}

	l = ALLOC(struct local, sizeof(struct desc));
	memset(l, 0, sizeof(struct local));
	l->csr = pcicfgread(tag, 0x14); /* use mem space */

	val = CTL1_RESET;
	CSR_WRITE_1(l, VR_CTL1, val);
	do {	
		val = CSR_READ_1(l, VR_CTL1);
	} while (val & CTL1_RESET);

	l->phy = CSR_READ_1(l, VR_MIICFG) & 0x1f;
	mii_initphy(l);

	en = cookie;
	en[0] = CSR_READ_1(l, VR_PAR0);
	en[1] = CSR_READ_1(l, VR_PAR1);
	en[2] = CSR_READ_1(l, VR_PAR2);
	en[3] = CSR_READ_1(l, VR_PAR3);
	en[4] = CSR_READ_1(l, VR_PAR4);
	en[5] = CSR_READ_1(l, VR_PAR5);
#if 1
	printf("MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
		en[0], en[1], en[2], en[3], en[4], en[5]);
#endif

	TxD = &l->TxD;
	RxD = &l->RxD[0];
	RxD[0].xd0 = htole32(R0_OWN);
	RxD[0].xd1 = htole32(FRAMESIZE << 16);
	RxD[0].xd2 = htole32(VTOPHYS(l->rxstore[0]));
	RxD[0].xd3 = htole32(&RxD[1]);
	RxD[1].xd0 = htole32(R0_OWN);
	RxD[1].xd1 = htole32(VTOPHYS(l->rxstore[1]));
	RxD[1].xd2 = htole32(FRAMESIZE << 16);
	RxD[1].xd3 = htole32(&RxD[0]);
	wbinv(TxD, sizeof(struct desc));
	wbinv(RxD, 2 * sizeof(struct desc));
	l->rx = 0;

	/* speed and duplexity can be seen in MIISR and MII 20 */

	/* enable transmitter and receiver */
	l->rcr = 0;
	l->ctl0 = (CTL0_TXON | CTL0_RXON | CTL0_START);
	CSR_WRITE_4(l, VR_RDBA, VTOPHYS(RxD));
	CSR_WRITE_4(l, VR_TDBA, VTOPHYS(TxD));
	CSR_WRITE_1(l, VR_RCR, l->rcr);
	CSR_WRITE_1(l, VR_TCR, 0);
	CSR_WRITE_2(l, VR_ISR, ~0);
	CSR_WRITE_2(l, VR_IEN, 0);

	CSR_WRITE_1(l, VR_CTL0, CTL0_START);
	CSR_WRITE_1(l, VR_CTL0, l->ctl0);

	return l;
}

int
nvt_send(void *dev, char *buf, unsigned len)
{
	struct local *l = dev;
	struct desc *TxD;
	int loop;
	
	wbinv(buf, len);
	TxD = &l->TxD;
	TxD->xd3 = htole32(TxD);
	TxD->xd2 = htole32(VTOPHYS(buf));
	TxD->xd1 = htole32(T1_STP | T1_EDP | (len & T1_FLMASK));
	TxD->xd0 = htole32(T0_OWN);
	wbinv(TxD, sizeof(struct desc));
	loop = 100;
	do {
		if ((le32toh(TxD->xd0) & T0_OWN) == 0)
			goto done;
		DELAY(10);
		inv(TxD, sizeof(struct desc));
	} while (--loop > 0);
	printf("xmit failed\n");
	return -1;
  done:
	return len;
}

int
nvt_recv(void *dev, char *buf, unsigned maxlen, unsigned timo)
{
	struct local *l = dev;
	struct desc *RxD;
	unsigned bound, rxstat, len;
	uint8_t *ptr;

	bound = 1000 * timo;
printf("recving with %u sec. timeout\n", timo);
  again:
	RxD = &l->RxD[l->rx];
	do {
		inv(RxD, sizeof(struct desc));
		rxstat = le32toh(RxD->xd0);
		if ((rxstat & R0_OWN) == 0)
			goto gotone;
		DELAY(1000);	/* 1 milli second */
	} while (bound-- > 0);
	errno = 0;
	return -1;
  gotone:
	if ((rxstat & R0_RXOK) == 0) {
		RxD->xd0 = htole32(R0_OWN);
		wbinv(RxD, sizeof(struct desc));
		l->rx ^= 1;
		goto again;
	}
	len = (rxstat & R0_FLMASK) >> 16; /* HASFCS? */
	if (len > maxlen)
		len = maxlen;
	ptr = l->rxstore[l->rx];
	inv(ptr, len);
	memcpy(buf, ptr, len);
	RxD->xd0 = htole32(R0_OWN);
	wbinv(RxD, sizeof(struct desc));
	l->rx ^= 1;
	return len;
}

static int
nvt_mii_read(struct local *l, int phy, int reg)
{
	int v;

	CSR_WRITE_1(l, VR_MIICFG, phy);
	CSR_WRITE_1(l, VR_MIIADR, reg);
	CSR_WRITE_1(l, VR_MIICR, MIICR_RCMD);
	do {
		v = CSR_READ_1(l, VR_MIICR);
	} while (v & MIICR_RCMD); 
	return CSR_READ_2(l, VR_MIIDATA);
}

static void
nvt_mii_write(struct local *l, int phy, int reg, int data)
{
	int v;

	CSR_WRITE_2(l, VR_MIIDATA, data);
	CSR_WRITE_1(l, VR_MIICFG, phy);
	CSR_WRITE_1(l, VR_MIIADR, reg);
	CSR_WRITE_1(l, VR_MIICR, MIICR_WCMD);
	do {
		v = CSR_READ_1(l, VR_MIICR);
	} while (v & MIICR_WCMD);
}

#define MII_BMCR	0x00	/* Basic mode control register (rw) */
#define	 BMCR_RESET	0x8000	/* reset */
#define	 BMCR_AUTOEN	0x1000	/* autonegotiation enable */
#define	 BMCR_ISO	0x0400	/* isolate */
#define	 BMCR_STARTNEG	0x0200	/* restart autonegotiation */
#define MII_BMSR	0x01	/* Basic mode status register (ro) */

static void
mii_initphy(struct local *l)
{
	int phy, ctl, sts, bound;

	l->bmsr = nvt_mii_read(l, l->phy, MII_BMSR);
	return; /* XXX */

	for (phy = 0; phy < 32; phy++) {
		ctl = nvt_mii_read(l, phy, MII_BMCR);
		sts = nvt_mii_read(l, phy, MII_BMSR);
		if (ctl != 0xffff && sts != 0xffff)
			goto found;
	}
	printf("MII: no PHY found\n");
	return;
  found:
	ctl = nvt_mii_read(l, phy, MII_BMCR);
	nvt_mii_write(l, phy, MII_BMCR, ctl | BMCR_RESET);
	bound = 100;
	do {
		DELAY(10);
		ctl = nvt_mii_read(l, phy, MII_BMCR);
		if (ctl == 0xffff) {
			printf("MII: PHY %d has died after reset\n", phy);
			return;
		}
	} while (bound-- > 0 && (ctl & BMCR_RESET));
	if (bound == 0) {
		printf("PHY %d reset failed\n", phy);
	}
	ctl &= ~BMCR_ISO;
	nvt_mii_write(l, phy, MII_BMCR, ctl);
	sts = nvt_mii_read(l, phy, MII_BMSR) |
	    nvt_mii_read(l, phy, MII_BMSR); /* read twice */
	l->phy = phy;
	l->bmsr = sts;
}
