/*	$NetBSD: uipc_socket.c,v 1.97.2.1.2.2.2.1 2005/10/31 13:36:41 tron Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of Wasabi Systems, Inc.
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

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)uipc_socket.c	8.6 (Berkeley) 5/2/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_socket.c,v 1.97.2.1.2.2.2.1 2005/10/31 13:36:41 tron Exp $");

#include "opt_sock_counters.h"
#include "opt_sosend_loan.h"
#include "opt_mbuftrace.h"
#include "opt_somaxkva.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/pool.h>
#include <sys/event.h>
#include <sys/poll.h>

#include <uvm/uvm.h>

struct pool	socket_pool;

MALLOC_DEFINE(M_SOOPTS, "soopts", "socket options");
MALLOC_DEFINE(M_SONAME, "soname", "socket name");

extern int	somaxconn;			/* patchable (XXX sysctl) */
int		somaxconn = SOMAXCONN;

#ifdef SOSEND_COUNTERS
#include <sys/device.h>

struct evcnt sosend_loan_big = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "loan big");
struct evcnt sosend_copy_big = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "copy big");
struct evcnt sosend_copy_small = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "copy small");
struct evcnt sosend_kvalimit = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "kva limit");

#define	SOSEND_COUNTER_INCR(ev)		(ev)->ev_count++

#else

#define	SOSEND_COUNTER_INCR(ev)		/* nothing */

#endif /* SOSEND_COUNTERS */

void
soinit(void)
{

	/* Set the initial adjusted socket buffer size. */
	if (sb_max_set(sb_max))
		panic("bad initial sb_max value: %lu\n", sb_max);

	pool_init(&socket_pool, sizeof(struct socket), 0, 0, 0,
	    "sockpl", NULL);

#ifdef SOSEND_COUNTERS
	evcnt_attach_static(&sosend_loan_big);
	evcnt_attach_static(&sosend_copy_big);
	evcnt_attach_static(&sosend_copy_small);
	evcnt_attach_static(&sosend_kvalimit);
#endif /* SOSEND_COUNTERS */
}

#ifdef SOSEND_NO_LOAN
int use_sosend_loan = 0;
#else
int use_sosend_loan = 1;
#endif

struct simplelock so_pendfree_slock = SIMPLELOCK_INITIALIZER;
struct mbuf *so_pendfree;

#ifndef SOMAXKVA
#define	SOMAXKVA (16 * 1024 * 1024)
#endif
int somaxkva = SOMAXKVA;
int socurkva;
int sokvawaiters;

#define	SOCK_LOAN_THRESH	4096
#define	SOCK_LOAN_CHUNK		65536

static size_t sodopendfree(struct socket *);
static size_t sodopendfreel(struct socket *);
static __inline void sokvareserve(struct socket *, vsize_t);
static __inline void sokvaunreserve(vsize_t);

static __inline void
sokvareserve(struct socket *so, vsize_t len)
{
	int s;

	s = splvm();
	simple_lock(&so_pendfree_slock);
	while (socurkva + len > somaxkva) {
		size_t freed;

		/*
		 * try to do pendfree.
		 */

		freed = sodopendfreel(so);

		/*
		 * if some kva was freed, try again.
		 */

		if (freed)
			continue;

		SOSEND_COUNTER_INCR(&sosend_kvalimit);
		sokvawaiters++;
		(void) ltsleep(&socurkva, PVM, "sokva", 0, &so_pendfree_slock);
		sokvawaiters--;
	}
	socurkva += len;
	simple_unlock(&so_pendfree_slock);
	splx(s);
}

static __inline void
sokvaunreserve(vsize_t len)
{
	int s;

	s = splvm();
	simple_lock(&so_pendfree_slock);
	socurkva -= len;
	if (sokvawaiters)
		wakeup(&socurkva);
	simple_unlock(&so_pendfree_slock);
	splx(s);
}

/*
 * sokvaalloc: allocate kva for loan.
 */

vaddr_t
sokvaalloc(vsize_t len, struct socket *so)
{
	vaddr_t lva;

	/*
	 * reserve kva.
	 */

	sokvareserve(so, len);

	/*
	 * allocate kva.
	 */

	lva = uvm_km_valloc_wait(kernel_map, len);
	if (lva == 0) {
		sokvaunreserve(len);
		return (0);
	}

	return lva;
}

/*
 * sokvafree: free kva for loan.
 */

void
sokvafree(vaddr_t sva, vsize_t len)
{

	/*
	 * free kva.
	 */

	uvm_km_free(kernel_map, sva, len);

	/*
	 * unreserve kva.
	 */

	sokvaunreserve(len);
}

static void
sodoloanfree(struct vm_page **pgs, caddr_t buf, size_t size)
{
	vaddr_t va, sva, eva;
	vsize_t len;
	paddr_t pa;
	int i, npgs;

	eva = round_page((vaddr_t) buf + size);
	sva = trunc_page((vaddr_t) buf);
	len = eva - sva;
	npgs = len >> PAGE_SHIFT;

	if (__predict_false(pgs == NULL)) {
		pgs = alloca(npgs * sizeof(*pgs));

		for (i = 0, va = sva; va < eva; i++, va += PAGE_SIZE) {
			if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
				panic("sodoloanfree: va 0x%lx not mapped", va);
			pgs[i] = PHYS_TO_VM_PAGE(pa);
		}
	}

	pmap_kremove(sva, len);
	pmap_update(pmap_kernel());
	uvm_unloan(pgs, npgs, UVM_LOAN_TOPAGE);
	sokvafree(sva, len);
}

