/*	$NetBSD: yp_master.c,v 1.3.2.1 1996/09/17 21:21:45 jtc Exp $	 */

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
static char rcsid[] = "$NetBSD: yp_master.c,v 1.3.2.1 1996/09/17 21:21:45 jtc Exp $";
#endif

#include "namespace.h"
#include <string.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#ifdef __weak_alias
__weak_alias(yp_master,_yp_master);
#endif

extern struct timeval _yplib_timeout;
extern int _yplib_nerrs;

int
yp_master(indomain, inmap, outname)
	const char     *indomain;
	const char     *inmap;
	char          **outname;
{
	struct dom_binding *ysd;
	struct ypresp_master yprm;
	struct ypreq_nokey yprnk;
	int r, nerrs = 0;

	if (indomain == NULL || *indomain == '\0'
	    || strlen(indomain) > YPMAXDOMAIN)
		return YPERR_BADARGS;
	if (inmap == NULL || *inmap == '\0'
	    || strlen(inmap) > YPMAXMAP)
		return YPERR_BADARGS;
	if (outname == NULL)
		return YPERR_BADARGS;

again:
	if (_yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	yprnk.domain = indomain;
	yprnk.map = inmap;

	(void)memset(&yprm, 0, sizeof yprm);

	r = clnt_call(ysd->dom_client, YPPROC_MASTER,
		      xdr_ypreq_nokey, &yprnk, xdr_ypresp_master, &yprm, 
		      _yplib_timeout);
	if (r != RPC_SUCCESS) {
		if (++nerrs == _yplib_nerrs) {
			clnt_perror(ysd->dom_client, "yp_master: clnt_call");
			nerrs = 0;
		}
		ysd->dom_vers = -1;
		goto again;
	}
	if (!(r = ypprot_err(yprm.status))) {
		if ((*outname = strdup(yprm.master)) == NULL)
			r = YPERR_RESRC;
	}
	xdr_free(xdr_ypresp_master, (char *) &yprm);
	__yp_unbind(ysd);
	return r;
}
