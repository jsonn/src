/*	$NetBSD: stdlib.h,v 1.55.2.1 2004/03/26 22:53:11 jmc Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)stdlib.h	8.5 (Berkeley) 5/19/95
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE) && \
    !defined(_XOPEN_SOURCE)
#include <sys/types.h>		/* for quad_t, etc. */
#endif

#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_WCHAR_T_
typedef	_BSD_WCHAR_T_	wchar_t;
#undef	_BSD_WCHAR_T_
#endif

typedef struct {
	int quot;		/* quotient */
	int rem;		/* remainder */
} div_t;

typedef struct {
	long quot;		/* quotient */
	long rem;		/* remainder */
} ldiv_t;

#if !defined(_ANSI_SOURCE) && \
    (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || \
     defined(_ISOC99_SOURCE) || (__STDC_VERSION__ - 0) >= 199901L)
typedef struct {
	/* LONGLONG */
	long long int quot;	/* quotient */
	/* LONGLONG */
	long long int rem;	/* remainder */
} lldiv_t;
#endif

#if !defined(_ANSI_SOURCE) && !defined(_ISOC99_SOURCE) && \
    !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
typedef struct {
	quad_t quot;		/* quotient */
	quad_t rem;		/* remainder */
} qdiv_t;
#endif


#include <sys/null.h>

#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS	0

#define	RAND_MAX	0x7fffffff

extern size_t __mb_cur_max;
#define	MB_CUR_MAX	__mb_cur_max

__BEGIN_DECLS
__dead	 void abort __P((void)) __attribute__((__noreturn__));
__pure	 int abs __P((int));
int	 atexit __P((void (*)(void)));
double	 atof __P((const char *));
int	 atoi __P((const char *));
long	 atol __P((const char *));
#ifndef __BSEARCH_DECLARED
#define __BSEARCH_DECLARED
/* also in search.h */
void	*bsearch __P((const void *, const void *, size_t, size_t,
    int (*)(const void *, const void *)));
#endif /* __BSEARCH_DECLARED */
void	*calloc __P((size_t, size_t));
div_t	 div __P((int, int));
__dead	 void exit __P((int)) __attribute__((__noreturn__));
void	 free __P((void *));
__aconst char *getenv __P((const char *));
__pure long
	 labs __P((long));
ldiv_t	 ldiv __P((long, long));
void	*malloc __P((size_t));
void	 qsort __P((void *, size_t, size_t,
	    int (*)(const void *, const void *)));
int	 rand __P((void));
void	*realloc __P((void *, size_t));
void	 srand __P((unsigned));
double	 strtod __P((const char * __restrict, char ** __restrict));
long	 strtol __P((const char * __restrict, char ** __restrict, int));
unsigned long
	 strtoul __P((const char * __restrict, char ** __restrict, int));
int	 system __P((const char *));

/* These are currently just stubs. */
int	 mblen __P((const char *, size_t));
size_t	 mbstowcs __P((wchar_t * __restrict, const char * __restrict, size_t));
int	 wctomb __P((char *, wchar_t));
int	 mbtowc __P((wchar_t * __restrict, const char * __restrict, size_t));
size_t	 wcstombs __P((char * __restrict, const wchar_t * __restrict, size_t));

#if !defined(_ANSI_SOURCE)


