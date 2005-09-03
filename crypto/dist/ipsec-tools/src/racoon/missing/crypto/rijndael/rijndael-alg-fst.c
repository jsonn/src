/*	$NetBSD: rijndael-alg-fst.c,v 1.1.1.2.2.1 2005/09/03 07:03:56 snj Exp $	*/

/*	$KAME: rijndael-alg-fst.c,v 1.1.1.1 2001/08/08 09:56:23 sakane Exp $	*/

/*
 * rijndael-alg-fst.c   v2.3   April '2000
 *
 * Optimised ANSI C code
 *
 * authors: v1.0: Antoon Bosselaers
 *          v2.0: Vincent Rijmen
 *          v2.3: Paulo Barreto
 *
 * This code is placed in the public domain.
 */

#include "config.h"

#include <sys/cdefs.h>
#include <sys/types.h>
#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <string.h>
#endif
#include <crypto/rijndael/rijndael-alg-fst.h>
#include <crypto/rijndael/rijndael_local.h>

#include <crypto/rijndael/boxes-fst.dat>

#include <err.h>
#define bcopy(a, b, c) memcpy((b), (a), (c))
#define bzero(a, b) memset((a), 0, (b))
#define panic(a) err(1, (a))

int rijndaelKeySched(word8 k[MAXKC][4], word8 W[MAXROUNDS+1][4][4], int ROUNDS) {
	/* Calculate the necessary round keys
	 * The number of calculations depends on keyBits and blockBits
	 */ 
	int j, r, t, rconpointer = 0;
	union {
		word8	x8[MAXKC][4];
		word32	x32[MAXKC];
	} xtk;
#define	tk	xtk.x8
	int KC = ROUNDS - 6;

	for (j = KC-1; j >= 0; j--) {
		*((word32*)tk[j]) = *((word32*)k[j]);
	}
	r = 0;
	t = 0;
	/* copy values into round key array */
	for (j = 0; (j < KC) && (r < ROUNDS + 1); ) {
		for (; (j < KC) && (t < 4); j++, t++) {
			*((word32*)W[r][t]) = *((word32*)tk[j]);
		}
		if (t == 4) {
			r++;
			t = 0;
		}
	}
		
	while (r < ROUNDS + 1) { /* while not enough round key material calculated */
		/* calculate new values */
		tk[0][0] ^= S[tk[KC-1][1]];
		tk[0][1] ^= S[tk[KC-1][2]];
		tk[0][2] ^= S[tk[KC-1][3]];
		tk[0][3] ^= S[tk[KC-1][0]];
		tk[0][0] ^= rcon[rconpointer++];

		if (KC != 8) {
			for (j = 1; j < KC; j++) {
				*((word32*)tk[j]) ^= *((word32*)tk[j-1]);
			}
		} else {
			for (j = 1; j < KC/2; j++) {
				*((word32*)tk[j]) ^= *((word32*)tk[j-1]);
			}
			tk[KC/2][0] ^= S[tk[KC/2 - 1][0]];
			tk[KC/2][1] ^= S[tk[KC/2 - 1][1]];
			tk[KC/2][2] ^= S[tk[KC/2 - 1][2]];
			tk[KC/2][3] ^= S[tk[KC/2 - 1][3]];
			for (j = KC/2 + 1; j < KC; j++) {
				*((word32*)tk[j]) ^= *((word32*)tk[j-1]);
			}
		}
		/* copy values into round key array */
		for (j = 0; (j < KC) && (r < ROUNDS + 1); ) {
			for (; (j < KC) && (t < 4); j++, t++) {
				*((word32*)W[r][t]) = *((word32*)tk[j]);
			}
			if (t == 4) {
				r++;
				t = 0;
			}
		}
	}		
	return 0;
#undef tk
}

