/*	$NetBSD: yplib.c,v 1.22.4.2 1997/02/03 05:01:23 rat Exp $	 */

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
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: yplib.c,v 1.22.4.2 1997/02/03 05:01:23 rat Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#define BINDINGDIR	"/var/yp/binding"
#define YPBINDLOCK	"/var/run/ypbind.lock"

struct dom_binding *_ypbindlist;
char _yp_domain[MAXHOSTNAMELEN];

#define YPLIB_TIMEOUT		10
#define YPLIB_RPC_RETRIES	4

struct timeval _yplib_timeout = { YPLIB_TIMEOUT, 0 };
struct timeval _yplib_rpc_timeout = { YPLIB_TIMEOUT / YPLIB_RPC_RETRIES,
	1000000 * (YPLIB_TIMEOUT % YPLIB_RPC_RETRIES) / YPLIB_RPC_RETRIES };
int _yplib_nerrs = 5;

void _yp_unbind __P((struct dom_binding *));

int
_yp_dobind(dom, ypdb)
	const char     *dom;
	struct dom_binding **ypdb;
{
	static int      pid = -1;
	char            path[MAXPATHLEN];
	struct dom_binding *ysd, *ysd2;
	struct ypbind_resp ypbr;
	struct sockaddr_in clnt_sin;
	int             clnt_sock, fd, gpid;
	CLIENT         *client;
	int             new = 0, r;
	int             nerrs = 0;

	if (dom == NULL || *dom == 0)
		return YPERR_BADARGS;

