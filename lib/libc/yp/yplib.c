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

#ifndef LINT
static char *rcsid = "$Id: yplib.c,v 1.13.2.1 1995/05/02 19:36:57 jtc Exp $";
#endif

#include "namespace.h"
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
#define YPMATCHCACHE

extern bool_t xdr_domainname(), xdr_ypbind_resp();
extern bool_t xdr_ypreq_key(), xdr_ypresp_val();
extern bool_t xdr_ypreq_nokey(), xdr_ypresp_key_val();
extern bool_t xdr_ypresp_all(), xdr_ypresp_all_seq();
extern bool_t xdr_ypresp_master();

int (*ypresp_allfn)();
void *ypresp_data;

struct dom_binding *_ypbindlist;
static char _yp_domain[MAXHOSTNAMELEN];
int _yplib_timeout = 10;

#ifdef YPMATCHCACHE
int _yplib_cache = 5;

static struct ypmatch_ent {
	struct ypmatch_ent *next;
	char *map, *key, *val;
	int keylen, vallen;
	time_t expire_t;
} *ypmc;

static void
ypmatch_add(map, key, keylen, val, vallen)
char *map;
char *key;
int keylen;
char *val;
int vallen;
{
	struct ypmatch_ent *ep;
	time_t t;

	time(&t);

	for(ep=ypmc; ep; ep=ep->next)
		if(ep->expire_t < t)
			break;
	if(ep==NULL) {
		ep = (struct ypmatch_ent *)malloc(sizeof *ep);
		memset(ep, 0, sizeof *ep);
		if(ypmc)
			ep->next = ypmc;
		ypmc = ep;
	}

	if(ep->key)
		free(ep->key);
	if(ep->val)
		free(ep->val);

	ep->key = NULL;
	ep->val = NULL;

	ep->key = (char *)malloc(keylen);
	if(ep->key==NULL)
		return;

	ep->val = (char *)malloc(vallen);
	if(ep->key==NULL) {
		free(ep->key);
		ep->key = NULL;
		return;
	}
	ep->keylen = keylen;
	ep->vallen = vallen;

	memcpy(ep->key, key, ep->keylen);
	memcpy(ep->val, val, ep->vallen);

	if(ep->map) {
		if( strcmp(ep->map, map) ) {
			free(ep->map);
			ep->map = strdup(map);
		}
	} else {
		ep->map = strdup(map);
	}

	ep->expire_t = t + _yplib_cache;
}

static bool_t
ypmatch_find(map, key, keylen, val, vallen)
char *map;
char *key;
int keylen;
char **val;
int *vallen;
{
	struct ypmatch_ent *ep;
	time_t t;

	if(ypmc==NULL)
		return 0;

	time(&t);

	for(ep=ypmc; ep; ep=ep->next) {
		if(ep->keylen != keylen)
			continue;
		if(strcmp(ep->map, map))
			continue;
		if(memcmp(ep->key, key, keylen))
			continue;
		if(t > ep->expire_t)
			continue;

		*val = ep->val;
		*vallen = ep->vallen;
		return 1;
	}
	return 0;
}
#endif

int
_yp_dobind(dom, ypdb)
char *dom;
struct dom_binding **ypdb;
{
	static int pid = -1;
	char path[MAXPATHLEN];
	struct dom_binding *ysd, *ysd2;
	struct ypbind_resp ypbr;
	struct timeval tv;
	struct sockaddr_in clnt_sin;
	int clnt_sock, fd, gpid;
	CLIENT *client;
	int new=0, r;
	int count = 0;

	/*
	 * test if YP is running or not
	 */
	if ((fd=open(YPBINDLOCK, O_RDONLY)) == -1)
		return YPERR_YPBIND;
	if( !(flock(fd, LOCK_EX|LOCK_NB) == -1 && errno==EWOULDBLOCK)) {
		close(fd);
		return YPERR_YPBIND;
	}
	close(fd);

	gpid = getpid();
	if( !(pid==-1 || pid==gpid) ) {
		ysd = _ypbindlist;
		while(ysd) {
			if(ysd->dom_client)
				clnt_destroy(ysd->dom_client);
			ysd2 = ysd->dom_pnext;
			free(ysd);
			ysd = ysd2;
		}
		_ypbindlist = NULL;
	}
	pid = gpid;

	if(ypdb!=NULL)
		*ypdb = NULL;

	if(dom==NULL || strlen(dom)==0)
		return YPERR_BADARGS;

	for(ysd = _ypbindlist; ysd; ysd = ysd->dom_pnext)
		if( strcmp(dom, ysd->dom_domain) == 0)
			break;
	if(ysd==NULL) {
		ysd = (struct dom_binding *)malloc(sizeof *ysd);
		memset(ysd, 0, sizeof *ysd);
		ysd->dom_socket = -1;
		ysd->dom_vers = 0;
		new = 1;
	}
again:
	if(ysd->dom_vers==0) {
		sprintf(path, "%s/%s.%d", BINDINGDIR, dom, 2);
		if( (fd=open(path, O_RDONLY)) == -1) {
			/* no binding file, YP is dead, or not yet fully alive. */
			goto trynet;
		}
		if( flock(fd, LOCK_EX|LOCK_NB) == -1 && errno==EWOULDBLOCK) {
			struct iovec iov[2];
			struct ypbind_resp ybr;
			u_short	ypb_port;

			iov[0].iov_base = (caddr_t)&ypb_port;
			iov[0].iov_len = sizeof ypb_port;
			iov[1].iov_base = (caddr_t)&ybr;
			iov[1].iov_len = sizeof ybr;

			r = readv(fd, iov, 2);
			if(r != iov[0].iov_len + iov[1].iov_len) {
				close(fd);
				ysd->dom_vers = -1;
				goto again;
			}

			memset(&ysd->dom_server_addr, 0, sizeof ysd->dom_server_addr);
			ysd->dom_server_addr.sin_family = AF_INET;
			ysd->dom_server_addr.sin_len = sizeof(struct sockaddr_in);
			ysd->dom_server_addr.sin_addr =
			    ybr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr;
			ysd->dom_server_addr.sin_port =
			    ybr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_port;

			ysd->dom_server_port = ysd->dom_server_addr.sin_port;
			close(fd);
			goto gotit;
		} else {
			/* no lock on binding file, YP is dead. */
			close(fd);
			if(new)
				free(ysd);
			return YPERR_YPBIND;
		}
	}
trynet:
	if(ysd->dom_vers==-1 || ysd->dom_vers==0) {
		memset(&clnt_sin, 0, sizeof clnt_sin);
		clnt_sin.sin_family = AF_INET;
		clnt_sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		clnt_sock = RPC_ANYSOCK;
		client = clnttcp_create(&clnt_sin, YPBINDPROG, YPBINDVERS, &clnt_sock,
			0, 0);
		if(client==NULL) {
			clnt_pcreateerror("clnttcp_create");
			if(new)
				free(ysd);
			return YPERR_YPBIND;
		}

		tv.tv_sec = _yplib_timeout;
		tv.tv_usec = 0;
		r = clnt_call(client, YPBINDPROC_DOMAIN,
			xdr_domainname, dom, xdr_ypbind_resp, &ypbr, tv);
		if(r != RPC_SUCCESS) {
			if (new==0 || count)
				fprintf(stderr,
				    "YP server for domain %s not responding, still trying\n",
				    dom);
			count++;
			clnt_destroy(client);
			ysd->dom_vers = -1;
			goto again;
		}
		clnt_destroy(client);

		memset(&ysd->dom_server_addr, 0, sizeof ysd->dom_server_addr);
		ysd->dom_server_addr.sin_family = AF_INET;
		ysd->dom_server_addr.sin_port =
			ypbr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_port;
		ysd->dom_server_addr.sin_addr.s_addr =
			ypbr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr.s_addr;
		ysd->dom_server_port =
			ypbr.ypbind_respbody.ypbind_bindinfo.ypbind_binding_port;
gotit:
		ysd->dom_vers = YPVERS;
		strcpy(ysd->dom_domain, dom);
	}

