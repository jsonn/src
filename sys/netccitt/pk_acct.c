/*	$NetBSD: pk_acct.c,v 1.16.2.1 2003/07/02 15:26:58 darrenr Exp $	*/

/*
 * Copyright (c) 1984 University of British Columbia.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Laboratory for Computation Vision and the Computer Science Department
 * of the University of British Columbia.
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
 *	@(#)pk_acct.c	8.2 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pk_acct.c,v 1.16.2.1 2003/07/02 15:26:58 darrenr Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <netccitt/x25.h>
#include <netccitt/pk.h>
#include <netccitt/pk_var.h>
#include <netccitt/pk_extern.h>
#include <netccitt/x25acct.h>


struct	vnode *pkacctp;
/* 
 *  Turn on packet accounting
 */
int
pk_accton(path)
	char *path;
{
	struct vnode *vp = NULL;
	struct nameidata nd;
	struct vnode *oacctp = pkacctp;
	struct lwp *l = curlwp;		/* XXX */
	struct proc *p;
	int error;

	if (path == 0)
		goto close;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, path, l);
	if ((error = vn_open (&nd, FWRITE, 0644)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_UNLOCK(vp, 0);
	if (vp -> v_type != VREG) {
		vrele (vp);
		return (EACCES);
	}
	pkacctp = vp;
	if (oacctp) {
	close:
		p = l->l_proc;
		error = vn_close (oacctp, FWRITE, p->p_ucred, l);
	}
	return (error);
}

/* 
 *  Write a record on the accounting file.
 */

void
pk_acct(lcp)
	struct pklcd *lcp;
{
	struct vnode *vp;
	struct sockaddr_x25 *sa;
	char *src, *dst;
	int len;
	static struct x25acct acbuf;

	if ((vp = pkacctp) == 0)
		return;
	bzero ((caddr_t)&acbuf, sizeof (acbuf));
	if (lcp -> lcd_ceaddr != 0)
		sa = lcp -> lcd_ceaddr;
	else if (lcp -> lcd_craddr != 0) {
		sa = lcp -> lcd_craddr;
		acbuf.x25acct_callin = 1;
	} else
		return;

	if (sa -> x25_opts.op_flags & X25_REVERSE_CHARGE)
		acbuf.x25acct_revcharge = 1;
	acbuf.x25acct_stime = lcp -> lcd_stime;
	acbuf.x25acct_etime = time.tv_sec - acbuf.x25acct_stime;
	acbuf.x25acct_uid = curproc -> p_cred -> p_ruid;
	acbuf.x25acct_psize = sa -> x25_opts.op_psize;
	acbuf.x25acct_net = sa -> x25_net;
	/*
	 * Convert address to bcd
	 */
	src = sa -> x25_addr;
	dst = acbuf.x25acct_addr;
	for (len = 0; *src; len++)
		if (len & 01)
			*dst++ |= *src++ & 0xf;
		else
			*dst = *src++ << 4;
	acbuf.x25acct_addrlen = len;

	bcopy (sa -> x25_udata, acbuf.x25acct_udata,
		sizeof (acbuf.x25acct_udata));
	acbuf.x25acct_txcnt = lcp -> lcd_txcnt;
	acbuf.x25acct_rxcnt = lcp -> lcd_rxcnt;

	(void) vn_rdwr(UIO_WRITE, vp, (caddr_t)&acbuf, sizeof (acbuf),
		(off_t)0, UIO_SYSSPACE, IO_UNIT|IO_APPEND,
		curproc -> p_ucred, (size_t *)0,
		(struct lwp *)0);
}
