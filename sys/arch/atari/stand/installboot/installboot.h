/*	$NetBSD: installboot.h,v 1.2.4.1 1996/06/30 19:56:27 jtc Exp $	*/

/*
 * Copyright (c) 1995 Waldi Ravens
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
 *        This product includes software developed by Waldi Ravens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#define	PATH_MDEC	"/usr/mdec/"
#define	PATH_NVRAM	"/dev/nvram"
#define	NVRAM_BOOTPREF	1
#define	BOOTPREF_NETBSD	0x20
#define	BOOTPREF_LINUX	0x10
#define	BOOTPREF_SYSV	0x40
#define	BOOTPREF_TOS	0x80

/*
 * OS_LIST contains all possible combinations of OS-type,
 * OS-release and OS-revision that are supported by this
 * version of installboot.
 *
 * Syntax of OS_LIST: (ostype(osrelease(osrevision)..)..)..
 *
 * Where the parentheses indicate grouping and the double
 * dots indicate repetition (each group must appear at
 * least once).
 *
 * Ostype, osrelease and osrevision are strings surrounded
 * resp. by braces, square brackets and angle brackets. It
 * should be obvious that those delimeters can not be part
 * of the strings, nor can the EOS marker ('\0').
 */
#define	OS_LIST		"{NetBSD}[1.1A]<199306>[1.1B]<199306>[1.2]<199606>"
#define	BRA_TYPE	"{}"
#define	BRA_RELEASE	"[]"
#define	BRA_REVISION	"<>"

u_int	dkcksum __P((struct disklabel *));
daddr_t	readdisklabel __P((char *, struct disklabel *));
