/*	$NetBSD: in4_cksum.c,v 1.2.16.2 2004/09/18 14:41:17 skrll Exp $ */

/*
 * Copyright (c) 1995 Matthew R. Green.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, and it's contributors.
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
 *	@(#)in_cksum.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Copyright (c) 2001 Eduardo Horvath.
 * Copyright (c) 1995 Zubin Dittia.
 * Copyright (c) 1994, 1998 Charles M. Hannum.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, and it's contributors.
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
 *	@(#)in_cksum.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in4_cksum.c,v 1.2.16.2 2004/09/18 14:41:17 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

extern int in_cksum_internal __P((struct mbuf *, int len, int offset, int sum));

int
in4_cksum(m, nxt, off, len)
	struct mbuf *m;
	u_int8_t nxt;
	int off, len;
{
	u_char *w;
	u_int sum = 0;
	struct ipovly ipov;

	/*
	 * Declare three temporary registers for use by the asm code.  We
	 * allow the compiler to pick which specific machine registers to
	 * use, instead of hard-coding this in the asm code.
	 */
	u_int tmp1, tmp2, tmp3;

	if (nxt != 0) {
		/* pseudo header */
		memset(&ipov, 0, sizeof(ipov));
		ipov.ih_len = htons(len);
		ipov.ih_pr = nxt; 
		ipov.ih_src = mtod(m, struct ip *)->ip_src; 
		ipov.ih_dst = mtod(m, struct ip *)->ip_dst;
		w = (u_char *)&ipov;
		/* assumes sizeof(ipov) == 20 */
#ifdef __arch64__
		__asm __volatile(" lduw [%5 + 0], %1; "
			" lduw [%5 + 4], %2; "
			" lduw [%5 + 8], %3; add %0, %1, %0; "
			" lduw [%5 + 12], %1; add %0, %2, %0; "
			" lduw [%5 + 16], %2; add %0, %3, %0; "
			" mov -1, %3; add %0, %1, %0; "
			" srl %3, 0, %3; add %0, %2, %0; "
			" srlx %0, 32, %2; and %0, %3, %1; "
			" add %0, %2, %0; "
			: "=r" (sum), "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3)
			: "0" (sum), "r" (w));
#else
		/* 
		 * Can't use upper 32-bits of the registers, so this needs to
		 * be recoded.  So instead of accumulating the carry in the 
		 * upper 32-bits of the registers, we use addxcc which cannot
		 * be grouped with any other instructions.
		 */
		__asm __volatile(" lduw [%5 + 0], %1; "
			" lduw [%5 + 4], %2; "
			" lduw [%5 + 8], %3; addcc %0, %1, %0; "
			" lduw [%5 + 12], %1; "
			" addxcc %0, %2, %0; "
			" lduw [%5 + 16], %2; "
			" addxcc %0, %3, %0; "
			" addxcc %0, %1, %0; "
			" addxcc %0, %2, %0; "
			" addxcc %0, 0, %0; "
			: "=r" (sum), "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3)
			: "0" (sum), "r" (w));
#endif
	}

	/* skip unnecessary part */
	while (m && off > 0) {
		if (m->m_len > off)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	return (in_cksum_internal(m, len, off, sum));
}
