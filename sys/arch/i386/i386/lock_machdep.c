/* $NetBSD: lock_machdep.c,v 1.1.2.1 2000/02/20 17:06:36 sommerfeld Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: lock_machdep.c,v 1.1.2.1 2000/02/20 17:06:36 sommerfeld Exp $");

/*
 * Machine-dependent spin lock operations.
 */

#include "opt_ddb.h"

#include <sys/types.h>
#include <sys/lock.h>
#include <sys/systm.h>

#include <machine/atomic.h>

#include <ddb/db_output.h>

void
cpu_simple_lock_init(alp)
	__volatile struct simplelock *alp;
{
	alp->lock_data = SIMPLELOCK_UNLOCKED;
}

#if defined (DEBUG) && defined(DDB)
int spin_limit = 10000000;
#endif

void
cpu_simple_lock(alp)
	__volatile struct simplelock *alp;
{
#if defined (DEBUG) && defined(DDB)	
	int spincount = 0;
#endif
	
	while (i386_atomic_testset_i(&alp->lock_data, SIMPLELOCK_LOCKED)
	    == SIMPLELOCK_LOCKED) {
#if defined(DEBUG) && defined(DDB)
		spincount++;
		if (spincount == spin_limit) {
			extern int db_active;
			db_printf("spundry\n");
			if (db_active) {
				db_printf("but already in debugger\n");
			} else {
				Debugger();
			}
		}
#endif
	}
}

int
cpu_simple_lock_try(alp)
	__volatile struct simplelock *alp;
{

	if (i386_atomic_testset_i(&alp->lock_data, SIMPLELOCK_LOCKED)
	    == SIMPLELOCK_UNLOCKED)
		return (1);
	return (0);
}

void
cpu_simple_unlock(alp)
	__volatile struct simplelock *alp;
{
	alp->lock_data = SIMPLELOCK_UNLOCKED;
}
