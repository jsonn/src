/*	$NetBSD: factor.c,v 1.2.2.2 1997/12/20 23:49:04 perry Exp $ */

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software develooped for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>			/* defs.h uses FILE* */
#include "defs.h"

/*
 * primes - prime table, built to include up to 46345 because
 * sqrt(2^31) = 46340.9500118415  
 *
 * building the table instead of storing a precomputed table saves
 * about 19K of space on the binary image.  
 */

long primes[4800];
int  num_primes = 2;

static void build_primes (long max)
{
	long pc;
	int j;
	long rem;
 
	/*
	 * Initialise primes at run-time rather than compile time
	 * so it's put in bss rather than data.
	 */
	primes[0] = 2;
	primes[1] = 3;
	
	for (pc = primes[num_primes-1]; pc < 46345 && pc*pc < max; pc+=2) {
		j = 0;
		rem = 1;
		while (j<num_primes && primes[j]*primes[j] <= pc) {
			if ((rem = pc % primes[j]) == 0)
				break;
			j++;
		}
		if (rem)
			primes[num_primes++] = pc;
	}
}

/* factor:  prepare a list of prime factors of val.
   The last number may not be a prime factor is the list is not
   long enough. */

void factor (long val, long *fact_list, int fact_size, int *num_fact)
{
	int i;

	/* Check to make sure we have enough primes. */
	build_primes(val);

	i = 0;
	*num_fact = 0;
	while (*num_fact < fact_size-1 && val > 1 && i < num_primes) {
		/* Find the next prime that divides. */
		while (i < num_primes && val % primes[i] != 0) i++;

		/* Put factors in array. */
		while (*num_fact < fact_size-1 && i < num_primes &&
		       val % primes[i] == 0) {
			fact_list[(*num_fact)++] = primes[i];
			val /= primes[i];
		}
	}
	if (val > 1)
		fact_list[(*num_fact)++] = val;
}

#ifdef TESTING
#include <stdio.h>

int main(int argc, char **argv)
{
	long facts[30];
	long val;
	int i, nfact;

	if (argc < 2) {
		fprintf (stderr, "usage: %s number\n", argv[0]);
		exit(1);
	}

	val = atol(argv[1]);
	factor (val, facts, 30, &nfact);

	printf ("%ld:", val);
	for (i = 0; i<nfact; i++)
		printf (" %d", facts[i]);
	printf ("\n");
}
#endif
