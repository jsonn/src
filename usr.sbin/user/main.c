/* $NetBSD: main.c,v 1.1.2.2 2000/01/21 00:00:53 he Exp $ */

/*
 * Copyright (c) 1999 Alistair G. Crooks.  All rights reserved.
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
 *	This product includes software developed by Alistair G. Crooks.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usermgmt.h"

enum {
	MaxCmdWords = 2
};

/* this struct describes a command */
typedef struct cmd_t {
	int	c_wc;					/* word count */
	char	*c_word[MaxCmdWords];			/* command words */
	int	(*c_func)(int argc, char **argv);	/* called function */
} cmd_t;

/* despatch table for commands */
static cmd_t	cmds[] = {
	{	1,	{ "useradd",	NULL },		useradd		},
	{	2,	{ "user",	"add" },	useradd		},
	{	1,	{ "usermod",	NULL },		usermod		},
	{	2,	{ "user",	"mod" },	usermod		},
	{	1,	{ "userdel",	NULL },		userdel		},
	{	2,	{ "user",	"del" },	userdel		},
#ifdef EXTENSIONS
	{	1,	{ "userinfo",	NULL },		userinfo	},
	{	2,	{ "user",	"info" },	userinfo	},
#endif
	{	1,	{ "groupadd",	NULL },		groupadd	},
	{	2,	{ "group",	"add" },	groupadd	},
	{	1,	{ "groupmod",	NULL },		groupmod	},
	{	2,	{ "group",	"mod" },	groupmod	},
	{	1,	{ "groupdel",	NULL },		groupdel	},
	{	2,	{ "group",	"del" },	groupdel	},
#ifdef EXTENSIONS
	{	1,	{ "groupinfo",	NULL },		groupinfo	},
	{	2,	{ "group",	"info" },	groupinfo	},
#endif
	{	0	}
};

extern char	*__progname;

int
main(int argc, char **argv)
{
	cmd_t	*cmdp;
	int	matched;
	int	i;

	for (cmdp = cmds ; cmdp->c_wc > 0 ; cmdp++) {
		for (matched = i = 0 ; i < cmdp->c_wc && i < MaxCmdWords ; i++) {
			if (argc > i) {
				if (strcmp((i == 0) ? __progname : argv[i],
						cmdp->c_word[i]) == 0) {
					matched += 1;
				} else {
					break;
				}
			}
		}
		if (matched == cmdp->c_wc) {
			return (*cmdp->c_func)(argc - (matched - 1), argv + (matched - 1));
		}
	}
	usermgmt_usage(__progname);
	errx(EXIT_FAILURE, "Program `%s' not recognised", __progname);
	/* NOTREACHED */
}
