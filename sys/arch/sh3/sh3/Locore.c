/*	$NetBSD: Locore.c,v 1.1.2.1 2000/11/20 20:24:31 bouyer Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)Locore.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/sched.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>

#if 0
/*
 * fillw(short pattern, caddr_t addr, size_t len);
 * Write len copies of pattern at addr.
 */
void
fillw(pattern, addr, len)
	short pattern;
	void *addr;
	size_t len;
{
	unsigned long *p;
	unsigned long pat;
	int cnt;

	p = addr;
	pat = pattern;
	pat = pat | (pat << 16);

	cnt = len >> 1;
	while (cnt--)
		*p++ = pat;

	if (len & 1)
		*(short *)p = pattern;
}

/*
 * bcopyb(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes, one byte at a time.
 */
void
bcopyb(from, to, len)
	caddr_t from;
	caddr_t to;
	size_t len;
{
	char *s = (char *)from;
	char *d = (char *)to;

	if (from >= to) {
		while (len--)
			*d++ = *s++;
	} else {
		s += len - 1;
		d += len - 1;
		while (len--)
			*d-- = *s--;
	}
}

/*
 * bcopyw(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes, two bytes at a time.
 */
void
bcopyw(from, to, len)
	caddr_t from;
	caddr_t to;
	size_t len;
{
	short *s = (short *)from;
	short *d = (short *)to;
	int cnt;

	cnt = len >> 1;

	if (from >= to) {
		while (cnt--)
			*d++ = *s++;
		if (len & 1)
			*(char *)d = *(char *)s;
	} else {
		s += len - sizeof(short);
		d += len - sizeof(short);
		while (cnt--)
			*d-- = *s--;
		if (len & 1)
			*(char *)d = *(char *)s;
	}
}
#endif

/*
 * copyout(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes into the user's address space.
 */
int
copyout(kaddr, uaddr, len)
	const void *kaddr;
	void *uaddr;
	size_t len;
{
	const void *from = kaddr;
	char *to = uaddr;

	if (to + len < to || to + len > (char *)VM_MAXUSER_ADDRESS)
		return EFAULT;

	curpcb->pcb_onfault = &&Err999;
	memcpy(to, from, len);
	curpcb->pcb_onfault = 0;
	return 0;

 Err999:
	curpcb->pcb_onfault = 0;
	return EFAULT;
}

/*
 * copyin(caddr_t from, caddr_t to, size_t len);
 * Copy len bytes from the user's address space.
 */
int
copyin(uaddr, kaddr, len)
	const void *uaddr;
	void *kaddr;
	size_t len;
{
	const char *from = uaddr;
	void *to = kaddr;

	if (from + len < from || from + len > (char *)VM_MAXUSER_ADDRESS)
		return EFAULT;

	curpcb->pcb_onfault = &&Err999;
	memcpy(to, from, len);
	curpcb->pcb_onfault = 0;
	return 0;

 Err999:
	curpcb->pcb_onfault = 0;
	return EFAULT;
}

/*
 * copyoutstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, into the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 */
int
copyoutstr(kaddr, uaddr, maxlen, lencopied)
	const void *kaddr;
	void *uaddr;
	size_t maxlen;
	size_t *lencopied;
{
	const char *from = kaddr;
	char *to = uaddr;
	int cnt;
	int rc = 0;
	const char *from_top = from;

	curpcb->pcb_onfault = &&Err999;

	if ((cnt = (char *)VM_MAXUSER_ADDRESS - to) > maxlen)
		cnt = maxlen;

	while (cnt--) {
		if ((*to++ = *from++) == 0) {
			rc = 0;
			goto out;
		}
	}

	if (to >= (char *)VM_MAXUSER_ADDRESS)
		rc = EFAULT;
	else
		rc = ENAMETOOLONG;

out:
	if (lencopied)
		*lencopied = from - from_top;
	curpcb->pcb_onfault = 0;
	return rc;

 Err999:
	if (lencopied)
		*lencopied = from - from_top;
	curpcb->pcb_onfault = 0;
	return EFAULT;
}

/*
 * copyinstr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long, from the
 * user's address space.  Return the number of characters copied (including the
 * NUL) in *lencopied.  If the string is too long, return ENAMETOOLONG; else
 * return 0 or EFAULT.
 */
