/* $NetBSD: maplevar.h,v 1.3.2.2 2001/02/03 23:25:52 marcus Exp $ */
/*-
 * Copyright (c) 2001 Marcus Comstedt
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
 *	This product includes software developed by Marcus Comstedt.
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

#include <machine/bus.h>

struct maple_softc {
	struct device sc_dev;

	struct callout maple_callout_ch;
	int maple_commands_pending;

	int sc_port_units[MAPLE_PORTS];

	struct maple_unit sc_unit[MAPLE_PORTS][MAPLE_SUBUNITS];

	u_int32_t *sc_txbuf;	/* start of allocated transmit buffer */
	u_int32_t *sc_txpos;	/* current write position in tx buffer */
	u_int32_t *sc_txlink;   /* start of last written frame */

	/* start of each receive buffer */
	u_int32_t *sc_rxbuf[MAPLE_PORTS][MAPLE_SUBUNITS];

	u_int32_t sc_txbuf_phys;	/* 29-bit physical address */
  	u_int32_t sc_rxbuf_phys[MAPLE_PORTS][MAPLE_SUBUNITS];
};

