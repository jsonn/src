/*	$NetBSD: arm_machdep.c,v 1.15.26.1 2007/12/13 21:54:35 bouyer Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_compat_netbsd.h"
#include "opt_execfmt.h"
#include "opt_arm_debug.h"

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: arm_machdep.c,v 1.15.26.1 2007/12/13 21:54:35 bouyer Exp $");

#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/pool.h>
#include <sys/ucontext.h>
#include <sys/evcnt.h>
#include <sys/cpu.h>

#include <arm/cpufunc.h>

#include <machine/pcb.h>
#include <machine/vmparam.h>

/*
 * The ARM architecture places the vector page at address 0.
 * Later ARM architecture versions, however, allow it to be
 * relocated to a high address (0xffff0000).  This is primarily
 * to support the Fast Context Switch Extension.
 *
 * This variable contains the address of the vector page.  It
 * defaults to 0; it only needs to be initialized if we enable
 * relocated vectors.
 */
vaddr_t	vector_page;

#if defined(ARM_LOCK_CAS_DEBUG)
/*
 * Event counters for tracking activity of the RAS-based _lock_cas()
 * routine.
 */
struct evcnt _lock_cas_restart =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "_lock_cas", "restart");
EVCNT_ATTACH_STATIC(_lock_cas_restart);

struct evcnt _lock_cas_success =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "_lock_cas", "success");
EVCNT_ATTACH_STATIC(_lock_cas_success);

struct evcnt _lock_cas_fail =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "_lock_cas", "fail");
EVCNT_ATTACH_STATIC(_lock_cas_fail);
#endif /* ARM_LOCK_CAS_DEBUG */

/*
 * Clear registers on exec
 */

void
setregs(struct lwp *l, struct exec_package *pack, u_long stack)
{
	struct trapframe *tf;

	tf = l->l_addr->u_pcb.pcb_tf;

	memset(tf, 0, sizeof(*tf));
	tf->tf_r0 = (u_int)l->l_proc->p_psstr;
#ifdef COMPAT_13
	tf->tf_r12 = stack;			/* needed by pre 1.4 crt0.c */
#endif
	tf->tf_usr_sp = stack;
	tf->tf_usr_lr = pack->ep_entry;
	tf->tf_svc_lr = 0x77777777;		/* Something we can see */
	tf->tf_pc = pack->ep_entry;
#ifdef __PROG32
	tf->tf_spsr = PSR_USR32_MODE;
#ifdef THUMB_CODE
	if (pack->ep_entry & 1)
		tf->tf_spsr |= PSR_T_bit;
#endif
#endif

#ifdef EXEC_AOUT
	if (pack->ep_esch->es_makecmds == exec_aout_makecmds)
		l->l_addr->u_pcb.pcb_flags = PCB_NOALIGNFLT;
	else
#endif
	l->l_addr->u_pcb.pcb_flags = 0;
}

/*
 * startlwp:
 *
 *	Start a new LWP.
 */
void
startlwp(void *arg)
{
	int err;
	ucontext_t *uc = arg; 
	struct lwp *l = curlwp;

	err = cpu_setmcontext(l, &uc->uc_mcontext, uc->uc_flags);
#ifdef DIAGNOSTIC
	if (err)
		printf("Error %d from cpu_setmcontext.", err);
#endif
	pool_put(&lwp_uc_pool, uc);

	userret(l);
}

bool
cpu_intr_p(void)
{

	return curcpu()->ci_idepth != 0;
}
