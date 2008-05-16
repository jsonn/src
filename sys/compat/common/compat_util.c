/* 	$NetBSD: compat_util.c,v 1.40.10.1 2008/05/16 02:23:34 yamt Exp $	*/

/*-
 * Copyright (c) 1994 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Frank van der Linden.
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
__KERNEL_RCSID(0, "$NetBSD: compat_util.c,v 1.40.10.1 2008/05/16 02:23:34 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/exec.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/mount.h>

#include <compat/common/compat_util.h>

void
emul_find_root(struct lwp *l, struct exec_package *epp)
{
	struct nameidata nd;
	const char *emul_path;

	if (epp->ep_emul_root != NULL)
		/* We've already found it */
		return;

	emul_path = epp->ep_esch->es_emul->e_path;
	if (emul_path == NULL)
		/* Emulation doesn't have a root */
		return;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, emul_path);
	if (namei(&nd) != 0)
		/* emulation root doesn't exist */
		return;

	epp->ep_emul_root = nd.ni_vp;
}

/*
 * Search the alternate path for dynamic binary interpreter. If not found
 * there, check if the interpreter exists in within 'proper' tree.
 */
int
emul_find_interp(struct lwp *l, struct exec_package *epp, const char *itp)
{
	int error;
	struct nameidata nd;
	unsigned int flags;

	/* If we haven't found the emulation root already, do so now */
	/* Maybe we should remember failures somehow ? */
	if (epp->ep_esch->es_emul->e_path != 0 && epp->ep_emul_root == NULL)
		emul_find_root(l, epp);

	if (epp->ep_interp != NULL)
		vrele(epp->ep_interp);

	/* We need to use the emulation root for the new program,
	 * not the one for the current process. */
	if (epp->ep_emul_root == NULL)
		flags = FOLLOW;
	else {
		nd.ni_erootdir = epp->ep_emul_root;
		/* hack: Pass in the emulation path for ktrace calls */
		nd.ni_next = epp->ep_esch->es_emul->e_path;
		flags = FOLLOW | TRYEMULROOT | EMULROOTSET;
	}

	NDINIT(&nd, LOOKUP, flags, UIO_SYSSPACE, itp);
	error = namei(&nd);
	if (error != 0) {
		epp->ep_interp = NULL;
		return error;
	}

	/* Save interpreter in case we actually need to load it */
	epp->ep_interp = nd.ni_vp;

	return 0;
}

/*
 * Translate one set of flags to another, based on the entries in
 * the given table.  If 'leftover' is specified, it is filled in
 * with any flags which could not be translated.
 */
unsigned long
emul_flags_translate(const struct emul_flags_xtab *tab,
		     unsigned long in, unsigned long *leftover)
{
	unsigned long out;

	for (out = 0; tab->omask != 0; tab++) {
		if ((in & tab->omask) == tab->oval) {
			in &= ~tab->omask;
			out |= tab->nval;
		}
	}
	if (leftover != NULL)
		*leftover = in;
	return (out);
}

void
compat_offseterr(struct vnode *vp, const char *msg)
{
	struct mount *mp;

	mp = vp->v_mount;

	log(LOG_ERR, "%s: dir offset too large on fs %s (mounted from %s)\n",
	    msg, mp->mnt_stat.f_mntonname, mp->mnt_stat.f_mntfromname);
	uprintf("%s: dir offset too large for emulated program\n", msg);
}