int
copyinstr(uaddr, kaddr, maxlen, lencopied)
	const void *uaddr;
	void *kaddr;
	size_t maxlen;
	size_t *lencopied;
{
	const char *from = uaddr;
	char *to = kaddr;
	int cnt;
	int rc = 0;
	const char *from_top = from;

	curpcb->pcb_onfault = &&Err999;

	if ((cnt = (char *)VM_MAXUSER_ADDRESS - to) > maxlen)
		cnt = maxlen;

	while (cnt--) {
		if ((*to++ = *from++) == 0) {
			rc = 0;
			goto out;
		}
	}

	if (to >= (char *)VM_MAXUSER_ADDRESS)
		rc = EFAULT;
	else
		rc = ENAMETOOLONG;

out:
	if (lencopied)
		*lencopied = from - from_top;
	curpcb->pcb_onfault = 0;
	return rc;

 Err999:
	if (lencopied)
		*lencopied = from - from_top;
	curpcb->pcb_onfault = 0;
	return EFAULT;
}

/*
 * copystr(caddr_t from, caddr_t to, size_t maxlen, size_t *lencopied);
 * Copy a NUL-terminated string, at most maxlen characters long.  Return the
 * number of characters copied (including the NUL) in *lencopied.  If the
 * string is too long, return ENAMETOOLONG; else return 0.
 */
int
copystr(kfaddr, kdaddr, maxlen, lencopied)
	const void *kfaddr;
	void *kdaddr;
	size_t maxlen;
	size_t *lencopied;
{
	const char *from = kfaddr;
	char *to = kdaddr;
	int i;

	for (i = 0; i < maxlen; i++) {
		if ((*to++ = *from++) == NULL) {
			if (lencopied)
				*lencopied = i + 1;
			return (0);
		}
	}

	if (lencopied)
		*lencopied = i;
	return (ENAMETOOLONG);
}

/*
 * fuword(caddr_t uaddr);
 * Fetch an int from the user's address space.
 */
long
fuword(base)
	const void *base;
{
	const char *uaddr = base;
	long rc;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(long))
		return -1;

	curpcb->pcb_onfault = &&Err999;

	rc = *(long *)uaddr;

	curpcb->pcb_onfault = 0;
	return rc;

 Err999:
	curpcb->pcb_onfault = 0;
	return -1;
}

/*
 * fusword(caddr_t uaddr);
 * Fetch a short from the user's address space.
 */
int
fusword(base)
	const void *base;
{
	const char *uaddr = base;
	int rc;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(short))
		return -1;

	curpcb->pcb_onfault = &&Err999;

	rc = *(unsigned short *)uaddr;

	curpcb->pcb_onfault = 0;
	return rc;

 Err999:
	curpcb->pcb_onfault = 0;
	return -1;
}

/*
 * fuswintr(caddr_t uaddr);
 * Fetch a short from the user's address space.  Can be called during an
 * interrupt.
 */
int
fuswintr(base)
	const void *base;
{
	const char *uaddr = base;
	int rc;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(short))
		return -1;

	curpcb->pcb_onfault = &&Err999;
	curpcb->fusubail = 1;

	rc = *(unsigned short *)uaddr;

	curpcb->pcb_onfault = 0;
	curpcb->fusubail = 0;
	return rc;

 Err999:
	curpcb->pcb_onfault = 0;
	curpcb->fusubail = 0;
	return -1;
}

/*
 * fubyte(caddr_t uaddr);
 * Fetch a byte from the user's address space.
 */
int
fubyte(base)
	const void *base;
{
	const char *uaddr = base;
	int rc;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(char))
		return -1;

	curpcb->pcb_onfault = &&Err999;

	rc = *(unsigned char *)uaddr;

	curpcb->pcb_onfault = 0;
	return rc;

 Err999:
	curpcb->pcb_onfault = 0;
	return -1;
}

/*
 * suword(caddr_t uaddr, int x);
 * Store an int in the user's address space.
 */
int
suword(base, x)
	void *base;
	long x;
{
	char *uaddr = base;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(long))
		return -1;

	curpcb->pcb_onfault = &&Err999;

	*(int *)uaddr = x;

	curpcb->pcb_onfault = 0;
	return 0;

 Err999:
	curpcb->pcb_onfault = 0;
	return -1;
}

/*
 * susword(void *uaddr, short x);
 * Store a short in the user's address space.
 */
