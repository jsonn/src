/*	$NetBSD: stdio.h,v 1.53.2.1 2004/07/02 18:13:45 he Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)stdio.h	8.5 (Berkeley) 4/29/95
 */

#ifndef	_STDIO_H_
#define	_STDIO_H_

#include <sys/ansi.h>

#include <sys/cdefs.h>
#include <sys/featuretest.h>

#include <machine/ansi.h>
#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#include <sys/null.h>

/*      
 * This is fairly grotesque, but pure ANSI code must not inspect the
 * innards of an fpos_t anyway.  The library internally uses off_t,
 * which we assume is exactly as big as eight chars.
 */
#if (!defined(_ANSI_SOURCE) && !defined(__STRICT_ANSI__)) || defined(_LIBC)
typedef __off_t fpos_t;
#else
typedef struct __sfpos {
	__off_t _pos;
} fpos_t;
#endif

#define	_FSTDIO			/* Define for new stdio with functions. */

/*
 * NB: to fit things in six character monocase externals, the stdio
 * code uses the prefix `__s' for stdio objects, typically followed
 * by a three-character attempt at a mnemonic.
 */

/* stdio buffers */
struct __sbuf {
	unsigned char *_base;
	int	_size;
};

/*
 * stdio state variables.
 *
 * The following always hold:
 *
 *	if (_flags&(__SLBF|__SWR)) == (__SLBF|__SWR),
 *		_lbfsize is -_bf._size, else _lbfsize is 0
 *	if _flags&__SRD, _w is 0
 *	if _flags&__SWR, _r is 0
 *
 * This ensures that the getc and putc macros (or inline functions) never
 * try to write or read from a file that is in `read' or `write' mode.
 * (Moreover, they can, and do, automatically switch from read mode to
 * write mode, and back, on "r+" and "w+" files.)
 *
 * _lbfsize is used only to make the inline line-buffered output stream
 * code as compact as possible.
 *
 * _ub, _up, and _ur are used when ungetc() pushes back more characters
 * than fit in the current _bf, or when ungetc() pushes back a character
 * that does not match the previous one in _bf.  When this happens,
 * _ub._base becomes non-nil (i.e., a stream has ungetc() data iff
 * _ub._base!=NULL) and _up and _ur save the current values of _p and _r.
 *
 * NB: see WARNING above before changing the layout of this structure!
 */
typedef	struct __sFILE {
	unsigned char *_p;	/* current position in (some) buffer */
	int	_r;		/* read space left for getc() */
	int	_w;		/* write space left for putc() */
	short	_flags;		/* flags, below; this FILE is free if 0 */
	short	_file;		/* fileno, if Unix descriptor, else -1 */
	struct	__sbuf _bf;	/* the buffer (at least 1 byte, if !NULL) */
	int	_lbfsize;	/* 0 or -_bf._size, for inline putc */

	/* operations */
	void	*_cookie;	/* cookie passed to io functions */
	int	(*_close) __P((void *));
	int	(*_read)  __P((void *, char *, int));
	fpos_t	(*_seek)  __P((void *, fpos_t, int));
	int	(*_write) __P((void *, const char *, int));

	/* file extension */
	struct	__sbuf _ext;

	/* separate buffer for long sequences of ungetc() */
	unsigned char *_up;	/* saved _p when _p is doing ungetc data */
	int	_ur;		/* saved _r when _r is counting ungetc data */

	/* tricks to meet minimum requirements even when malloc() fails */
	unsigned char _ubuf[3];	/* guarantee an ungetc() buffer */
	unsigned char _nbuf[1];	/* guarantee a getc() buffer */

	/* separate buffer for fgetln() when line crosses buffer boundary */
	struct	__sbuf _lb;	/* buffer for fgetln() */

	/* Unix stdio files get aligned to block boundaries on fseek() */
	int	_blksize;	/* stat.st_blksize (may be != _bf._size) */
	fpos_t	_offset;	/* current lseek offset */
} FILE;

__BEGIN_DECLS
extern FILE __sF[];
__END_DECLS

#define	__SLBF	0x0001		/* line buffered */
#define	__SNBF	0x0002		/* unbuffered */
#define	__SRD	0x0004		/* OK to read */
#define	__SWR	0x0008		/* OK to write */
	/* RD and WR are never simultaneously asserted */
