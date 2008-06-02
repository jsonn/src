/* $NetBSD: lkminit_exec.c,v 1.9.6.1 2008/06/02 13:24:18 mjf Exp $ */

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Graff <explorer@flame.org>.
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
__KERNEL_RCSID(0, "$NetBSD: lkminit_exec.c,v 1.9.6.1 2008/06/02 13:24:18 mjf Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>
#include <sys/proc.h>
#include <sys/lkm.h>
#include <sys/signalvar.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_exec.h>

int exec_osf1_ecoff_lkmentry(struct lkm_table *, int, int);

static struct execsw exec_osf1_ecoff =
	/* OSF/1 (Digital Unix) ECOFF */
	{ ECOFF_HDR_SIZE,
	  exec_ecoff_makecmds,
	  { .ecoff_probe_func = osf1_exec_ecoff_probe },
	  NULL,
	  EXECSW_PRIO_ANY,
  	  howmany(OSF1_MAX_AUX_ENTRIES * sizeof (struct osf1_auxv) +
	    2 * (MAXPATHLEN + 1), sizeof (char *)), /* exec & loader names */
	  osf1_copyargs,
	  cpu_exec_ecoff_setregs,
	  coredump_netbsd,
	  exec_setup_stack };

/*
 * declare the exec
 */
MOD_EXEC("exec_osf1_ecoff", -1, &exec_osf1_ecoff, "osf1");

/*
 * entry point
 */
int
exec_osf1_ecoff_lkmentry(lkmtp, cmd, ver)
	struct lkm_table *lkmtp;
	int cmd;
	int ver;
{
	DISPATCH(lkmtp, cmd, ver,
		 lkm_nofunc,
		 lkm_nofunc,
		 lkm_nofunc);
}
