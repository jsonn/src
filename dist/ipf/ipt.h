/*	$NetBSD: ipt.h,v 1.2.4.2 2002/02/09 16:55:44 he Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 * Id: ipt.h,v 2.2.2.1 2001/06/26 10:43:19 darrenr Exp
 */

#ifndef	__IPT_H__
#define	__IPT_H__

#ifndef	__P
# define P_DEF
# ifdef	__STDC__
#  define	__P(x) x
# else
#  define	__P(x) ()
# endif
#endif

#include <fcntl.h>


struct	ipread	{
	int	(*r_open) __P((char *));
	int	(*r_close) __P((void));
	int	(*r_readip) __P((char *, int, char **, int *));
};

extern	void	debug __P((char *, ...))
	__attribute__((__format__(__printf__, 1, 2)));
extern	void	verbose __P((char *, ...))
	__attribute__((__format__(__printf__, 1, 2)));

#ifdef P_DEF
# undef	__P
# undef	P_DEF
#endif

#endif /* __IPT_H__ */
