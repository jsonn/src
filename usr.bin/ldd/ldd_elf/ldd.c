/*	$NetBSD: ldd.c,v 1.11.2.1 2004/05/28 08:31:23 tron Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include "debug.h"
#include "rtld.h"

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

static void print_needed(Obj_Entry *);
static int ldd_aout(char *, int);

int
main(int argc, char **argv)
{
	struct stat st;

#ifdef DEBUG
	debug = 1;
#endif
	_rtld_add_paths(&_rtld_default_paths, RTLD_DEFAULT_LIBRARY_PATH);

	_rtld_pagesz = sysconf(_SC_PAGESIZE);

	for (argc--, argv++; argc != 0; argc--, argv++) {
		int fd = open(*argv, O_RDONLY);
		if (fd == -1) {
			warn("%s", *argv);
			continue;
		}
		if (fstat(fd, &st) < 0) {
			warn("%s", *argv);
			close(fd);
			continue;
		}

		_rtld_paths = NULL;
		_rtld_trust = (st.st_mode & (S_ISUID | S_ISGID)) == 0;
		if (_rtld_trust)
			_rtld_add_paths(&_rtld_paths, getenv("LD_LIBRARY_PATH"));

		_rtld_process_hints(&_rtld_paths, &_rtld_xforms, _PATH_LD_HINTS);
		_rtld_objmain = _rtld_map_object(xstrdup(*argv), fd, &st);
		if (_rtld_objmain == NULL) {
			if (ldd_aout(*argv, fd) < 0)
				warnx("%s", error_message);
			close(fd);
			continue;
		}
		close(fd);

		_rtld_objmain->path = xstrdup(*argv);
		_rtld_digest_dynamic(_rtld_objmain);

		/* Link the main program into the list of objects. */
		*_rtld_objtail = _rtld_objmain;
		_rtld_objtail = &_rtld_objmain->next;
		++_rtld_objmain->refcount;

		(void) _rtld_load_needed_objects(_rtld_objmain, 0);

		printf("%s:\n", _rtld_objmain->path);
		print_needed(_rtld_objmain);

		while (_rtld_objlist != NULL) {
			Obj_Entry *obj = _rtld_objlist;
			_rtld_objlist = obj->next;
			while (obj->rpaths != NULL) {
				const Search_Path *rpath = obj->rpaths;
				obj->rpaths = rpath->sp_next;
				free((void *) rpath->sp_path);
				free((void *) rpath);
			}
			while (obj->needed != NULL) {
				const Needed_Entry *needed = obj->needed;
				obj->needed = needed->next;
				free((void *) needed);
			}
			(void) munmap(obj->mapbase, obj->mapsize);
			free(obj->path);
			free(obj);
		}

		_rtld_objmain = NULL;
		_rtld_objtail = &_rtld_objlist;
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

static void
print_needed(Obj_Entry *obj)
{
	const Needed_Entry *needed;

	for (needed = obj->needed; needed != NULL; needed = needed->next) {
		char libnamebuf[200];
		const char *libname = obj->strtab + needed->name, *cp;
		if (strncmp(libname, "lib", 3) == 0
		    && (cp = strstr(libname, ".so")) != NULL) {
			strcpy(libnamebuf, "-l");
			memcpy(&libnamebuf[2], libname + 3, cp - (libname + 3));
			strcpy(&libnamebuf[cp - (libname + 3) + 2], cp + 3);
			libname = libnamebuf;
		}

		if (needed->obj != NULL) {
			print_needed(needed->obj);
			if (!needed->obj->printed) {
				printf("\t %s => %s\n", libname,
				    needed->obj->path);
				needed->obj->printed = 1;
			}
		} else {
			printf("\t %s => not found\n", libname);
		}
	}
}

static int
ldd_aout(char *file, int fd)
{
	struct exec hdr;
	int status, rval;

	lseek(fd, 0, SEEK_SET);
	if (read(fd, &hdr, sizeof hdr) != sizeof hdr
	    || (N_GETFLAG(hdr) & EX_DPMASK) != EX_DYNAMIC
#if 1 /* Compatibility */
	    || hdr.a_entry < N_PAGSIZ(hdr)
#endif
	    ) {
		/* calling function prints warning */
		return -1;
	}

	setenv("LD_TRACE_LOADED_OBJECTS", "", 1);
	setenv("LD_TRACE_LOADED_OBJECTS_PROGNAME", file, 1);
	printf("%s:\n", file);
	fflush(stdout);

	rval = 0;
	switch (fork()) {
	case -1:
		err(1, "fork");
		break;
	default:
		if (wait(&status) <= 0) {
			warn("wait");
			rval |= 1;
		} else if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: signal %d\n",
					file, WTERMSIG(status));
			rval |= 1;
		} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
			fprintf(stderr, "%s: exit status %d\n",
					file, WEXITSTATUS(status));
			rval |= 1;
		}
		break;
	case 0:
		rval |= execl(file, file, NULL) != 0;
		perror(file);
		_exit(1);
	}
	return rval;
}