static size_t
sodopendfree(struct socket *so)
{
	int s;
	size_t rv;

	s = splvm();
	simple_lock(&so_pendfree_slock);
	rv = sodopendfreel(so);
	simple_unlock(&so_pendfree_slock);
	splx(s);

	return rv;
}

/*
 * sodopendfreel: free mbufs on "pendfree" list.
 * unlock and relock so_pendfree_slock when freeing mbufs.
 *
 * => called with so_pendfree_slock held.
 * => called at splvm.
 */

static size_t
sodopendfreel(struct socket *so)
{
	size_t rv = 0;

	LOCK_ASSERT(simple_lock_held(&so_pendfree_slock));

	for (;;) {
		struct mbuf *m;
		struct mbuf *next;

		m = so_pendfree;
		if (m == NULL)
			break;
		so_pendfree = NULL;
		simple_unlock(&so_pendfree_slock);
		/* XXX splx */

		for (; m != NULL; m = next) {
			next = m->m_next;

			rv += m->m_ext.ext_size;
			sodoloanfree((m->m_flags & M_EXT_PAGES) ?
			    m->m_ext.ext_pgs : NULL, m->m_ext.ext_buf,
			    m->m_ext.ext_size);
			pool_cache_put(&mbpool_cache, m);
		}

		/* XXX splvm */
		simple_lock(&so_pendfree_slock);
	}

	return (rv);
}

void
soloanfree(struct mbuf *m, caddr_t buf, size_t size, void *arg)
{
	int s;

	if (m == NULL) {

		/*
		 * called from MEXTREMOVE.
		 */

		sodoloanfree(NULL, buf, size);
		return;
	}

	/*
	 * postpone freeing mbuf.
	 *
	 * we can't do it in interrupt context
	 * because we need to put kva back to kernel_map.
	 */

	s = splvm();
	simple_lock(&so_pendfree_slock);
	m->m_next = so_pendfree;
	so_pendfree = m;
	if (sokvawaiters)
		wakeup(&socurkva);
	simple_unlock(&so_pendfree_slock);
	splx(s);
}

static long
sosend_loan(struct socket *so, struct uio *uio, struct mbuf *m, long space)
{
	struct iovec *iov = uio->uio_iov;
	vaddr_t sva, eva;
	vsize_t len;
	vaddr_t lva, va;
	int npgs, i, error;

	if (uio->uio_segflg != UIO_USERSPACE)
		return (0);

	if (iov->iov_len < (size_t) space)
		space = iov->iov_len;
	if (space > SOCK_LOAN_CHUNK)
		space = SOCK_LOAN_CHUNK;

	eva = round_page((vaddr_t) iov->iov_base + space);
	sva = trunc_page((vaddr_t) iov->iov_base);
	len = eva - sva;
	npgs = len >> PAGE_SHIFT;

	/* XXX KDASSERT */
	KASSERT(npgs <= M_EXT_MAXPAGES);

	lva = sokvaalloc(len, so);
	if (lva == 0)
		return 0;

	error = uvm_loan(&uio->uio_procp->p_vmspace->vm_map, sva, len,
	    m->m_ext.ext_pgs, UVM_LOAN_TOPAGE);
	if (error) {
		sokvafree(lva, len);
		return (0);
	}

	for (i = 0, va = lva; i < npgs; i++, va += PAGE_SIZE)
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(m->m_ext.ext_pgs[i]),
		    VM_PROT_READ);
	pmap_update(pmap_kernel());

	lva += (vaddr_t) iov->iov_base & PAGE_MASK;

	MEXTADD(m, (caddr_t) lva, space, M_MBUF, soloanfree, so);
	m->m_flags |= M_EXT_PAGES | M_EXT_ROMAP;

	uio->uio_resid -= space;
	/* uio_offset not updated, not set/used for write(2) */
	uio->uio_iov->iov_base = (caddr_t) uio->uio_iov->iov_base + space;
	uio->uio_iov->iov_len -= space;
	if (uio->uio_iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}

	return (space);
}

/*
 * Socket operation routines.
 * These routines are called by the routines in
 * sys_socket.c or from a system process, and
 * implement the semantics of socket operations by
 * switching out to the protocol specific routines.
 */
/*ARGSUSED*/
int
socreate(int dom, struct socket **aso, int type, int proto)
{
	struct proc	*p;
	struct protosw	*prp;
	struct socket	*so;
	int		error, s;

	p = curproc;		/* XXX */
	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);
	if (prp == 0 || prp->pr_usrreq == 0)
		return (EPROTONOSUPPORT);
	if (prp->pr_type != type)
		return (EPROTOTYPE);
	s = splsoftnet();
	so = pool_get(&socket_pool, PR_WAITOK);
	memset((caddr_t)so, 0, sizeof(*so));
	TAILQ_INIT(&so->so_q0);
	TAILQ_INIT(&so->so_q);
	so->so_type = type;
	so->so_proto = prp;
	so->so_send = sosend;
	so->so_receive = soreceive;
#ifdef MBUFTRACE
	so->so_rcv.sb_mowner = &prp->pr_domain->dom_mowner;
	so->so_snd.sb_mowner = &prp->pr_domain->dom_mowner;
	so->so_mowner = &prp->pr_domain->dom_mowner;
