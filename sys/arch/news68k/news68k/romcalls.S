/*	$NetBSD: romcalls.S,v 1.1 2011/11/20 15:38:00 tsutsui Exp $	*/

/*-
 * Copyright (c) 1999 Izumi Tsutsui.  All rights reserved.
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

#include <m68k/asm.h>
#include <machine/romcall.h>

	.text
	.align	4

#define SYSCALL	trap	#14	/* XXX */

#if 0
ENTRY_NOPROFILE(rom_halt)
	moveq	#8, %d7		| RB_HALT
	trap	#15
#endif

#if 0
ENTRY_NOPROFILE(rom_open)
	linkw	%a6, #0
	moveml	%d2-%d7/%a2-%a5,%sp@-	| save %d2-%d7, %a2-%a5
	moveq	#SYS_open, %d0
	SYSCALL
	moveml	%a6@(-40),%d2-%d7/%a2-%a5
	unlk	%a6
	rts

ENTRY_NOPROFILE(rom_close)
	linkw	%a6, #0
	moveml	%d2-%d7/%a2-%a5,%sp@-	| save %d2-%d7, %a2-%a5
	moveq	#SYS_close, %d0
	SYSCALL
	moveml	%a6@(-40),%d2-%d7/%a2-%a5
	unlk	%a6
	rts
#endif

ENTRY_NOPROFILE(rom_read)
	linkw	%a6, #0
	moveml	%d2-%d7/%a2-%a5,%sp@-	| save %d2-%d7, %a2-%a5
	moveq	#SYS_read, %d0
	SYSCALL
	moveml	%a6@(-40),%d2-%d7/%a2-%a5
	unlk	%a6
	rts

ENTRY_NOPROFILE(rom_write)
	linkw	%a6, #0
	moveml	%d2-%d7/%a2-%a5,%sp@-	| save %d2-%d7, %a2-%a5
	moveq	#SYS_write, %d0
	SYSCALL
	moveml	%a6@(-40),%d2-%d7/%a2-%a5
	unlk	%a6
	rts

ENTRY_NOPROFILE(rom_lseek)
	linkw	%a6, #0
	moveml	%d2-%d7/%a0-%a5,%sp@-	| save %d2-%d7, %a2-%a5
	moveq	#SYS_lseek, %d0
	SYSCALL
	moveml	%a6@(-40),%d0-%d7/%a2-%a5
	unlk	%a6
	rts
