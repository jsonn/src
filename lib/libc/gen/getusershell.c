/*	$NetBSD: getusershell.c,v 1.21.2.1 2002/06/21 18:18:10 nathanw Exp $	*/

/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getusershell.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getusershell.c,v 1.21.2.1 2002/06/21 18:18:10 nathanw Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/file.h>

#include <ctype.h>
#include <errno.h>
#include <nsswitch.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <unistd.h>

#ifdef HESIOD
#include <hesiod.h>
#endif
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#endif

#ifdef __weak_alias
__weak_alias(endusershell,_endusershell)
__weak_alias(getusershell,_getusershell)
__weak_alias(setusershell,_setusershell)
#endif

/*
 * Local shells should NOT be added here.  They should be added in
 * /etc/shells.
 */

static const char *const okshells[] = { _PATH_BSHELL, _PATH_CSHELL, NULL };
static const char *const *curshell;
static StringList	 *sl;

static const char *const *initshells __P((void));

/*
 * Get a list of shells from "shells" nsswitch database
 */
__aconst char *
getusershell()
{
	__aconst char *ret;

	if (curshell == NULL)
		curshell = initshells();
	/*LINTED*/
	ret = (__aconst char *)*curshell;
	if (ret != NULL)
		curshell++;
	return (ret);
}

void
endusershell()
{
	if (sl)
		sl_free(sl, 1);
	sl = NULL;
	curshell = NULL;
}

void
setusershell()
{

	curshell = initshells();
}


static int	_local_initshells __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_local_initshells(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	char	*sp, *cp;
	FILE	*fp;
	char	 line[MAXPATHLEN + 2];

	if (sl)
		sl_free(sl, 1);
	sl = sl_init();
	if (!sl)
		return (NS_UNAVAIL);

	if ((fp = fopen(_PATH_SHELLS, "r")) == NULL)
		return (NS_UNAVAIL);

	sp = cp = line;
	while (fgets(cp, MAXPATHLEN + 1, fp) != NULL) {
		while (*cp != '#' && *cp != '/' && *cp != '\0')
			cp++;
		if (*cp == '#' || *cp == '\0')
			continue;
		sp = cp;
		while (!isspace((unsigned char) *cp) && *cp != '#'
		    && *cp != '\0')
			cp++;
		*cp++ = '\0';
		if (sl_add(sl, strdup(sp)) == -1)
			return (NS_UNAVAIL);
	}
	(void)fclose(fp);
	return (NS_SUCCESS);
}

#ifdef HESIOD
static int	_dns_initshells __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_dns_initshells(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	char	  shellname[] = "shells-XXXXX";
	int	  hsindex, hpi, r;
	char	**hp;
	void	 *context;

	if (sl)
		sl_free(sl, 1);
	sl = sl_init();
	if (!sl)
		return (NS_UNAVAIL);

	r = NS_UNAVAIL;
	if (hesiod_init(&context) == -1)
		return (r);

	for (hsindex = 0; ; hsindex++) {
		snprintf(shellname, sizeof(shellname)-1, "shells-%d", hsindex);
		hp = hesiod_resolve(context, shellname, "shells");
		if (hp == NULL) {
			if (errno == ENOENT) {
				if (hsindex == 0)
					r = NS_NOTFOUND;
				else
					r = NS_SUCCESS;
			}
			break;
		} else {
			int bad = 0;

			for (hpi = 0; hp[hpi]; hpi++)
				if (sl_add(sl, hp[hpi]) == -1) {
					bad = 1;
					break;
				}
			free(hp);
			if (bad)
				break;
		}
	}
	hesiod_end(context);
	return (r);
}
#endif /* HESIOD */

#ifdef YP
static int	_nis_initshells __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_nis_initshells(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	static char *ypdomain;

	if (sl)
		sl_free(sl, 1);
	sl = sl_init();
	if (!sl)
		return (NS_UNAVAIL);

	if (ypdomain == NULL) {
		switch (yp_get_default_domain(&ypdomain)) {
		case 0:
			break;
		case YPERR_RESRC:
			return (NS_TRYAGAIN);
		default:
			return (NS_UNAVAIL);
		}
	}

	for (;;) {
		char	*ypcur = NULL;
		int	 ypcurlen = 0;	/* XXX: GCC */
		char	*key, *data;
		int	 keylen, datalen;
		int	 r;

		key = data = NULL;
		if (ypcur) {
			r = yp_next(ypdomain, "shells", ypcur, ypcurlen,
					&key, &keylen, &data, &datalen);
			free(ypcur);
			switch (r) {
			case 0:
				break;
			case YPERR_NOMORE:
				free(key);
				free(data);
				return (NS_SUCCESS);
			default:
				free(key);
				free(data);
				return (NS_UNAVAIL);
			}
			ypcur = key;
			ypcurlen = keylen;
		} else {
			if (yp_first(ypdomain, "shells", &ypcur,
				    &ypcurlen, &data, &datalen)) {
				free(data);
				return (NS_UNAVAIL);
			}
		}
		data[datalen] = '\0';		/* clear trailing \n */
		if (sl_add(sl, data) == -1)
			return (NS_UNAVAIL);
	}
}
#endif /* YP */

static const char *const *
initshells()
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_initshells, NULL)
		NS_DNS_CB(_dns_initshells, NULL)
		NS_NIS_CB(_nis_initshells, NULL)
		{ 0 }
	};
	if (sl)
		sl_free(sl, 1);
	sl = sl_init();
	if (!sl)
		goto badinitshells;

	if (nsdispatch(NULL, dtab, NSDB_SHELLS, "initshells", __nsdefaultsrc)
	    != NS_SUCCESS) {
 badinitshells:
		if (sl)
			sl_free(sl, 1);
		sl = NULL;
		return (okshells);
	}
	if (sl_add(sl, NULL) == -1)
		goto badinitshells;

	return (const char *const *)(sl->sl_str);
}
