/*	$NetBSD: bt_subr.c,v 1.1.1.1.2.1 1998/08/02 00:06:46 eeh Exp $ */

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)bt_subr.c	8.2 (Berkeley) 1/21/94
 */

#include "opt_uvm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/errno.h>

#include <vm/vm.h>

#include <machine/fbio.h>

#include <sparc64/dev/btreg.h>
#include <sparc64/dev/btvar.h>

/*
 * Common code for dealing with Brooktree video DACs.
 * (Contains some software-only code as well, since the colormap
 * ioctls are shared between the cgthree and cgsix drivers.)
 */

/*
 * Implement an FBIOGETCMAP-like ioctl.
 */
int
bt_getcmap(p, cm, cmsize)
	register struct fbcmap *p;
	union bt_cmap *cm;
	int cmsize;
{
	register u_int i, start, count;
	register u_char *cp;

	start = fuword(&p->index);
	count = fuword(&p->count);
	if (start >= cmsize || start + count > cmsize)
		return (EINVAL);
	for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 3, i++) {
		if (subyte(&p->red[i], cp[0]) ||
		    subyte(&p->green[i], cp[1]) ||
		    subyte(&p->blue[i], cp[2]))
			return (EFAULT);
	}
	return (0);
}

/*
 * Implement the software portion of an FBIOPUTCMAP-like ioctl.
 */
int
bt_putcmap(p, cm, cmsize)
	register struct fbcmap *p;
	union bt_cmap *cm;
	int cmsize;
{
	register u_int i, start, count;
	register u_char *cp;

	/* Apparently the ioctl interface does a copyin of this structure for us. */
	start = p->index;
	count = p->count;
	if (start >= cmsize || start + count > cmsize)
		return (EINVAL);
#if defined(UVM)
	if (!uvm_useracc(p->red, count, B_READ) ||
	    !uvm_useracc(p->green, count, B_READ) ||
	    !uvm_useracc(p->blue, count, B_READ))
		return (EFAULT);
#else
	if (!useracc(p->red, count, B_READ) ||
	    !useracc(p->green, count, B_READ) ||
	    !useracc(p->blue, count, B_READ))
		return (EFAULT);
#endif
	for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 3, i++) {
		cp[0] = fubyte(&p->red[i]);
		cp[1] = fubyte(&p->green[i]);
		cp[2] = fubyte(&p->blue[i]);
	}
	return (0);
}
