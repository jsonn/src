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
 *
 *	$Id: crt0.c,v 1.9.2.1 1994/07/26 19:59:52 cgd Exp $
 */


#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "%W% (Erasmus) %G%";
#endif /* LIBC_SCCS and not lint */

extern void exit();
int _callmain();

#include <sys/param.h>

#ifdef DYNAMIC
#include <sys/types.h>
#include <sys/syscall.h>
#include <a.out.h>
#ifndef N_GETMAGIC
#define N_GETMAGIC(x)	((x).a_magic)
#endif
#ifndef N_BSSADDR
#define N_BSSADDR(x)	(N_DATADDR(x)+(x).a_data)
#endif
#include <sys/mman.h>
#ifdef sun
#define MAP_COPY	MAP_PRIVATE
#define MAP_ANON	0
#endif
#include <link.h>

extern struct _dynamic _DYNAMIC;
static struct ld_entry	*ld_entry;
static void	__do_dynamic_link ();
static void	__set_progname ();
static char	*_getenv();
static int	_strncmp();

#if defined(sun) && defined(sparc)
static		__call();
#endif

#ifdef sun
#define LDSO	"/usr/lib/ld.so"
#endif
#ifdef BSD
#define LDSO	"/usr/libexec/ld.so"
#endif

#endif /* DYNAMIC */

static char		*_strrchr();

char			**environ;

#ifdef BSD
extern	unsigned char	etext;
extern	unsigned char	eprol asm ("eprol");
extern			start() asm("start");
extern			mcount() asm ("mcount");

int			errno;
static char		empty[1];
char			*__progname = empty;
#endif

/*
 * We need these system calls, but can't use library stubs
 */
#define _exit(v)		__syscall(SYS_exit, (v))
#define open(name, f, m)	__syscall(SYS_open, (name), (f), (m))
#define close(fd)		__syscall(SYS_close, (fd))
#define read(fd, s, n)		__syscall(SYS_read, (fd), (s), (n))
#define write(fd, s, n)		__syscall(SYS_write, (fd), (s), (n))
#define dup(fd)			__syscall(SYS_dup, (fd))
#define dup2(fd, fdnew)		__syscall(SYS_dup2, (fd), (fdnew))

#ifdef sun
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall(SYS_mmap, (addr), (len), (prot), _MAP_NEW|(flags), (fd), (off))
#else
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall2((quad_t)SYS_mmap, (addr), (len), (prot), (flags), \
	(fd), 0, (off_t)(off))
#endif

#define _FATAL(str) \
	write(2, str, sizeof(str)), \
	_exit(1);


asm ("	.global start");
asm ("	.text");
asm ("	start:");

/* Set up `argc', `argv', and `envp' into local registers (from GNU Emacs). */
asm ("	mov	0, %fp");
asm ("	ld	[%sp + 64], %l0");	/* argc */
asm ("	add	%sp, 68, %l1");		/* argv */
asm ("	sll	%l0, 2,	%l2");		/**/
asm ("	add	%l2, 4,	%l2");		/* envp = argv + (argc << 2) + 4 */
asm ("	add	%l1, %l2, %l2");	/**/
asm ("	sethi	%hi(_environ), %l3");
asm ("	st	%l2, [%l3+%lo(_environ)]");	/* *environ = l2 */

/* Finish diddling with stack. */
asm ("	andn	%sp, 7,	%sp");
asm ("	sub	%sp, 24, %sp");

/* Set __progname */
asm ("	mov	%l0, %o0");		/**/
asm ("	call	___set_progname");	/**/
asm ("	mov	%l1, %o1");		/**/

#ifdef DYNAMIC
/* Resolve symbols in dynamic libraries */
asm ("	call	___do_dynamic_link");
asm ("	nop");
#endif

/* From here, all symbols should have been resolved, so we can use libc */
#ifdef MCRT0
asm ("	call	___do_mcrt");
asm ("	nop");
#endif