int
susword(base, x)
	void *base;
	short x;
{
	char *uaddr = base;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(short))
		return -1;

	curpcb->pcb_onfault = &&Err999;

	*(short *)uaddr = x;

	curpcb->pcb_onfault = 0;
	return 0;

 Err999:
	curpcb->pcb_onfault = 0;
	return -1;
}

/*
 * suswintr(caddr_t uaddr, short x);
 * Store a short in the user's address space.  Can be called during an
 * interrupt.
 */
int
suswintr(base, x)
	void *base;
	short x;
{
	char *uaddr = base;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(short))
		return -1;

	curpcb->pcb_onfault = &&Err999;
	curpcb->fusubail = 1;

	*(short *)uaddr = x;

	curpcb->pcb_onfault = 0;
	curpcb->fusubail = 0;
	return 0;

 Err999:
	curpcb->pcb_onfault = 0;
	curpcb->fusubail = 0;
	return -1;
}

/*
 * subyte(caddr_t uaddr, char x);
 * Store a byte in the user's address space.
 */
int
subyte(base, x)
	void *base;
	int x;
{
	char *uaddr = base;

	if (uaddr > (char *)VM_MAXUSER_ADDRESS - sizeof(char))
		return -1;

	curpcb->pcb_onfault = &&Err999;

	*(char *)uaddr = x;

	curpcb->pcb_onfault = 0;
	return 0;

 Err999:
	curpcb->pcb_onfault = 0;
	return -1;
}

int
kcopy(src, dst, len)
	const void *src;
	void *dst;
	size_t len;
{
	caddr_t oldfault;

	oldfault = curpcb->pcb_onfault;

	curpcb->pcb_onfault = &&Err999;
	memcpy(dst, src, len);
	curpcb->pcb_onfault = oldfault;

	return 0;

 Err999:
	curpcb->pcb_onfault = oldfault;

	return EFAULT;
}

/*
 * Put process p on the run queue indicated by its priority.
 * Calls should be made at splstatclock(), and p->p_stat should be SRUN.
 */
void
setrunqueue(p)
	struct proc *p;
{
	struct prochd *q;
	struct proc *oldlast;
	int which = p->p_priority >> 2;

#ifdef sh3_debug
	printf("setrunque[whichqs = 0x%x,which=%d]\n",
	    sched_whichqs, which);
#endif

#define DIAGNOSTIC 1
#ifdef DIAGNOSTIC
	if (p->p_back || which >= 32 || which < 0)
		panic("setrunqueue");
#endif
	q = &sched_qs[which];
	sched_whichqs |= 0x00000001 << which;
	if (sched_whichqs == 0) {
		panic("setrunqueue[whichqs == 0 ]");
	}
	p->p_forw = (struct proc *)q;
	p->p_back = oldlast = q->ph_rlink;
	q->ph_rlink = p;
	oldlast->p_forw = p;
#ifdef sh3_debug
	printf("setrunque[whichqs = 0x%x,which=%d]\n",
	    sched_whichqs, which);
#endif
}

/*
 * Remove process p from its run queue, which should be the one
 * indicated by its priority.
 * Calls should be made at splstatclock().
 */
void
remrunqueue(p)
	struct proc *p;
{
	int which = p->p_priority >> 2;
	struct prochd *q;

#ifdef sh3_debug
	printf("remrunque[whichqs = 0x%x,which=%d]\n",
	    sched_whichqs, which);
#endif
#ifdef DIAGNOSTIC
	if (!(sched_whichqs & (0x00000001 << which)))
		panic("remrunqueue");
#endif
	p->p_forw->p_back = p->p_back;
	p->p_back->p_forw = p->p_forw;
	p->p_back = NULL;
	q = &sched_qs[which];
	if (q->ph_link == (struct proc *)q)
		sched_whichqs &= ~(0x00000001 << which);
#ifdef sh3_debug
	printf("remrunque[whichqs = 0x%x,which=%d]\n",
	    sched_whichqs, which);
#endif
}

void
setPageDirReg(int pgdir)
{
#if 0
	PageDirReg = pgdir;
#else
	SHREG_TTB = pgdir;
#endif
	tlbflush();
}

#if 0
u_long
random()
{
	static u_long randseed;

	randseed = randseed*1103515245 + 12345;
	return randseed;
}
#endif