/*
 * IEEE Std 1003.1c-95, also adopted by X/Open CAE Spec Issue 5 Version 2
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
    (_POSIX_C_SOURCE - 0) >= 199506L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_REENTRANT)
int	 rand_r __P((unsigned int *));
#endif


/*
 * X/Open Portability Guide >= Issue 4
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
    (_XOPEN_SOURCE - 0) >= 4
double	 drand48 __P((void));
double	 erand48 __P((unsigned short[3]));
long	 jrand48 __P((unsigned short[3]));
void	 lcong48 __P((unsigned short[7]));
long	 lrand48 __P((void));
long	 mrand48 __P((void));
long	 nrand48 __P((unsigned short[3]));
unsigned short *
	 seed48 __P((unsigned short[3]));
void	 srand48 __P((long));

int	 putenv __P((const char *));
#endif


/*
 * X/Open Portability Guide >= Issue 4 Version 2
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
    (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500
long	 a64l __P((const char *));
char	*l64a __P((long));

char	*initstate __P((unsigned long, char *, size_t));
long	 random __P((void));
char	*setstate __P((char *));
void	 srandom __P((unsigned long));

char	*mkdtemp __P((char *));
int	 mkstemp __P((char *));
#ifndef __AUDIT__
char	*mktemp __P((char *));
#endif

int	 setkey __P((const char *));

char	*realpath __P((const char *, char *));

int	 ttyslot __P((void));

void	*valloc __P((size_t));		/* obsoleted by malloc() */
#endif

/*
 * ISO C99
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
    defined(_ISOC99_SOURCE) || (__STDC_VERSION__ - 0) >= 199901L
/* LONGLONG */
long long int	atoll __P((const char *));
/* LONGLONG */
long long int	llabs __P((long long int));
/* LONGLONG */
lldiv_t		lldiv __P((long long int, long long int));
/* LONGLONG */
long long int	strtoll __P((const char * __restrict, char ** __restrict, int));
/* LONGLONG */
unsigned long long int
		strtoull __P((const char * __restrict, char ** __restrict,
		    int));
#endif

/*
 * Implementation-defined extensions
 */
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
#if defined(alloca) && (alloca == __builtin_alloca) && (__GNUC__ < 2)
void	*alloca __P((int));     /* built-in for gcc */ 
#else 
void	*alloca __P((size_t)); 
#endif /* __GNUC__ */ 

u_int32_t arc4random __P((void));
void	 arc4random_stir __P((void));
void	 arc4random_addrandom __P((u_char *, int));
char	*getbsize __P((int *, long *));
char	*cgetcap __P((char *, const char *, int));
int	 cgetclose __P((void));
int	 cgetent __P((char **, char **, const char *));
int	 cgetfirst __P((char **, char **));
int	 cgetmatch __P((const char *, const char *));
int	 cgetnext __P((char **, char **));
int	 cgetnum __P((char *, const char *, long *));
int	 cgetset __P((const char *));
int	 cgetstr __P((char *, const char *, char **));
int	 cgetustr __P((char *, const char *, char **));

int	 daemon __P((int, int));
__aconst char *devname __P((dev_t, mode_t));
int	 getloadavg __P((double [], int));

void	 cfree __P((void *));

int	 heapsort __P((void *, size_t, size_t,
	    int (*)(const void *, const void *)));
int	 mergesort __P((void *, size_t, size_t,
	    int (*)(const void *, const void *)));
int	 radixsort __P((const unsigned char **, int, const unsigned char *,
	    unsigned));
int	 sradixsort __P((const unsigned char **, int, const unsigned char *,
	    unsigned));

int	 setenv __P((const char *, const char *, int));
void	 unsetenv __P((const char *));
void	 setproctitle __P((const char *, ...))
	    __attribute__((__format__(__printf__, 1, 2)));
const char *getprogname __P((void)) __attribute__((__const__));
void	setprogname __P((const char *));

quad_t	 qabs __P((quad_t));
quad_t	 strtoq __P((const char * __restrict, char ** __restrict, int));
u_quad_t strtouq __P((const char * __restrict, char ** __restrict, int));

int	 l64a_r __P((long, char *, int));

size_t	shquote __P((const char *arg, char *buf, size_t bufsize));
size_t	shquotev __P((int argc, char * const * argv, char *buf,
	    size_t bufsize));
#endif /* !_POSIX_C_SOURCE && !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE */

#if !defined(_ANSI_SOURCE) && !defined(_ISOC99_SOURCE) && \
    !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
qdiv_t	 qdiv __P((quad_t, quad_t));
#endif
__END_DECLS

#endif /* !_STDLIB_H_ */
