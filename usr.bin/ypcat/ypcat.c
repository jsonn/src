/*	$NetBSD: ypcat.c,v 1.7.2.1 1997/11/28 09:21:17 mellon Exp $	*/

/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
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
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ypcat.c,v 1.7.2.1 1997/11/28 09:21:17 mellon Exp $");
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

const struct ypalias {
	char *alias, *name;
} ypaliases[] = {
	{ "passwd", "passwd.byname" },
	{ "group", "group.byname" },
	{ "networks", "networks.byaddr" },
	{ "hosts", "hosts.byaddr" },
	{ "protocols", "protocols.bynumber" },
	{ "services", "services.byname" },
	{ "aliases", "mail.aliases" },
	{ "ethers", "ethers.byname" },
};

int	main __P((int, char *[]));
int	printit __P((int, char *, int, char *, int, char *));
void	usage __P((void));

int key;

extern	char *__progname;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *domainname;
	struct ypall_callback ypcb;
	char *inmap;
	int notrans;
	int c, r, i;

	domainname = NULL;
	notrans = key = 0;
	while((c = getopt(argc, argv, "xd:kt")) != -1) {
		switch (c) {
		case 'x':
			for (i = 0;
			    i < sizeof(ypaliases)/sizeof(ypaliases[0]); i++)
				printf("Use \"%s\" for \"%s\"\n",
					ypaliases[i].alias,
					ypaliases[i].name);
			exit(0);

		case 'd':
			domainname = optarg;
			break;

		case 't':
			notrans++;
			break;

		case 'k':
			key++;
			break;

		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if (domainname == NULL)
		yp_get_default_domain(&domainname);

	inmap = argv[0];
	if (notrans == 0) {
		for (i = 0; i < sizeof(ypaliases)/sizeof(ypaliases[0]); i++)
			if (strcmp(inmap, ypaliases[i].alias) == 0)
				inmap = ypaliases[i].name;
	}

	ypcb.foreach = printit;
	ypcb.data = NULL;

	r = yp_all(domainname, inmap, &ypcb);
	switch (r) {
	case 0:
		break;

	case YPERR_YPBIND:
		errx(1, "not running ypbind");

	default:
		errx(1, "no such map %s.  Reason: %s", inmap, yperr_string(r));
	}
	exit(0);
}

int
printit(instatus, inkey, inkeylen, inval, invallen, indata)
	int instatus;
	char *inkey;
	int inkeylen;
	char *inval;
	int invallen;
	char *indata;
{

	if (instatus != YP_TRUE)
		return instatus;
	if (key)
		printf("%*.*s", inkeylen, inkeylen, inkey);
	if (invallen)
		printf("%s%*.*s", (key ? " " : ""), invallen, invallen, inval);
	printf("\n");
	return 0;
}

void
usage()
{

	fprintf(stderr, "usage: %s [-k] [-d domainname] [-t] mapname\n",
	    __progname);
	fprintf(stderr, "       %s -x\n", __progname);
	exit(1);
}
