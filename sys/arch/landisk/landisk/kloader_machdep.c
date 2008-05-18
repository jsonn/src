/*	$NetBSD: kloader_machdep.c,v 1.1.64.1 2008/05/18 12:32:19 yamt Exp $	*/

/*-
 * Copyright (c) 2001, 2002, 2004 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kloader_machdep.c,v 1.1.64.1 2008/05/18 12:32:19 yamt Exp $");

#include "debug_kloader.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sh3/mmu.h>
#include <sh3/mmu_sh4.h>
#include <sh3/cache.h>
#include <sh3/cache_sh4.h>

#include <machine/cpu.h>
#include <machine/kloader.h>

kloader_jumpfunc_t kloader_landisk_jump;
kloader_bootfunc_t kloader_landisk_boot;

struct kloader_ops landisk_kloader_ops = {
	.jump = kloader_landisk_jump,
	.boot = kloader_landisk_boot,
	.reset = machine_reset,
};

void
kloader_reboot_setup(const char *filename)
{

	__kloader_reboot_setup(&landisk_kloader_ops, filename);
}

void
kloader_landisk_jump(kloader_bootfunc_t func, vaddr_t sp,
    struct kloader_bootinfo *info, struct kloader_page_tag *tag)
{

	sh_icache_sync_all();	/* also flush d-cache */

	__asm volatile(
	    	"mov	%0, r4;"
		"mov	%1, r5;"
		"jmp	@%2;"
		" mov	%3, sp"
		: : "r" (info), "r" (tag), "r" (func), "r" (sp));
	/* NOTREACHED */
}

/* 
 * 2nd-bootloader. Make sure that PIC and its size is lower than page size.
 */
void
kloader_landisk_boot(struct kloader_bootinfo *kbi, struct kloader_page_tag *p)
{
	int howto;
	int tmp = 0;

	/* Disable interrupt. block exception. */
	__asm volatile(
		"stc	sr, %1;"
		"or	%0, %1;"
		"ldc	%1, sr" : : "r" (0x500000f0), "r" (tmp));

	/* Now I run on P1, TLB flush. and disable. */
	SH4_TLB_DISABLE;

	do {
		uint32_t *dst =(uint32_t *)p->dst;
		uint32_t *src =(uint32_t *)p->src;
		uint32_t sz = p->sz / sizeof(int);
		while (sz--) {
			*dst++ = *src++;
		}
	} while ((p = (struct kloader_page_tag *)p->next) != 0);

	/* XXX: howto */
	howto = 0;

	SH7751R_CACHE_FLUSH();

	/* jump to kernel entry. */
	__asm volatile(
		"mov	%0, r4;"
		"jmp	@%2;"
		" mov	%1, r5;"
		: : "r" (howto), "r" (&kbi->bootinfo), "r" (kbi->entry));
	/* NOTREACHED */
}
