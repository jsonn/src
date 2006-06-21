/*	$NetBSD: sunos_exec.h,v 1.6.18.1 2006/06/21 14:59:42 yamt Exp $	*/

/*
 * Copyright (c) 1993 Theo de Raadt
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
 * 3. The name of the author may not be used to endorse or promote products
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

#ifndef	_SUNOS_EXEC_H_
#define	_SUNOS_EXEC_H_

struct sunos_exec {
	u_char	a_dynamic:1;	/* has a __DYNAMIC */
	u_char	a_toolversion:7;/* version of toolset used to create this file */
	u_char	a_machtype;	/* machine type */
	u_short	a_magic;	/* magic number */
};
#define SUNOS_M_68010	1	/* runs on 68010 and 68020 */
#define SUNOS_M_68020	2	/* runs only on 68020 */
#define	SUNOS_M_SPARC	3	/* runs only on SPARC */

#ifdef __sparc__
#define SUNOS_M_NATIVE(m)	((m) == SUNOS_M_SPARC)
#elif defined(__mc68010__)
#define SUNOS_M_NATIVE(m)	((m) == SUNOS_M_68010)
#else
#define SUNOS_M_NATIVE(m)	((m) == SUNOS_M_68010 || (m) == SUNOS_M_68020)
#endif

#define SUNOS_AOUT_HDR_SIZE (sizeof(struct exec))

extern const struct emul emul_sunos;

int exec_sunos_aout_makecmds __P((struct lwp *, struct exec_package *));

#endif /* !_SUNOS_EXEC_H_ */
