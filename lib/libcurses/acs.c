/*	$NetBSD: acs.c,v 1.1.2.2 2000/03/30 19:27:45 jdc Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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

#include "curses.h"

chtype _acs_char[NUM_ACS];

/*
 * __init_acs --
 *	Fill in the ACS characters.  The 'ac' termcap entry is a list of
 *	character pairs - ACS definition then terminal representation.
 */
void
__init_acs()
{
	int		count;
	char		*aofac;	/* Address of 'ac' */
	char		acs;
	unsigned char	term;

	/* Default value '+' for all ACS characters */
	for (count=0; count < NUM_ACS; count++)
		_acs_char[count]= '+';

	/* Add the SUSv2 defaults (those that are not '+') */
	ACS_RARROW = '>';
	ACS_LARROW = '<';
	ACS_UARROW = '^';
	ACS_DARROW = 'v';
	ACS_BLOCK = '#';
/*	ACS_DIAMOND = '+';	*/
	ACS_CKBOARD = ':';
	ACS_DEGREE = 39;	/* ' */
	ACS_PLMINUS = '#';
	ACS_BOARD = '#';
	ACS_LANTERN = '#';
/*	ACS_LRCORNER = '+';	*/
/*	ACS_URCORNER = '+';	*/
/*	ACS_ULCORNER = '+';	*/
/*	ACS_LLCORNER = '+';	*/
/*	ACS_PLUS = '+';		*/
	ACS_HLINE = '-';
	ACS_S1 = '-';
	ACS_S9 = '_';
/*	ACS_LTEE = '+';		*/
/*	ACS_RTEE = '+';		*/
/*	ACS_BTEE = '+';		*/
/*	ACS_TTEE = '+';		*/
	ACS_VLINE = '|';
	ACS_BULLET = 'o';

	if (AC == NULL)
		return;

	aofac = AC;

	while (*aofac != '\0') {
		if ((acs = *aofac) == '\0')
			return;
		if (++aofac == '\0')
			return;
		if ((term = *aofac) == '\0')
			return;
		if (acs > 0) 	/* Only add characters 1 to 127 */
			_acs_char[acs] = term | __ALTCHARSET;
		aofac++;
#ifdef DEBUG
		__CTRACE("__init_acs: %c = %c\n", acs, term);
#endif
	}

	if (Ea != NULL)
		tputs(Ea, 0, __cputchar);
}
