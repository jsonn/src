/*	$NetBSD: giovar.h,v 1.6.8.1 2006/09/03 15:23:31 yamt Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
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
 */

/*
 * GIO 32/32-bis/64 bus
 */

#include <machine/bus.h>

struct giobus_attach_args {
	bus_space_tag_t	gba_iot;
	bus_space_handle_t gba_ioh;

#define GIOBUS_GIO32	0
#define GIOBUS_GIO32BIS	1
#define GIOBUS_GIO64	2
	int		gba_type;
};

struct gio_attach_args {
	bus_space_tag_t	ga_iot;
	bus_space_handle_t ga_ioh;
	bus_dma_tag_t	ga_dmat;

	int		ga_slot; /* (?) */
	u_int32_t	ga_addr; /* (?) */

	u_int32_t	ga_product;
};


#define GIO_SLOT_GFX	0
#define GIO_SLOT_EXP0	1
#define GIO_SLOT_EXP1	2

#define GIO_ARB_RT	0x01	/* real-time device */
#define GIO_ARB_LB	0x02	/* long-burst device */

#define GIO_ARB_MST	0x04	/* bus master enable */
#define GIO_ARB_SLV	0x08	/* slave */

#define GIO_ARB_PIPE	0x10	/* pipelining enable */
#define GIO_ARB_NOPIPE	0x20	/* pipelining disable */

int		gio_cnattach(void);
int		gio_arb_config(int, uint32_t);
void	       *gio_intr_establish(int, int, int (*)(void *), void *);
const char     *gio_product_string(int);

