/*	$NetBSD: getconf.c,v 1.14.2.1 2002/08/05 07:08:05 lukem Exp $	*/

/*-
 * Copyright (c) 1996, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: getconf.c,v 1.14.2.1 2002/08/05 07:08:05 lukem Exp $");
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int	main __P((int, char **));
static void usage __P((void));

struct conf_variable
{
  const char *name;
  enum { SYSCONF, CONFSTR, PATHCONF, CONSTANT } type;
  long value;
};

const struct conf_variable conf_table[] =
{
  { "PATH",			CONFSTR,	_CS_PATH		},

  /* Utility Limit Minimum Values */
  { "POSIX2_BC_BASE_MAX",	CONSTANT,	_POSIX2_BC_BASE_MAX	},
  { "POSIX2_BC_DIM_MAX",	CONSTANT,	_POSIX2_BC_DIM_MAX	},
  { "POSIX2_BC_SCALE_MAX",	CONSTANT,	_POSIX2_BC_SCALE_MAX	},
  { "POSIX2_BC_STRING_MAX",	CONSTANT,	_POSIX2_BC_STRING_MAX	},
  { "POSIX2_COLL_WEIGHTS_MAX",	CONSTANT,	_POSIX2_COLL_WEIGHTS_MAX },
  { "POSIX2_EXPR_NEST_MAX",	CONSTANT,	_POSIX2_EXPR_NEST_MAX	},
  { "POSIX2_LINE_MAX",		CONSTANT,	_POSIX2_LINE_MAX	},
  { "POSIX2_RE_DUP_MAX",	CONSTANT,	_POSIX2_RE_DUP_MAX	},
  { "POSIX2_VERSION",		CONSTANT,	_POSIX2_VERSION		},

  /* POSIX.1 Minimum Values */
  { "_POSIX_ARG_MAX",		CONSTANT,	_POSIX_ARG_MAX		},
  { "_POSIX_CHILD_MAX",		CONSTANT,	_POSIX_CHILD_MAX	},
  { "_POSIX_LINK_MAX",		CONSTANT,	_POSIX_LINK_MAX		},
  { "_POSIX_MAX_CANON",		CONSTANT,	_POSIX_MAX_CANON	},
  { "_POSIX_MAX_INPUT",		CONSTANT,	_POSIX_MAX_INPUT	},
  { "_POSIX_NAME_MAX",		CONSTANT,	_POSIX_NAME_MAX		},
  { "_POSIX_NGROUPS_MAX",	CONSTANT,	_POSIX_NGROUPS_MAX	},
  { "_POSIX_OPEN_MAX",		CONSTANT,	_POSIX_OPEN_MAX		},
  { "_POSIX_PATH_MAX",		CONSTANT,	_POSIX_PATH_BUF		},
  { "_POSIX_PIPE_BUF",		CONSTANT,	_POSIX_PIPE_BUF		},
  { "_POSIX_SSIZE_MAX",		CONSTANT,	_POSIX_SSIZE_MAX	},
  { "_POSIX_STREAM_MAX",	CONSTANT,	_POSIX_STREAM_MAX	},
  { "_POSIX_TZNAME_MAX",	CONSTANT,	_POSIX_TZNAME_MAX	},

  /* Symbolic Utility Limits */
  { "BC_BASE_MAX",		SYSCONF,	_SC_BC_BASE_MAX		},
  { "BC_DIM_MAX",		SYSCONF,	_SC_BC_DIM_MAX		},
  { "BC_SCALE_MAX",		SYSCONF,	_SC_BC_SCALE_MAX	},
  { "BC_STRING_MAX",		SYSCONF,	_SC_BC_STRING_MAX	},
  { "COLL_WEIGHTS_MAX",		SYSCONF,	_SC_COLL_WEIGHTS_MAX	},
  { "EXPR_NEST_MAX",		SYSCONF,	_SC_EXPR_NEST_MAX	},
  { "LINE_MAX",			SYSCONF,	_SC_LINE_MAX		},
  { "RE_DUP_MAX",		SYSCONF,	_SC_RE_DUP_MAX		},

  /* Optional Facility Configuration Values */
  { "POSIX2_C_BIND",		SYSCONF,	_SC_2_C_BIND		},
  { "POSIX2_C_DEV",		SYSCONF,	_SC_2_C_DEV		},
  { "POSIX2_CHAR_TERM",		SYSCONF,	_SC_2_CHAR_TERM		},
  { "POSIX2_FORT_DEV",		SYSCONF,	_SC_2_FORT_DEV		},
  { "POSIX2_FORT_RUN",		SYSCONF,	_SC_2_FORT_RUN		},
  { "POSIX2_LOCALEDEF",		SYSCONF,	_SC_2_LOCALEDEF		},
  { "POSIX2_SW_DEV",		SYSCONF,	_SC_2_SW_DEV		},
  { "POSIX2_UPE",		SYSCONF,	_SC_2_UPE		},

  /* POSIX.1 Configurable System Variables */
  { "ARG_MAX",			SYSCONF,	_SC_ARG_MAX 		},
  { "CHILD_MAX",		SYSCONF,	_SC_CHILD_MAX		},
  { "CLK_TCK",			SYSCONF,	_SC_CLK_TCK		},
  { "NGROUPS_MAX",		SYSCONF,	_SC_NGROUPS_MAX		},
  { "OPEN_MAX",			SYSCONF,	_SC_OPEN_MAX		},
  { "STREAM_MAX",		SYSCONF,	_SC_STREAM_MAX		},
  { "TZNAME_MAX",		SYSCONF,	_SC_TZNAME_MAX		},
  { "_POSIX_JOB_CONTROL",	SYSCONF,	_SC_JOB_CONTROL 	},
  { "_POSIX_SAVED_IDS",		SYSCONF,	_SC_SAVED_IDS		},
  { "_POSIX_VERSION",		SYSCONF,	_SC_VERSION		},

  { "LINK_MAX",			PATHCONF,	_PC_LINK_MAX		},
  { "MAX_CANON",		PATHCONF,	_PC_MAX_CANON		},
  { "MAX_INPUT",		PATHCONF,	_PC_MAX_INPUT		},
  { "NAME_MAX",			PATHCONF,	_PC_NAME_MAX		},
  { "PATH_MAX",			PATHCONF,	_PC_PATH_MAX		},
  { "PIPE_BUF",			PATHCONF,	_PC_PIPE_BUF		},
  { "_POSIX_CHOWN_RESTRICTED",	PATHCONF,	_PC_CHOWN_RESTRICTED	},
  { "_POSIX_NO_TRUNC",		PATHCONF,	_PC_NO_TRUNC		},
  { "_POSIX_VDISABLE",		PATHCONF,	_PC_VDISABLE		},

  /* POSIX.1b Configurable System Variables */
  { "PAGESIZE",			SYSCONF,	_SC_PAGESIZE		},
  { "_POSIX_FSYNC",		SYSCONF,	_SC_FSYNC		},
  { "_POSIX_MAPPED_FILES",	SYSCONF,	_SC_MAPPED_FILES	},
  { "_POSIX_MEMLOCK",		SYSCONF,	_SC_MEMLOCK		},
  { "_POSIX_MEMLOCK_RANGE",	SYSCONF,	_SC_MEMLOCK_RANGE	},
  { "_POSIX_MEMORY_PROTECTION",	SYSCONF,	_SC_MEMORY_PROTECTION	},
  { "_POSIX_MONOTONIC_CLOCK",	SYSCONF,	_SC_MONOTONIC_CLOCK	},
  { "_POSIX_SYNCHRONIZED_IO",	SYSCONF,	_SC_SYNCHRONIZED_IO	},

  { "_POSIX_SYNC_IO",		PATHCONF,	_PC_SYNC_IO		},

  /* POSIX.1c Configurable System Variables */
  { "LOGIN_NAME_MAX",		SYSCONF,	_SC_LOGIN_NAME_MAX	},

  /* XPG4.2 Configurable System Variables */
  { "IOV_MAX",			SYSCONF,	_SC_IOV_MAX		},
  { "PAGE_SIZE",		SYSCONF,	_SC_PAGE_SIZE		},
  { "_XOPEN_SHM",		SYSCONF,	_SC_XOPEN_SHM		},

  /* X/Open CAE Spec. Issue 5 Version 2 Configurable System Variables */
  { "FILESIZEBITS",		PATHCONF,	_PC_FILESIZEBITS	},
  { NULL }
};