#endif
	if (p != 0)
		so->so_uid = p->p_ucred->cr_uid;
	error = (*prp->pr_usrreq)(so, PRU_ATTACH, (struct mbuf *)0,
	    (struct mbuf *)(long)proto, (struct mbuf *)0, p);
	if (error) {
		so->so_state |= SS_NOFDREF;
		sofree(so);
		splx(s);
		return (error);
	}
	splx(s);
	*aso = so;
	return (0);
}

int
sobind(struct socket *so, struct mbuf *nam, struct proc *p)
{
	int	s, error;

	s = splsoftnet();
	error = (*so->so_proto->pr_usrreq)(so, PRU_BIND, (struct mbuf *)0,
	    nam, (struct mbuf *)0, p);
	splx(s);
	return (error);
}

int
solisten(struct socket *so, int backlog)
{
	int	s, error;

	s = splsoftnet();
	error = (*so->so_proto->pr_usrreq)(so, PRU_LISTEN, (struct mbuf *)0,
	    (struct mbuf *)0, (struct mbuf *)0, (struct proc *)0);
	if (error) {
		splx(s);
		return (error);
	}
	if (TAILQ_EMPTY(&so->so_q))
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0)
		backlog = 0;
	so->so_qlimit = min(backlog, somaxconn);
	splx(s);
	return (0);
}

void
sofree(struct socket *so)
{

	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0)
		return;
	if (so->so_head) {
		/*
		 * We must not decommission a socket that's on the accept(2)
		 * queue.  If we do, then accept(2) may hang after select(2)
		 * indicated that the listening socket was ready.
		 */
		if (!soqremque(so, 0))
			return;
	}
	sbrelease(&so->so_snd);
	sorflush(so);
	pool_put(&socket_pool, so);
}

/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 */
int
soclose(struct socket *so)
{
	struct socket	*so2;
	int		s, error;

	error = 0;
	s = splsoftnet();		/* conservative */
	if (so->so_options & SO_ACCEPTCONN) {
		while ((so2 = TAILQ_FIRST(&so->so_q0)) != 0) {
			(void) soqremque(so2, 0);
			(void) soabort(so2);
		}
		while ((so2 = TAILQ_FIRST(&so->so_q)) != 0) {
			(void) soqremque(so2, 1);
			(void) soabort(so2);
		}
	}
	if (so->so_pcb == 0)
		goto discard;
	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			if (error)
				goto drop;
		}
		if (so->so_options & SO_LINGER) {
			if ((so->so_state & SS_ISDISCONNECTING) &&
			    (so->so_state & SS_NBIO))
				goto drop;
			while (so->so_state & SS_ISCONNECTED) {
				error = tsleep((caddr_t)&so->so_timeo,
					       PSOCK | PCATCH, netcls,
					       so->so_linger * hz);
				if (error)
					break;
			}
		}
	}
 drop:
	if (so->so_pcb) {
		int error2 = (*so->so_proto->pr_usrreq)(so, PRU_DETACH,
		    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0,
		    (struct proc *)0);
		if (error == 0)
			error = error2;
	}
 discard:
	if (so->so_state & SS_NOFDREF)
		panic("soclose: NOFDREF");
	so->so_state |= SS_NOFDREF;
	sofree(so);
	splx(s);
	return (error);
}

/*
 * Must be called at splsoftnet...
 */
int
soabort(struct socket *so)
{

	return (*so->so_proto->pr_usrreq)(so, PRU_ABORT, (struct mbuf *)0,
	    (struct mbuf *)0, (struct mbuf *)0, (struct proc *)0);
}

int
soaccept(struct socket *so, struct mbuf *nam)
{
	int	s, error;

	error = 0;
	s = splsoftnet();
	if ((so->so_state & SS_NOFDREF) == 0)
		panic("soaccept: !NOFDREF");
	so->so_state &= ~SS_NOFDREF;
	if ((so->so_state & SS_ISDISCONNECTED) == 0 ||
	    (so->so_proto->pr_flags & PR_ABRTACPTDIS) == 0)
		error = (*so->so_proto->pr_usrreq)(so, PRU_ACCEPT,
		    (struct mbuf *)0, nam, (struct mbuf *)0, (struct proc *)0);
	else
		error = ECONNABORTED;

	splx(s);
	return (error);
}

int
soconnect(struct socket *so, struct mbuf *nam)
{
	struct proc	*p;
	int		s, error;

	p = curproc;		/* XXX */
	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);
	s = splsoftnet();
	/*
	 * If protocol is connection-based, can only connect once.
	 * Otherwise, if connected, try to disconnect first.
	 * This allows user to disconnect by connecting to, e.g.,
	 * a null address.
	 */
	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
	    (error = sodisconnect(so))))
		error = EISCONN;
	else
		error = (*so->so_proto->pr_usrreq)(so, PRU_CONNECT,
		    (struct mbuf *)0, nam, (struct mbuf *)0, p);
	splx(s);
	return (error);
}

int
soconnect2(struct socket *so1, struct socket *so2)
{
	int	s, error;

	s = splsoftnet();
	error = (*so1->so_proto->pr_usrreq)(so1, PRU_CONNECT2,
	    (struct mbuf *)0, (struct mbuf *)so2, (struct mbuf *)0,
	    (struct proc *)0);
	splx(s);
	return (error);
}

