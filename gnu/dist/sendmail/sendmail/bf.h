/*
 * Copyright (c) 1999, 2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	Id: bf.h,v 8.5.16.2 2001/02/14 04:07:27 gshapiro Exp
 *
 * Contributed by Exactis.com, Inc.
 *
 */

#ifndef BF_H
#define BF_H 1

extern FILE	*bfopen __P((char *, int, size_t, long));
extern FILE	*bfdup __P((FILE *));
extern int	bfcommit __P((FILE *));
extern int	bfrewind __P((FILE *));
extern int	bftruncate __P((FILE *));
extern int	bffsync __P((FILE *));
extern int	bfclose __P((FILE *));
extern bool	bftest __P((FILE *));

#endif /* BF_H */