int rijndaelKeyEncToDec(word8 W[MAXROUNDS+1][4][4], int ROUNDS) {
	int r;
	word8 *w;

	for (r = 1; r < ROUNDS; r++) {
		w = W[r][0];
		*((word32*)w) =
			  *((const word32*)U1[w[0]])
			^ *((const word32*)U2[w[1]])
			^ *((const word32*)U3[w[2]])
			^ *((const word32*)U4[w[3]]);

		w = W[r][1];
		*((word32*)w) =
			  *((const word32*)U1[w[0]])
			^ *((const word32*)U2[w[1]])
			^ *((const word32*)U3[w[2]])
			^ *((const word32*)U4[w[3]]);

		w = W[r][2];
		*((word32*)w) =
			  *((const word32*)U1[w[0]])
			^ *((const word32*)U2[w[1]])
			^ *((const word32*)U3[w[2]])
			^ *((const word32*)U4[w[3]]);

		w = W[r][3];
		*((word32*)w) =
			  *((const word32*)U1[w[0]])
			^ *((const word32*)U2[w[1]])
			^ *((const word32*)U3[w[2]])
			^ *((const word32*)U4[w[3]]);
	}
	return 0;
}	

/**
 * Encrypt a single block. 
 */
int rijndaelEncrypt(word8 in[16], word8 out[16], word8 rk[MAXROUNDS+1][4][4], int ROUNDS) {
	int r;
	union {
		word8	x8[16];
		word32	x32[4];
	} xa, xb;
#define	a	xa.x8
#define	b	xb.x8
	union {
		word8	x8[4][4];
		word32	x32[4];
	} xtemp;
#define	temp	xtemp.x8

    memcpy(a, in, sizeof a);

    *((word32*)temp[0]) = *((word32*)(a   )) ^ *((word32*)rk[0][0]);
    *((word32*)temp[1]) = *((word32*)(a+ 4)) ^ *((word32*)rk[0][1]);
    *((word32*)temp[2]) = *((word32*)(a+ 8)) ^ *((word32*)rk[0][2]);
    *((word32*)temp[3]) = *((word32*)(a+12)) ^ *((word32*)rk[0][3]);
    *((word32*)(b    )) = *((const word32*)T1[temp[0][0]])
					^ *((const word32*)T2[temp[1][1]])
					^ *((const word32*)T3[temp[2][2]]) 
					^ *((const word32*)T4[temp[3][3]]);
    *((word32*)(b + 4)) = *((const word32*)T1[temp[1][0]])
					^ *((const word32*)T2[temp[2][1]])
					^ *((const word32*)T3[temp[3][2]]) 
					^ *((const word32*)T4[temp[0][3]]);
    *((word32*)(b + 8)) = *((const word32*)T1[temp[2][0]])
					^ *((const word32*)T2[temp[3][1]])
					^ *((const word32*)T3[temp[0][2]]) 
					^ *((const word32*)T4[temp[1][3]]);
    *((word32*)(b +12)) = *((const word32*)T1[temp[3][0]])
					^ *((const word32*)T2[temp[0][1]])
					^ *((const word32*)T3[temp[1][2]]) 
					^ *((const word32*)T4[temp[2][3]]);
	for (r = 1; r < ROUNDS-1; r++) {
		*((word32*)temp[0]) = *((word32*)(b   )) ^ *((word32*)rk[r][0]);
		*((word32*)temp[1]) = *((word32*)(b+ 4)) ^ *((word32*)rk[r][1]);
		*((word32*)temp[2]) = *((word32*)(b+ 8)) ^ *((word32*)rk[r][2]);
		*((word32*)temp[3]) = *((word32*)(b+12)) ^ *((word32*)rk[r][3]);

		*((word32*)(b    )) = *((const word32*)T1[temp[0][0]])
					^ *((const word32*)T2[temp[1][1]])
					^ *((const word32*)T3[temp[2][2]]) 
					^ *((const word32*)T4[temp[3][3]]);
		*((word32*)(b + 4)) = *((const word32*)T1[temp[1][0]])
					^ *((const word32*)T2[temp[2][1]])
					^ *((const word32*)T3[temp[3][2]]) 
					^ *((const word32*)T4[temp[0][3]]);
		*((word32*)(b + 8)) = *((const word32*)T1[temp[2][0]])
					^ *((const word32*)T2[temp[3][1]])
					^ *((const word32*)T3[temp[0][2]]) 
					^ *((const word32*)T4[temp[1][3]]);
		*((word32*)(b +12)) = *((const word32*)T1[temp[3][0]])
					^ *((const word32*)T2[temp[0][1]])
					^ *((const word32*)T3[temp[1][2]]) 
					^ *((const word32*)T4[temp[2][3]]);
	}
	/* last round is special */   
	*((word32*)temp[0]) = *((word32*)(b   )) ^ *((word32*)rk[ROUNDS-1][0]);
	*((word32*)temp[1]) = *((word32*)(b+ 4)) ^ *((word32*)rk[ROUNDS-1][1]);
	*((word32*)temp[2]) = *((word32*)(b+ 8)) ^ *((word32*)rk[ROUNDS-1][2]);
	*((word32*)temp[3]) = *((word32*)(b+12)) ^ *((word32*)rk[ROUNDS-1][3]);
	b[ 0] = T1[temp[0][0]][1];
	b[ 1] = T1[temp[1][1]][1];
	b[ 2] = T1[temp[2][2]][1];
	b[ 3] = T1[temp[3][3]][1];
	b[ 4] = T1[temp[1][0]][1];
	b[ 5] = T1[temp[2][1]][1];
	b[ 6] = T1[temp[3][2]][1];
	b[ 7] = T1[temp[0][3]][1];
	b[ 8] = T1[temp[2][0]][1];
	b[ 9] = T1[temp[3][1]][1];
	b[10] = T1[temp[0][2]][1];
	b[11] = T1[temp[1][3]][1];
	b[12] = T1[temp[3][0]][1];
	b[13] = T1[temp[0][1]][1];
	b[14] = T1[temp[1][2]][1];
	b[15] = T1[temp[2][3]][1];
	*((word32*)(b   )) ^= *((word32*)rk[ROUNDS][0]);
	*((word32*)(b+ 4)) ^= *((word32*)rk[ROUNDS][1]);
	*((word32*)(b+ 8)) ^= *((word32*)rk[ROUNDS][2]);
	*((word32*)(b+12)) ^= *((word32*)rk[ROUNDS][3]);

	memcpy(out, b, sizeof b /* XXX out */);

	return 0;
#undef a
#undef b
#undef temp
}

