/*	$NetBSD: algor_p6032_bus_io.c,v 1.3.74.1 2008/06/02 13:21:44 mjf Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Platform-specific PCI bus I/O support for the Algorithmics P-6032.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: algor_p6032_bus_io.c,v 1.3.74.1 2008/06/02 13:21:44 mjf Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/locore.h>

#include <algor/algor/algor_p6032reg.h>
#include <algor/algor/algor_p6032var.h>

#define	CHIP		algor_p6032

#define	CHIP_EX_MALLOC_SAFE(v)	(((struct p6032_config *)(v))->ac_mallocsafe)
#define	CHIP_IO_EXTENT(v)	(((struct p6032_config *)(v))->ac_io_ex)

/* IO region 1 */
#define	CHIP_IO_W1_BUS_START(v)	0x00000000UL
#define	CHIP_IO_W1_BUS_END(v)	0x000fffffUL
#define	CHIP_IO_W1_SYS_START(v)	((u_long)BONITO_PCIIO_BASE)
#define	CHIP_IO_W1_SYS_END(v)	((u_long)BONITO_PCIIO_BASE + 0x000fffffUL)

#include <algor/pci/pci_alignstride_bus_io_chipdep.c>
