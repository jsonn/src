/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
 * Copyright (c) 1992, 1993 John Brezak
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
 *	This product includes software developed by Theo de Raadt and
 *	John Brezak.
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

#ifndef lint
static char rcsid[] = "$Id: yppoll.c,v 1.4.2.2 1994/08/23 17:03:16 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

void
usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\typpoll [-h host] [-d domainname] mapname\n");
	exit(1);
}

int
get_remote_info(indomain, inmap, server, outorder, outname)
	char *indomain;
	char *inmap;
	char *server;
	int *outorder;
	char **outname;
{
	struct ypresp_order ypro;
	struct ypresp_master yprm;
	struct ypreq_nokey yprnk;
	struct timeval tv;
	int r;
	struct sockaddr_in rsrv_sin;
	int rsrv_sock;
	CLIENT *client;
	struct hostent *h;

	bzero((char *)&rsrv_sin, sizeof rsrv_sin);
	rsrv_sin.sin_len = sizeof rsrv_sin;
	rsrv_sin.sin_family = AF_INET;
	rsrv_sock = RPC_ANYSOCK;

	h = gethostbyname(server);
	if (h == NULL) {
		if (inet_aton(server, &rsrv_sin.sin_addr) == 0) {
			fprintf(stderr, "unknown host %s\n", server);
			exit(1);
		}
	} else {
		rsrv_sin.sin_addr.s_addr = *(u_long *)h->h_addr;
	}

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	client = clntudp_create(&rsrv_sin, YPPROG, YPVERS, tv, &rsrv_sock);
	if (client == NULL) {
		fprintf(stderr, "clntudp_create: no contact with host %s.\n",
		    server);
		exit(1);
	}
	
	yprnk.domain = indomain;
	yprnk.map = inmap;

	bzero((char *)(char *)&ypro, sizeof ypro);

	r = clnt_call(client, YPPROC_ORDER, xdr_ypreq_nokey, &yprnk,
	    xdr_ypresp_order, &ypro, tv);
	if (r != RPC_SUCCESS)
		clnt_perror(client, "yp_order: clnt_call");

	*outorder = ypro.ordernum;
	xdr_free(xdr_ypresp_order, (char *)&ypro);

	r = ypprot_err(ypro.status);
	if (r == RPC_SUCCESS) {
		bzero((char *)&yprm, sizeof yprm);

		r = clnt_call(client, YPPROC_MASTER, xdr_ypreq_nokey,
		    &yprnk, xdr_ypresp_master, &yprm, tv);
		if (r != RPC_SUCCESS)
			clnt_perror(client, "yp_master: clnt_call");
		r = ypprot_err(yprm.status);
		if (r==0)
			*outname = (char *)strdup(yprm.master);
		xdr_free(xdr_ypresp_master, (char *)&yprm);
	}
	clnt_destroy(client);
	return r;
}

int
main(argc, argv)
	int  argc;
	char **argv;
{
	char *domainname;
	char *hostname = NULL;
	char *inmap, *master;
	int order;
	extern char *optarg;
	extern int optind;
	int c, r;

	yp_get_default_domain(&domainname);

	while ((c=getopt(argc, argv, "h:d:?")) != -1)
		switch (c) {
		case 'd':
			domainname = optarg;
			break;
		case 'h':
			hostname = optarg;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}

	if (optind + 1 != argc )
		usage();
	inmap = argv[optind];

	if (hostname != NULL) {
		r = get_remote_info(domainname, inmap, hostname,
		    &order, &master);
	} else {
		r = yp_order(domainname, inmap, &order);
		if (r == 0)
			r = yp_master(domainname, inmap, &master);
	}

	if (r != 0) {
		fprintf(stderr, "No such map %s. Reason: %s\n",
		    inmap, yperr_string(r));
		exit(1);
	}

	printf("Map %s has order number %d. %s", inmap, order,
	    ctime((time_t *)&order));
	printf("The master server is %s.\n", master);
	exit(0);
}
