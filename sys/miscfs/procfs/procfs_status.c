/*	$NetBSD: procfs_status.c,v 1.16.6.2 2001/10/13 17:42:52 fvdl Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <miscfs/procfs/procfs.h>
#include <miscfs/specfs/specdev.h>

int
procfs_dostatus(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	struct session *sess;
	struct tty *tp;
	struct ucred *cr;
	char *ps;
	char *sep;
	int pid, ppid, pgid, sid;
	int i;
	int xlen;
	int error;
	char psbuf[256+MAXHOSTNAMELEN];		/* XXX - conservative */

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	pid = p->p_pid;
	ppid = p->p_pptr ? p->p_pptr->p_pid : 0,
	pgid = p->p_pgrp->pg_id;
	sess = p->p_pgrp->pg_session;
	sid = sess->s_sid;

/* comm pid ppid pgid sid maj,min ctty,sldr start ut st wmsg uid gid groups ... */

	ps = psbuf;
	memcpy(ps, p->p_comm, MAXCOMLEN);
	ps[MAXCOMLEN] = '\0';
	ps += strlen(ps);
	ps += sprintf(ps, " %d %d %d %d ", pid, ppid, pgid, sid);

	if ((p->p_flag&P_CONTROLT) && (tp = sess->s_ttyp))
		ps += sprintf(ps, "%d,%d ", major(tp->t_dev),
		    minor(tp->t_dev));
	else
		ps += sprintf(ps, "%d,%d ", -1, -1);

	sep = "";
	if (sess->s_ttyvp) {
		ps += sprintf(ps, "%sctty", sep);
		sep = ",";
	}
	if (SESS_LEADER(p)) {
		ps += sprintf(ps, "%ssldr", sep);
		sep = ",";
	}
	if (*sep != ',')
		ps += sprintf(ps, "noflags");

	if (p->p_flag & P_INMEM)
		ps += sprintf(ps, " %ld,%ld",
			p->p_stats->p_start.tv_sec,
			p->p_stats->p_start.tv_usec);
	else
		ps += sprintf(ps, " -1,-1");
	
	{
		struct timeval ut, st;

		calcru(p, &ut, &st, (void *) 0);
		ps += sprintf(ps, " %ld,%ld %ld,%ld",
			ut.tv_sec,
			ut.tv_usec,
			st.tv_sec,
			st.tv_usec);
	}

	ps += sprintf(ps, " %s",
	    (p->p_wchan && p->p_wmesg) ? p->p_wmesg : "nochan");

	cr = p->p_ucred;

	ps += sprintf(ps, " %d", cr->cr_uid);
	ps += sprintf(ps, " %d", cr->cr_gid);
	for (i = 0; i < cr->cr_ngroups; i++)
		ps += sprintf(ps, ",%d", cr->cr_groups[i]);
	ps += sprintf(ps, "\n");

	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	if (xlen <= 0)
		error = 0;
	else
		error = uiomove(ps, xlen, uio);

	return (error);
}
