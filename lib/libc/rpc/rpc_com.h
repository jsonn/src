/*	$NetBSD: rpc_com.h,v 1.1.4.2 2000/06/23 16:17:46 minoura Exp $	*/

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
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/*
 * rpc_com.h, Common definitions for both the server and client side.
 * All for the topmost layer of rpc
 *
 * In Sun's tirpc distribution, this was installed as <rpc/rpc_com.h>,
 * but as it contains only non-exported interfaces, it was moved here.
 */

#ifndef _RPC_RPCCOM_H
#define	_RPC_RPCCOM_H

#include <sys/cdefs.h>

/* #pragma ident	"@(#)rpc_com.h	1.11	93/07/05 SMI" */

/*
 * The max size of the transport, if the size cannot be determined
 * by other means.
 */
#define	RPC_MAXDATASIZE 9000
#define	RPC_MAXADDRSIZE 1024

__BEGIN_DECLS
extern u_int __rpc_get_a_size __P((int));
extern int __rpc_dtbsize __P((void));
extern struct netconfig * __rpcgettp __P((int));
extern  int  __rpc_get_default_domain __P((char **));
void *__rpc_setconf __P((char *));
struct netconfig *__rpc_getconf __P((void *));
struct netconfig * __rpc_getconfip __P((char *));
void __rpc_endconf __P((void *));

char *__rpc_taddr2uaddr_af __P((int, const struct netbuf *));
struct netbuf *__rpc_uaddr2taddr_af __P((int, const char *));
int __rpc_fixup_addr __P((struct netbuf *, const struct netbuf *));
int __rpc_sockinfo2netid __P((struct __rpc_sockinfo *, const char **));
int __rpc_seman2socktype __P((int));
int __rpc_socktype2seman __P((int));
void *rpc_nullproc __P((CLIENT *));
int __rpc_sockisbound __P((int));

void _seterr_reply __P((struct rpc_msg *, struct rpc_err *));

struct netbuf *__rpcb_findaddr __P((rpcprog_t, rpcvers_t,
				    const struct netconfig *,
				    const char *, CLIENT **));
bool_t __rpc_control __P((int,void *));
__END_DECLS

#endif /* _RPC_RPCCOM_H */
