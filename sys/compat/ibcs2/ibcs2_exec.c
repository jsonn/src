/*	$NetBSD: ibcs2_exec.c,v 1.23.2.3 2000/12/08 09:08:17 bouyer Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1998 Scott Bartram
 * Copyright (c) 1994 Adam Glass
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
 * All rights reserved.
 *
 * originally from kern/exec_ecoff.c
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
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/exec.h>

#include <machine/ibcs2_machdep.h>

#include <compat/ibcs2/ibcs2_types.h>
#include <compat/ibcs2/ibcs2_exec.h>
#include <compat/ibcs2/ibcs2_errno.h>
#include <compat/ibcs2/ibcs2_syscall.h>

static void ibcs2_e_proc_exec __P((struct proc *, struct exec_package *));

extern struct sysent ibcs2_sysent[];
extern const char * const ibcs2_syscallnames[];
extern char ibcs2_sigcode[], ibcs2_esigcode[];

#ifdef IBCS2_DEBUG
int ibcs2_debug = 1;
#else
int ibcs2_debug = 0;
#endif

const struct emul emul_ibcs2 = {
	"ibcs2",
	"/emul/ibcs2",
	native_to_ibcs2_errno,
	ibcs2_sendsig,
	0,
	IBCS2_SYS_MAXSYSCALL,
	ibcs2_sysent,
	ibcs2_syscallnames,
	ibcs2_sigcode,
	ibcs2_esigcode,
	ibcs2_e_proc_exec,
	NULL,
	NULL,
	EMUL_GETPID_PASS_PPID|EMUL_GETID_PASS_EID,
#ifdef IBCS2_MACHDEP_HAS_SEPARATED_SYSCALL
	ibcs2_syscall,
#else
	NULL,
#endif
#
};

/*
 * This is exec process hook. Find out if this is x.out executable, if
 * yes, set flag appropriately, so that emul code which needs to adjust
 * behaviour accordingly can do so.
 */ 
static void
ibcs2_e_proc_exec(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	if (epp->ep_es->es_check == exec_ibcs2_xout_makecmds)
		p->p_emuldata = IBCS2_EXEC_XENIX;
	else
		p->p_emuldata = IBCS2_EXEC_OTHER;
}
