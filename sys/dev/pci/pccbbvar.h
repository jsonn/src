/*	$NetBSD: pccbbvar.h,v 1.4.4.1 1999/12/27 18:35:21 wrstuden Exp $	*/
/*
 * Copyright (c) 1999 HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
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


/* require sys/device.h */
/* require sys/queue.h */
/* require dev/ic/i82365reg.h */
/* require dev/ic/i82365var.h */

#ifndef _DEV_PCI_PCCBBVAR_H_
#define	_DEV_PCI_PCCBBVAR_H_


#define	PCIC_FLAG_SOCKETP	0x0001
#define	PCIC_FLAG_CARDP		0x0002



/* Chipset ID */
#define CB_UNKNOWN  0		/* NOT Cardbus-PCI bridge */
#define CB_TI113X   1		/* TI PCI1130/1131 */
#define CB_TI12XX   2		/* TI PCI1250/1220 */
#define CB_RX5C47X  3		/* RICOH RX5C475/476/477 */
#define CB_RX5C46X  4		/* RICOH RX5C465/466/467 */
#define CB_TOPIC95  5		/* Toshiba ToPIC95 */
#define CB_TOPIC95B 6		/* Toshiba ToPIC95B */
#define CB_TOPIC97  7		/* Toshiba ToPIC97 */
#define CB_CIRRUS   8		/* Cirrus Logic CL-PD683X */
#define CB_CHIPS_LAST  9	/* Sentinel */

#if 0
static char *cb_chipset_name[CB_CHIPS_LAST] = {
  "unknown", "TI 113X", "TI 12XX", "RF5C47X", "RF5C46X", "ToPIC95",
  "ToPIC95B", "ToPIC97", "CL-PD 683X",
};
#endif

struct pccbb_softc;

#if pccard
struct cbb_pcmcia_softc {
  pccard_chipset_t cpc_ct;
  struct pccard_softc *cpc_csc;
  struct pccbb_softc *cpc_parent;
  u_int8_t cpc_statreg;		/* status register */
  u_int32_t cpc_regbase;	/* base index of the slot */
  u_int16_t cpc_flags;
  bus_space_tag_t cpc_iot;
  bus_space_handle_t cpc_ioh;
};
#endif /* pccard */

struct cbb_pcic_handle {
  struct device *ph_parent;
  bus_space_tag_t ph_base_t;
  bus_space_handle_t ph_base_h;
  u_int8_t (* ph_read) __P((struct cbb_pcic_handle *, int));
  void (* ph_write) __P((struct cbb_pcic_handle *, int, u_int8_t));
  int	sock;

  int	vendor;
  int	flags;
  int	memalloc;
  struct {
    bus_addr_t	addr;
    bus_size_t	size;
    long	offset;
    int		kind;
  } mem[PCIC_MEM_WINS];
  int	ioalloc;
  struct {
    bus_addr_t	addr;
    bus_size_t	size;
    int		width;
  } io[PCIC_IO_WINS];
  int	ih_irq;
  struct device *pcmcia;

  int shutdown;
};


struct pccbb_win_chain {
  bus_addr_t wc_start;		/* Caution: region [start, end], */
  bus_addr_t wc_end;		/* instead of [start, end). */
  int wc_flags;
#define PCCBB_MEM_CACHABLE 1
  bus_space_handle_t wc_handle;
  struct pccbb_win_chain *wc_next;
};


struct pccbb_softc {
  struct device sc_dev;
  bus_space_tag_t sc_iot;
  bus_space_tag_t sc_memt;
  bus_dma_tag_t sc_dmat;

#if rbus
  rbus_tag_t sc_rbus_iot;	/* rbus for i/o donated from parent */
  rbus_tag_t sc_rbus_memt;	/* rbus for mem donated from parent */
#endif

  bus_space_tag_t sc_base_memt;
  bus_space_handle_t sc_base_memh;

  void *sc_ih;			/* interrupt handler */
  int sc_intrline;		/* interrupt line */
  pcitag_t sc_intrtag;		/* copy of pa->pa_intrtag */
  pci_intr_pin_t sc_intrpin;	/* copy of pa->pa_intrpin */
  int sc_function;
  u_int32_t sc_flags;
#define CBB_CARDEXIST 0x01
#define CBB_INSERTING 0x01000000
#define CBB_16BITCARD 0x04
#define CBB_32BITCARD 0x08

#if pccard
  struct cbb_pcmcia_softc sc_pcmcia;
#endif /* pccard */
  pci_chipset_tag_t sc_pc;
  pcitag_t sc_tag;
  int sc_chipset;		/* chipset id */

  bus_addr_t sc_mem_start;	/* CardBus/PCMCIA memory start */
  bus_addr_t sc_mem_end;	/* CardBus/PCMCIA memory end */
  bus_addr_t sc_io_start;	/* CardBus/PCMCIA io start */
  bus_addr_t sc_io_end;		/* CardBus/PCMCIA io end */

  /* CardBus stuff */
  struct cardslot_softc *sc_csc;

  struct pccbb_win_chain *sc_memwindow;
  struct pccbb_win_chain *sc_iowindow;

  /* pcmcia stuff */
  struct pcic_handle sc_pcmcia_h;
  pcmcia_chipset_tag_t sc_pct;
  int sc_pcmcia_flags;
#define PCCBB_PCMCIA_IO_RELOC   0x01 /* IO address relocatable stuff exists */
#define PCCBB_PCMCIA_MEM_32     0x02 /* 32-bit memory address ready */
#define PCCBB_PCMCIA_16BITONLY  0x04 /* 32-bit mode disable */

  struct proc *sc_event_thread;
  SIMPLEQ_HEAD(, pcic_event) sc_events;
};

#endif /* _DEV_PCI_PCCBBREG_H_ */
