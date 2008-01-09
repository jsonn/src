/*	$NetBSD: pnullfs.c,v 1.13.4.2 2008/01/09 01:38:59 matt Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * pnullfs: puffs nullfs example
 */

#include <err.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(void);

static void
usage()
{

	errx(1, "usage: %s [-s]�[-o mntopts]�nullpath mountpath",
	    getprogname());
}

int
main(int argc, char *argv[])
{
	struct puffs_usermount *pu;
	struct puffs_ops *pops;
	struct puffs_pathobj *po_root;
	struct puffs_node *pn_root;
	struct stat sb;
	mntoptparse_t mp;
	int mntflags, pflags;
	int ch;
	int detach;

	setprogname(argv[0]);

	if (argc < 3)
		usage();

	pflags = mntflags = 0;
	detach = 1;
	while ((ch = getopt(argc, argv, "o:s")) != -1) {
		switch (ch) {
		case 'o':
			mp = getmntopts(optarg, puffsmopts, &mntflags, &pflags);
			if (mp == NULL)
				err(1, "getmntopts");
			freemntopts(mp);
			break;
		case 's':
			detach = 0;
			break;
		}
	}
	pflags |= PUFFS_FLAG_BUILDPATH;
	argv += optind;
	argc -= optind;

	if (pflags & PUFFS_FLAG_OPDUMP)
		detach = 0;

	if (argc != 2)
		usage();

	if (lstat(argv[0], &sb) == -1)
		err(1, "stat %s", argv[0]);
	if ((sb.st_mode & S_IFDIR) == 0)
		errx(1, "%s is not a directory", argv[0]);

	PUFFSOP_INIT(pops);
	puffs_null_setops(pops);

	if ((pu = puffs_init(pops, argv[0], "pnullfs", NULL, pflags)) == NULL)
		err(1, "init");

	pn_root = puffs_pn_new(pu, NULL);
	if (pn_root == NULL)
		err(1, "puffs_pn_new");
	puffs_setroot(pu, pn_root);

	po_root = puffs_getrootpathobj(pu);
	if (po_root == NULL)
		err(1, "getrootpathobj");
	po_root->po_path = argv[0];
	po_root->po_len = strlen(argv[0]);
	puffs_stat2vattr(&pn_root->pn_va, &sb);

	if (detach)
		if (puffs_daemon(pu, 1, 1) == -1)
			err(1, "puffs_daemon");

	if (puffs_mount(pu, argv[1], mntflags, pn_root) == -1)
		err(1, "puffs_mount");
	if (puffs_mainloop(pu) == -1)
		err(1, "mainloop");

	return 0;
}
