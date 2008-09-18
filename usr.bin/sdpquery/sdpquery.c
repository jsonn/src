/*	$NetBSD: sdpquery.c,v 1.3.12.1 2008/09/18 04:29:20 wrstuden Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2006 Itronix, Inc.  All rights reserved.");
__RCSID("$NetBSD: sdpquery.c,v 1.3.12.1 2008/09/18 04:29:20 wrstuden Exp $");

#include <assert.h>
#include <bluetooth.h>
#include <err.h>
#include <errno.h>
#include <sdp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sdpquery.h"

static void usage(void);

const char *control_socket;

static struct command {
	const char	*command;
	int		(*handler)(bdaddr_t *, bdaddr_t *, int, char const **);
	const char	*usage;
} commands[] = {
	{ "Browse",	do_sdp_browse,	"[UUID]"	},
	{ "Search",	do_sdp_search,	"<service>"	},
	{ NULL,		NULL,		NULL		}
};

int
main(int argc, char *argv[])
{
	bdaddr_t	laddr, raddr;
	struct command *cmd;
	int		ch, local;

	bdaddr_copy(&laddr, BDADDR_ANY);
	bdaddr_copy(&raddr, BDADDR_ANY);
	control_socket = NULL;
	local = 0;

	while ((ch = getopt(argc, argv, "a:c:d:hl")) != -1) {
		switch (ch) {
		case 'a': /* remote address */
			if (!bt_aton(optarg, &raddr)) {
				struct hostent  *he = NULL;

				if ((he = bt_gethostbyname(optarg)) == NULL)
					errx(EXIT_FAILURE, "%s: %s",
						optarg, hstrerror(h_errno));

				bdaddr_copy(&raddr, (bdaddr_t *)he->h_addr);
			}
			break;

		case 'c':
			control_socket = optarg;
			break;

		case 'd': /* local device address */
			if (!bt_devaddr(optarg, &laddr))
				err(EXIT_FAILURE, "%s", optarg);

			break;

		case 'l': /* local sdpd */
			local = 1;
			break;

		case 'h':
		default:
			usage();
			/* NOT REACHED */
		}
	}

	argc -= optind;
	argv += optind;

	optind = 0;
	optreset = 1;

	if (argc < 1
	    || (bdaddr_any(&raddr) && !local)
	    || (!bdaddr_any(&raddr) && local))
		usage();

	for (cmd = commands ; cmd->command != NULL; cmd++) {
		if (strcasecmp(*argv, cmd->command) == 0)
			return (*cmd->handler)(&laddr, &raddr, --argc, (char const **)++argv);
	}

	errx(EXIT_FAILURE, "%s: Unknown Command", *argv);
}

/* Usage */
static void
usage(void)
{
	struct command *cmd;

	fprintf(stderr,
		"Usage: %s [-d device] -a bdaddr <command> [parameters..]\n"
		"       %s [-c path] -l <command> [parameters..]\n"
		"\n", getprogname(), getprogname());

	fprintf(stderr,
		"Where:\n"
		"\t-a bdaddr    remote address\n"
		"\t-c path      path to control socket\n"
		"\t-d device    local device address\n"
		"\t-l           connect to the local SDP server via control socket\n"
		"\t-h           display usage and quit\n"
		"\n"
		"Commands:\n");

	for (cmd = commands ; cmd->command != NULL ; cmd++)
		fprintf(stderr, "\t%-13s%s\n", cmd->command, cmd->usage);

	exit(EXIT_FAILURE);
}
