/*	$NetBSD: ypwhich.c,v 1.6.4.1 1996/05/26 06:16:46 jtc Exp $	*/

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

#ifndef LINT
static char rcsid[] = "$NetBSD: ypwhich.c,v 1.6.4.1 1996/05/26 06:16:46 jtc Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

struct ypalias {
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

usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\typwhich [-d domain] [[-t] -m [mname] | host]\n");
	fprintf(stderr, "\typwhich -x\n");
	exit(1);
}


/*
 * Like yp_bind except can query a specific host
 */
bind_host(dom, sin)
char *dom;
struct sockaddr_in *sin;
{
	struct hostent *hent = NULL;
	struct ypbind_resp ypbr;
	struct dom_binding *ysd;
	struct timeval tv;
	CLIENT *client;
	int sock, r;
	u_long ss_addr;

	sock = RPC_ANYSOCK;
	tv.tv_sec = 15;
	tv.tv_usec = 0;
	client = clntudp_create(sin, YPBINDPROG, YPBINDVERS, tv, &sock);
	if(client==NULL) {
		fprintf(stderr, "can't clntudp_create: %s\n",
			yperr_string(YPERR_YPBIND));
		return YPERR_YPBIND;
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	r = clnt_call(client, YPBINDPROC_DOMAIN,
		xdr_ypdomain_wrap_string, &dom, xdr_ypbind_resp, &ypbr, tv);
	if( r != RPC_SUCCESS) {
		fprintf(stderr, "can't clnt_call: %s\n",
			yperr_string(YPERR_YPBIND));
		clnt_destroy(client);
		return YPERR_YPBIND;
	} else {
		if (ypbr.ypbind_status != YPBIND_SUCC_VAL) {
			fprintf(stderr, "can't yp_bind: Reason: %s\n",
				yperr_string(ypbr.ypbind_status));
			clnt_destroy(client);
			return r;
		}
	}
	clnt_destroy(client);

	ss_addr = ypbr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr.s_addr;
	/*printf("%08x\n", ss_addr);*/
	hent = gethostbyaddr((char *)&ss_addr, sizeof(ss_addr), AF_INET);
	if (hent)
		printf("%s\n", hent->h_name);
	else
		printf("%s\n", inet_ntoa(ss_addr));
	return 0;
}
	
int
main(argc, argv)
char **argv;
{
	char *domainname, *master, *map;
	struct ypmaplist *ypml, *y;
	struct hostent *hent;
	struct sockaddr_in sin;
	int notrans, mode, getmap;
	int c, r, i;

	yp_get_default_domain(&domainname);

	map = NULL;
	getmap = notrans = mode = 0;
	while( (c=getopt(argc, argv, "xd:mt")) != -1)
		switch(c) {
		case 'x':
			for(i=0; i<sizeof ypaliases/sizeof ypaliases[0]; i++)
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
		case 'm':
			mode++;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if(mode==0) {
		switch(argc) {
		case 0:
			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

			if(bind_host(domainname, &sin))
				exit(1);
			break;
		case 1:
			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			if (inet_aton(argv[0], &sin.sin_addr) == 0) {
				hent = gethostbyname(argv[0]);
				if(!hent) {
					fprintf(stderr, "ypwhich: host %s unknown\n",
					    argv[0]);
					exit(1);
				}
				bcopy((char *)hent->h_addr,
					(char *)&sin.sin_addr, sizeof sin.sin_addr);
			}
			if(bind_host(domainname, &sin))
				exit(1);
			break;
		default:
			usage();
		}
		exit(0);
	}

	if( argc > 1)
		usage();

	if(argv[0]) {
		map = argv[0];
		for(i=0; (!notrans) && i<sizeof ypaliases/sizeof ypaliases[0]; i++)
			if( strcmp(map, ypaliases[i].alias) == 0)
				map = ypaliases[i].name;
		r = yp_master(domainname, map, &master);
		switch(r) {
		case 0:
			printf("%s\n", master);
			free(master);
			break;
		case YPERR_YPBIND:
			fprintf(stderr, "ypwhich: not running ypbind\n");
			exit(1);
		default:
			fprintf(stderr, "Can't find master for map %s. Reason: %s\n",
				map, yperr_string(r));
			exit(1);
		}
		exit(0);
	}

	ypml = NULL;
	r = yp_maplist(domainname, &ypml);
	switch(r) {
	case 0:
		for(y=ypml; y; ) {
			ypml = y;
			r = yp_master(domainname, ypml->ypml_name, &master);
			switch(r) {
			case 0:
				printf("%s %s\n", ypml->ypml_name, master);
				free(master);
				break;
			default:
				fprintf(stderr,
					"YP: can't find the master of %s: Reason: %s\n",
					ypml->ypml_name, yperr_string(r));
				break;
			}
			y = ypml->ypml_next;
			free(ypml);
		}
		break;
	case YPERR_YPBIND:
		fprintf(stderr, "ypwhich: not running ypbind\n");
		exit(1);
	default:
		fprintf(stderr, "Can't get map list for domain %s. Reason: %s\n",
			domainname, yperr_string(r));
		exit(1);
	}
	exit(0);
}
