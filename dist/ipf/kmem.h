/*	$NetBSD: kmem.h,v 1.2.4.1 2002/02/09 16:55:33 he Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 * Id: kmem.h,v 2.2.2.4 2002/01/01 13:43:48 darrenr Exp
 */

#ifndef	__KMEM_H__
#define	__KMEM_H__

#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif
extern	int	openkmem __P((char *, char *));
extern	int	kmemcpy __P((char *, long, int));
extern	int	kstrncpy __P((char *, long, int));
extern	char	*getifname __P((void *));

#if defined(__NetBSD__) || defined(__OpenBSD)
# include <paths.h>
#endif

#ifdef _PATH_KMEM
# define	KMEM	_PATH_KMEM
#else
# define	KMEM	"/dev/kmem"
#endif

#endif /* __KMEM_H__ */
