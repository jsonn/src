/*	$NetBSD: intr.h,v 1.1.4.2 2000/12/08 09:30:35 bouyer Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/* XXX - arbitrary numbers; no interpretation is defined yet */
#define	IPL_NONE	0		/* nothing */
#define	IPL_SOFTINT	1		/* softint */
#define	IPL_SOFTCLOCK	1		/* timeouts */
#define	IPL_SOFTNET	1		/* protocol stack */
#define	IPL_BIO		PIL_BIO		/* block I/O */
#define	IPL_NET		PIL_NET		/* network */
#define	IPL_SOFTSERIAL	4		/* serial */
#define	IPL_TTY		PIL_TTY		/* terminal */
#define	IPL_IMP		PIL_IMP		/* memory allocation */
#define	IPL_AUDIO	PIL_AUD		/* audio */
#define	IPL_CLOCK	PIL_CLOCK	/* clock */
#define	IPL_SERIAL	PIL_SER		/* serial */
#define	IPL_SCHED	PIL_SCHED	/* scheduler */
#define	IPL_LOCK	PIL_LOCK	/* locks */
#define	IPL_HIGH	PIL_HIGH	/* everything */

#define	__GENERIC_SOFT_INTERRUPTS

void *
softintr_establish __P((int level, void (*fun)(void *), void *arg));

void
softintr_disestablish __P((void *cookie));

void
softintr_schedule __P((void *cookie));
