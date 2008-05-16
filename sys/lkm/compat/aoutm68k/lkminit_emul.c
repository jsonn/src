/* $NetBSD: lkminit_emul.c,v 1.8.10.1 2008/05/16 02:25:36 yamt Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: lkminit_emul.c,v 1.8.10.1 2008/05/16 02:25:36 yamt Exp $");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/exec_aout.h>
#include <sys/lkm.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

/*
 * This module is different to other compat modules - it adds the
 * execsw[] entry as well. As the compat_aoutm68k is emulation more of
 * executable format support, it's probably better to be placed here
 * than under lkm/exec/.
 */

extern const struct emul emul_netbsd_aoutm68k;

int compat_aoutm68k_lkmentry(struct lkm_table *, int, int);

static struct execsw exec_netbsd_aoutm68k =
	/* Native a.out */
	{ sizeof(struct exec),
	  exec_aout_makecmds,
	  { NULL },
	  &emul_netbsd_aoutm68k,
	  EXECSW_PRIO_FIRST,	/* Note: this differs from exec_conf.c entry */
	  0,
	  copyargs,
	  NULL,
	  coredump_netbsd };

/*
 * declare the executable format
 */
MOD_EXEC("compat_aoutm68k", -1, &exec_netbsd_aoutm68k, "aoutm68k");

/*
 * entry point
 */
int
compat_aoutm68k_lkmentry(lkmtp, cmd, ver)
	struct lkm_table *lkmtp;
	int cmd;
	int ver;
{

	DISPATCH(lkmtp, cmd, ver, lkm_nofunc, lkm_nofunc, lkm_nofunc);
}
