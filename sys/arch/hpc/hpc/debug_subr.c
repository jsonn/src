/*	$NetBSD: debug_subr.c,v 1.1.2.2 2002/02/11 20:07:48 jdolecek Exp $	*/

/*-
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/debug.h>

#define BANNER_LENGTH		80

static const char onoff[2] = "_x";

void
__dbg_bit_print(u_int32_t a, int len, int start, int end, char *title,
    int count)
{
	u_int32_t j, j1;
	int i, n;
	char buf[64];

	n = len * NBBY - 1;
	j1 = 1 << n;
	end = end ? end : n;

	printf(" ");
	if (title) {
		printf("[%-16s] ", title);
	}

	for (j = j1, i = n; j > 0; j >>=1, i--) {
		if (i > end || i < start) {
			printf("%c", a & j ? '+' : '-'); /* out of range */
		} else {
			printf("%c", a & j ? '|' : '.');
		}
	}

	snprintf(buf, sizeof buf, " [0x%%0%dx %%12d]", len << 1);
	printf(buf, a, a);

	if (count) {
		for (j = j1, i = n; j > 0; j >>=1, i--) {
			if (!(i > end || i < start) && (a & j)) {
				printf(" %d", i);
			}
		}
	}

	printf("\n");
}

void
dbg_bitmask_print(u_int32_t reg, u_int32_t mask, const char *name)
{

	printf("%s[%c] ", name, onoff[reg & mask ? 1 : 0]);
}

void
dbg_banner_title(const char *name, size_t len)
{
	int n = (BANNER_LENGTH - (len + 2)) >> 1;

	dbg_draw_line(n);
	printf("[%s]", name);
	dbg_draw_line(n);
	printf("\n");
}

void
dbg_banner_line()
{

	dbg_draw_line(BANNER_LENGTH);
	printf("\n");
}

void
dbg_draw_line(int n)
{
	int i;

	for (i = 0; i < n; i++)
		printf("-");
}
