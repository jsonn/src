/*	$NetBSD: auxiovar.h,v 1.4.6.2 2000/11/20 20:26:42 bouyer Exp $	*/

/*
 * Copyright (c) 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * on sun4u, auxio exists with one register (LED) on the sbus, and 5
 * registers on the ebus2 (pci) (LED, PCIMODE, FREQUENCY, SCSI
 * OSCILLATOR, and TEMP SENSE.
 *
 * clients of the auxio registers (eg, blinken lights, or the sbus
 * floppy) should search in auxio_cd for their matching auxio register
 * (to deal with multiple auxio's that may appear.)
 */

struct auxio_registers {
#if 0	/* these do not exist on the Ebus2 */
	volatile u_int32_t *auxio_fd;
	volatile u_int32_t *auxio_audio;
	volatile u_int32_t *auxio_power;
#endif
	volatile u_int32_t *auxio_led;
	volatile u_int32_t *auxio_pci;
	volatile u_int32_t *auxio_freq;
	volatile u_int32_t *auxio_scsi;
	volatile u_int32_t *auxio_temp;
};

struct auxio_softc {
	struct device		sc_dev;
	struct auxio_registers	sc_registers;
	int			sc_flags;
#define	AUXIO_LEDONLY		0x1
#define	AUXIO_EBUS		0x2
#define	AUXIO_SBUS		0x4
};

/*
 * XXX: old interfaces.  we set auxio_reg the first auxio we attach.
 */
#ifndef _LOCORE
/*
 * Copy of AUXIO_REG for the benefit of assembler modules (eg. trap handlers)
 * as AUXREG_VA depends on NBPG which is not a constant.
 */
volatile u_char *auxio_reg;
unsigned int auxregbisc __P((int, int));
#endif
