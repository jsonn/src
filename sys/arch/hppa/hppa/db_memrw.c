/*	$NetBSD: db_memrw.c,v 1.1.4.2 2002/07/14 17:47:08 gehenna Exp $	*/

/*	$OpenBSD: db_interface.c,v 1.16 2001/03/22 23:31:45 mickey Exp $	*/

/*
 * Copyright (c) 1999-2000 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
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
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/cpufunc.h>

#include <hppa/hppa/machdep.h>

#include <ddb/db_access.h>

void
db_read_bytes(addr, size, data)
	vaddr_t addr;
	size_t size;
	char *data;
{
	register char *src = (char*)addr;

	while (size--)
		*data++ = *src++;
}

void
db_write_bytes(addr, size, data)
	vaddr_t addr;
	size_t size;
	char *data;
{
	register char *dst = (char *)addr;
	extern int etext;

	/*
	 * Since (most of) the text segment is mapped unwritable,
	 * (but directly), we need to use a special function when
	 * dst is less than etext.
	 */
	while (size--) {
		if (dst < (char *) &etext) {
			hppa_ktext_stb((vaddr_t)dst, *data);
			dst++;
			data++;
		} else
			*dst++ = *data++;
	}

	fcacheall();
	__asm volatile(
			"	nop	\n"
			"	nop	\n"
			"	nop	\n"
			"	nop	\n"
			"	nop	\n"
			"	nop	\n"
			"	nop	\n");
}