int
sodisconnect(struct socket *so)
{
	int	s, error;

	s = splsoftnet();
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto bad;
	}
	if (so->so_state & SS_ISDISCONNECTING) {
		error = EALREADY;
		goto bad;
	}
	error = (*so->so_proto->pr_usrreq)(so, PRU_DISCONNECT,
	    (struct mbuf *)0, (struct mbuf *)0, (struct mbuf *)0,
	    (struct proc *)0);
 bad:
	splx(s);
	sodopendfree(so);
	return (error);
}

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)
/*
 * Send on a socket.
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * Lock against other senders.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 * The data to be sent is described by "uio" if nonzero,
 * otherwise by the mbuf chain "top" (which must be null
 * if uio is not).  Data provided in mbuf chain must be small
 * enough to send all at once.
 *
 * Returns nonzero on error, timeout or signal; callers
 * must check for short counts if EINTR/ERESTART are returned.
 * Data and control buffers are freed on return.
 */
int
sosend(struct socket *so, struct mbuf *addr, struct uio *uio, struct mbuf *top,
	struct mbuf *control, int flags)
{
	struct proc	*p;
	struct mbuf	**mp, *m;
	long		space, len, resid, clen, mlen;
	int		error, s, dontroute, atomic;

	sodopendfree(so);

	p = curproc;		/* XXX */
	clen = 0;
	atomic = sosendallatonce(so) || top;
	if (uio)
		resid = uio->uio_resid;
	else
		resid = top->m_pkthdr.len;
	/*
	 * In theory resid should be unsigned.
	 * However, space must be signed, as it might be less than 0
	 * if we over-committed, and we must use a signed comparison
	 * of space and resid.  On the other hand, a negative resid
	 * causes us to loop sending 0-length segments to the protocol.
	 */
	if (resid < 0) {
		error = EINVAL;
		goto out;
	}
	dontroute =
	    (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 &&
	    (so->so_proto->pr_flags & PR_ATOMIC);
	p->p_stats->p_ru.ru_msgsnd++;
	if (control)
		clen = control->m_len;
#define	snderr(errno)	{ error = errno; splx(s); goto release; }

 restart:
	if ((error = sblock(&so->so_snd, SBLOCKWAIT(flags))) != 0)
		goto out;
	do {
		s = splsoftnet();
		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			splx(s);
			goto release;
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
				if ((so->so_state & SS_ISCONFIRMING) == 0 &&
				    !(resid == 0 && clen != 0))
					snderr(ENOTCONN);
			} else if (addr == 0)
				snderr(EDESTADDRREQ);
		}
		space = sbspace(&so->so_snd);
		if (flags & MSG_OOB)
			space += 1024;
		if ((atomic && resid > so->so_snd.sb_hiwat) ||
		    clen > so->so_snd.sb_hiwat)
			snderr(EMSGSIZE);
		if (space < resid + clen &&
		    (atomic || space < so->so_snd.sb_lowat || space < clen)) {
			if (so->so_state & SS_NBIO)
				snderr(EWOULDBLOCK);
			sbunlock(&so->so_snd);
			error = sbwait(&so->so_snd);
			splx(s);
			if (error)
				goto out;
			goto restart;
		}
		splx(s);
		mp = &top;
		space -= clen;
		do {
			if (uio == NULL) {
				/*
				 * Data is prepackaged in "top".
				 */
				resid = 0;
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
			} else do {
				if (top == 0) {
					m = m_gethdr(M_WAIT, MT_DATA);
					mlen = MHLEN;
					m->m_pkthdr.len = 0;
					m->m_pkthdr.rcvif = (struct ifnet *)0;
				} else {
					m = m_get(M_WAIT, MT_DATA);
					mlen = MLEN;
				}
				MCLAIM(m, so->so_snd.sb_mowner);
				if (use_sosend_loan &&
				    uio->uio_iov->iov_len >= SOCK_LOAN_THRESH &&
				    space >= SOCK_LOAN_THRESH &&
				    (len = sosend_loan(so, uio, m,
						       space)) != 0) {
					SOSEND_COUNTER_INCR(&sosend_loan_big);
					space -= len;
					goto have_data;
				}
				if (resid >= MINCLSIZE && space >= MCLBYTES) {
					SOSEND_COUNTER_INCR(&sosend_copy_big);
					m_clget(m, M_WAIT);
					if ((m->m_flags & M_EXT) == 0)
						goto nopages;
					mlen = MCLBYTES;
					if (atomic && top == 0) {
						len = lmin(MCLBYTES - max_hdr,
						    resid);
						m->m_data += max_hdr;
					} else
						len = lmin(MCLBYTES, resid);
					space -= len;
				} else {
 nopages:
					SOSEND_COUNTER_INCR(&sosend_copy_small);
					len = lmin(lmin(mlen, resid), space);
					space -= len;
					/*
					 * For datagram protocols, leave room
					 * for protocol headers in first mbuf.
					 */
					if (atomic && top == 0 && len < mlen)
						MH_ALIGN(m, len);
				}
				error = uiomove(mtod(m, caddr_t), (int)len,
				    uio);
 have_data:
				resid = uio->uio_resid;
				m->m_len = len;
				*mp = m;
				top->m_pkthdr.len += len;
				if (error)
					goto release;
				mp = &m->m_next;
				if (resid <= 0) {
					if (flags & MSG_EOR)
						top->m_flags |= M_EOR;
					break;
				}
			} while (space > 0 && atomic);
			
			s = splsoftnet();

			if (so->so_state & SS_CANTSENDMORE)
				snderr(EPIPE);

			if (dontroute)
				so->so_options |= SO_DONTROUTE;
			if (resid > 0)
				so->so_state |= SS_MORETOCOME;
			error = (*so->so_proto->pr_usrreq)(so,
			    (flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND,
			    top, addr, control, p);
			if (dontroute)
				so->so_options &= ~SO_DONTROUTE;
			if (resid > 0)
				so->so_state &= ~SS_MORETOCOME;
			splx(s);

			clen = 0;
			control = 0;
			top = 0;
			mp = &top;
			if (error)
				goto release;
		} while (resid && space > 0);
	} while (resid);

