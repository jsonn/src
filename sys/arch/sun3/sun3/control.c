/*	$NetBSD: control.c,v 1.15.8.1 1997/10/14 10:19:12 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/param.h>
#include <sys/types.h>

#include <machine/pte.h>
#include <machine/control.h>

#define CONTROL_ADDR_BUILD(as, va) \
	((as) | ((va) & CONTROL_ADDR_MASK))

int
get_context()
{
	return (get_control_byte(CONTEXT_REG) & CONTEXT_MASK);
}

void
set_context(c)
	int c;
{
	set_control_byte(CONTEXT_REG, (c & CONTEXT_MASK));
}

int
get_pte(va)
	vm_offset_t va;
{
	return (get_control_word(CONTROL_ADDR_BUILD(PGMAP_BASE, va)));
}

void set_pte(va, pte)
	vm_offset_t va;
	int pte;
{
	set_control_word(CONTROL_ADDR_BUILD(PGMAP_BASE, va), pte);
}

int
get_segmap(va)
	vm_offset_t va;
{
	return (get_control_byte(CONTROL_ADDR_BUILD(SEGMAP_BASE, va)));
}

void set_segmap(va, sme)
	vm_offset_t va;
	int sme;
{
	set_control_byte(CONTROL_ADDR_BUILD(SEGMAP_BASE, va), sme);
}

/*
 * Set a segmap entry in all contexts.
 * (i.e. somewhere in kernel space.)
 * XXX - Should optimize:  "(get|set)_control_(word|byte)"
 * calls so this does save/restore of sfc/dfc only once!
 */
void
set_segmap_allctx(va, sme)
	vm_offset_t va;
	int sme;	/* segmap entry */
{
	vm_offset_t ctrladdr;
	register int ctx, oldctx;

	/* Inline get_context() */
	oldctx = get_control_byte(CONTEXT_REG);
	oldctx &= CONTEXT_MASK;

	ctrladdr = CONTROL_ADDR_BUILD(SEGMAP_BASE, va);
	for (ctx = 0; ctx < NCONTEXT; ctx++) {
		/* Inlined set_context() */
		set_control_byte(CONTEXT_REG, ctx);
		/* Inlined set_segmap() */
		set_control_byte(ctrladdr, sme);
	}

	/* Inlined set_context(ctx); */
	set_control_byte(CONTEXT_REG, oldctx);
}
