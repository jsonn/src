/*	$NetBSD: pciconf.h,v 1.7.16.2 2005/03/26 18:19:19 yamt Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * args: pci_chipset_tag_t, io_extent, mem_extent, pmem_extent
 * where pmem_extent is "pre-fetchable" memory -- if NULL, mem_extent will
 * be used for both
 */
int	pci_configure_bus(pci_chipset_tag_t, struct extent *,
	    struct extent *, struct extent *, int, int);

/* Defined in machdep code.  Returns the interrupt line to set */
/* args: chipset_tag, bus, dev, ipin, ptr to interrupt line */
#ifndef pci_conf_interrupt
void	pci_conf_interrupt(pci_chipset_tag_t, int, int, int, int, int *);
#endif

#define PCI_CONF_MAP_IO		0x01
#define PCI_CONF_MAP_MEM	0x02
#define PCI_CONF_MAP_ROM	0x04
#define PCI_CONF_ENABLE_IO	0x08
#define PCI_CONF_ENABLE_MEM	0x10
#define PCI_CONF_ENABLE_BM	0x20
#define PCI_CONF_ENABLE_PARITY	0x40
#define PCI_CONF_ENABLE_SERR	0x80

#define PCI_CONF_ALL		0xff
