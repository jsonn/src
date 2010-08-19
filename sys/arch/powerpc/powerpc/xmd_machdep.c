/*	$NetBSD: xmd_machdep.c,v 1.1.2.1 2010/08/19 12:36:58 uebayasi Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#include "opt_xip.h"

#include <sys/param.h>
#include <sys/mman.h>

#include <uvm/uvm_page.h>

#include <dev/xmdvar.h>

paddr_t
xmd_machdep_mmap(vaddr_t addr, off_t off, int prot)
{

	/* addr is in PA == VA RAM area. */
	KASSERT(addr >= 0x00000000);
	KASSERT(addr < 0x80000000);

	return addr;
}

void *
xmd_machdep_physload(vaddr_t addr, size_t size)
{
	paddr_t s, e, as, ae;

	/* addr is in PA == VA RAM area. */
	KASSERT(addr >= 0x00000000);
	KASSERT(addr < 0x80000000);

	s = as = (addr) >> PAGE_SHIFT;
	e = ae = (addr + size) >> PAGE_SHIFT;

	return uvm_page_physload_device(s, e, as, ae, PROT_READ, 0);
}

void
xmd_machdep_physunload(void *phys)
{

	uvm_page_physunload_device(phys);
}
