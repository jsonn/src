/*	$NetBSD: byteswap.h,v 1.2.2.2 1999/02/02 06:21:06 cgd Exp $	*/

/*
 * inline macros for doing byteswapping on a little-endian machine.
 * for boot.
 */
#define ntohs(x) \
 ( ( ((u_short)(x)&0xff) << 8) | ( ((u_short) (x)&0xff00) >> 8) )
#define ntohl(x) \
 ( ((ntohs((u_short)(x))) << 16) | (ntohs( (u_short) ((x)>>16) )) )