int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	const struct conf_variable *cp;

	long val;
	size_t slen;
	char * sval;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1 || argc > 2) {
		usage();
		/* NOTREACHED */
	}

	for (cp = conf_table; cp->name != NULL; cp++) {
		if (strcmp(*argv, cp->name) == 0)
			break;
	}
	if (cp->name == NULL) {
		errx(1, "%s: unknown variable", *argv);
		/* NOTREACHED */
	}

	if (cp->type == PATHCONF) {
		if (argc != 2) usage();
	} else {
		if (argc != 1) usage();
	}

	switch (cp->type) {
	case CONSTANT:
		printf("%ld\n", cp->value);
		break;

	case CONFSTR:
		slen = confstr (cp->value, (char *) 0, (size_t) 0);

		if ((sval = malloc(slen)) == NULL)
			err(1, "malloc");

		confstr(cp->value, sval, slen);
		printf("%s\n", sval);
		break;

	case SYSCONF:
		errno = 0;
		if ((val = sysconf(cp->value)) == -1) {
			if (errno != 0) {
				err(1, "malloc");
				/* NOTREACHED */
			}

			printf ("undefined\n");
		} else {
			printf("%ld\n", val);
		}
		break;

	case PATHCONF:
		errno = 0;
		if ((val = pathconf(argv[1], cp->value)) == -1) {
			if (errno != 0) {
				err(1, "%s", argv[1]);
				/* NOTREACHED */
			}

			printf ("undefined\n");
		} else {
			printf ("%ld\n", val);
		}
		break;
	}

	exit (ferror(stdout));
}


static void
usage()
{
  fprintf (stderr, "usage: getconf system_var\n");
  fprintf (stderr, "       getconf path_var pathname\n");
  exit(1);
}
