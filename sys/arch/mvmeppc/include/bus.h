/*	$NetBSD: bus.h,v 1.1.14.2 2002/06/23 17:38:27 jdolecek Exp $	*/

/*-
 * Copyright (c) 200e The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#ifndef _MVMEPPC_BUS_H_
#define _MVMEPPC_BUS_H_

#include <powerpc/bus.h>


#define	MVMEPPC_PHYS_BASE_IO		0x80000000
#define	MVMEPPC_PHYS_SIZE_IO		0x3f800000
#define	MVMEPPC_PHYS_RESVD_START_IO	0x00010000
#define	MVMEPPC_PHYS_RESVD_SIZE_IO	0x00800000
#define MVMEPPC_KVA_BASE_IO		0x80000000
#define MVMEPPC_KVA_SIZE_IO		0x10000000

#define	MVMEPPC_PHYS_BASE_MEM		0xc0000000
#define	MVMEPPC_PHYS_SIZE_MEM		0x3f000000
#define	MVMEPPC_KVA_BASE_MEM		0xc0000000
#define	MVMEPPC_KVA_SIZE_MEM		0x10000000

#define	MVMEPPC_BUS_SPACE_IO		0
#define	MVMEPPC_BUS_SPACE_MEM		1
#define	MVMEPPC_BUS_SPACE_NUM_REGIONS	2

#define PHYS_TO_PCI_MEM(x)	((x) | 0x80000000)
#define PCI_MEM_TO_PHYS(x)	((x) & ~0x80000000)

#ifdef _KERNEL
extern void mvmeppc_bus_space_init(void);
extern const struct powerpc_bus_space mvmeppc_isa_io_bs_tag;
extern const struct powerpc_bus_space mvmeppc_isa_mem_bs_tag;
extern const struct powerpc_bus_space mvmeppc_pci_io_bs_tag;
extern const struct powerpc_bus_space mvmeppc_pci_mem_bs_tag;
#endif

#endif /* _MVMEPPC_BUS_H_ */
