/*	$NetBSD: ipt.h,v 1.1.1.6.2.1 1997/10/30 07:16:37 mrg Exp $	*/

/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 * Id: ipt.h,v 2.0.2.7 1997/09/28 07:12:00 darrenr Exp 
 */

#ifndef	__IPT_H__
#define	__IPT_H__

#include <fcntl.h>
#ifdef	__STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif


struct	ipread	{
	int	(*r_open) __P((char *));
	int	(*r_close) __P((void));
	int	(*r_readip) __P((char *, int, char **, int *));
};

extern	void	debug __P((char *, ...));
extern	void	verbose __P((char *, ...));

#endif /* __IPT_H__ */