#define	__SRW	0x0010		/* open for reading & writing */
#define	__SEOF	0x0020		/* found EOF */
#define	__SERR	0x0040		/* found error */
#define	__SMBF	0x0080		/* _buf is from malloc */
#define	__SAPP	0x0100		/* fdopen()ed in append mode */
#define	__SSTR	0x0200		/* this is an sprintf/snprintf string */
#define	__SOPT	0x0400		/* do fseek() optimization */
#define	__SNPT	0x0800		/* do not do fseek() optimization */
#define	__SOFF	0x1000		/* set iff _offset is in fact correct */
#define	__SMOD	0x2000		/* true => fgetln modified _p text */
#define	__SALC	0x4000		/* allocate string space dynamically */

/*
 * The following three definitions are for ANSI C, which took them
 * from System V, which brilliantly took internal interface macros and
 * made them official arguments to setvbuf(), without renaming them.
 * Hence, these ugly _IOxxx names are *supposed* to appear in user code.
 *
 * Although numbered as their counterparts above, the implementation
 * does not rely on this.
 */
#define	_IOFBF	0		/* setvbuf should set fully buffered */
#define	_IOLBF	1		/* setvbuf should set line buffered */
#define	_IONBF	2		/* setvbuf should set unbuffered */

#define	BUFSIZ	1024		/* size of buffer used by setbuf */
#define	EOF	(-1)

/*
 * FOPEN_MAX is a minimum maximum, and is the number of streams that
 * stdio can provide without attempting to allocate further resources
 * (which could fail).  Do not use this for anything.
 */
				/* must be == _POSIX_STREAM_MAX <limits.h> */
#define	FOPEN_MAX	20	/* must be <= OPEN_MAX <sys/syslimits.h> */
#define	FILENAME_MAX	1024	/* must be <= PATH_MAX <sys/syslimits.h> */

/* System V/ANSI C; this is the wrong way to do this, do *not* use these. */
#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
#define	P_tmpdir	"/var/tmp/"
#endif
#define	L_tmpnam	1024	/* XXX must be == PATH_MAX */
/* Always ensure that this is consistent with <limits.h> */
#ifndef TMP_MAX
#define TMP_MAX			308915776	/* Legacy */
#endif

/* Always ensure that these are consistent with <fcntl.h> and <unistd.h>! */
#ifndef SEEK_SET
#define	SEEK_SET	0	/* set file offset to offset */
#endif
#ifndef SEEK_CUR
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#endif
#ifndef SEEK_END
#define	SEEK_END	2	/* set file offset to EOF plus offset */
#endif

#define	stdin	(&__sF[0])
#define	stdout	(&__sF[1])
#define	stderr	(&__sF[2])

/*
 * Functions defined in ANSI C standard.
 */
__BEGIN_DECLS
void	 clearerr __P((FILE *));
int	 fclose __P((FILE *));
int	 feof __P((FILE *));
int	 ferror __P((FILE *));
int	 fflush __P((FILE *));
int	 fgetc __P((FILE *));
int	 fgetpos __P((FILE * __restrict, fpos_t * __restrict));
char	*fgets __P((char * __restrict, int, FILE * __restrict));
FILE	*fopen __P((const char *, const char *));
int	 fprintf __P((FILE * __restrict , const char * __restrict, ...));
int	 fputc __P((int, FILE *));
int	 fputs __P((const char * __restrict, FILE * __restrict));
size_t	 fread __P((void * __restrict, size_t, size_t, FILE * __restrict));
FILE	*freopen __P((const char *, const char *, FILE * __restrict));
int	 fscanf __P((FILE * __restrict, const char * __restrict, ...));
int	 fseek __P((FILE *, long, int));
int	 fsetpos __P((FILE *, const fpos_t *));
long	 ftell __P((FILE *));
size_t	 fwrite __P((const void * __restrict, size_t, size_t,
	    FILE * __restrict));
int	 getc __P((FILE *));
int	 getchar __P((void));
void	 perror __P((const char *));
int	 printf __P((const char * __restrict, ...));
int	 putc __P((int, FILE *));
int	 putchar __P((int));
int	 puts __P((const char *));
int	 remove __P((const char *));
void	 rewind __P((FILE *));
int	 scanf __P((const char * __restrict, ...));
void	 setbuf __P((FILE * __restrict, char * __restrict));
int	 setvbuf __P((FILE * __restrict, char * __restrict, int, size_t));
int	 sscanf __P((const char * __restrict, const char * __restrict, ...));
FILE	*tmpfile __P((void));
int	 ungetc __P((int, FILE *));
int	 vfprintf __P((FILE * __restrict, const char * __restrict,
	    _BSD_VA_LIST_));
