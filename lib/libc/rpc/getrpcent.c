/*	$NetBSD: getrpcent.c,v 1.4.4.1 1996/09/16 23:44:26 jtc Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
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

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)getrpcent.c 1.14 91/03/11 Copyr 1984 Sun Micro";*/
static char *rcsid = "$NetBSD: getrpcent.c,v 1.4.4.1 1996/09/16 23:44:26 jtc Exp $";
#endif

/*
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

#include "namespace.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <rpc/rpc.h>

#ifdef __weak_alias
__weak_alias(endrpcent,_endrpcent);
__weak_alias(getrpcbyname,_getrpcbyname);
__weak_alias(getrpcbynumber,_getrpcbynumber);
__weak_alias(getrpcent,_getrpcent);
__weak_alias(setrpcent,_setrpcent);
#endif

/*
 * Internet version.
 */
struct rpcdata {
	FILE	*rpcf;
	int	stayopen;
#define	MAXALIASES	35
	char	*rpc_aliases[MAXALIASES];
	struct	rpcent rpc;
	char	line[BUFSIZ+1];
} *rpcdata;

static	struct rpcent *interpret();
struct	hostent *gethostent();
char	*inet_ntoa();

static char RPCDB[] = "/etc/rpc";

static struct rpcdata *
_rpcdata()
{
	register struct rpcdata *d = rpcdata;

	if (d == 0) {
		d = (struct rpcdata *)calloc(1, sizeof (struct rpcdata));
		rpcdata = d;
	}
	return (d);
}

struct rpcent *
getrpcbynumber(number)
	register int number;
{
	register struct rpcdata *d = _rpcdata();
	register struct rpcent *p;

	if (d == 0)
		return (0);
	setrpcent(0);
	while (p = getrpcent()) {
		if (p->r_number == number)
			break;
	}
	endrpcent();
	return (p);
}

struct rpcent *
getrpcbyname(name)
	char *name;
{
	struct rpcent *rpc;
	char **rp;

	setrpcent(0);
	while (rpc = getrpcent()) {
		if (strcmp(rpc->r_name, name) == 0)
			return (rpc);
		for (rp = rpc->r_aliases; *rp != NULL; rp++) {
			if (strcmp(*rp, name) == 0)
				return (rpc);
		}
	}
	endrpcent();
	return (NULL);
}

void
setrpcent(f)
	int f;
{
	register struct rpcdata *d = _rpcdata();

	if (d == 0)
		return;
	if (d->rpcf == NULL)
		d->rpcf = fopen(RPCDB, "r");
	else
		rewind(d->rpcf);
	d->stayopen |= f;
}

void
endrpcent()
{
	register struct rpcdata *d = _rpcdata();

	if (d == 0)
		return;
	if (d->rpcf && !d->stayopen) {
		fclose(d->rpcf);
		d->rpcf = NULL;
	}
}

struct rpcent *
getrpcent()
{
	struct rpcent *hp;
	int reason;
	register struct rpcdata *d = _rpcdata();

	if (d == 0)
		return(NULL);
	if (d->rpcf == NULL && (d->rpcf = fopen(RPCDB, "r")) == NULL)
		return (NULL);
        if (fgets(d->line, BUFSIZ, d->rpcf) == NULL)
		return (NULL);
	return (interpret(d->line, strlen(d->line)));
}

static struct rpcent *
interpret(val, len)
	char *val;
	int len;
{
	register struct rpcdata *d = _rpcdata();
	char *p;
	register char *cp, **q;

	if (d == 0)
		return (0);
	(void) strncpy(d->line, val, len);
	p = d->line;
	d->line[len] = '\n';
	if (*p == '#')
		return (getrpcent());
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		return (getrpcent());
	*cp = '\0';
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		return (getrpcent());
	*cp++ = '\0';
	/* THIS STUFF IS INTERNET SPECIFIC */
	d->rpc.r_name = d->line;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	d->rpc.r_number = atoi(cp);
	q = d->rpc.r_aliases = d->rpc_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL) 
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(d->rpc_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&d->rpc);
}

