/*	$NetBSD: hd_timer.c,v 1.11.8.1 2005/04/29 11:29:33 kent Exp $	*/

/*
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)hd_timer.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 1984 University of British Columbia.
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
 *	@(#)hd_timer.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hd_timer.c,v 1.11.8.1 2005/04/29 11:29:33 kent Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>

#include <netccitt/hdlc.h>
#include <netccitt/hd_var.h>
#include <netccitt/x25.h>
#include <netccitt/pk_extern.h>

/*
 * these can be patched with adb if the
 * default values are inappropriate
 */

int             hd_t1 = T1;
int             hd_t3 = T3;
int             hd_n2 = N2;

/*
 *  HDLC TIMER
 *
 *  This routine is called every 500ms by the kernel. Decrement timer by this
 *  amount - if expired then process the event.
 */

void
hd_timer()
{
	struct hdcb *hdp;
	int    s = splnet();

	for (hdp = hdcbhead; hdp; hdp = hdp->hd_next) {
		if (hdp->hd_rrtimer && (--hdp->hd_rrtimer == 0)) {
			if (hdp->hd_lasttxnr != hdp->hd_vr)
				hd_writeinternal(hdp, RR, POLLOFF);
		}
		if (!(hdp->hd_timer && --hdp->hd_timer == 0))
			continue;

		switch (hdp->hd_state) {
		case INIT:
		case DISC_SENT:
			hd_writeinternal(hdp, DISC, POLLON);
			break;

		case ABM:
			if (hdp->hd_lastrxnr != hdp->hd_vs) {	/* XXX */
				hdp->hd_timeouts++;
				hd_resend_iframe(hdp);
			}
			break;

		case WAIT_SABM:
			hd_writeinternal(hdp, FRMR, POLLOFF);
			if (++hdp->hd_retxcnt == hd_n2) {
				hdp->hd_retxcnt = 0;
				hd_writeinternal(hdp, SABM, POLLOFF);
				hdp->hd_state = WAIT_UA;
			}
			break;

		case DM_SENT:
			if (++hdp->hd_retxcnt == hd_n2) {
				/* Notify the packet level. */
				(void) pk_ctlinput(PRC_LINKDOWN,
				       (struct sockaddr *) hdp->hd_pkp, NULL);
				hdp->hd_retxcnt = 0;
				hdp->hd_state = SABM_SENT;
				hd_writeinternal(hdp, SABM, POLLOFF);
			} else
				hd_writeinternal(hdp, DM, POLLOFF);
			break;

		case WAIT_UA:
			if (++hdp->hd_retxcnt == hd_n2) {
				hdp->hd_retxcnt = 0;
				hd_writeinternal(hdp, DM, POLLOFF);
				hdp->hd_state = DM_SENT;
			} else
				hd_writeinternal(hdp, SABM, POLLOFF);
			break;

		case SABM_SENT:
			/* Do this indefinitely. */
			hd_writeinternal(hdp, SABM, POLLON);
			break;

		case DISCONNECTED:
			/*
			 * Poll the interface driver flags waiting
			 * for the IFF_UP bit to come on.
			 */
			if (hdp->hd_ifp->if_flags & IFF_UP)
				hdp->hd_state = INIT;

		}
		SET_TIMER(hdp);
	}

	splx(s);
}