#ifdef INTERMEDIATE_VALUE_KAT
/**
 * Encrypt only a certain number of rounds.
 * Only used in the Intermediate Value Known Answer Test.
 */
int rijndaelEncryptRound(word8 a[4][4], word8 rk[MAXROUNDS+1][4][4], int ROUNDS, int rounds) {
	int r;
	word8 temp[4][4];

	/* make number of rounds sane */
	if (rounds > ROUNDS) {
		rounds = ROUNDS;
	}

	*((word32*)a[0]) = *((word32*)a[0]) ^ *((word32*)rk[0][0]);
	*((word32*)a[1]) = *((word32*)a[1]) ^ *((word32*)rk[0][1]);
	*((word32*)a[2]) = *((word32*)a[2]) ^ *((word32*)rk[0][2]);
	*((word32*)a[3]) = *((word32*)a[3]) ^ *((word32*)rk[0][3]);

	for (r = 1; (r <= rounds) && (r < ROUNDS); r++) {
		*((word32*)temp[0]) = *((const word32*)T1[a[0][0]])
					   ^ *((const word32*)T2[a[1][1]])
					   ^ *((const word32*)T3[a[2][2]]) 
					   ^ *((const word32*)T4[a[3][3]]);
		*((word32*)temp[1]) = *((const word32*)T1[a[1][0]])
					   ^ *((const word32*)T2[a[2][1]])
					   ^ *((const word32*)T3[a[3][2]]) 
					   ^ *((const word32*)T4[a[0][3]]);
		*((word32*)temp[2]) = *((const word32*)T1[a[2][0]])
					   ^ *((const word32*)T2[a[3][1]])
					   ^ *((const word32*)T3[a[0][2]]) 
					   ^ *((const word32*)T4[a[1][3]]);
		*((word32*)temp[3]) = *((const word32*)T1[a[3][0]])
					   ^ *((const word32*)T2[a[0][1]])
					   ^ *((const word32*)T3[a[1][2]]) 
					   ^ *((const word32*)T4[a[2][3]]);
		*((word32*)a[0]) = *((word32*)temp[0]) ^ *((word32*)rk[r][0]);
		*((word32*)a[1]) = *((word32*)temp[1]) ^ *((word32*)rk[r][1]);
		*((word32*)a[2]) = *((word32*)temp[2]) ^ *((word32*)rk[r][2]);
		*((word32*)a[3]) = *((word32*)temp[3]) ^ *((word32*)rk[r][3]);
	}
	if (rounds == ROUNDS) {
	   	/* last round is special */   
	   	temp[0][0] = T1[a[0][0]][1];
	   	temp[0][1] = T1[a[1][1]][1];
	   	temp[0][2] = T1[a[2][2]][1]; 
	   	temp[0][3] = T1[a[3][3]][1];
	   	temp[1][0] = T1[a[1][0]][1];
	   	temp[1][1] = T1[a[2][1]][1];
	   	temp[1][2] = T1[a[3][2]][1]; 
	   	temp[1][3] = T1[a[0][3]][1];
	   	temp[2][0] = T1[a[2][0]][1];
	   	temp[2][1] = T1[a[3][1]][1];
	   	temp[2][2] = T1[a[0][2]][1]; 
	   	temp[2][3] = T1[a[1][3]][1];
	   	temp[3][0] = T1[a[3][0]][1];
	   	temp[3][1] = T1[a[0][1]][1];
	   	temp[3][2] = T1[a[1][2]][1]; 
	   	temp[3][3] = T1[a[2][3]][1];
		*((word32*)a[0]) = *((word32*)temp[0]) ^ *((word32*)rk[ROUNDS][0]);
		*((word32*)a[1]) = *((word32*)temp[1]) ^ *((word32*)rk[ROUNDS][1]);
		*((word32*)a[2]) = *((word32*)temp[2]) ^ *((word32*)rk[ROUNDS][2]);
		*((word32*)a[3]) = *((word32*)temp[3]) ^ *((word32*)rk[ROUNDS][3]);
	}

	return 0;
}   
#endif /* INTERMEDIATE_VALUE_KAT */

