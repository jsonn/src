/* $NetBSD: infcodes.h,v 1.5.12.1 2005/03/19 08:36:26 yamt Exp $ */

/* infcodes.h -- header to use infcodes.c
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

struct inflate_codes_state;
typedef struct inflate_codes_state FAR inflate_codes_statef;

extern inflate_codes_statef *inflate_codes_new __P((
    uInt, uInt,
    const inflate_huft *, const inflate_huft *,
    z_streamp ));

extern int inflate_codes __P((
    inflate_blocks_statef *,
    z_streamp ,
    int));

extern void inflate_codes_free __P((
    inflate_codes_statef *,
    z_streamp ));

