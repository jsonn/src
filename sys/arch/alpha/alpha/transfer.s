/*	$NetBSD: transfer.s,v 1.1.2.2 1997/11/10 21:57:01 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 Avalon Computer Systems, Inc.
 * All rights reserved.
 *
 * Author: Ross Harvey
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Avalon Computer Systems, Inc. nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. This copyright will be assigned to The NetBSD Foundation on
 *    1/1/1999 unless these terms (including possibly the assignment
 *    date) are updated by Avalon prior to the latest specified assignment
 *    date.
 *    
 *
 * THIS SOFTWARE IS PROVIDED BY AVALON COMPUTER SYSTEMS, INC. AND CONTRIBUTORS
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

#include <machine/asm.h>

__KERNEL_RCSID(0, "$NetBSD: transfer.s,v 1.1.2.2 1997/11/10 21:57:01 thorpej Exp $")

/*
 * New transfer point for NetBSD Alpha kernel.  This could be written in
 * C, actually. It's a bit safer from things like mcount this way. This file
 * exists because having the entry point in locore confuses gdb.
 */

.globl	__transfer
.globl	locorestart
.ent	__transfer 0
__transfer:
	br	pv,Lt1		/* paranoia, we transfer here from C code! */
Lt1:	ldgp	gp,0(pv)
	lda	pv,locorestart
	jmp	zero,(pv)
.end	__transfer
/*
 * Temporary trap to divert upgrading sites into config.
 */
.globl	U_need_2_run_config
.ent	U_need_2_run_config 0
	U_need_2_run_config:
.end	U_need_2_run_config
