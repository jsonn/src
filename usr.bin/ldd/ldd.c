/*	$NetBSD: ldd.c,v 1.2.12.3 2010/03/06 22:48:22 sborrill Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
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
 *      This product includes software developed by John Polstra.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ldd.c,v 1.2.12.3 2010/03/06 22:48:22 sborrill Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <a.out.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "debug.h"
#include "rtld.h"
#include "ldd.h"

/*
 * Data declarations.
 */
static char *error_message;	/* Message for dlopen(), or NULL */
bool _rtld_trust;		/* False for setuid and setgid programs */
Obj_Entry *_rtld_objlist;	/* Head of linked list of shared objects */
Obj_Entry **_rtld_objtail = &_rtld_objlist;
				/* Link field of last object in list */
Obj_Entry *_rtld_objmain;	/* The main program shared object */
int _rtld_pagesz;

Search_Path *_rtld_default_paths;
Search_Path *_rtld_paths;
Library_Xform *_rtld_xforms;

static void usage(void) __dead;
char *main_local;
char *main_progname;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-f <format 1>] [-f <format 2>] <filename>"
		" ...\n", getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	char *fmt1 = NULL, *fmt2 = NULL;
	int c;

#ifdef DEBUG
	debug = 1;
#endif
	while ((c = getopt(argc, argv, "f:")) != -1) {
		switch (c) {
		case 'f':
			if (fmt1) {
				if (fmt2)
					errx(1, "Too many formats");
				fmt2 = optarg;
			} else
				fmt1 = optarg;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (argc <= 0) {
		usage();
		/*NOTREACHED*/
	}

	for (; argc != 0; argc--, argv++) {
		int fd;

		fd = open(*argv, O_RDONLY);
		if (fd == -1) {
			warn("%s", *argv);
			continue;
		}
		if (elf_ldd(fd, *argv, fmt1, fmt2) == -1 &&
		    /* Alpha never had 32 bit support. */
#if defined(_LP64) && !defined(__alpha__)
		    elf32_ldd(fd, *argv, fmt1, fmt2) == -1 &&
#endif
		    aout_ldd(fd, *argv, fmt1, fmt2) == -1)
			warnx("%s", error_message);
		close(fd);
	}

	return 0;
}

/*
 * Error reporting function.  Use it like printf.  If formats the message
 * into a buffer, and sets things up so that the next call to dlerror()
 * will return the message.
 */
void
_rtld_error(const char *fmt, ...)
{
	static char buf[512];
	va_list ap;
	va_start(ap, fmt);
	xvsnprintf(buf, sizeof buf, fmt, ap);
	error_message = buf;
	va_end(ap);
}

char *
dlerror()
{
	char *msg = error_message;
	error_message = NULL;
	return msg;
}

void
fmtprint(const char *libname, Obj_Entry *obj, const char *fmt1,
    const char *fmt2)
{
	const char *libpath = obj ? obj->path : "not found";
	char libnamebuf[200];
	char *libmajor = NULL;
	const char *fmt;
	char *cp;
	int c;

	if (strncmp(libname, "lib", 3) == 0 &&
	    (cp = strstr(libname, ".so")) != NULL) {
		int i = cp - (libname + 3);

		if (i >= sizeof(libnamebuf))
			i = sizeof(libnamebuf) - 1;
		(void)memcpy(libnamebuf, libname + 3, i);
		libnamebuf[i] = '\0';
		if (cp[3] && isdigit((unsigned char)cp[4]))
			libmajor = &cp[4];
		libname = libnamebuf;
	}

	if (fmt1 == NULL)
		fmt1 = libmajor != NULL ?
		    "\t-l%o.%m => %p\n" :
		    "\t-l%o => %p\n";
	if (fmt2 == NULL)
		fmt2 = "\t%o => %p\n";

	fmt = libname == libnamebuf ? fmt1 : fmt2;
	while ((c = *fmt++) != '\0') {
		switch (c) {
		default:
			putchar(c);
			continue;
		case '\\':
			switch (c = *fmt) {
			case '\0':
				continue;
			case 'n':
				putchar('\n');
				break;
			case 't':
				putchar('\t');
				break;
			}
			break;
		case '%':
			switch (c = *fmt) {
			case '\0':
				continue;
			case '%':
			default:
				putchar(c);
				break;
			case 'A':
				printf("%s", main_local);
				break;
			case 'a':
				printf("%s", main_progname);
				break;
			case 'o':
				printf("%s", libname);
				break;
			case 'm':
				printf("%s", libmajor);
				break;
			case 'n':
				/* XXX: not supported for elf */
				break;
			case 'p':
				printf("%s", libpath);
				break;
			case 'x':
				printf("%p", obj ? obj->mapbase : 0);
				break;
			}
			break;
		}
		++fmt;
	}
}

void
print_needed(Obj_Entry *obj, const char *fmt1, const char *fmt2)
{
	const Needed_Entry *needed;

	for (needed = obj->needed; needed != NULL; needed = needed->next) {
		const char *libname = obj->strtab + needed->name;

		if (needed->obj != NULL) {
			if (!needed->obj->printed) {
				fmtprint(libname, needed->obj, fmt1, fmt2);
				needed->obj->printed = 1;
				print_needed(needed->obj, fmt1, fmt2);
			}
		} else {
			fmtprint(libname, needed->obj, fmt1, fmt2);
		}
	}
}
