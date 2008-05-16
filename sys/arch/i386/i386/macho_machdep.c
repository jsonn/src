/*	$NetBSD: macho_machdep.c,v 1.4.78.1 2008/05/16 02:22:34 yamt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__KERNEL_RCSID(0, "$NetBSD: macho_machdep.c,v 1.4.78.1 2008/05/16 02:22:34 yamt Exp $");

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/exec_macho.h>

uint32_t exec_macho_supported_cpu[] = { MACHO_CPU_TYPE_I386, 0 };

#ifdef DEBUG_MACHO
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#ifdef DEBUG_MACHO
static void
exec_macho_print_i386_thread_state(struct exec_macho_i386_thread_state *ts) {
	printf("ts.eax 0x%x\n", ts->eax);
	printf("ts.ebx 0x%x\n", ts->ebx);
	printf("ts.ecx 0x%x\n", ts->ecx);
	printf("ts.edx 0x%x\n", ts->edx);
	printf("ts.edi 0x%x\n", ts->edi);
	printf("ts.esi 0x%x\n", ts->esi);
	printf("ts.ebp 0x%x\n", ts->ebp);
	printf("ts.esp 0x%x\n", ts->esp);
	printf("ts.esp 0x%x\n", ts->esp);
	printf("ts.ss 0x%x\n", ts->ss);
	printf("ts.eflags 0x%x\n", ts->eflags);
	printf("ts.eip 0x%x\n", ts->eip);
	printf("ts.cs 0x%x\n", ts->cs);
	printf("ts.ds 0x%x\n", ts->ds);
	printf("ts.es 0x%x\n", ts->es);
	printf("ts.fs 0x%x\n", ts->fs);
	printf("ts.gs 0x%x\n", ts->gs);
}

static void
exec_macho_print_i386_saved_state(struct exec_macho_i386_saved_state *ts) {
	int i;

	printf("ts.gs 0x%x\n", ts->gs);
	printf("ts.fs 0x%x\n", ts->fs);
	printf("ts.es 0x%x\n", ts->es);
	printf("ts.ds 0x%x\n", ts->ds);
	printf("ts.edi 0x%x\n", ts->edi);
	printf("ts.esi 0x%x\n", ts->esi);
	printf("ts.ebp 0x%x\n", ts->ebp);
	printf("ts.esp 0x%x\n", ts->esp);
	printf("ts.ebx 0x%x\n", ts->ebx);
	printf("ts.edx 0x%x\n", ts->edx);
	printf("ts.ecx 0x%x\n", ts->ecx);
	printf("ts.eax 0x%x\n", ts->eax);
	printf("ts.trapno 0x%x\n", ts->trapno);
	printf("ts.err 0x%x\n", ts->err);
	printf("ts.eip 0x%x\n", ts->eip);
	printf("ts.cs 0x%x\n", ts->cs);
	printf("ts.efl 0x%x\n", ts->efl);
	printf("ts.uesp 0x%x\n", ts->uesp);
	printf("ts.ss 0x%x\n", ts->ss);
	printf("ts.vm86_segs.es 0x%x\n", ts->vm86_segs.es);
	printf("ts.vm86_segs.ds 0x%x\n", ts->vm86_segs.ds);
	printf("ts.vm86_segs.fs 0x%x\n", ts->vm86_segs.fs);
	printf("ts.vm86_segs.gs 0x%x\n", ts->vm86_segs.gs);
	printf("ts.argv_status 0x%x\n", ts->argv_status);
	for (i = 0; i < MACHO_I386_SAVED_ARGV_COUNT; i++)
		printf("ts.argv[%d] 0x%x\n", i, ts->argv[i]);
}
#endif

u_long
exec_macho_thread_entry(struct exec_macho_thread_command *tc) {
	switch (tc->flavor) {
	case MACHO_I386_THREAD_STATE: {
		struct exec_macho_i386_thread_state *ts =
		    (struct exec_macho_i386_thread_state *)
		    (void *)((char *)tc + sizeof(*tc));
#ifdef DEBUG_MACHO
		exec_macho_print_i386_thread_state(ts);
#endif
		return ts->eip;
	}
	case MACHO_I386_NEW_THREAD_STATE: {
		struct exec_macho_i386_saved_state *ts =
		    (struct exec_macho_i386_saved_state *)
		    (void *)((char *)tc + sizeof(*tc));
#ifdef DEBUG_MACHO
		exec_macho_print_i386_saved_state(ts);
#endif
		return ts->eip;
	}
	default:
		DPRINTF(("Unknown thread flavor %ld\n", tc->flavor));
		return 0;
	}
}
