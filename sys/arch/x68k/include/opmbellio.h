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
 * $Id: opmbellio.h,v 1.1.1.1 1996/05/05 12:17:03 oki Exp $
 */

#ifndef _IOCTL_
#include <sys/ioctl.h>
#endif
#include <x68k/dev/opmreg.h>

struct bell_info {
    int	volume;
    int	pitch;
    int	msec;
};

/* default values */
#define	BELL_VOLUME	100	/* percentage */
#define BELL_PITCH	440
#define BELL_DURATION	100
#define BELL_CHANNEL	7

/* limits */
#define	MAXBVOLUME	100	/* 100 percent */
#define	MAXBPITCH	4698
#define MINBPITCH	20
#define MAXBTIME	5000	/* 5 seconds */

#define BELLIOCSPARAM	_IOW('B', 0x1, struct bell_info)
#define BELLIOCGPARAM	_IOR('B', 0x2, struct bell_info)
#define BELLIOCSVOICE	_IOW('B', 0x3, struct opm_voice)
#define BELLIOCGVOICE	_IOR('B', 0x4, struct opm_voice)
