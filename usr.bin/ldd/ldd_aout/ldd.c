/*	$NetBSD: ldd.c,v 1.19.32.1 2008/05/18 12:36:06 yamt Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <a.out.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	main __P((int, char *[]));
void	usage __P((void));

void
usage()
{

	fprintf(stderr, "Usage: %s [-f <format 1>] [-f <format 2>] <filename>"
		" ...\n", getprogname());
	exit(1);
}

int
main(argc, argv)
int	argc;
char	*argv[];
{
	char		*fmt1 = NULL, *fmt2 = NULL;
	int		rval;
	int		c;

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

	/* ld.so magic */
	setenv("LD_TRACE_LOADED_OBJECTS", "", 1);
	if (fmt1)
		setenv("LD_TRACE_LOADED_OBJECTS_FMT1", fmt1, 1);
	if (fmt2)
		setenv("LD_TRACE_LOADED_OBJECTS_FMT2", fmt2, 1);

	rval = 0;
	while (argc--) {
		int	fd;
		struct exec hdr;
		int	status;

		if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
			warn("%s", *argv);
			rval |= 1;
			argv++;
			continue;
		}
		if (read(fd, &hdr, sizeof hdr) != sizeof hdr
		    || (N_GETFLAG(hdr) & EX_DPMASK) != EX_DYNAMIC
#if 1 /* Compatibility */
		    || hdr.a_entry < N_PAGSIZ(hdr)
#endif
		    ) {
			warnx("%s: not a dynamic executable", *argv);
			(void)close(fd);
			rval |= 1;
			argv++;
			continue;
		}
		(void)close(fd);

		setenv("LD_TRACE_LOADED_OBJECTS_PROGNAME", *argv, 1);
		if (fmt1 == NULL && fmt2 == NULL)
			/* Default formats */
			printf("%s:\n", *argv);
		fflush(stdout);

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
						*argv, WTERMSIG(status));
				rval |= 1;
			} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
				fprintf(stderr, "%s: exit status %d\n",
						*argv, WEXITSTATUS(status));
				rval |= 1;
			}
			break;
		case 0:
			rval |= execl(*argv, *argv, NULL) != 0;
			perror(*argv);
			_exit(1);
		}
		argv++;
	}

	return rval;
}
