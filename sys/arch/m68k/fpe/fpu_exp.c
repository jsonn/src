/*	$NetBSD: fpu_exp.c,v 1.1.64.1 2004/08/03 10:36:40 skrll Exp $	*/

/*
 * Copyright (c) 1995  Ken Nakata
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fpu_exp.c	10/24/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fpu_exp.c,v 1.1.64.1 2004/08/03 10:36:40 skrll Exp $");

#include "fpu_emulate.h"

/*
 * fpu_exp.c: defines fpu_etox(), fpu_etoxm1(), fpu_tentox(), and fpu_twotox();
 */

struct fpn *
fpu_etox(fe)
     struct fpemu *fe;
{
  /* stub */
  return &fe->fe_f2;
}

struct fpn *
fpu_etoxm1(fe)
     struct fpemu *fe;
{
  /* stub */
  return &fe->fe_f2;
}

struct fpn *
fpu_tentox(fe)
     struct fpemu *fe;
{
  /* stub */
  return &fe->fe_f2;
}

struct fpn *
fpu_twotox(fe)
     struct fpemu *fe;
{
  /* stub */
  return &fe->fe_f2;
}
