/*	$NetBSD: getextmemx.c,v 1.1.2.2 1997/08/23 07:09:40 thorpej Exp $	*/

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Try 2 more fancy BIOS calls to get the size of extended
 memory besides the classical int15/88, take maximum.
 needs lowlevel parts from biosmemx.S and biosmem.S
 */

#include <lib/libsa/stand.h>
#include "libi386.h"

extern int getextmem2 __P((int*));
extern int getmementry __P((int, int*));

int getextmemx()
{
	int buf[5], i;
	int extmem = getextmem1();

	if(!getextmem2(buf) && buf[0] <= 15 * 1024) {
		int help = buf[0];
		if(help == 15 * 1024)
			help += buf[1] * 64;
		if(extmem < help)
			extmem = help;
	}

	for(i = 0; (i = getmementry(i, buf)) != 0;) {
		if((buf[4] == 1 && buf[0] == 0x100000)
		   && extmem < buf[2] / 1024)
			extmem = buf[2] / 1024;
	}

	return(extmem);
}