	/*
	 * test if YP is running or not
	 */
	if ((fd = open(YPBINDLOCK, O_RDONLY)) == -1)
		return YPERR_YPBIND;
	if (!(flock(fd, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK)) {
		(void)close(fd);
		return YPERR_YPBIND;
	}
	(void)close(fd);

	gpid = getpid();
	if (!(pid == -1 || pid == gpid)) {
		ysd = _ypbindlist;
		while (ysd) {
			if (ysd->dom_client)
				clnt_destroy(ysd->dom_client);
			ysd2 = ysd->dom_pnext;
			free(ysd);
			ysd = ysd2;
		}
		_ypbindlist = NULL;
	}
	pid = gpid;

	if (ypdb != NULL)
		*ypdb = NULL;

	for (ysd = _ypbindlist; ysd; ysd = ysd->dom_pnext)
		if (strcmp(dom, ysd->dom_domain) == 0)
			break;
	if (ysd == NULL) {
		if ((ysd = malloc(sizeof *ysd)) == NULL)
			return YPERR_YPERR;
		(void)memset(ysd, 0, sizeof *ysd);
		ysd->dom_socket = -1;
		ysd->dom_vers = 0;
		new = 1;
	}
again:
	if (ysd->dom_vers == 0) {
		(void) snprintf(path, sizeof(path), "%s/%s.%d",
				BINDINGDIR, dom, 2);
		if ((fd = open(path, O_RDONLY)) == -1) {
			/*
			 * no binding file, YP is dead, or not yet fully
			 * alive.
			 */
			goto trynet;
		}
		if (flock(fd, LOCK_EX | LOCK_NB) == -1 &&
		    errno == EWOULDBLOCK) {
			struct iovec    iov[2];
			struct ypbind_resp ybr;
			u_short         ypb_port;
			struct ypbind_binding *bn;

			iov[0].iov_base = (caddr_t) & ypb_port;
			iov[0].iov_len = sizeof ypb_port;
			iov[1].iov_base = (caddr_t) & ybr;
			iov[1].iov_len = sizeof ybr;

			r = readv(fd, iov, 2);
			if (r != iov[0].iov_len + iov[1].iov_len) {
				(void)close(fd);
				ysd->dom_vers = -1;
				goto again;
			}
			(void)memset(&ysd->dom_server_addr, 0,
				     sizeof ysd->dom_server_addr);
			ysd->dom_server_addr.sin_len =
				sizeof(struct sockaddr_in);
			ysd->dom_server_addr.sin_family = AF_INET;
			bn = &ybr.ypbind_respbody.ypbind_bindinfo;
			ysd->dom_server_addr.sin_port =
				bn->ypbind_binding_port;
				
			ysd->dom_server_addr.sin_addr =
				bn->ypbind_binding_addr;

			ysd->dom_server_port = ysd->dom_server_addr.sin_port;
			(void)close(fd);
			goto gotit;
		} else {
			/* no lock on binding file, YP is dead. */
			(void)close(fd);
			if (new)
				free(ysd);
			return YPERR_YPBIND;
		}
	}
trynet:
	if (ysd->dom_vers == -1 || ysd->dom_vers == 0) {
		struct ypbind_binding *bn;
		(void)memset(&clnt_sin, 0, sizeof clnt_sin);
		clnt_sin.sin_len = sizeof(struct sockaddr_in);
		clnt_sin.sin_family = AF_INET;
		clnt_sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		clnt_sock = RPC_ANYSOCK;
		client = clnttcp_create(&clnt_sin, YPBINDPROG, YPBINDVERS,
					&clnt_sock, 0, 0);
		if (client == NULL) {
			clnt_pcreateerror("clnttcp_create");
			if (new)
				free(ysd);
			return YPERR_YPBIND;
		}
		r = clnt_call(client, YPBINDPROC_DOMAIN,
		    xdr_ypdomain_wrap_string, &dom, xdr_ypbind_resp,
		    &ypbr, _yplib_timeout);
		if (r != RPC_SUCCESS) {
			if (new == 0 && ++nerrs == _yplib_nerrs) {
				nerrs = 0;
				fprintf(stderr,
		    "YP server for domain %s not responding, still trying\n",
				    dom);
			}
			clnt_destroy(client);
			ysd->dom_vers = -1;
			goto again;
		}
		clnt_destroy(client);

		(void)memset(&ysd->dom_server_addr, 0, 
			     sizeof ysd->dom_server_addr);
		ysd->dom_server_addr.sin_len = sizeof(struct sockaddr_in);
		ysd->dom_server_addr.sin_family = AF_INET;
		bn = &ypbr.ypbind_respbody.ypbind_bindinfo;
		ysd->dom_server_addr.sin_port =
			bn->ypbind_binding_port;
		ysd->dom_server_addr.sin_addr.s_addr =
			bn->ypbind_binding_addr.s_addr;
		ysd->dom_server_port =
			bn->ypbind_binding_port;
gotit:
		ysd->dom_vers = YPVERS;
		(void)strcpy(ysd->dom_domain, dom);
	}
	if (ysd->dom_client)
		clnt_destroy(ysd->dom_client);
	ysd->dom_socket = RPC_ANYSOCK;
	ysd->dom_client = clntudp_create(&ysd->dom_server_addr,
	    YPPROG, YPVERS, _yplib_rpc_timeout, &ysd->dom_socket);
	if (ysd->dom_client == NULL) {
		clnt_pcreateerror("clntudp_create");
		ysd->dom_vers = -1;
		goto again;
	}
	if (fcntl(ysd->dom_socket, F_SETFD, 1) == -1)
		perror("fcntl: F_SETFD");

	if (new) {
		ysd->dom_pnext = _ypbindlist;
		_ypbindlist = ysd;
	}
	if (ypdb != NULL)
		*ypdb = ysd;
	return 0;
}

void
_yp_unbind(ypb)
	struct dom_binding *ypb;
{
	clnt_destroy(ypb->dom_client);
	ypb->dom_client = NULL;
	ypb->dom_socket = -1;
}

int
yp_bind(dom)
	const char     *dom;
{
	return _yp_dobind(dom, NULL);
}

void
yp_unbind(dom)
	const char     *dom;
{
	struct dom_binding *ypb, *ypbp;

	ypbp = NULL;
	for (ypb = _ypbindlist; ypb; ypb = ypb->dom_pnext) {
		if (strcmp(dom, ypb->dom_domain) == 0) {
			clnt_destroy(ypb->dom_client);
			if (ypbp)
				ypbp->dom_pnext = ypb->dom_pnext;
			else
				_ypbindlist = ypb->dom_pnext;
			free(ypb);
			return;
		}
		ypbp = ypb;
	}
	return;
}

int
yp_get_default_domain(domp)
	char          **domp;
{
	*domp = NULL;
	if (_yp_domain[0] == '\0')
		if (getdomainname(_yp_domain, sizeof _yp_domain))
			return YPERR_NODOM;
	*domp = _yp_domain;
	return 0;
}

int
_yp_check(dom)
	char          **dom;
{
	char           *unused;

	if (_yp_domain[0] == '\0')
		if (yp_get_default_domain(&unused))
			return 0;

	if (dom)
		*dom = _yp_domain;

	if (yp_bind(_yp_domain) == 0)
		return 1;
	return 0;
}
