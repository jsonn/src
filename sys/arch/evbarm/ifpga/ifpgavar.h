/*	$NetBSD: ifpgavar.h,v 1.1.2.2 2002/10/18 02:36:28 nathanw Exp $ */

/*
 * Copyright (c) 2001 ARM Ltd
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
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

#include <machine/bus.h>

#include "locators.h"

/* We statically map the UARTS at boot so that we can access the console
   before we've probed for the IFPGA. */
#define UART0_BOOT_BASE		0xfde00000
#define UART1_BOOT_BASE		0xfdf00000

#define IFPGA_UART0		0x06000000	/* Uart 0 */
#define IFPGA_UART1		0x07000000	/* Uart 1 */

typedef paddr_t ifpga_addr_t;

struct ifpga_softc {
	struct device		sc_dev;		/* Device node */
	bus_space_tag_t		sc_iot;		/* Bus tag */
	bus_space_handle_t	sc_sc_ioh;	/* System Controller handle */
	bus_space_handle_t	sc_cm_ioh;	/* Core Module handle */
	bus_space_handle_t	sc_tmr_ioh;	/* Timers handle */
	bus_space_handle_t	sc_irq_ioh;	/* IRQ controller handle */

	/* Clock variables.  */
	int			sc_statclock_count;
	int			sc_clock_count;
	int			sc_clock_ticks_per_256us;
	void *			sc_clockintr;
	void *			sc_statclockintr;
};

#define cf_iobase			cf_loc[IFPGACF_OFFSET]
#define cf_irq				cf_loc[IFPGACF_IRQ]

#define IRQUNK		IFPGACF_IRQ_DEFAULT

struct ifpga_attach_args {
	char *ifa_name;			/* Device name */
	bus_space_tag_t    ifa_iot;	/* Bus space tag for io */
	bus_space_handle_t ifa_sc_ioh;	/* System controller handle */

	ifpga_addr_t	   ifa_addr;	/* Address of device.  */
	int		   ifa_irq;	/* IRQ to use.  */
	/*
	 * Other data extracted from the system should go here.  Eg UART clock
	 * rates. 
	 */
};

void ifpga_create_io_bs_tag(struct bus_space *, void *);
void ifpga_create_mem_bs_tag(struct bus_space *, void *);

void ifpga_reset(void);