int	 vfprintf_unlocked __P((FILE * __restrict, const char * __restrict,
	    _BSD_VA_LIST_));
int	 vprintf __P((const char * __restrict, _BSD_VA_LIST_));

#ifndef __AUDIT__
char	*gets __P((char *));
int	 sprintf __P((char * __restrict, const char * __restrict, ...));
char	*tmpnam __P((char *));
int	 vsprintf __P((char * __restrict, const char * __restrict,
	    _BSD_VA_LIST_));
#endif

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)
int	 rename  __P((const char *, const char *)) __RENAME(__posix_rename);
#else
int	 rename  __P((const char *, const char *));
#endif
__END_DECLS

/*
 * IEEE Std 1003.1-90
 */
#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
#define	L_ctermid	1024	/* size for ctermid(); PATH_MAX */
#define L_cuserid	9	/* size for cuserid(); UT_NAMESIZE + 1 */

__BEGIN_DECLS
char	*ctermid __P((char *));
#ifndef __CUSERID_DECLARED
#define __CUSERID_DECLARED
/* also declared in unistd.h */
char	*cuserid __P((char *));
#endif /* __CUSERID_DECLARED */
FILE	*fdopen __P((int, const char *));
int	 fileno __P((FILE *));
__END_DECLS
#endif /* not ANSI */

/*
 * IEEE Std 1003.1c-95, also adopted by X/Open CAE Spec Issue 5 Version 2
 */
#if (_POSIX_C_SOURCE - 0) >= 199506L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_REENTRANT) || defined(_NETBSD_SOURCE)
__BEGIN_DECLS
void	flockfile __P((FILE *));
int	ftrylockfile __P((FILE *));
void	funlockfile __P((FILE *));
int	getc_unlocked __P((FILE *));
int	getchar_unlocked __P((void));
int	putc_unlocked __P((int, FILE *));
int	putchar_unlocked __P((int));
__END_DECLS
#endif /* _POSIX_C_SOURCE >= 1995056 || _XOPEN_SOURCE >= 500 || ... */

/*
 * Functions defined in POSIX 1003.2 and XPG2 or later.
 */
#if (_POSIX_C_SOURCE - 0) >= 2 || (_XOPEN_SOURCE - 0) >= 2 || \
    defined(_NETBSD_SOURCE)
__BEGIN_DECLS
int	 pclose __P((FILE *));
FILE	*popen __P((const char *, const char *));
__END_DECLS
#endif

/*
 * Functions defined in XPG4.2.
 */
#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
__BEGIN_DECLS
int	 getw __P((FILE *));
int	 putw __P((int, FILE *));
int	 snprintf __P((char * __restrict, size_t, const char * __restrict, ...))
	    __attribute__((__format__(__printf__, 3, 4)));
int	 vsnprintf __P((char * __restrict, size_t, const char * __restrict,
	    _BSD_VA_LIST_))
	    __attribute__((__format__(__printf__, 3, 0)));

#ifndef __AUDIT__
char	*tempnam __P((const char *, const char *));
#endif
__END_DECLS
#endif

/*
 * X/Open CAE Specification Issue 5 Version 2
 */
#if (_XOPEN_SOURCE - 0) >= 500 || defined(_LARGEFILE_SOURCE) || \
    defined(_NETBSD_SOURCE)
#ifndef	off_t
typedef	__off_t		off_t;
#define	off_t		__off_t
#endif /* off_t */

__BEGIN_DECLS
int	 fseeko __P((FILE *, off_t, int));
off_t	 ftello __P((FILE *));
__END_DECLS
#endif /* _XOPEN_SOURCE >= 500 || _LARGEFILE_SOURCE || _NETBSD_SOURCE */

/*
 * Routines that are purely local.
 */
#if defined(_NETBSD_SOURCE)

#define	FPARSELN_UNESCESC	0x01
#define	FPARSELN_UNESCCONT	0x02
#define	FPARSELN_UNESCCOMM	0x04
#define	FPARSELN_UNESCREST	0x08
#define	FPARSELN_UNESCALL	0x0f

__BEGIN_DECLS
int	 asprintf __P((char ** __restrict, const char * __restrict, ...))
	    __attribute__((__format__(__printf__, 2, 3)));