/**
 * Decrypt a single block.
 */
int rijndaelDecrypt(word8 in[16], word8 out[16], word8 rk[MAXROUNDS+1][4][4], int ROUNDS) {
	int r;
	union {
		word8	x8[16];
		word32	x32[4];
	} xa, xb;
#define	a	xa.x8
#define	b	xb.x8
	union {
		word8	x8[4][4];
		word32	x32[4];
	} xtemp;
#define	temp	xtemp.x8
	
    memcpy(a, in, sizeof a);

    *((word32*)temp[0]) = *((word32*)(a   )) ^ *((word32*)rk[ROUNDS][0]);
    *((word32*)temp[1]) = *((word32*)(a+ 4)) ^ *((word32*)rk[ROUNDS][1]);
    *((word32*)temp[2]) = *((word32*)(a+ 8)) ^ *((word32*)rk[ROUNDS][2]);
    *((word32*)temp[3]) = *((word32*)(a+12)) ^ *((word32*)rk[ROUNDS][3]);

    *((word32*)(b   )) = *((const word32*)T5[temp[0][0]])
           ^ *((const word32*)T6[temp[3][1]])
           ^ *((const word32*)T7[temp[2][2]]) 
           ^ *((const word32*)T8[temp[1][3]]);
	*((word32*)(b+ 4)) = *((const word32*)T5[temp[1][0]])
           ^ *((const word32*)T6[temp[0][1]])
           ^ *((const word32*)T7[temp[3][2]]) 
           ^ *((const word32*)T8[temp[2][3]]);
	*((word32*)(b+ 8)) = *((const word32*)T5[temp[2][0]])
           ^ *((const word32*)T6[temp[1][1]])
           ^ *((const word32*)T7[temp[0][2]]) 
           ^ *((const word32*)T8[temp[3][3]]);
	*((word32*)(b+12)) = *((const word32*)T5[temp[3][0]])
           ^ *((const word32*)T6[temp[2][1]])
           ^ *((const word32*)T7[temp[1][2]]) 
           ^ *((const word32*)T8[temp[0][3]]);
	for (r = ROUNDS-1; r > 1; r--) {
		*((word32*)temp[0]) = *((word32*)(b   )) ^ *((word32*)rk[r][0]);
		*((word32*)temp[1]) = *((word32*)(b+ 4)) ^ *((word32*)rk[r][1]);
		*((word32*)temp[2]) = *((word32*)(b+ 8)) ^ *((word32*)rk[r][2]);
		*((word32*)temp[3]) = *((word32*)(b+12)) ^ *((word32*)rk[r][3]);
		*((word32*)(b   )) = *((const word32*)T5[temp[0][0]])
		   ^ *((const word32*)T6[temp[3][1]])
		   ^ *((const word32*)T7[temp[2][2]]) 
		   ^ *((const word32*)T8[temp[1][3]]);
		*((word32*)(b+ 4)) = *((const word32*)T5[temp[1][0]])
		   ^ *((const word32*)T6[temp[0][1]])
		   ^ *((const word32*)T7[temp[3][2]]) 
		   ^ *((const word32*)T8[temp[2][3]]);
		*((word32*)(b+ 8)) = *((const word32*)T5[temp[2][0]])
		   ^ *((const word32*)T6[temp[1][1]])
		   ^ *((const word32*)T7[temp[0][2]]) 
		   ^ *((const word32*)T8[temp[3][3]]);
		*((word32*)(b+12)) = *((const word32*)T5[temp[3][0]])
		   ^ *((const word32*)T6[temp[2][1]])
		   ^ *((const word32*)T7[temp[1][2]]) 
		   ^ *((const word32*)T8[temp[0][3]]);
	}
	/* last round is special */   
	*((word32*)temp[0]) = *((word32*)(b   )) ^ *((word32*)rk[1][0]);
	*((word32*)temp[1]) = *((word32*)(b+ 4)) ^ *((word32*)rk[1][1]);
	*((word32*)temp[2]) = *((word32*)(b+ 8)) ^ *((word32*)rk[1][2]);
	*((word32*)temp[3]) = *((word32*)(b+12)) ^ *((word32*)rk[1][3]);
	b[ 0] = S5[temp[0][0]];
	b[ 1] = S5[temp[3][1]];
	b[ 2] = S5[temp[2][2]];
	b[ 3] = S5[temp[1][3]];
	b[ 4] = S5[temp[1][0]];
	b[ 5] = S5[temp[0][1]];
	b[ 6] = S5[temp[3][2]];
	b[ 7] = S5[temp[2][3]];
	b[ 8] = S5[temp[2][0]];
	b[ 9] = S5[temp[1][1]];
	b[10] = S5[temp[0][2]];
	b[11] = S5[temp[3][3]];
	b[12] = S5[temp[3][0]];
	b[13] = S5[temp[2][1]];
	b[14] = S5[temp[1][2]];
	b[15] = S5[temp[0][3]];
	*((word32*)(b   )) ^= *((word32*)rk[0][0]);
	*((word32*)(b+ 4)) ^= *((word32*)rk[0][1]);
	*((word32*)(b+ 8)) ^= *((word32*)rk[0][2]);
	*((word32*)(b+12)) ^= *((word32*)rk[0][3]);

	memcpy(out, b, sizeof b /* XXX out */);

	return 0;
#undef a
#undef b
#undef temp
}


