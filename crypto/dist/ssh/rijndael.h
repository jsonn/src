/*	$OpenBSD: rijndael.h,v 1.6 2001/01/29 01:58:17 niklas Exp $	*/

#ifndef _RIJNDAEL_H_
#define _RIJNDAEL_H_

/* 1. Standard types for AES cryptography source code               */

typedef u_int8_t   u1byte; /* an 8 bit unsigned character type */
typedef u_int16_t  u2byte; /* a 16 bit unsigned integer type   */
typedef u_int32_t  u4byte; /* a 32 bit unsigned integer type   */

typedef int8_t     s1byte; /* an 8 bit signed character type   */
typedef int16_t    s2byte; /* a 16 bit signed integer type     */
typedef int32_t    s4byte; /* a 32 bit signed integer type     */

typedef struct _rijndael_ctx {
	u4byte  k_len;
	int decrypt;
	u4byte  e_key[64];
	u4byte  d_key[64];
} rijndael_ctx;


/* 2. Standard interface for AES cryptographic routines             */

/* These are all based on 32 bit unsigned values and will therefore */
/* require endian conversions for big-endian architectures          */

rijndael_ctx *rijndael_set_key  __P((rijndael_ctx *, const u4byte *, u4byte, int));
void rijndael_encrypt __P((rijndael_ctx *, const u4byte *, u4byte *));
void rijndael_decrypt __P((rijndael_ctx *, const u4byte *, u4byte *));

#endif /* _RIJNDAEL_H_ */
