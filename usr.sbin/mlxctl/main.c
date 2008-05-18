/*	$NetBSD: main.c,v 1.6.10.1 2008/05/18 12:36:20 yamt Exp $	*/

/*-
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#ifndef lint
#include <sys/cdefs.h>
__RCSID("$NetBSD: main.c,v 1.6.10.1 2008/05/18 12:36:20 yamt Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <dev/ic/mlxreg.h>
#include <dev/ic/mlxio.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

const char	*mlxname;
const char	*memf;
const char	*nlistf;
struct	mlx_cinfo ci;
int	mlxfd = -1;
int	verbosity;

struct cmd {
	const char	*label;
	int	flags;
	int	(*func)(char **);
};
#define	CMD_DISKS	0x01
#define	CMD_NOARGS	0x02

static const struct cmd cmdtab[] = {
	{ "check",	CMD_DISKS,	cmd_check },
	{ "config",	CMD_NOARGS,	cmd_config },
	{ "cstatus",	CMD_NOARGS,	cmd_cstatus },
	{ "detach",	CMD_DISKS,	cmd_detach },
	{ "rebuild",	0,		cmd_rebuild },
	{ "rescan",	CMD_NOARGS,	cmd_rescan },
	{ "status",	CMD_DISKS,	cmd_status },
};

int
main(int argc, char **argv)
{
	const struct cmd *cmd, *maxcmd;
	const char *cmdname, *dv;
	int ch, i, rv, all;

	all = 0;
	dv = "/dev/mlx0";
	mlx_disk_init();

	while ((ch = getopt(argc, argv, "af:v")) != -1) {
		switch (ch) {
		case 'a':
			all = 1;
			break;

		case 'f':
			dv = optarg;
			break;

		case 'v':
			verbosity++;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}

	if ((cmdname = argv[optind++]) == NULL)
		usage();

	for (i = 0; dv[i] != '\0'; i++)
		if (dv[i] == '/')
			mlxname = &dv[i + 1];

	if ((mlxfd = open(dv, O_RDWR)) < 0)
		err(EXIT_FAILURE, "%s", dv);

	cmd = &cmdtab[0];
	maxcmd = &cmdtab[sizeof(cmdtab) / sizeof(cmdtab[0])];
	while (cmd < maxcmd) {
		if (strcmp(cmdname, cmd->label) == 0)
			break;
		cmd++;
	}
	if (cmd == maxcmd)
		usage();

	if (ioctl(mlxfd, MLX_GET_CINFO, &ci))
		err(EXIT_FAILURE, "ioctl(MLX_GET_CINFO)");

	if ((cmd->flags & CMD_DISKS) != 0) {
		if (all)
			mlx_disk_add_all();

		while (argv[optind] != NULL)
			mlx_disk_add(argv[optind++]);
		if (mlx_disk_empty())
			errx(EXIT_FAILURE,
			    "no logical drives attached to this controller");
	} else if ((cmd->flags & CMD_NOARGS) != 0)
		if (argv[optind] != NULL)
			usage();

	rv = (*cmd->func)(&argv[optind]);
	close(mlxfd);
	exit(rv);
	/* NOTREACHED */
}

void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-f dev] [-av] command [...]\n",
	    getprogname());
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
