/*
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: @(#)ppiioctl.h	7.2 (Berkeley) 12/16/90
 *	$NetBSD: parioctl.h,v 1.1.48.2 2004/09/18 14:42:28 skrll Exp $
 */

#ifndef _X68K_PARIOCTL_H_
#define _X68K_PARIOCTL_H_

#ifndef _IOCTL_
#include <sys/ioctl.h>
#endif

struct parparam {
	int	burst;	/* chars to send/recv in one call */
	int	timo;	/* timeout: -1 blocking, 0 non-blocking, >0 msec */
	int	delay;	/* delay between polls (msec) */
};

#define PAR_BLOCK	-1
#define PAR_NOBLOCK	0

/* default values */
#define	PAR_BURST	1024
#define PAR_TIMO	PAR_BLOCK
#define PAR_DELAY	10

/* limits */
#define	PAR_BURST_MIN	1
#define	PAR_BURST_MAX	1024
#define PAR_DELAY_MIN	0
#define PAR_DELAY_MAX	30000

#define PARIOCSPARAM	_IOW('P', 0x1, struct parparam)
#define PARIOCGPARAM	_IOR('P', 0x2, struct parparam)

#endif
