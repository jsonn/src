/*
 * Copyright (c) 1993 Theo de Raadt
 * All rights reserved.
 *
 * Per Lindqvist <pgd@compuram.bbt.se> supplied an almost fully working
 * set of true clist functions that this is very loosely based on.
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
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of Theo de Raadt may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *	$Id: tty_subr.c,v 1.1.4.1 1993/09/24 08:51:49 mycroft Exp $
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "ioctl.h"
#include "tty.h"
#include "clist.h"
#include "malloc.h"

#include "machine/cpu.h"

/*
 * At compile time, choose:
 * There are two ways the TTY_QUOTE bit can be stored. If QBITS is
 * defined we allocate an array of bits -- 1/8th as much memory but
 * setbit(), clrbit(), and isset() take more cpu. If QBITS is
 * undefined, we just use an array of bytes.
 * 
 * If TTY_QUOTE functionality isn't required by a line discipline,
 * it can free c_cq and set it to NULL. This speeds things up,
 * and also does not use any extra memory. This is useful for (say)
 * a SLIP line discipline that wants a 32K ring buffer for data
 * but doesn't need quoting.
 */
#define QBITS

#ifdef QBITS
#define QMEM(n)		((((n)-1)/NBBY)+1)
#else
#define QMEM(n)		(n)
#endif


/*
 * Initialize clists.
 */
void
cinit()
{
}

/*
 * Initialize a particular clist. Ok, they are really ring buffers,
 * of the specified length, with/without quoting support.
 */
int
clalloc(clp, size, quot)
	struct clist *clp;
	int size;
	int quot;
{

	MALLOC(clp->c_cs, u_char *, size, M_TTYS, M_WAITOK);
	if (!clp->c_cs)
		return (-1);
	bzero(clp->c_cs, size);

	if(quot) {
		MALLOC(clp->c_cq, u_char *, QMEM(size), M_TTYS, M_WAITOK);
		if (!clp->c_cq) {
			FREE(clp->c_cs, M_TTYS);
			return (-1);
		}
		bzero(clp->c_cs, QMEM(size));
	} else
		clp->c_cq = (u_char *)0;

	clp->c_cf = clp->c_cl = (u_char *)0;
	clp->c_ce = clp->c_cs + size;
	clp->c_cn = size;
	clp->c_cc = 0;
	return (0);
}

void
clfree(clp)
	struct clist *clp;
{
	if(clp->c_cs)
		FREE(clp->c_cs, M_TTYS);
	if(clp->c_cq)
		FREE(clp->c_cq, M_TTYS);
	clp->c_cs = clp->c_cq = (u_char *)0;
}


/*
 * Get a character from a clist.
 */
int
getc(clp)
	struct clist *clp;
{
	register int c = -1;
	int s;

	s = spltty();
	if (clp->c_cc == 0)
		goto out;

	c = *clp->c_cf & 0xff;
	if (clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, clp->c_cf - clp->c_cs) )
			c |= TTY_QUOTE;
#else
		if (*(clp->c_cf - clp->c_cs + clp->c_cq))
			c |= TTY_QUOTE;
#endif
	}
	if (++clp->c_cf == clp->c_ce)
		clp->c_cf = clp->c_cs;
	if (--clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
out:
	splx(s);
	return c;
}

/*
 * Copy clist to buffer.
 * Return number of bytes moved.
 */
