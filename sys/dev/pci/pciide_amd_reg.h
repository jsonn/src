/*	$NetBSD: pciide_amd_reg.h,v 1.2.6.1 2001/06/21 20:05:10 nathanw Exp $	*/

/*
 * Copyright (c) 2000 David Sainty.
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
 */

/*
 * Registers definitions for AMD 7x6 PCI IDE controller.  Documentation
 * available at:
 * 	http://www.amd.com/products/cpg/athlon/techdocs/pdf/22548.pdf (756)
 *	http://www.amd.com/products/cpg/athlon/techdocs/pdf/23167.pdf (766)
 */

/* Chip revisions */
#define AMD756_CHIPREV_D2 3

/* Chip revision tests */

/*
 * The AMD756 chip revision D2 has a bug affecting DMA (but not UDMA)
 * modes.  The workaround documented by AMD is to not use DMA on any 
 * drive which does not support UDMA modes.
 * 
 * See: http://www.amd.com/products/cpg/athlon/techdocs/pdf/22591.pdf
 */
#define AMD756_CHIPREV_DISABLEDMA(rev) ((rev) <= AMD756_CHIPREV_D2)


/* Channel enable */
#define AMD7X6_CHANSTATUS_EN		0x40
#define AMD7X6_CHAN_EN(chan)		(0x01 << (1 - (chan)))

/* Data port timing controls */
#define AMD7X6_DATATIM 0x48
#define AMD7X6_DATATIM_MASK(channel) (0xffff << ((1 - (channel)) << 4))
#define AMD7X6_DATATIM_RECOV(channel, drive, x) (((x) & 0xf) << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))
#define AMD7X6_DATATIM_PULSE(channel, drive, x) (((x) & 0xf) << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3) + 4))

static const int8_t amd7x6_pio_set[] = {0x0a, 0x0a, 0x0a, 0x02, 0x02};
static const int8_t amd7x6_pio_rec[] = {0x08, 0x08, 0x08, 0x02, 0x00};

/* Ultra-DMA/33 control */
#define AMD7X6_UDMA 0x50
#define AMD7X6_UDMA_MASK(channel) (0xffff << ((1 - (channel)) << 4))
#define AMD7X6_UDMA_TIME(channel, drive, x) (((x) & 0x7) << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))
#define AMD7X6_UDMA_EN(channel, drive) (0x40 << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))
#define AMD7X6_UDMA_EN_MTH(channel, drive) (0x80 << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))

static const int8_t amd7x6_udma_tim[] = {0x02, 0x01, 0x00, 0x04, 0x05, 0x06};