#ifdef sun
/* Stay Sun compatible (currently (SunOS 4.1.2) does nothing on sun4) */
asm ("	call	start_float");
asm ("	nop");
#endif

/* Move `argc', `argv', and `envp' from locals to parameters for `main'.  */
asm ("	mov	%l0,%o0");
asm ("	mov	%l1,%o1");
asm ("__callmain:");		/* Defined for the benefit of debuggers */
asm ("	call	_main");
asm ("	mov	%l2,%o2");

asm ("	call	_exit");
asm ("	nop");

#ifdef DYNAMIC
/* System call entry */
asm("	.set	SYSCALL_G2RFLAG, 0x400");
asm("	.set	SYS___syscall, 198");
asm("___syscall2:");
asm("	sethi	%hi(SYS___syscall), %g1");	/* `SYS___syscall' */
asm("	ba	1f");
asm("	or	%g1, %lo(SYS___syscall), %g1");
asm("___syscall:");
asm("	clr	%g1");				/* `SYS_syscall' */
asm("1:");
asm("	or	%g1, SYSCALL_G2RFLAG, %g1");	/* Use quick return */
asm("	add	%o7, 8, %g2");
asm("	ta	%g0");
asm("	mov	-0x1, %o0");			/* Note: no `errno' */
asm("	jmp	%o7 + 0x8");
asm("	mov	-0x1, %o1");
#endif

#ifdef MCRT0
static void
__do_mcrt ()
{
	extern unsigned char eprol, etext;
	extern void _mcleanup();

	atexit(_mcleanup);
	monstartup(&eprol, &etext);
	return;
}
#endif

static void
__set_progname(argc, argv)
int	argc;
char	*argv[];
{
	if (argv[0])
		if ((__progname = _strrchr(argv[0], '/')) == NULL)
			__progname = argv[0];
		else
			++__progname;
}

