/*	$NetBSD: freebsd_exec.c,v 1.34.48.2 2008/12/13 01:13:47 haad Exp $	*/

/*
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: freebsd_exec.c,v 1.34.48.2 2008/12/13 01:13:47 haad Exp $");

#if defined(_KERNEL_OPT)
#include "opt_syscall_debug.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <compat/sys/signal.h>
#include <compat/sys/signalvar.h>

#include <compat/freebsd/freebsd_syscall.h>
#include <compat/freebsd/freebsd_exec.h>
#include <compat/freebsd/freebsd_signal.h>
#include <compat/common/compat_util.h>

#include <machine/freebsd_machdep.h>

extern struct sysent freebsd_sysent[];
extern const char * const freebsd_syscallnames[];

struct uvm_object *emul_freebsd_object;

#ifndef __HAVE_SYSCALL_INTERN
void	syscall(void);
#endif

struct emul emul_freebsd = {
	"freebsd",
	"/emul/freebsd",
#ifndef __HAVE_MINIMAL_EMUL
	EMUL_HAS_SYS___syscall,
	NULL,
	FREEBSD_SYS_syscall,
	FREEBSD_SYS_NSYSENT,
#endif
	freebsd_sysent,
#ifdef SYSCALL_DEBUG
	freebsd_syscallnames,
#else
	NULL,
#endif
	freebsd_sendsig,
	trapsignal,
	NULL,
	freebsd_sigcode,
	freebsd_esigcode,
	&emul_freebsd_object,
	freebsd_setregs,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
#ifdef __HAVE_SYSCALL_INTERN
	freebsd_syscall_intern,
#else
	syscall,
#endif
	NULL,
	NULL,

	uvm_default_mapaddr,
	NULL,			/* e_usertrap */
	NULL,			/* e_sa */
	0,			/* e_ucsize */
	NULL,			/* e_startlwp */
};
