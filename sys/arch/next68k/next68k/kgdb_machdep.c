/*	$NetBSD: kgdb_machdep.c,v 1.1.16.1 2002/06/20 03:40:23 nathanw Exp $	*/

/*
 * This file was taken from sun3/sun3/kgdb_machdep.c
 * should probably be re-synced when needed.
 * Darrin B. Jewell <dbj@netbsd.org> 1999-12-14T06:05:22-0500
 * original cvs id:
 *	NetBSD: kgdb_machdep.c,v 1.4 1999/11/22 19:05:32 jdolecek Exp 
 * This file was updated with changes from mac68k (rev. 1.4)
 * and alpha (rev. 1.12) on 2002-05-14
 */

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kgdb_stub.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Machine-dependent part of the KGDB remote "stub"
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kgdb.h>

#define INVOKE_KGDB()   __asm __volatile("trap  #15")

/*
 * Determine if the memory at va..(va+len) is valid.
 */
int
kgdb_acc(va, ulen)
	vm_offset_t va;
	size_t ulen;
{

	/* Just let the trap handler deal with it. */
	return (1);
}

/*
 * Trap into kgdb to wait for debugger to connect,
 * noting on the console why nothing else is going on.
 */
void
kgdb_connect(verbose)
	int verbose;
{

	if (kgdb_dev < 0)
		return;

	if (verbose)
		printf("kgdb waiting...");

	INVOKE_KGDB();

	if (verbose)
		printf("connected.\n");

	kgdb_debug_panic = 1;
}

/*
 * Decide what to do on panic.
 */
void
kgdb_panic()
{
	if (kgdb_dev >= 0 && kgdb_debug_panic) {
		printf("entering kgdb\n");
		kgdb_connect(kgdb_active == 0);
	}
}
