/*	$NetBSD: boot.h,v 1.1.6.1 2005/04/13 21:52:29 tron Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
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

#if defined(_DEBUG)
#define DPRINTF(x)	printf x;
#else
#define DPRINTF(x)
#endif

#define MAXDEVNAME	16
#define DEFBOOTDEV	"wd0a"
#define DEFKERNELNAME	kernelnames[0]

extern char *kernelnames[];

/*
 * com
 */
int comspeed	__P((long));

/*
 * console
 */
char* cninit	__P((int *, int *));
int   cngetc	__P((void));
void  cnputc	__P((int));
int   cnscan	__P((void));

/*
 * clock
 */
void delay(int);

/*
 * wd
 */
int wdstrategy	__P((void *, int, daddr_t, size_t, void *, size_t *));
int wdopen	__P((struct open_file *, ...));
int wdclose	__P((struct open_file *));

/*
 * devopen
 */
int devparse (const char *fname, int *dev, u_int8_t *unit,
			u_int8_t *part, const char **file);

int tgets(char *);