#ifdef INTERMEDIATE_VALUE_KAT
/**
 * Decrypt only a certain number of rounds.
 * Only used in the Intermediate Value Known Answer Test.
 * Operations rearranged such that the intermediate values
 * of decryption correspond with the intermediate values
 * of encryption.
 */
int rijndaelDecryptRound(word8 a[4][4], word8 rk[MAXROUNDS+1][4][4], int ROUNDS, int rounds) {
	int r, i;
	word8 temp[4], shift;

	/* make number of rounds sane */
	if (rounds > ROUNDS) {
		rounds = ROUNDS;
	}
    /* first round is special: */
	*(word32 *)a[0] ^= *(word32 *)rk[ROUNDS][0];
	*(word32 *)a[1] ^= *(word32 *)rk[ROUNDS][1];
	*(word32 *)a[2] ^= *(word32 *)rk[ROUNDS][2];
	*(word32 *)a[3] ^= *(word32 *)rk[ROUNDS][3];
	for (i = 0; i < 4; i++) {
		a[i][0] = Si[a[i][0]];
		a[i][1] = Si[a[i][1]];
		a[i][2] = Si[a[i][2]];
		a[i][3] = Si[a[i][3]];
	}
	for (i = 1; i < 4; i++) {
		shift = (4 - i) & 3;
		temp[0] = a[(0 + shift) & 3][i];
		temp[1] = a[(1 + shift) & 3][i];
		temp[2] = a[(2 + shift) & 3][i];
		temp[3] = a[(3 + shift) & 3][i];
		a[0][i] = temp[0];
		a[1][i] = temp[1];
		a[2][i] = temp[2];
		a[3][i] = temp[3];
	}
	/* ROUNDS-1 ordinary rounds */
	for (r = ROUNDS-1; r > rounds; r--) {
		*(word32 *)a[0] ^= *(word32 *)rk[r][0];
		*(word32 *)a[1] ^= *(word32 *)rk[r][1];
		*(word32 *)a[2] ^= *(word32 *)rk[r][2];
		*(word32 *)a[3] ^= *(word32 *)rk[r][3];

		*((word32*)a[0]) =
			  *((const word32*)U1[a[0][0]])
			^ *((const word32*)U2[a[0][1]])
			^ *((const word32*)U3[a[0][2]])
			^ *((const word32*)U4[a[0][3]]);

		*((word32*)a[1]) =
			  *((const word32*)U1[a[1][0]])
			^ *((const word32*)U2[a[1][1]])
			^ *((const word32*)U3[a[1][2]])
			^ *((const word32*)U4[a[1][3]]);

		*((word32*)a[2]) =
			  *((const word32*)U1[a[2][0]])
			^ *((const word32*)U2[a[2][1]])
			^ *((const word32*)U3[a[2][2]])
			^ *((const word32*)U4[a[2][3]]);

		*((word32*)a[3]) =
			  *((const word32*)U1[a[3][0]])
			^ *((const word32*)U2[a[3][1]])
			^ *((const word32*)U3[a[3][2]])
			^ *((const word32*)U4[a[3][3]]);
		for (i = 0; i < 4; i++) {
			a[i][0] = Si[a[i][0]];
			a[i][1] = Si[a[i][1]];
			a[i][2] = Si[a[i][2]];
			a[i][3] = Si[a[i][3]];
		}
		for (i = 1; i < 4; i++) {
			shift = (4 - i) & 3;
			temp[0] = a[(0 + shift) & 3][i];
			temp[1] = a[(1 + shift) & 3][i];
			temp[2] = a[(2 + shift) & 3][i];
			temp[3] = a[(3 + shift) & 3][i];
			a[0][i] = temp[0];
			a[1][i] = temp[1];
			a[2][i] = temp[2];
			a[3][i] = temp[3];
		}
	}
	if (rounds == 0) {
		/* End with the extra key addition */	
		*(word32 *)a[0] ^= *(word32 *)rk[0][0];
		*(word32 *)a[1] ^= *(word32 *)rk[0][1];
		*(word32 *)a[2] ^= *(word32 *)rk[0][2];
		*(word32 *)a[3] ^= *(word32 *)rk[0][3];
	}    
	return 0;
}
#endif /* INTERMEDIATE_VALUE_KAT */