 release:
	sbunlock(&so->so_snd);
 out:
	if (top)
		m_freem(top);
	if (control)
		m_freem(control);
	return (error);
}

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * The caller may receive the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  The uio is then used
 * only for the count in uio_resid.
 */
int
soreceive(struct socket *so, struct mbuf **paddr, struct uio *uio,
	struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct mbuf	*m, **mp;
	int		flags, len, error, s, offset, moff, type, orig_resid;
	struct protosw	*pr;
	struct mbuf	*nextrecord;
	int		mbuf_removed = 0;

	pr = so->so_proto;
	mp = mp0;
	type = 0;
	orig_resid = uio->uio_resid;
	if (paddr)
		*paddr = 0;
	if (controlp)
		*controlp = 0;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;

	if ((flags & MSG_DONTWAIT) == 0)
		sodopendfree(so);

	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB, m,
		    (struct mbuf *)(long)(flags & MSG_PEEK), (struct mbuf *)0,
		    (struct proc *)0);
		if (error)
			goto bad;
		do {
			error = uiomove(mtod(m, caddr_t),
			    (int) min(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
 bad:
		if (m)
			m_freem(m);
		return (error);
	}
	if (mp)
		*mp = (struct mbuf *)0;
	if (so->so_state & SS_ISCONFIRMING && uio->uio_resid)
		(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
		    (struct mbuf *)0, (struct mbuf *)0, (struct proc *)0);

 restart:
	if ((error = sblock(&so->so_rcv, SBLOCKWAIT(flags))) != 0)
		return (error);
	s = splsoftnet();

	m = so->so_rcv.sb_mb;
	/*
	 * If we have less data than requested, block awaiting more
	 * (subject to any timeout) if:
	 *   1. the current count is less than the low water mark,
	 *   2. MSG_WAITALL is set, and it is possible to do the entire
	 *	receive operation at once if we block (resid <= hiwat), or
	 *   3. MSG_DONTWAIT is not set.
	 * If MSG_WAITALL is set but resid is larger than the receive buffer,
	 * we have to do the receive in sections, and thus risk returning
	 * a short count if a timeout or signal occurs after we start.
	 */
	if (m == 0 || (((flags & MSG_DONTWAIT) == 0 &&
	    so->so_rcv.sb_cc < uio->uio_resid) &&
	    (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	    ((flags & MSG_WAITALL) && uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	    m->m_nextpkt == 0 && (pr->pr_flags & PR_ATOMIC) == 0)) {
#ifdef DIAGNOSTIC
		if (m == 0 && so->so_rcv.sb_cc)
			panic("receive 1");
#endif
		if (so->so_error) {
			if (m)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			if (m)
				goto dontblock;
			else
				goto release;
		}
		for (; m; m = m->m_next)
			if (m->m_type == MT_OOBDATA  || (m->m_flags & M_EOR)) {
				m = so->so_rcv.sb_mb;
				goto dontblock;
			}
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 1");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 1");
		sbunlock(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		splx(s);
		if (error)
			return (error);
		goto restart;
	}
 dontblock:
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * While we process the initial mbufs containing address and control
	 * info, we save a copy of m->m_nextpkt into nextrecord.
	 */
#ifdef notyet /* XXXX */
	if (uio->uio_procp)
		uio->uio_procp->p_stats->p_ru.ru_msgrcv++;
#endif
	KASSERT(m == so->so_rcv.sb_mb);
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 1");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 1");
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
#endif
		orig_resid = 0;
		if (flags & MSG_PEEK) {
			if (paddr)
				*paddr = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			mbuf_removed = 1;
			if (paddr) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
	}
	while (m && m->m_type == MT_CONTROL && error == 0) {
		if (flags & MSG_PEEK) {
			if (controlp)
				*controlp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			mbuf_removed = 1;
			if (controlp) {
				if (pr->pr_domain->dom_externalize &&
				    mtod(m, struct cmsghdr *)->cmsg_type ==
				    SCM_RIGHTS)
					error = (*pr->pr_domain->dom_externalize)(m);
				*controlp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
		if (controlp) {
			orig_resid = 0;
			controlp = &(*controlp)->m_next;
		}
	}

	/*
	 * If m is non-NULL, we have some data to read.  From now on,
	 * make sure to keep sb_lastrecord consistent when working on
	 * the last packet on the chain (nextrecord == NULL) and we
	 * change m->m_nextpkt.
	 */
	if (m) {
		if ((flags & MSG_PEEK) == 0) {
			m->m_nextpkt = nextrecord;
			/*
			 * If nextrecord == NULL (this is a single chain),
			 * then sb_lastrecord may not be valid here if m
			 * was changed earlier.
			 */
			if (nextrecord == NULL) {
				KASSERT(so->so_rcv.sb_mb == m);
				so->so_rcv.sb_lastrecord = m;
			}
		}
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	} else {
		if ((flags & MSG_PEEK) == 0) {
			KASSERT(so->so_rcv.sb_mb == m);
			so->so_rcv.sb_mb = nextrecord;
			SB_EMPTY_FIXUP(&so->so_rcv);
		}
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 2");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 2");

	moff = 0;
	offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
#ifdef DIAGNOSTIC
		else if (m->m_type != MT_DATA && m->m_type != MT_HEADER)
			panic("receive 3");
#endif
		so->so_state &= ~SS_RCVATMARK;
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		/*
		 * If mp is set, just pass back the mbufs.
		 * Otherwise copy them out via the uio, then free.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (mp == 0) {
			SBLASTRECORDCHK(&so->so_rcv, "soreceive uiomove");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive uiomove");
			splx(s);
			error = uiomove(mtod(m, caddr_t) + moff, (int)len, uio);
			s = splsoftnet();
			if (error) {
				/*
				 * If any part of the record has been removed
				 * (such as the MT_SONAME mbuf, which will
				 * happen when PR_ADDR, and thus also
				 * PR_ATOMIC, is set), then drop the entire
				 * record to maintain the atomicity of the
				 * receive operation.
				 *
				 * This avoids a later panic("receive 1a")
				 * when compiled with DIAGNOSTIC.
				 */
				if (m && mbuf_removed
				    && (pr->pr_flags & PR_ATOMIC))
					(void) sbdroprecord(&so->so_rcv);

				goto release;
			}
		} else
			uio->uio_resid -= len;
		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				sbfree(&so->so_rcv, m);
				if (mp) {
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = (struct mbuf *)0;
				} else {
					MFREE(m, so->so_rcv.sb_mb);
					m = so->so_rcv.sb_mb;
				}
				/*
				 * If m != NULL, we also know that
				 * so->so_rcv.sb_mb != NULL.
				 */
				KASSERT(so->so_rcv.sb_mb == m);
				if (m) {
					m->m_nextpkt = nextrecord;
					if (nextrecord == NULL)
						so->so_rcv.sb_lastrecord = m;
				} else {
					so->so_rcv.sb_mb = nextrecord;
					SB_EMPTY_FIXUP(&so->so_rcv);
				}
				SBLASTRECORDCHK(&so->so_rcv, "soreceive 3");
				SBLASTMBUFCHK(&so->so_rcv, "soreceive 3");
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp)
					*mp = m_copym(m, 0, len, M_WAIT);
				m->m_data += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
			}
		}
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else {
				offset += len;
				if (offset == so->so_oobmark)
					break;
			}
		}
		if (flags & MSG_EOR)
			break;
		/*
		 * If the MSG_WAITALL flag is set (for non-atomic socket),
		 * we must not quit until "uio->uio_resid == 0" or an error
		 * termination.  If a signal/timeout occurs, return
		 * with a short count but without error.
		 * Keep sockbuf locked against other readers.
		 */
		while (flags & MSG_WAITALL && m == 0 && uio->uio_resid > 0 &&
		    !sosendallatonce(so) && !nextrecord) {
			if (so->so_error || so->so_state & SS_CANTRCVMORE)
				break;
			/*
			 * If we are peeking and the socket receive buffer is
			 * full, stop since we can't get more data to peek at.
			 */
			if ((flags & MSG_PEEK) && sbspace(&so->so_rcv) <= 0)
				break;
			/*
			 * If we've drained the socket buffer, tell the
			 * protocol in case it needs to do something to
			 * get it filled again.
			 */
			if ((pr->pr_flags & PR_WANTRCVD) && so->so_pcb)
				(*pr->pr_usrreq)(so, PRU_RCVD,
				    (struct mbuf *)0,
				    (struct mbuf *)(long)flags,
				    (struct mbuf *)0,
				    (struct proc *)0);
			SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 2");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 2");
			error = sbwait(&so->so_rcv);
			if (error) {
				sbunlock(&so->so_rcv);
				splx(s);
				return (0);
			}
			if ((m = so->so_rcv.sb_mb) != NULL)
				nextrecord = m->m_nextpkt;
		}
	}

	if (m && pr->pr_flags & PR_ATOMIC) {
		flags |= MSG_TRUNC;
		if ((flags & MSG_PEEK) == 0)
			(void) sbdroprecord(&so->so_rcv);
	}
	if ((flags & MSG_PEEK) == 0) {
		if (m == 0) {
			/*
			 * First part is an inline SB_EMPTY_FIXUP().  Second
			 * part makes sure sb_lastrecord is up-to-date if
			 * there is still data in the socket buffer.
			 */
			so->so_rcv.sb_mb = nextrecord;
			if (so->so_rcv.sb_mb == NULL) {
				so->so_rcv.sb_mbtail = NULL;
				so->so_rcv.sb_lastrecord = NULL;
			} else if (nextrecord->m_nextpkt == NULL)
				so->so_rcv.sb_lastrecord = nextrecord;
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive 4");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive 4");
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
			    (struct mbuf *)(long)flags, (struct mbuf *)0,
			    (struct proc *)0);
	}
	if (orig_resid == uio->uio_resid && orig_resid &&
	    (flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		splx(s);
		goto restart;
	}
		
	if (flagsp)
		*flagsp |= flags;
 release:
	sbunlock(&so->so_rcv);
	splx(s);
	return (error);
}

