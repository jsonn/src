/*	$NetBSD: c++rt0.c,v 1.6.2.1 1998/06/10 22:26:28 tv Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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

/*
 * Run-time module for GNU C++ compiled shared libraries.
 *
 * The linker constructs the following arrays of pointers to global
 * constructors and destructors. The first element contains the
 * number of pointers in each.
 * The tables are also null-terminated.
 */
#include <stdlib.h>


/*
 * We make the __{C,D}TOR_LIST__ symbols appear as type `SETD' and
 * include a dummy local function in the set. This keeps references
 * to these symbols local to the shared object this module is linked to.
 */
static void dummy __P((void)) { return; }

/* Note: this is "a.out" dependent. */
__asm(".stabs \"___CTOR_LIST__\",22,0,0,_dummy");
__asm(".stabs \"___DTOR_LIST__\",22,0,0,_dummy");

#ifdef __arm32__			/* XXX ARM32_BROKEN_RELOCATIONS */
#define ARM32_BROKEN_RELOCATIONS	/* XXX ARM32_BROKEN_RELOCATIONS */
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */

void (*__CTOR_LIST__[0]) __P((void));
void (*__DTOR_LIST__[0]) __P((void));

#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
static void	__dtors __P((long));	/* XXX ARM32_BROKEN_RELOCATIONS */
static void	__ctors __P((long));	/* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
static void	__dtors __P((void));
static void	__ctors __P((void));
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */

static void
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
__dtors(base)				/* XXX ARM32_BROKEN_RELOCATIONS */
	long base;			/* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
__dtors()
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
{
	unsigned long i = (unsigned long) __DTOR_LIST__[0];
	void (**p)(void) = __DTOR_LIST__ + i;
 
	while (i--)
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
		(*(void (*)(void))((char *)(*p--) + base))(); /* XXX ... */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
		(**p--)();
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
}

static void
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
__ctors(base)				/* XXX ARM32_BROKEN_RELOCATIONS */
	long base;			/* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
__ctors()
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
{
	void (**p)(void) = __CTOR_LIST__ + 1;

	while (*p)
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
		(*(void (*)(void))((char *)(*p++) + base))(); /* XXX ... */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
		(**p++)();
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
}

#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
extern void __init __P((long)) asm(".init"); /* XXX ARM32_BROKEN_RELOCATIONS */
extern void __fini __P((long)) asm(".fini"); /* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
extern void __init __P((void)) asm(".init");
extern void __fini __P((void)) asm(".fini");
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */

void
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
__init(base)				/* XXX ARM32_BROKEN_RELOCATIONS */
	long base;			/* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
__init()
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
{
	static int initialized = 0;

	/*
	 * Call global constructors.
	 * Arrange to call global destructors at exit.
	 */
	if (!initialized) {
		initialized = 1;
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
		__ctors(base);		/* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
		__ctors();
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
	}

}

void
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
__fini(base)				/* XXX ARM32_BROKEN_RELOCATIONS */
	long base;			/* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
__fini()
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
{
	/*
	 * Call global destructors.
	 */
#ifdef ARM32_BROKEN_RELOCATIONS		/* XXX ARM32_BROKEN_RELOCATIONS */
	__dtors(base);			/* XXX ARM32_BROKEN_RELOCATIONS */
#else					/* XXX ARM32_BROKEN_RELOCATIONS */
	__dtors();
#endif					/* XXX ARM32_BROKEN_RELOCATIONS */
}
