/*	$NetBSD: rot13fs.c,v 1.16.10.1 2008/09/24 16:41:21 wrstuden Exp $	*/

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
 * rot13fs: puffs layering experiment
 * (unfinished, as is probably fairly easy to tell)
 *
 * This also demonstrates how namemod can be easily set to any
 * function which reverses itself (argument -f provides a case-flipping
 * file system).
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

PUFFSOP_PROTOS(rot13)

static void usage(void);

static void
usage()
{

	errx(1, "usage: %s [-s] [-o mntopts] rot13path mountpath",
	    getprogname());
}

static void (*flipflop)(void *, size_t);

static uint8_t tbl[256];

static void
dorot13(void *buf, size_t buflen)
{
	uint8_t *b = buf;

	while (buflen--) {
		*b = tbl[*b];
		b++;
	}
}

static void
docase(void *buf, size_t buflen)
{
	unsigned char *b = buf;

	while (buflen--) {
		if (isalpha(*b))
			*b ^= 0x20;
		b++;
	}
}

static int
rot13path(struct puffs_usermount *pu, struct puffs_pathobj *base,
	struct puffs_cn *pcn)
{

	flipflop(pcn->pcn_name, pcn->pcn_namelen);

	return 0;
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
	int detach;
	int ch;
	int i;

	setprogname(argv[0]);

	if (argc < 3)
		usage();

	flipflop = dorot13;
	pflags = mntflags = 0;
	detach = 1;
	while ((ch = getopt(argc, argv, "fo:s")) != -1) {
		switch (ch) {
		case 'f':
			flipflop = docase;
			break;
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

	PUFFSOP_SET(pops, rot13, node, readdir);
	PUFFSOP_SET(pops, rot13, node, read);
	PUFFSOP_SET(pops, rot13, node, write);

	if ((pu = puffs_init(pops, argv[0], "rot13", NULL, pflags)) == NULL)
		err(1, "mount");

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

	puffs_set_namemod(pu, rot13path);

	/* initialize rot13 tables */
	for (i = 0; i < 256; i++)
		tbl[i] = (uint8_t)i;
	for (i = 0; i <= 26; i++)
		tbl[i + 'a'] = 'a' + ((i + 13) % 26);
	for (i = 0; i < 26; i++)
		tbl[i + 'A'] = 'A' + ((i + 13) % 26);

	if (detach)
		if (puffs_daemon(pu, 1, 1) == -1)
			err(1, "puffs_daemon");

	if (puffs_mount(pu, argv[1], mntflags, pn_root) == -1)
		err(1, "puffs_mount");
	if (puffs_mainloop(pu) == -1)
		err(1, "mainloop");

	return 0;
}

int
rot13_node_readdir(struct puffs_usermount *pu, void *opc, struct dirent *dent,
	off_t *readoff, size_t *reslen, const struct puffs_cred *pcr,
	int *eofflag, off_t *cookies, size_t *ncookies)
{
	struct dirent *dp;
	size_t rl;
	int rv;

	dp = dent;
	rl = *reslen;

	rv = puffs_null_node_readdir(pu, opc, dent, readoff, reslen, pcr,
	    eofflag, cookies, ncookies);
	if (rv)
		return rv;

	while (rl > *reslen) {
		flipflop((uint8_t *)dp->d_name, dp->d_namlen);
		rl -= _DIRENT_SIZE(dp);
		dp = _DIRENT_NEXT(dp);
	}

	return 0;
}

int
rot13_node_read(struct puffs_usermount *pu, void *opc,
	uint8_t *buf, off_t offset, size_t *resid,
	const struct puffs_cred *pcr, int ioflag)
{
	uint8_t *prebuf = buf;
	size_t preres = *resid;
	int rv;

	rv = puffs_null_node_read(pu, opc, buf, offset, resid, pcr, ioflag);
	if (rv)
		return rv;

	flipflop(prebuf, preres - *resid);

	return rv;
}

int
rot13_node_write(struct puffs_usermount *pu, void *opc,
	uint8_t *buf, off_t offset, size_t *resid,
	const struct puffs_cred *pcr, int ioflag)
{

	flipflop(buf, *resid);
	return puffs_null_node_write(pu, opc, buf, offset, resid, pcr, ioflag);
}
