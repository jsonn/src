/*      $NetBSD: pciide_hpt_reg.h,v 1.2.2.3 2001/01/05 17:36:17 bouyer Exp $       */

/*
 * Copyright (c) 2000 Manuel Bouyer.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,     
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


/*
 * Register definitions for the Highpoint HPT366 UDMA/66 * and HPT370 UDMA/100
 * PCI IDE controller.  
 *
 * The HPT366 has 2 PCI IDE functions, each of them has only one channel.
 * The HPT370 has the 2 channels on the same PCI IDE function.
 */

/*
 * The HPT366 and HPT370 have the save vendor/device ID but not the
 * same revision
 */
#define HPT366_REV 0x01
#define HPT370_REV 0x03

#define HPT_IDETIM(chan, drive) (0x40 + ((drive) * 4) + ((chan) * 8))
#define HPT_IDETIM_BUFEN		0x80000000
#define HPT_IDETIM_MSTEN		0x40000000
#define HPT_IDETIM_DMAEN		0x20000000
#define HPT_IDETIM_UDMAEN		0x10000000

#define HPT366_CTRL1		0x50
#define HPT366_CTRL1_BLKDIS(chan)	(0x40 << (chan))
#define HPT366_CTRL1_CHANEN(chan)	(0x10 << (chan))
#define HPT366_CTRL1_CLRBUF(chan)	(0x04 << (chan))
#define HPT366_CTRL1_LEG(chan)		(0x01 << (chan))

#define HPT366_CTRL2		0x51
#define HPT366_CTRL2_FASTIRQ		0x80
#define HPT366_CTRL2_HOLDIRQ(chan)	(0x20 << (chan))
#define HPT366_CTRL2_SGEN		0x10
#define HPT366_CTRL2_CLEARFIFO(chan)	(0x04 << (chan))
#define HPT366_CTRL2_CLEARBMSM		0x02
#define HPT366_CTRL2_CLEARSG		0x01

#define HPT366_CTRL3(chan)	(0x52 + ((chan) * 4))
#define HPT366_CTRL3_PDMA		0x8000
#define HPT366_CTRL3_BP			0x4000
#define HPT366_CTRL3_FASTIRQ_OFFSET	9
#define HPT366_CTRL3_FASTIRQ_MASK	0x3

#define HPT370_CTRL1(chan)	(0x50 + ((chan) * 4))
#define HPT370_CTRL1_CLRSG		0x80
#define HPT370_CTRL1_READF		0x40
#define HPT370_CTRL1_CLRST		0x20
#define HPT370_CTRL1_CLRSGC		0x10
#define HPT370_CTRL1_BLKDIS		0x08
#define HPT370_CTRL1_EN			0x04
#define HPT370_CTRL1_CLRDBUF		0x02
#define HPT370_CTRL1_LEGEN		0x01

#define HPT370_CTRL2(chan)	(0x51 + ((chan) * 4))
#define HPT370_CTRL2_FASTIRQ		0x02
#define HPT370_CTRL2_HIRQ		0x01

#define HPT370_CTRL3(chan)	(0x52 + ((chan) * 4))
#define HPT370_CTRL3_HIZ		0x8000
#define HPT370_CTRL3_BP			0x4000
#define HPT370_CTRL3_FASTIRQ_OFFSET	9
#define HPT370_CTRL3_FASTIRQ_MASK	0x3

#define HPT_STAT1		0x58
#define HPT_STAT1_IRQPOLL(chan)		(0x40 << (chan)) /* 366 only */
#define HPT_STAT1_DMARQ(chan) 		(0x04 << ((chan) * 3))
#define HPT_STAT1_DMACK(chan) 		(0x02 << ((chan) * 3))
#define HPT_STAT1_IORDY(chan) 		(0x01 << ((chan) * 3))

#define HPT_STAT2		0x59
#define HPT_STAT2_FLT_RST		0x40 /* 366 only */
#define HPT_STAT2_RST(chan)		(0x40 << (chan))  /* 370 only */
#define HPT_STAT2_POLLEN(chan)		(0x04 << ((chan) * 3))
#define HPT_STAT2_IRQD1(chan)		(0x02 << ((chan) * 3))
#define HPT_STAT2_IRQD0_CH1		0x08
#define HPT_STAT2_POLLST		0x01

#define HPT_CSEL		0x5a
#define HPT_CSEL_IRQDIS			0x10 /* 370 only */
#define HPT_CSEL_PCIDIS			0x08 /* 370 only */
#define HPT_CSEL_PCIWR			0x04 /* 370 only */
#define HPT_CSEL_CBLID(chan)		 (0x01 << (1 - (chan)))

static u_int32_t hpt366_pio[] =
	{0x00d0a7aa, 0x00c8a753, 0x00c8a742, 0x00c8a731};
static u_int32_t hpt366_dma[] =
	{0x20c8a797, 0x20c8a742, 0x20c8a731};
static u_int32_t hpt366_udma[] =
	{0x10c8a731, 0x10cba731, 0x10caa731, 0x10cfa731, 0x10c9a731};

static u_int32_t hpt370_pio[] =
	{0x06914e8a, 0x06914e65, 0x06514e33, 0x06514e22, 0x06514e21};
static u_int32_t hpt370_dma[] =
	{0x26514e97, 0x26514e33, 0x26514e21};
static u_int32_t hpt370_udma[] = 
	{0x16514e31, 0x164d4e31, 0x16494e31, 0x166d4e31, 0x16454e31,
	 0x1a85f442};