int
soshutdown(struct socket *so, int how)
{
	struct protosw	*pr;

	pr = so->so_proto;
	if (!(how == SHUT_RD || how == SHUT_WR || how == SHUT_RDWR))
		return (EINVAL);

	if (how == SHUT_RD || how == SHUT_RDWR)
		sorflush(so);
	if (how == SHUT_WR || how == SHUT_RDWR)
		return (*pr->pr_usrreq)(so, PRU_SHUTDOWN, (struct mbuf *)0,
		    (struct mbuf *)0, (struct mbuf *)0, (struct proc *)0);
	return (0);
}

void
sorflush(struct socket *so)
{
	struct sockbuf	*sb, asb;
	struct protosw	*pr;
	int		s;

	sb = &so->so_rcv;
	pr = so->so_proto;
	sb->sb_flags |= SB_NOINTR;
	(void) sblock(sb, M_WAITOK);
	s = splnet();
	socantrcvmore(so);
	sbunlock(sb);
	asb = *sb;
	/*
	 * Clear most of the sockbuf structure, but leave some of the
	 * fields valid.
	 */
	memset(&sb->sb_startzero, 0,
	    sizeof(*sb) - offsetof(struct sockbuf, sb_startzero));
	splx(s);
	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
		(*pr->pr_domain->dom_dispose)(asb.sb_mb);
	sbrelease(&asb);
}