#ifdef DYNAMIC
static void
__do_dynamic_link ()
{
	struct crt_ldso	crt;
	struct exec	hdr;
	char		*ldso;
	int		dupzfd;
	int		(*entry)();

	/* ld(1) convention: if DYNAMIC = 0 then statically linked */
	if (&_DYNAMIC == (struct _dynamic *)0)
		return;

#ifdef DEBUG
	/* Provision for alternate ld.so - security risk! */
	if (!(ldso = _getenv("LDSO")))
#endif
		ldso = LDSO;

	crt.crt_ldfd = open(ldso, 0, 0);
	if (crt.crt_ldfd == -1) {
		_FATAL("No ld.so\n");
	}

	/* Read LDSO exec header */
	if (read(crt.crt_ldfd, &hdr, sizeof hdr) < sizeof hdr) {
		_FATAL("Failure reading ld.so\n");
	}
	if (N_GETMAGIC(hdr) != ZMAGIC && N_GETMAGIC(hdr) != QMAGIC) {
		_FATAL("Bad magic: ld.so\n");
	}

#ifdef sun
	/* Get bucket of zeroes */
	crt.crt_dzfd = open("/dev/zero", 0, 0);
	if (crt.crt_dzfd == -1) {
		_FATAL("No /dev/zero\n");
	}
#endif
#ifdef BSD
	/* We use MAP_ANON */
	crt.crt_dzfd = -1;
#endif

#if defined(sun) && defined(DUPZFD)
	if ((dupzfd = dup(crt.crt_dzfd)) < 0) {
		_FATAL("Cannot dup /dev/zero\n");
	}
#endif

	/* Map in ld.so */
	crt.crt_ba = mmap(0, hdr.a_text+hdr.a_data+hdr.a_bss,
			PROT_READ|PROT_EXEC,
			MAP_COPY,
			crt.crt_ldfd, N_TXTOFF(hdr));
	if (crt.crt_ba == -1) {
		_FATAL("Cannot map ld.so\n");
	}

#ifdef BSD
/* !!!
 * This is gross, ld.so is a ZMAGIC a.out, but has `sizeof(hdr)' for
 * an entry point and not at PAGSIZ as the N_*ADDR macros assume.
 */
#undef N_DATADDR
#undef N_BSSADDR
#define N_DATADDR(x)	((x).a_text)
#define N_BSSADDR(x)	((x).a_text + (x).a_data)
#endif

	/* Map in data segment of ld.so writable */
	if (mmap(crt.crt_ba+N_DATADDR(hdr), hdr.a_data,
			PROT_READ|PROT_WRITE,
			MAP_FIXED|MAP_COPY,
			crt.crt_ldfd, N_DATOFF(hdr)) == -1) {
		_FATAL("Cannot map ld.so\n");
	}

	/* Map bss segment of ld.so zero */
	if (hdr.a_bss && mmap(crt.crt_ba+N_BSSADDR(hdr), hdr.a_bss,
			PROT_READ|PROT_WRITE,
			MAP_FIXED|MAP_ANON|MAP_COPY,
			crt.crt_dzfd, 0) == -1) {
		_FATAL("Cannot map ld.so\n");
	}

	crt.crt_dp = &_DYNAMIC;
	crt.crt_ep = environ;
	crt.crt_bp = (caddr_t)_callmain;
	crt.crt_prog = __progname;

#ifdef sun
	/* Call Sun's ld.so entry point: version 1, offset crt */
	__call(CRT_VERSION_SUN, &crt, crt.crt_ba + sizeof hdr);
#else
	entry = (int (*)())(crt.crt_ba + sizeof hdr);
	if ((*entry)(CRT_VERSION_BSD_3, &crt) == -1) {
		_FATAL("ld.so failed\n");
	}
	ld_entry = _DYNAMIC.d_entry;
#endif

#if defined(sun) && defined(DUPZFD)
	if (dup2(dupzfd, crt.crt_dzfd) < 0) {
		_FATAL("Cannot dup2 /dev/zero\n");
	}
	(void)close(dupzfd);
#endif
	return;
}

#ifdef sun
static
__call()
{
	/*
	 * adjust the C generated pointer to the crt struct to the
	 * likings of ld.so, which is an offset relative to its %fp
	 */
#if 0
	asm("___call:");
	asm("call	%o2");
	asm("sub	%o1, %sp, %o1");	/* adjust parameter */
#else Hmmm...
	asm("mov	%i0, %o0");
	asm("mov	%i1, %o1");
	asm("call	%i2");
	asm("sub	%o1, %sp, %o1");
	/*NOTREACHED, control is transferred directly to our caller */
#endif
}
#endif

/*
 * DL stubs
 */

void *
dlopen(name, mode)
char	*name;
int	mode;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlopen)(name, mode);
}

int
dlclose(fd)
void	*fd;
{
	if (ld_entry == NULL)
		return -1;

	return (ld_entry->dlclose)(fd);
}

void *
dlsym(fd, name)
void	*fd;
char	*name;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlsym)(fd, name);
}

int
dlctl(fd, cmd, arg)
void	*fd, *arg;
int	cmd;
{
	if (ld_entry == NULL)
		return -1;

	return (ld_entry->dlctl)(fd, cmd, arg);
}

/*
 * Support routines
 */

static int
_strncmp(s1, s2, n)
	register char *s1, *s2;
	register n;
{

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

static char *
_getenv(name)
	register char *name;
{
	extern char **environ;
	register int len;
	register char **P, *C;

	for (C = name, len = 0; *C && *C != '='; ++C, ++len);
	for (P = environ; *P; ++P)
		if (!_strncmp(*P, name, len))
			if (*(C = *P + len) == '=') {
				return(++C);
			}
	return (char *)0;
}

#endif /* DYNAMIC */

static char *
_strrchr(p, ch)
register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif
