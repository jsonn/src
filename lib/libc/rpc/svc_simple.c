/*	$NetBSD: svc_simple.c,v 1.16.4.1 1999/12/27 18:29:43 wrstuden Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)svc_simple.c 1.18 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc_simple.c	2.2 88/08/01 4.0 RPCSRC";
#else
__RCSID("$NetBSD: svc_simple.c,v 1.16.4.1 1999/12/27 18:29:43 wrstuden Exp $");
#endif
#endif

/* 
 * svc_simple.c
 * Simplified front end to rpc.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include "namespace.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#ifdef __weak_alias
__weak_alias(registerrpc,_registerrpc);
#endif

static struct proglst {
	char *(*p_progname) __P((char [UDPMSGSIZE]));
	int  p_prognum;
	int  p_procnum;
	xdrproc_t p_inproc, p_outproc;
	struct proglst *p_nxt;
} *proglst;

static SVCXPRT *transp;
static struct proglst *pl;

static void universal __P((struct svc_req *, SVCXPRT *));

int
registerrpc(prognum, versnum, procnum, progname, inproc, outproc)
	int prognum, versnum, procnum;
	char *(*progname) __P((char [UDPMSGSIZE]));
	xdrproc_t inproc, outproc;
{
	
	if (procnum == NULLPROC) {
		warnx("can't reassign procedure number %ld", NULLPROC);
		return (-1);
	}
	if (transp == 0) {
		transp = svcudp_create(RPC_ANYSOCK);
		if (transp == NULL) {
			warnx("couldn't create an rpc server");
			return (-1);
		}
	}
	(void) pmap_unset((u_long)prognum, (u_long)versnum);
	if (!svc_register(transp, (u_long)prognum, (u_long)versnum, 
	    universal, IPPROTO_UDP)) {
	    	warnx("couldn't register prog %d vers %d", prognum, versnum);
		return (-1);
	}
	pl = (struct proglst *)malloc(sizeof(struct proglst));
	if (pl == NULL) {
		warnx("registerrpc: out of memory");
		return (-1);
	}
	pl->p_progname = progname;
	pl->p_prognum = prognum;
	pl->p_procnum = procnum;
	pl->p_inproc = inproc;
	pl->p_outproc = outproc;
	pl->p_nxt = proglst;
	proglst = pl;
	return (0);
}

static void
universal(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	int prog, proc;
	char *outdata;
	char xdrbuf[UDPMSGSIZE];
	struct proglst *plist;

	_DIAGASSERT(rqstp != NULL);
	_DIAGASSERT(transp != NULL);

	/* 
	 * enforce "procnum 0 is echo" convention
	 */
	if (rqstp->rq_proc == NULLPROC) {
		if (svc_sendreply(transp, (xdrproc_t)xdr_void, NULL) == FALSE)
			errx(1, "svc_sendreply failed");
		return;
	}
	prog = rqstp->rq_prog;
	proc = rqstp->rq_proc;
	for (plist = proglst; plist != NULL; plist = plist->p_nxt)
		if (plist->p_prognum == prog && plist->p_procnum == proc) {
			/* decode arguments into a CLEAN buffer */
			memset(xdrbuf, 0, sizeof(xdrbuf)); /* required ! */
			if (!svc_getargs(transp, plist->p_inproc, xdrbuf)) {
				svcerr_decode(transp);
				return;
			}
			outdata = (*(plist->p_progname))(xdrbuf);
			if (outdata == NULL &&
			    plist->p_outproc != (xdrproc_t)xdr_void)
				/* there was an error */
				return;
			if (!svc_sendreply(transp, plist->p_outproc, outdata))
				errx(1, "trouble replying to prog %d",
				    plist->p_prognum);
			/* free the decoded arguments */
			(void)svc_freeargs(transp, plist->p_inproc, xdrbuf);
			return;
		}
	errx(1, "never registered prog %d", prog);
}