int
sosetopt(struct socket *so, int level, int optname, struct mbuf *m0)
{
	int		error;
	struct mbuf	*m;

	error = 0;
	m = m0;
	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput)
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
		error = ENOPROTOOPT;
	} else {
		switch (optname) {

		case SO_LINGER:
			if (m == NULL || m->m_len != sizeof(struct linger)) {
				error = EINVAL;
				goto bad;
			}
			if (mtod(m, struct linger *)->l_linger < 0 ||
			    mtod(m, struct linger *)->l_linger > (INT_MAX / hz)) {
				error = EDOM;
				goto bad;
			}
			so->so_linger = mtod(m, struct linger *)->l_linger;
			/* fall thru... */

		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_OOBINLINE:
		case SO_TIMESTAMP:
			if (m == NULL || m->m_len < sizeof(int)) {
				error = EINVAL;
				goto bad;
			}
			if (*mtod(m, int *))
				so->so_options |= optname;
			else
				so->so_options &= ~optname;
			break;

		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		    {
			int optval;

			if (m == NULL || m->m_len < sizeof(int)) {
				error = EINVAL;
				goto bad;
			}

			/*
			 * Values < 1 make no sense for any of these
			 * options, so disallow them.
			 */
			optval = *mtod(m, int *);
			if (optval < 1) {
				error = EINVAL;
				goto bad;
			}

			switch (optname) {

			case SO_SNDBUF:
			case SO_RCVBUF:
				if (sbreserve(optname == SO_SNDBUF ?
				    &so->so_snd : &so->so_rcv,
				    (u_long) optval) == 0) {
					error = ENOBUFS;
					goto bad;
				}
				break;

			/*
			 * Make sure the low-water is never greater than
			 * the high-water.
			 */
			case SO_SNDLOWAT:
				so->so_snd.sb_lowat =
				    (optval > so->so_snd.sb_hiwat) ?
				    so->so_snd.sb_hiwat : optval;
				break;
			case SO_RCVLOWAT:
				so->so_rcv.sb_lowat =
				    (optval > so->so_rcv.sb_hiwat) ?
				    so->so_rcv.sb_hiwat : optval;
				break;
			}
			break;
		    }

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
			struct timeval *tv;
			short val;

			if (m == NULL || m->m_len < sizeof(*tv)) {
				error = EINVAL;
				goto bad;
			}
			tv = mtod(m, struct timeval *);
			if (tv->tv_sec > (SHRT_MAX - tv->tv_usec / tick) / hz) {
				error = EDOM;
				goto bad;
			}
			val = tv->tv_sec * hz + tv->tv_usec / tick;
			if (val == 0 && tv->tv_usec != 0)
				val = 1;

			switch (optname) {

			case SO_SNDTIMEO:
				so->so_snd.sb_timeo = val;
				break;
			case SO_RCVTIMEO:
				so->so_rcv.sb_timeo = val;
				break;
			}
			break;
		    }

		default:
			error = ENOPROTOOPT;
			break;
		}
		if (error == 0 && so->so_proto && so->so_proto->pr_ctloutput) {
			(void) ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
			m = NULL;	/* freed by protocol */
		}
	}
 bad:
	if (m)
		(void) m_free(m);
	return (error);
}

int
sogetopt(struct socket *so, int level, int optname, struct mbuf **mp)
{
	struct mbuf	*m;

	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_GETOPT, so, level, optname, mp));
		} else
			return (ENOPROTOOPT);
	} else {
		m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sizeof(int);

		switch (optname) {

		case SO_LINGER:
			m->m_len = sizeof(struct linger);
			mtod(m, struct linger *)->l_onoff =
				so->so_options & SO_LINGER;
			mtod(m, struct linger *)->l_linger = so->so_linger;
			break;

		case SO_USELOOPBACK:
		case SO_DONTROUTE:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_BROADCAST:
		case SO_OOBINLINE:
		case SO_TIMESTAMP:
			*mtod(m, int *) = so->so_options & optname;
			break;

		case SO_TYPE:
			*mtod(m, int *) = so->so_type;
			break;

		case SO_ERROR:
			*mtod(m, int *) = so->so_error;
			so->so_error = 0;
			break;

		case SO_SNDBUF:
			*mtod(m, int *) = so->so_snd.sb_hiwat;
			break;

		case SO_RCVBUF:
			*mtod(m, int *) = so->so_rcv.sb_hiwat;
			break;

		case SO_SNDLOWAT:
			*mtod(m, int *) = so->so_snd.sb_lowat;
			break;

		case SO_RCVLOWAT:
			*mtod(m, int *) = so->so_rcv.sb_lowat;
			break;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
			int val = (optname == SO_SNDTIMEO ?
			     so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

			m->m_len = sizeof(struct timeval);
			mtod(m, struct timeval *)->tv_sec = val / hz;
			mtod(m, struct timeval *)->tv_usec =
			    (val % hz) * tick;
			break;
		    }

		default:
			(void)m_free(m);
			return (ENOPROTOOPT);
		}
		*mp = m;
		return (0);
	}
}