int
q_to_b(clp, cp, count)
	struct clist *clp;
	u_char *cp;
	int count;
{
	register int cc;
	u_char *p = cp;
	int s;

	s = spltty();
	/* optimize this while loop */
	while (count > 0 && clp->c_cc > 0) {
		cc = clp->c_cl - clp->c_cf;
		if (clp->c_cf >= clp->c_cl)
			cc = clp->c_ce - clp->c_cf;
		if (cc > count)
			cc = count;
		bcopy(clp->c_cf, p, cc);
		count -= cc;
		p += cc;
		clp->c_cc -= cc;
		clp->c_cf += cc;
		if (clp->c_cf == clp->c_ce)
			clp->c_cf = clp->c_cs;
	}
	if (clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
	splx(s);
	return p - cp;
}

/*
 * Return count of contiguous characters in clist.
 * Stop counting if flag&character is non-null.
 */
ndqb(clp, flag)
	struct clist *clp;
	int flag;
{
	int count = 0;
	register int i;
	register int cc;
	int s;

	s = spltty();
	if ((cc = clp->c_cc) == 0 || flag == 0)
		goto out;

	i = clp->c_cf - clp->c_cs;
	while (cc-- > 0 && (clp->c_cs[i++] & flag) == 0) {
		count++;
		if (i == clp->c_cn)
			break;
	}
out:
	splx(s);
	return count;
}

/*
 * Flush count bytes from clist.
 */
void
ndflush(clp, count)
	struct clist *clp;
	int count;
{
	register int cc;
	int s;

	s = spltty();
	if (count == clp->c_cc) {
		clp->c_cc = 0;
		clp->c_cf = clp->c_cl = (u_char *)0;
		goto out;
	}
	/* optimize this while loop */
	while (count > 0 && clp->c_cc > 0) {
		cc = clp->c_cl - clp->c_cf;
		if (clp->c_cf >= clp->c_cl)
			cc = clp->c_ce - clp->c_cf;
		if (cc > count)
			cc = count;
		count -= cc;
		clp->c_cc -= cc;
		clp->c_cf += cc;
		if (clp->c_cf == clp->c_ce)
			clp->c_cf = clp->c_cs;
	}
	if (clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
out:
	splx(s);
}

/*
 * Put a character into the output queue.
 */
int
putc(c, clp)
	int c;
	struct clist *clp;
{
	register u_char *q;
	register int i;
	int r = -1;
	int s;

	s = spltty();
	if (clp->c_cc == clp->c_cn)
		goto out;

	if (clp->c_cc == 0) {
		if (!clp->c_cs) {
#if defined(DIAGNOSTIC) || 1
			printf("b_to_q: required clalloc\n");
#endif
			if(clalloc(clp, 1024, 1)) {
out:
				splx(s);
				return -1;
			}
		}
		clp->c_cf = clp->c_cl = clp->c_cs;
	}

	*clp->c_cl = c & 0xff;
	i = clp->c_cl - clp->c_cs;
	if (clp->c_cq) {
#ifdef QBITS
		if (c & TTY_QUOTE)
			setbit(clp->c_cq, i); 
		else
			clrbit(clp->c_cq, i);
#else
		q = clp->c_cq + i;
		*q = (c & TTY_QUOTE) ? 1 : 0;
#endif
	}
	clp->c_cc++;
	clp->c_cl++;
	if (clp->c_cl == clp->c_ce)
		clp->c_cl = clp->c_cs;
	splx(s);
	return 0;
}

#ifdef QBITS
/*
 * optimized version of
 *
 * for (i = 0; i < len; i++)
 *	clrbit(cp, off + len);
 */
void
clrbits(cp, off, len)
	u_char *cp;
	int off;
	int len;
{
	int sby, sbi, eby, ebi;
	register int i;
	u_char mask;

	if(len==1) {
		clrbit(cp, off);
		return;
	}

	sby = off / NBBY;
	sbi = off % NBBY;
	eby = (off+len) / NBBY;
	ebi = (off+len) % NBBY;
	if (sby == eby) {
		mask = ((1 << (ebi - sbi)) - 1) << sbi;
		cp[sby] &= ~mask;
	} else {
		mask = (1<<sbi) - 1;
		cp[sby++] &= mask;

		mask = (1<<ebi) - 1;
		cp[eby] &= ~mask;

		for (i = sby; i < eby; i++)
			cp[i] = 0x00;
	}
}
#endif

/*
 * Copy buffer to clist.
 * Return number of bytes not transfered.
 */
int
b_to_q(cp, count, clp)
	u_char *cp;
	int count;
	struct clist *clp;
{
	register int i, cc;
	register u_char *p = cp;
	int off, s;

	if (count <= 0)
		return 0;

	s = spltty();
	if (clp->c_cc == clp->c_cn)
		goto out;

	if (clp->c_cc == 0) {
		if (!clp->c_cs) {
#if defined(DIAGNOSTIC) || 1
			printf("b_to_q: required clalloc\n");
#endif
			if(clalloc(clp, 1024, 1))
				goto out;
		}
		clp->c_cf = clp->c_cl = clp->c_cs;
	}

	/* optimize this while loop */
	while (count > 0 && clp->c_cc < clp->c_cn) {
		cc = clp->c_ce - clp->c_cl;
		if (clp->c_cf > clp->c_cl)
			cc = clp->c_cf - clp->c_cl;
		if (cc > count)
			cc = count;
		bcopy(p, clp->c_cl, cc);
		if (clp->c_cq) {
#ifdef QBITS
			clrbits(clp->c_cq, clp->c_cl - clp->c_cs, cc);
#else
			bzero(clp->c_cl - clp->c_cs + clp->c_cq, cc);
#endif
		}
		p += cc;
		count -= cc;
		clp->c_cc += cc;
		clp->c_cl += cc;
		if (clp->c_cl == clp->c_ce)
			clp->c_cl = clp->c_cs;
	}
out:
	splx(s);
	return count;
}

static int cc;

/*
 * Given a non-NULL pointer into the clist return the pointer
 * to the next character in the list or return NULL if no more chars.
 *
 * Callers must not allow getc's to happen between firstc's and getc's
 * so that the pointer becomes invalid.  Note that interrupts are NOT
 * masked.
 */
u_char *
nextc(clp, cp, c)
	struct clist *clp;
	register u_char *cp;
	int *c;
{

	if (clp->c_cf == cp) {
		/*
		 * First time initialization.
		 */
		cc = clp->c_cc;
	}
	if (cc == 0 || cp == NULL)
		return NULL;
	if (--cc == 0)
		return NULL;
	if (++cp == clp->c_ce)
		cp = clp->c_cs;
	*c = *cp & 0xff;
	if (clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, cp - clp->c_cs))
			*c |= TTY_QUOTE;
#else
		if (*(clp->c_cf - clp->c_cs + clp->c_cq))
			*c |= TTY_QUOTE;
#endif
	}
	return cp;
}

