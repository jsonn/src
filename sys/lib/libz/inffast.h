/* $NetBSD: inffast.h,v 1.5.12.1 2005/03/19 08:36:26 yamt Exp $ */

/* inffast.h -- header to use inffast.c
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

extern int inflate_fast __P((
    uInt,
    uInt,
    const inflate_huft *,
    const inflate_huft *,
    inflate_blocks_statef *,
    z_streamp ));