char	*fgetln __P((FILE * __restrict, size_t * __restrict));
char	*fparseln(FILE *, size_t *, size_t *, const char[3], int);
int	 fpurge __P((FILE *));
void	 setbuffer __P((FILE *, char *, int));
int	 setlinebuf __P((FILE *));
int	 vasprintf __P((char ** __restrict, const char * __restrict,
	    _BSD_VA_LIST_))
	    __attribute__((__format__(__printf__, 2, 0)));
int	 vscanf __P((const char * __restrict, _BSD_VA_LIST_))
	    __attribute__((__format__(__scanf__, 1, 0)));
int	 vfscanf __P((FILE * __restrict, const char * __restrict,
	    _BSD_VA_LIST_))
	    __attribute__((__format__(__scanf__, 2, 0)));
int	 vsscanf __P((const char * __restrict, const char * __restrict,
	    _BSD_VA_LIST_))
	    __attribute__((__format__(__scanf__, 2, 0)));
__const char *fmtcheck __P((const char *, const char *))
	    __attribute__((__format_arg__(2)));
__END_DECLS

/*
 * Stdio function-access interface.
 */
__BEGIN_DECLS
FILE	*funopen __P((const void *,
		int (*)(void *, char *, int),
		int (*)(void *, const char *, int),
		fpos_t (*)(void *, fpos_t, int),
		int (*)(void *)));
__END_DECLS
#define	fropen(cookie, fn) funopen(cookie, fn, 0, 0, 0)
#define	fwopen(cookie, fn) funopen(cookie, 0, fn, 0, 0)
#endif /* _NETBSD_SOURCE */

/*
 * Functions internal to the implementation.
 */
__BEGIN_DECLS
int	__srget __P((FILE *));
int	__swbuf __P((int, FILE *));
__END_DECLS

/*
 * The __sfoo macros are here so that we can 
 * define function versions in the C library.
 */
#define	__sgetc(p) (--(p)->_r < 0 ? __srget(p) : (int)(*(p)->_p++))
#if defined(__GNUC__) && defined(__STDC__)
static __inline int __sputc(int _c, FILE *_p) {
	if (--_p->_w >= 0 || (_p->_w >= _p->_lbfsize && (char)_c != '\n'))
		return (*_p->_p++ = _c);
	else
		return (__swbuf(_c, _p));
}
#else
/*
 * This has been tuned to generate reasonable code on the vax using pcc.
 */
#define	__sputc(c, p) \
	(--(p)->_w < 0 ? \
		(p)->_w >= (p)->_lbfsize ? \
			(*(p)->_p = (c)), *(p)->_p != '\n' ? \
				(int)*(p)->_p++ : \
				__swbuf('\n', p) : \
			__swbuf((int)(c), p) : \
		(*(p)->_p = (c), (int)*(p)->_p++))
#endif

#define	__sfeof(p)	(((p)->_flags & __SEOF) != 0)
#define	__sferror(p)	(((p)->_flags & __SERR) != 0)
#define	__sclearerr(p)	((void)((p)->_flags &= ~(__SERR|__SEOF)))
#define	__sfileno(p)	((p)->_file)

#ifndef __lint__
#if !defined(_REENTRANT) && !defined(_PTHREADS)
#define	feof(p)		__sfeof(p)
#define	ferror(p)	__sferror(p)
#define	clearerr(p)	__sclearerr(p)

#define	getc(fp)	__sgetc(fp)
#define putc(x, fp)	__sputc(x, fp)
#endif /* !_REENTRANT && !_PTHREADS */
#endif /* __lint__ */

#define	getchar()	getc(stdin)
#define	putchar(x)	putc(x, stdout)

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
#if !defined(_REENTRANT) && !defined(_PTHREADS)
#define	fileno(p)	__sfileno(p)
#endif /* !_REENTRANT && !_PTHREADS */
#endif /* !_ANSI_SOURCE */

#if (_POSIX_C_SOURCE - 0) >= 199506L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_REENTRANT) || defined(_NETBSD_SOURCE)
#define getc_unlocked(fp)	__sgetc(fp)
#define putc_unlocked(x, fp)	__sputc(x, fp)

#define getchar_unlocked()	getc_unlocked(stdin)
#define putchar_unlocked(x)	putc_unlocked(x, stdout)
#endif /* _POSIX_C_SOURCE >= 199506 || _XOPEN_SOURCE >= 500 || _REENTRANT... */

#endif /* _STDIO_H_ */
