/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)in_cksum.c	7.6 (Berkeley) 12/16/90
 *	$Id: yyyin_cksum.c,v 1.1.2.2 1994/08/02 20:21:56 ragge Exp $
 */

#include "sys/param.h"
#include "sys/mbuf.h"

/*
 * Checksum routine for Internet Protocol family headers (VAX Version).
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 */

in_cksum(m, len)
	register struct mbuf *m;
	register int len;
{
	register u_short *w;		/* on vax, known to be r9 */
	register int sum = 0;		/* on vax, known to be r8 */
	register int mlen = 0;

	for (;;) {
		/*
		 * Each trip around loop adds in
		 * word from one mbuf segment.
		 */
		w = mtod(m, u_short *);
		if (mlen == -1) {
			/*
			 * There is a byte left from the last segment;
			 * add it into the checksum.  Don't have to worry
			 * about a carry-out here because we make sure
			 * that high part of (32 bit) sum is small below.
			 */
			sum += *(u_char *)w << 8;
			w = (u_short *)((char *)w + 1);
			mlen = m->m_len - 1;
			len--;
		} else
			mlen = m->m_len;
		m = m->m_next;
		if (len < mlen)
			mlen = len;
		len -= mlen;
		/*
		 * Force to long boundary so we do longword aligned
		 * memory operations.  It is too hard to do byte
		 * adjustment, do only word adjustment.
		 */
		if (((int)w&0x2) && mlen >= 2) {
			sum += *w++;
			mlen -= 2;
		}
		/*
		 * Do as much of the checksum as possible 32 bits at at time.
		 * In fact, this loop is unrolled to make overhead from
		 * branches &c small.
		 *
		 * We can do a 16 bit ones complement sum 32 bits at a time
		 * because the 32 bit register is acting as two 16 bit
		 * registers for adding, with carries from the low added
		 * into the high (by normal carry-chaining) and carries
		 * from the high carried into the low on the next word
		 * by use of the adwc instruction.  This lets us run
		 * this loop at almost memory speed.
		 *
		 * Here there is the danger of high order carry out, and
		 * we carefully use adwc.
		 */
		while ((mlen -= 32) >= 0) {
#undef ADD
#ifdef unneeded		 /* The loop construct clears carry for us... */
			asm("bicpsr $1");		/* clears carry */
#endif
#define ADD		asm("adwc (r9)+,r8;");
			ADD; ADD; ADD; ADD; ADD; ADD; ADD; ADD;
			asm("adwc $0,r8");
		}
		mlen += 32;
		while ((mlen -= 8) >= 0) {
#ifdef unneeded		 /* The loop construct clears carry for us... */
			asm("bicpsr $1");		/* clears carry */
#endif
			ADD; ADD;
			asm("adwc $0,r8");
		}
		mlen += 8;
		/*
		 * Now eliminate the possibility of carry-out's by
		 * folding back to a 16 bit number (adding high and
		 * low parts together.)  Then mop up trailing words
		 * and maybe an odd byte.
		 */
		{ asm("ashl $-16,r8,r0; addw2 r0,r8");
		  asm("adwc $0,r8; movzwl r8,r8"); }
		while ((mlen -= 2) >= 0) {
			asm("movzwl (r9)+,r0; addl2 r0,r8");
		}
		if (mlen == -1) {
			sum += *(u_char *)w;
		}
		if (len == 0)
			break;
		/*
		 * Locate the next block with some data.
		 * If there is a word split across a boundary we
		 * will wrap to the top with mlen == -1 and
		 * then add it in shifted appropriately.
		 */
		for (;;) {
			if (m == 0) {
				printf("cksum: out of data\n");
				goto done;
			}
			if (m->m_len)
				break;
			m = m->m_next;
		}
	}
done:
	/*
	 * Add together high and low parts of sum
	 * and carry to get cksum.
	 * Have to be careful to not drop the last
	 * carry here.
	 */
	{ asm("ashl $-16,r8,r0; addw2 r0,r8; adwc $0,r8");
	  asm("mcoml r8,r8; movzwl r8,r8"); }
	return (sum);
}
