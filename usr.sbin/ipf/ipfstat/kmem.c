/*	$NetBSD: kmem.c,v 1.1.1.11 1998/11/22 14:21:46 mrg Exp $	*/

/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
/*
 * kmemcpy() - copies n bytes from kernel memory into user buffer.
 * returns 0 on success, -1 on error.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include "kmem.h"

#if !defined(lint)
static const char sccsid[] = "@(#)kmem.c	1.4 1/12/96 (C) 1992 Darren Reed";
static const char rcsid[] = "@(#)Id: kmem.c,v 2.0.2.5.2.1 1998/11/22 01:50:38 darrenr Exp ";
#endif

static	int	kmemfd = -1;

int	openkmem()
{
	if ((kmemfd = open(KMEM,O_RDONLY)) == -1)
	    {
		perror("kmeminit:open");
		return -1;
	    }
	return kmemfd;
}

int	kmemcpy(buf, pos, n)
register char	*buf;
long	pos;
register int	n;
{
	register int	r;

	if (!n)
		return 0;
	if (kmemfd == -1)
		if (openkmem() == -1)
			return -1;
	if (lseek(kmemfd, pos, 0) == -1)
	    {
		perror("kmemcpy:lseek");
		return -1;
	    }
	while ((r = read(kmemfd, buf, n)) < n)
		if (r <= 0)
		    {
			perror("kmemcpy:read");
			return -1;
		    }
		else
		    {
			buf += r;
			n -= r;
		    }
	return 0;
}