void
sohasoutofband(struct socket *so)
{
	fownsignal(so->so_pgid, SIGURG, POLL_PRI, POLLPRI|POLLRDBAND, so);
	selwakeup(&so->so_rcv.sb_sel);
}

static void
filt_sordetach(struct knote *kn)
{
	struct socket	*so;

	so = (struct socket *)kn->kn_fp->f_data;
	SLIST_REMOVE(&so->so_rcv.sb_sel.sel_klist, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_rcv.sb_sel.sel_klist))
		so->so_rcv.sb_flags &= ~SB_KNOTE;
}

/*ARGSUSED*/
static int
filt_soread(struct knote *kn, long hint)
{
	struct socket	*so;

	so = (struct socket *)kn->kn_fp->f_data;
	kn->kn_data = so->so_rcv.sb_cc;
	if (so->so_state & SS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF; 
		kn->kn_fflags = so->so_error;
		return (1);
	}
	if (so->so_error)	/* temporary udp error */
		return (1);
	if (kn->kn_sfflags & NOTE_LOWAT)
		return (kn->kn_data >= kn->kn_sdata);
	return (kn->kn_data >= so->so_rcv.sb_lowat);
}

static void
filt_sowdetach(struct knote *kn)
{
	struct socket	*so;

	so = (struct socket *)kn->kn_fp->f_data;
	SLIST_REMOVE(&so->so_snd.sb_sel.sel_klist, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_snd.sb_sel.sel_klist))
		so->so_snd.sb_flags &= ~SB_KNOTE;
}

/*ARGSUSED*/
static int
filt_sowrite(struct knote *kn, long hint)
{
	struct socket	*so;

	so = (struct socket *)kn->kn_fp->f_data;
	kn->kn_data = sbspace(&so->so_snd);
	if (so->so_state & SS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF; 
		kn->kn_fflags = so->so_error;
		return (1);
	}
	if (so->so_error)	/* temporary udp error */
		return (1);
	if (((so->so_state & SS_ISCONNECTED) == 0) &&
	    (so->so_proto->pr_flags & PR_CONNREQUIRED))
		return (0);
	if (kn->kn_sfflags & NOTE_LOWAT)
		return (kn->kn_data >= kn->kn_sdata);
	return (kn->kn_data >= so->so_snd.sb_lowat);
}

/*ARGSUSED*/
static int
filt_solisten(struct knote *kn, long hint)
{
	struct socket	*so;

	so = (struct socket *)kn->kn_fp->f_data;

	/*
	 * Set kn_data to number of incoming connections, not
	 * counting partial (incomplete) connections.
	 */ 
	kn->kn_data = so->so_qlen;
	return (kn->kn_data > 0);
}

static const struct filterops solisten_filtops =
	{ 1, NULL, filt_sordetach, filt_solisten };
static const struct filterops soread_filtops =
	{ 1, NULL, filt_sordetach, filt_soread };
static const struct filterops sowrite_filtops =
	{ 1, NULL, filt_sowdetach, filt_sowrite };

int
soo_kqfilter(struct file *fp, struct knote *kn)
{
	struct socket	*so;
	struct sockbuf	*sb;

	so = (struct socket *)kn->kn_fp->f_data;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		if (so->so_options & SO_ACCEPTCONN)
			kn->kn_fop = &solisten_filtops;
		else
			kn->kn_fop = &soread_filtops;
		sb = &so->so_rcv;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &sowrite_filtops;
		sb = &so->so_snd;
		break;
	default:
		return (1);
	}
	SLIST_INSERT_HEAD(&sb->sb_sel.sel_klist, kn, kn_selnext);
	sb->sb_flags |= SB_KNOTE;
	return (0);
}

#include <sys/sysctl.h>

static int sysctl_kern_somaxkva(SYSCTLFN_PROTO);

/*
 * sysctl helper routine for kern.somaxkva.  ensures that the given
 * value is not too small.
 * (XXX should we maybe make sure it's not too large as well?)
 */
static int
sysctl_kern_somaxkva(SYSCTLFN_ARGS)
{
	int error, new_somaxkva;
	struct sysctlnode node;
	int s;

	new_somaxkva = somaxkva;
	node = *rnode;
	node.sysctl_data = &new_somaxkva;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (new_somaxkva < (16 * 1024 * 1024)) /* sanity */
		return (EINVAL);

	s = splvm();
	simple_lock(&so_pendfree_slock);
	somaxkva = new_somaxkva;
	wakeup(&socurkva);
	simple_unlock(&so_pendfree_slock);
	splx(s);

	return (error);
}

SYSCTL_SETUP(sysctl_kern_somaxkva_setup, "sysctl kern.somaxkva setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "kern", NULL,
		       NULL, 0, NULL, 0,
		       CTL_KERN, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "somaxkva",
		       SYSCTL_DESCR("Maximum amount of kernel memory to be "
				    "used for socket buffers"),
		       sysctl_kern_somaxkva, 0, NULL, 0,
		       CTL_KERN, KERN_SOMAXKVA, CTL_EOL);
}
