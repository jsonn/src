/*	$NetBSD: tp_meas.c,v 1.8.8.1 2002/01/10 20:03:54 thorpej Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tp_meas.c	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */
/*
 * tp_meas.c : create a performance measurement event
 * in the circular buffer tp_Meas[]
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tp_meas.c,v 1.8.8.1 2002/01/10 20:03:54 thorpej Exp $");

#include <sys/types.h>
#include <sys/time.h>

#include <netiso/argo_debug.h>
#include <netiso/tp_meas.h>

extern struct timeval time;

#ifdef TP_PERF_MEAS
int             tp_Measn = 0;
struct tp_Meas  tp_Meas[TPMEASN];

/*
 * NAME:	 tpmeas()
 *
 * CALLED FROM: tp_emit(), tp_soisdisconecting(), tp_soisdisconnected()
 *	tp0_stash(), tp_stash(), tp_send(), tp_goodack(), tp_usrreq()
 *
 * FUNCTION and ARGUMENTS:
 *  stashes a performance-measurement event for the given reference (ref)
 *  (kind) tells which kind of event, timev is the time to be stored
 *  with this event, (seq), (win), and (size) are integers that usually
 *  refer to the sequence number, window number (on send) and
 *  size of tpdu or window.
 *
 * RETURNS:		Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
Tpmeas(ref, kind, timev, seq, win, size)
	u_int           ref;
	u_int           kind;
	struct timeval *timev;
	u_int           seq, win, size;
{
	struct tp_Meas *tpm;
	static int      mseq;

	tpm = &tp_Meas[tp_Measn++];
	tp_Measn %= TPMEASN;

	tpm->tpm_kind = kind;
	tpm->tpm_tseq = mseq++;
	tpm->tpm_ref = ref;
	if (kind == TPtime_from_ll)
		bcopy((caddr_t) timev, (caddr_t) & tpm->tpm_time, sizeof(struct timeval));
	else
		bcopy((caddr_t) & time,
		      (caddr_t) & tpm->tpm_time, sizeof(struct timeval));
	tpm->tpm_seq = seq;
	tpm->tpm_window = win;
	tpm->tpm_size = size;
}

#endif				/* TP_PERF_MEAS */