	tv.tv_sec = _yplib_timeout/2;
	tv.tv_usec = 0;
	if(ysd->dom_client)
		clnt_destroy(ysd->dom_client);
	ysd->dom_socket = RPC_ANYSOCK;
	ysd->dom_client = clntudp_create(&ysd->dom_server_addr,
		YPPROG, YPVERS, tv, &ysd->dom_socket);
	if(ysd->dom_client==NULL) {
		clnt_pcreateerror("clntudp_create");
		ysd->dom_vers = -1;
		goto again;
	}
	if( fcntl(ysd->dom_socket, F_SETFD, 1) == -1)
		perror("fcntl: F_SETFD");

	if(new) {
		ysd->dom_pnext = _ypbindlist;
		_ypbindlist = ysd;
	}

	if(ypdb!=NULL)
		*ypdb = ysd;
	return 0;
}

static void
_yp_unbind(ypb)
struct dom_binding *ypb;
{
	clnt_destroy(ypb->dom_client);
	ypb->dom_client = NULL;
	ypb->dom_socket = -1;
}

int
yp_bind(dom)
char *dom;
{
	return _yp_dobind(dom, NULL);
}

void
yp_unbind(dom)
char *dom;
{
	struct dom_binding *ypb, *ypbp;

	ypbp = NULL;
	for(ypb=_ypbindlist; ypb; ypb=ypb->dom_pnext) {
		if( strcmp(dom, ypb->dom_domain) == 0) {
			clnt_destroy(ypb->dom_client);
			if(ypbp)
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
yp_match(indomain, inmap, inkey, inkeylen, outval, outvallen)
char *indomain;
char *inmap;
const char *inkey;
int inkeylen;
char **outval;
int *outvallen;
{
	struct dom_binding *ysd;
	struct ypresp_val yprv;
	struct timeval tv;
	struct ypreq_key yprk;
	int r;

	*outval = NULL;
	*outvallen = 0;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

#ifdef YPMATCHCACHE
	if( !strcmp(_yp_domain, indomain) && ypmatch_find(inmap, inkey,
	    inkeylen, &yprv.valdat.dptr, &yprv.valdat.dsize)) {
		*outvallen = yprv.valdat.dsize;
		*outval = (char *)malloc(*outvallen+1);
		memcpy(*outval, yprv.valdat.dptr, *outvallen);
		(*outval)[*outvallen] = '\0';
		return 0;
	}
#endif

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprk.domain = indomain;
	yprk.map = inmap;
	yprk.keydat.dptr = (char *)inkey;
	yprk.keydat.dsize = inkeylen;

	memset(&yprv, 0, sizeof yprv);

	r = clnt_call(ysd->dom_client, YPPROC_MATCH,
		xdr_ypreq_key, &yprk, xdr_ypresp_val, &yprv, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_match: clnt_call");
		ysd->dom_vers = -1;
		goto again;
	}
	if( !(r=ypprot_err(yprv.status)) ) {
		*outvallen = yprv.valdat.dsize;
		*outval = (char *)malloc(*outvallen+1);
		memcpy(*outval, yprv.valdat.dptr, *outvallen);
		(*outval)[*outvallen] = '\0';
#ifdef YPMATCHCACHE
		if( strcmp(_yp_domain, indomain)==0 )
			 ypmatch_add(inmap, inkey, inkeylen, *outval, *outvallen);
#endif
	}
	xdr_free(xdr_ypresp_val, (char *)&yprv);
	_yp_unbind(ysd);
	return r;
}

int
yp_get_default_domain(domp)
char **domp;
{
	*domp = NULL;
	if(_yp_domain[0] == '\0')
		if( getdomainname(_yp_domain, sizeof _yp_domain))
			return YPERR_NODOM;
	*domp = _yp_domain;
	return 0;
}

int
yp_first(indomain, inmap, outkey, outkeylen, outval, outvallen)
char *indomain;
char *inmap;
char **outkey;
int *outkeylen;
char **outval;
int *outvallen;
{
	struct ypresp_key_val yprkv;
	struct ypreq_nokey yprnk;
	struct dom_binding *ysd;
	struct timeval tv;
	int r;

	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprnk.domain = indomain;
	yprnk.map = inmap;
	memset(&yprkv, 0, sizeof yprkv);

	r = clnt_call(ysd->dom_client, YPPROC_FIRST,
		xdr_ypreq_nokey, &yprnk, xdr_ypresp_key_val, &yprkv, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_first: clnt_call");
		ysd->dom_vers = -1;
		goto again;
	}
	if( !(r=ypprot_err(yprkv.status)) ) {
		*outkeylen = yprkv.keydat.dsize;
		*outkey = (char *)malloc(*outkeylen+1);
		memcpy(*outkey, yprkv.keydat.dptr, *outkeylen);
		(*outkey)[*outkeylen] = '\0';
		*outvallen = yprkv.valdat.dsize;
		*outval = (char *)malloc(*outvallen+1);
		memcpy(*outval, yprkv.valdat.dptr, *outvallen);
		(*outval)[*outvallen] = '\0';
	}
	xdr_free(xdr_ypresp_key_val, (char *)&yprkv);
	_yp_unbind(ysd);
	return r;
}

int
yp_next(indomain, inmap, inkey, inkeylen, outkey, outkeylen, outval, outvallen)
char *indomain;
char *inmap;
char *inkey;
int inkeylen;
char **outkey;
int *outkeylen;
char **outval;
int *outvallen;
{
	struct ypresp_key_val yprkv;
	struct ypreq_key yprk;
	struct dom_binding *ysd;
	struct timeval tv;
	int r;

	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprk.domain = indomain;
	yprk.map = inmap;
	yprk.keydat.dptr = inkey;
	yprk.keydat.dsize = inkeylen;
	memset(&yprkv, 0, sizeof yprkv);

	r = clnt_call(ysd->dom_client, YPPROC_NEXT,
		xdr_ypreq_key, &yprk, xdr_ypresp_key_val, &yprkv, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_next: clnt_call");
		ysd->dom_vers = -1;
		goto again;
	}
	if( !(r=ypprot_err(yprkv.status)) ) {
		*outkeylen = yprkv.keydat.dsize;
		*outkey = (char *)malloc(*outkeylen+1);
		memcpy(*outkey, yprkv.keydat.dptr, *outkeylen);
		(*outkey)[*outkeylen] = '\0';
		*outvallen = yprkv.valdat.dsize;
		*outval = (char *)malloc(*outvallen+1);
		memcpy(*outval, yprkv.valdat.dptr, *outvallen);
		(*outval)[*outvallen] = '\0';
	}
	xdr_free(xdr_ypresp_key_val, (char *)&yprkv);
	_yp_unbind(ysd);
	return r;
}

int
yp_all(indomain, inmap, incallback)
char *indomain;
char *inmap;
struct ypall_callback *incallback;
{
	struct ypreq_nokey yprnk;
	struct dom_binding *ysd;
	struct timeval tv;
	struct sockaddr_in clnt_sin;
	CLIENT *clnt;
	u_long status;
	int clnt_sock;

	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;
	clnt_sock = RPC_ANYSOCK;
	clnt_sin = ysd->dom_server_addr;
	clnt_sin.sin_port = 0;
	clnt = clnttcp_create(&clnt_sin, YPPROG, YPVERS, &clnt_sock, 0, 0);
	if(clnt==NULL) {
		printf("clnttcp_create failed\n");
		return YPERR_PMAP;
	}

	yprnk.domain = indomain;
	yprnk.map = inmap;
	ypresp_allfn = incallback->foreach;
	ypresp_data = (void *)incallback->data;

	(void) clnt_call(clnt, YPPROC_ALL,
		xdr_ypreq_nokey, &yprnk, xdr_ypresp_all_seq, &status, tv);
	clnt_destroy(clnt);
	xdr_free(xdr_ypresp_all_seq, (char *)&status);	/* not really needed... */
	_yp_unbind(ysd);

	if(status != YP_FALSE)
		return ypprot_err(status);
	return 0;
}

int
yp_order(indomain, inmap, outorder)
char *indomain;
char *inmap;
int *outorder;
{
 	struct dom_binding *ysd;
	struct ypresp_order ypro;
	struct ypreq_nokey yprnk;
	struct timeval tv;
	int r;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprnk.domain = indomain;
	yprnk.map = inmap;

	memset(&ypro, 0, sizeof ypro);

	r = clnt_call(ysd->dom_client, YPPROC_ORDER,
		xdr_ypreq_nokey, &yprnk, xdr_ypresp_order, &ypro, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_order: clnt_call");
		ysd->dom_vers = -1;
		goto again;
	}

	*outorder = ypro.ordernum;
	xdr_free(xdr_ypresp_order, (char *)&ypro);
	_yp_unbind(ysd);
	return ypprot_err(ypro.status);
}

int
yp_master(indomain, inmap, outname)
char *indomain;
char *inmap;
char **outname;
{
	struct dom_binding *ysd;
	struct ypresp_master yprm;
	struct ypreq_nokey yprnk;
	struct timeval tv;
	int r;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprnk.domain = indomain;
	yprnk.map = inmap;

	memset(&yprm, 0, sizeof yprm);

	r = clnt_call(ysd->dom_client, YPPROC_MASTER,
		xdr_ypreq_nokey, &yprnk, xdr_ypresp_master, &yprm, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_master: clnt_call");
		ysd->dom_vers = -1;
		goto again;
	}
	if( !(r=ypprot_err(yprm.status)) ) {
		*outname = (char *)strdup(yprm.master);
	}
	xdr_free(xdr_ypresp_master, (char *)&yprm);
	_yp_unbind(ysd);
	return r;
}

yp_maplist(indomain, outmaplist)
char *indomain;
struct ypmaplist **outmaplist;
{
	struct dom_binding *ysd;
	struct ypresp_maplist ypml;
	struct timeval tv;
	int r;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	memset(&ypml, 0, sizeof ypml);

	r = clnt_call(ysd->dom_client, YPPROC_MAPLIST,
		xdr_domainname, indomain, xdr_ypresp_maplist, &ypml, tv);
	if (r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_maplist: clnt_call");
		ysd->dom_vers = -1;
		goto again;
	}
	*outmaplist = ypml.list;
	/* NO: xdr_free(xdr_ypresp_maplist, &ypml);*/
	_yp_unbind(ysd);
	return ypprot_err(ypml.status);
}

char *
yperr_string(incode)
int incode;
{
	static char err[80];

	switch(incode) {
	case 0:
		return "Success";
	case YPERR_BADARGS:
		return "Request arguments bad";
	case YPERR_RPC:
		return "RPC failure";
	case YPERR_DOMAIN:
		return "Can't bind to server which serves this domain";
	case YPERR_MAP:
		return "No such map in server's domain";
	case YPERR_KEY:
		return "No such key in map";
	case YPERR_YPERR:
		return "YP server error";
	case YPERR_RESRC:
		return "Local resource allocation failure";
	case YPERR_NOMORE:
		return "No more records in map database";
	case YPERR_PMAP:
		return "Can't communicate with portmapper";
	case YPERR_YPBIND:
		return "Can't communicate with ypbind";
	case YPERR_YPSERV:
		return "Can't communicate with ypserv";
	case YPERR_NODOM:
		return "Local domain name not set";
	case YPERR_BADDB:
		return "Server data base is bad";
	case YPERR_VERS:
		return "YP server version mismatch - server can't supply service.";
	case YPERR_ACCESS:
		return "Access violation";
	case YPERR_BUSY:
		return "Database is busy";
	}
	sprintf(err, "YP unknown error %d\n", incode);
	return err;
}

int
ypprot_err(incode)
unsigned int incode;
{
	switch(incode) {
	case YP_TRUE:
		return 0;
	case YP_FALSE:
		return YPERR_YPBIND;
	case YP_NOMORE:
		return YPERR_NOMORE;
	case YP_NOMAP:
		return YPERR_MAP;
	case YP_NODOM:
		return YPERR_NODOM;
	case YP_NOKEY:
		return YPERR_KEY;
	case YP_BADOP:
		return YPERR_YPERR;
	case YP_BADDB:
		return YPERR_BADDB;
	case YP_YPERR:
		return YPERR_YPERR;
	case YP_BADARGS:
		return YPERR_BADARGS;
	case YP_VERS:
		return YPERR_VERS;
	}
	return YPERR_YPERR;
}

int
_yp_check(dom)
char **dom;
{
	int use_yp = 0;
	char *unused;

	if( _yp_domain[0]=='\0' )
		if( yp_get_default_domain(&unused) )
			return 0;

	if(dom)
		*dom = _yp_domain;

	if( yp_bind(_yp_domain)==0 )
		return 1;
	return 0;
}
