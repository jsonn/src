/*	$NetBSD: mul.c,v 1.2.22.1 2002/11/11 22:22:36 nathanw Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mul.c	8.1 (Berkeley) 6/4/93";
#else
static char rcsid[] = "$NetBSD: mul.c,v 1.2.22.1 2002/11/11 22:22:36 nathanw Exp $";
#endif
#endif /* not lint */

#include <stdio.h>

main()
{
	union { long long q; unsigned int v[2]; } a, b, m;
	char buf[300];
	extern long long __muldi3(long long, long long);

	for (;;) {
		printf("> ");
		if (fgets(buf, sizeof buf, stdin) == NULL)
			break;
		if (sscanf(buf, "%u:%u %u:%u",
			    &a.v[0], &a.v[1], &b.v[0], &b.v[1]) != 4 &&
		    sscanf(buf, "0x%x:%x 0x%x:%x",
			    &a.v[0], &a.v[1], &b.v[0], &b.v[1]) != 4) {
			printf("eh?\n");
			continue;
		}
		m.q = __muldi3(a.q, b.q);
		printf("%x:%x * %x:%x => %x:%x\n",
		    a.v[0], a.v[1], b.v[0], b.v[1], m.v[0], m.v[1]);
		printf("  = %X%08X * %X%08X => %X%08X\n",
		    a.v[0], a.v[1], b.v[0], b.v[1], m.v[0], m.v[1]);
	}
	exit(0);
}