/*
 * Given a non-NULL pointer into the clist return the pointer
 * to the first character in the list or return NULL if no more chars.
 *
 * Callers must not allow getc's to happen between firstc's and getc's
 * so that the pointer becomes invalid.  Note that interrupts are NOT
 * masked.
 *
 * *c is set to the NEXT character
 */
u_char *
firstc(clp, c)
	struct clist *clp;
	int *c;
{
	int empty = 0;
	register u_char *cp;
	register int i;

	cc = clp->c_cc;
	if (cc == 0)
		return NULL;
	cp = clp->c_cf;
	*c = *cp & 0xff;
	if(clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, cp - clp->c_cs))
			*c |= TTY_QUOTE;
#else
		if (*(cp - clp->c_cs + clp->c_cq))
			*c |= TTY_QUOTE;
#endif
	}
	return clp->c_cf;
}

/*
 * Remove the last character in the clist and return it.
 */
int
unputc(clp)
	struct clist *clp;
{
	unsigned int c = -1;
	int s;

	s = spltty();
	if (clp->c_cc == 0)
		goto out;

	if (clp->c_cl == clp->c_cs)
		clp->c_cl = clp->c_ce - 1;
	else
		--clp->c_cl;
	clp->c_cc--;

	c = *clp->c_cl & 0xff;
	if (clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, clp->c_cl - clp->c_cs))
			c |= TTY_QUOTE;
#else
		if (*(clp->c_cf - clp->c_cs + clp->c_cq))
			c | TTY_QUOTE;
#endif
	}
	if (clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
out:
	splx(s);
	return c;
}

/*
 * Put the chars in the from queue on the end of the to queue.
 */
void
catq(from, to)
	struct clist *from, *to;
{
	int c;

	while ((c = getc(from)) != -1)
		putc(c, to);
}
