/*	$NetBSD: conf.c,v 1.6.100.1 2011/03/05 20:49:46 rmind Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)conf.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <lib/libsa/stand.h>

extern int fdstrategy(void *, int, daddr_t, size_t, void *, size_t *);
extern int fdopen(struct open_file *, ...);
extern int fdclose(struct open_file *);

extern int instrategy(void *, int, daddr_t, size_t, void *, size_t *);
extern int inopen(struct open_file *, ...);
extern int inclose(struct open_file *);

extern int sdstrategy(void *, int, daddr_t, size_t, void *, size_t *);
extern int sdopen(struct open_file *, ...);
extern int sdclose(struct open_file *);

extern int wdstrategy(void *, int, daddr_t, size_t, void *, size_t *);
extern int wdopen(struct open_file *, ...);
extern int wdclose(struct open_file *);

struct devsw devsw[] = {
	{ "fd", fdstrategy, fdopen, fdclose, noioctl },
	{ "sd", sdstrategy, sdopen, sdclose, noioctl },
	{ "wd", wdstrategy, wdopen, wdclose, noioctl },

	{ NULL, NULL,       NULL,   NULL,    NULL },
};
struct devsw pseudo_devsw = { "in", instrategy, inopen, inclose, noioctl };

int ndevs = sizeof(devsw) / sizeof(devsw[0]);
