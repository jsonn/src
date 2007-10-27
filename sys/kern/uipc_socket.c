/*	$NetBSD: uipc_socket.c,v 1.111.2.8 2007/10/27 11:35:38 yamt Exp $	*/

/*-
 * Copyright (c) 2002, 2007 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: uipc_socket.c,v 1.111.2.8 2007/10/27 11:35:38 yamt Exp $");

#include "opt_sock_counters.h"
#include "opt_sosend_loan.h"
#include "opt_mbuftrace.h"
#include "opt_somaxkva.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
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
#include <sys/kauth.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#include <uvm/uvm.h>

POOL_INIT(socket_pool, sizeof(struct socket), 0, 0, 0, "sockpl", NULL,
    IPL_SOFTNET);

MALLOC_DEFINE(M_SOOPTS, "soopts", "socket options");
MALLOC_DEFINE(M_SONAME, "soname", "socket name");

extern const struct fileops socketops;

extern int	somaxconn;			/* patchable (XXX sysctl) */
int		somaxconn = SOMAXCONN;

#ifdef SOSEND_COUNTERS
#include <sys/device.h>

static struct evcnt sosend_loan_big = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "loan big");
static struct evcnt sosend_copy_big = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "copy big");
static struct evcnt sosend_copy_small = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "copy small");
static struct evcnt sosend_kvalimit = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "kva limit");

#define	SOSEND_COUNTER_INCR(ev)		(ev)->ev_count++

EVCNT_ATTACH_STATIC(sosend_loan_big);
EVCNT_ATTACH_STATIC(sosend_copy_big);
EVCNT_ATTACH_STATIC(sosend_copy_small);
EVCNT_ATTACH_STATIC(sosend_kvalimit);
#else

#define	SOSEND_COUNTER_INCR(ev)		/* nothing */

#endif /* SOSEND_COUNTERS */

static struct callback_entry sokva_reclaimerentry;

#ifdef SOSEND_NO_LOAN
int sock_loan_thresh = -1;
#else
int sock_loan_thresh = 4096;
#endif

static kmutex_t so_pendfree_lock;
static struct mbuf *so_pendfree;

#ifndef SOMAXKVA
#define	SOMAXKVA (16 * 1024 * 1024)
#endif
int somaxkva = SOMAXKVA;
static int socurkva;
static kcondvar_t socurkva_cv;

#define	SOCK_LOAN_CHUNK		65536

static size_t sodopendfree(void);
static size_t sodopendfreel(void);

static vsize_t
sokvareserve(struct socket *so, vsize_t len)
{
	int error;

	mutex_enter(&so_pendfree_lock);
	while (socurkva + len > somaxkva) {
		size_t freed;

		/*
		 * try to do pendfree.
		 */

		freed = sodopendfreel();

		/*
		 * if some kva was freed, try again.
		 */

		if (freed)
			continue;

		SOSEND_COUNTER_INCR(&sosend_kvalimit);
		error = cv_wait_sig(&socurkva_cv, &so_pendfree_lock);
		if (error) {
			len = 0;
			break;
		}
	}
	socurkva += len;
	mutex_exit(&so_pendfree_lock);
	return len;
}

static void
sokvaunreserve(vsize_t len)
{

	mutex_enter(&so_pendfree_lock);
	socurkva -= len;
	cv_broadcast(&socurkva_cv);
	mutex_exit(&so_pendfree_lock);
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

	if (sokvareserve(so, len) == 0)
		return 0;

	/*
	 * allocate kva.
	 */

	lva = uvm_km_alloc(kernel_map, len, 0, UVM_KMF_VAONLY | UVM_KMF_WAITVA);
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

	uvm_km_free(kernel_map, sva, len, UVM_KMF_VAONLY);

	/*
	 * unreserve kva.
	 */

	sokvaunreserve(len);
}

static void
sodoloanfree(struct vm_page **pgs, void *buf, size_t size, bool mapped)
{
	vaddr_t sva, eva;
	vsize_t len;
	int npgs;

	KASSERT(pgs != NULL);

	eva = round_page((vaddr_t) buf + size);
	sva = trunc_page((vaddr_t) buf);
	len = eva - sva;
	npgs = len >> PAGE_SHIFT;

	if (mapped) {
		pmap_kremove(sva, len);
		pmap_update(pmap_kernel());
	}
	uvm_unloan(pgs, npgs, UVM_LOAN_TOPAGE);
	sokvafree(sva, len);
}

static size_t
sodopendfree()
{
	size_t rv;

	mutex_enter(&so_pendfree_lock);
	rv = sodopendfreel();
	mutex_exit(&so_pendfree_lock);

	return rv;
}

/*
 * sodopendfreel: free mbufs on "pendfree" list.
 * unlock and relock so_pendfree_lock when freeing mbufs.
 *
 * => called with so_pendfree_lock held.
 */

static size_t
sodopendfreel()
{
	struct mbuf *m, *next;
	size_t rv = 0;
	int s;

	KASSERT(mutex_owned(&so_pendfree_lock));

	while (so_pendfree != NULL) {
		m = so_pendfree;
		so_pendfree = NULL;
		mutex_exit(&so_pendfree_lock);

		for (; m != NULL; m = next) {
			next = m->m_next;
			KASSERT((~m->m_flags & (M_EXT|M_EXT_PAGES)) == 0);
			KASSERT(m->m_ext.ext_refcnt == 0);

			rv += m->m_ext.ext_size;
			sodoloanfree(m->m_ext.ext_pgs, m->m_ext.ext_buf,
			    m->m_ext.ext_size,
			    (m->m_ext.ext_flags & M_EXT_LAZY) == 0);
			s = splvm();
			pool_cache_put(&mbpool_cache, m);
			splx(s);
		}

		mutex_enter(&so_pendfree_lock);
	}

	return (rv);
}

void
soloanfree(struct mbuf *m, void *buf, size_t size, void *arg)
{

	KASSERT(m != NULL);

	/*
	 * postpone freeing mbuf.
	 *
	 * we can't do it in interrupt context
	 * because we need to put kva back to kernel_map.
	 */

	mutex_enter(&so_pendfree_lock);
	m->m_next = so_pendfree;
	so_pendfree = m;
	cv_broadcast(&socurkva_cv);
	mutex_exit(&so_pendfree_lock);
}

static long
sosend_loan(struct socket *so, struct uio *uio, struct mbuf *m, long space)
{
	struct iovec *iov = uio->uio_iov;
	vaddr_t sva, eva;
	vsize_t len;
	vaddr_t lva;
	int npgs, error;
#if !defined(__HAVE_LAZY_MBUF)
	vaddr_t va;
	int i;
#endif /* !defined(__HAVE_LAZY_MBUF) */

	if (VMSPACE_IS_KERNEL_P(uio->uio_vmspace))
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

	error = uvm_loan(&uio->uio_vmspace->vm_map, sva, len,
	    m->m_ext.ext_pgs, UVM_LOAN_TOPAGE);
	if (error) {
		sokvafree(lva, len);
		return (0);
	}

#if !defined(__HAVE_LAZY_MBUF)
	for (i = 0, va = lva; i < npgs; i++, va += PAGE_SIZE)
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(m->m_ext.ext_pgs[i]),
		    VM_PROT_READ);
	pmap_update(pmap_kernel());
#endif /* !defined(__HAVE_LAZY_MBUF) */

	lva += (vaddr_t) iov->iov_base & PAGE_MASK;

	MEXTADD(m, (void *) lva, space, M_MBUF, soloanfree, so);
	m->m_flags |= M_EXT_PAGES | M_EXT_ROMAP;

#if defined(__HAVE_LAZY_MBUF)
	m->m_flags |= M_EXT_LAZY;
	m->m_ext.ext_flags |= M_EXT_LAZY;
#endif /* defined(__HAVE_LAZY_MBUF) */

	uio->uio_resid -= space;
	/* uio_offset not updated, not set/used for write(2) */
	uio->uio_iov->iov_base = (char *)uio->uio_iov->iov_base + space;
	uio->uio_iov->iov_len -= space;
	if (uio->uio_iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}

	return (space);
}

static int
sokva_reclaim_callback(struct callback_entry *ce, void *obj, void *arg)
{

	KASSERT(ce == &sokva_reclaimerentry);
	KASSERT(obj == NULL);

	sodopendfree();
	if (!vm_map_starved_p(kernel_map)) {
		return CALLBACK_CHAIN_ABORT;
	}
	return CALLBACK_CHAIN_CONTINUE;
}

struct mbuf *
getsombuf(struct socket *so)
{
	struct mbuf *m;

	m = m_get(M_WAIT, MT_SONAME);
	MCLAIM(m, so->so_mowner);
	return m;
}

struct mbuf *
m_intopt(struct socket *so, int val)
{
	struct mbuf *m;

	m = getsombuf(so);
	m->m_len = sizeof(int);
	*mtod(m, int *) = val;
	return m;
}

void
soinit(void)
{

	mutex_init(&so_pendfree_lock, MUTEX_DRIVER, IPL_VM);
	cv_init(&socurkva_cv, "sokva");

	/* Set the initial adjusted socket buffer size. */
	if (sb_max_set(sb_max))
		panic("bad initial sb_max value: %lu", sb_max);

	callback_register(&vm_map_to_kernel(kernel_map)->vmk_reclaim_callback,
	    &sokva_reclaimerentry, NULL, sokva_reclaim_callback);
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
socreate(int dom, struct socket **aso, int type, int proto, struct lwp *l)
{
	const struct protosw	*prp;
	struct socket	*so;
	uid_t		uid;
	int		error, s;

	error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_SOCKET,
	    KAUTH_REQ_NETWORK_SOCKET_OPEN, KAUTH_ARG(dom), KAUTH_ARG(type),
	    KAUTH_ARG(proto));
	if (error != 0)
		return error;

	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);
	if (prp == NULL) {
		/* no support for domain */
		if (pffinddomain(dom) == 0)
			return EAFNOSUPPORT;
		/* no support for socket type */
		if (proto == 0 && type != 0)
			return EPROTOTYPE;
		return EPROTONOSUPPORT;
	}
	if (prp->pr_usrreq == NULL)
		return EPROTONOSUPPORT;
	if (prp->pr_type != type)
		return EPROTOTYPE;
	s = splsoftnet();
	so = pool_get(&socket_pool, PR_WAITOK);
	memset(so, 0, sizeof(*so));
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
	selinit(&so->so_rcv.sb_sel);
	selinit(&so->so_snd.sb_sel);
	uid = kauth_cred_geteuid(l->l_cred);
	so->so_uidinfo = uid_find(uid);
	error = (*prp->pr_usrreq)(so, PRU_ATTACH, NULL,
	    (struct mbuf *)(long)proto, NULL, l);
	if (error != 0) {
		so->so_state |= SS_NOFDREF;
		sofree(so);
		splx(s);
		return error;
	}
	splx(s);
	*aso = so;
	return 0;
}

/* On success, write file descriptor to fdout and return zero.  On
 * failure, return non-zero; *fdout will be undefined.
 */
int
fsocreate(int domain, struct socket **sop, int type, int protocol,
    struct lwp *l, int *fdout)
{
	struct filedesc	*fdp;
	struct socket	*so;
	struct file	*fp;
	int		fd, error;

	fdp = l->l_proc->p_fd;
	/* falloc() will use the desciptor for us */
	if ((error = falloc(l, &fp, &fd)) != 0)
		return (error);
	fp->f_flag = FREAD|FWRITE;
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;
	error = socreate(domain, &so, type, protocol, l);
	if (error != 0) {
		FILE_UNUSE(fp, l);
		fdremove(fdp, fd);
		ffree(fp);
	} else {
		if (sop != NULL)
			*sop = so;
		fp->f_data = so;
		FILE_SET_MATURE(fp);
		FILE_UNUSE(fp, l);
		*fdout = fd;
	}
	return error;
}

int
sobind(struct socket *so, struct mbuf *nam, struct lwp *l)
{
	int	s, error;

	s = splsoftnet();
	error = (*so->so_proto->pr_usrreq)(so, PRU_BIND, NULL, nam, NULL, l);
	splx(s);
	return error;
}

int
solisten(struct socket *so, int backlog)
{
	int	s, error;

	s = splsoftnet();
	error = (*so->so_proto->pr_usrreq)(so, PRU_LISTEN, NULL,
	    NULL, NULL, NULL);
	if (error != 0) {
		splx(s);
		return error;
	}
	if (TAILQ_EMPTY(&so->so_q))
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0)
		backlog = 0;
	so->so_qlimit = min(backlog, somaxconn);
	splx(s);
	return 0;
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
	if (so->so_rcv.sb_hiwat)
		(void)chgsbsize(so->so_uidinfo, &so->so_rcv.sb_hiwat, 0,
		    RLIM_INFINITY);
	if (so->so_snd.sb_hiwat)
		(void)chgsbsize(so->so_uidinfo, &so->so_snd.sb_hiwat, 0,
		    RLIM_INFINITY);
	sbrelease(&so->so_snd, so);
	sorflush(so);
	seldestroy(&so->so_rcv.sb_sel);
	seldestroy(&so->so_snd.sb_sel);
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
				error = tsleep((void *)&so->so_timeo,
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
		    NULL, NULL, NULL, NULL);
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
	int error;

	KASSERT(so->so_head == NULL);
	error = (*so->so_proto->pr_usrreq)(so, PRU_ABORT, NULL,
	    NULL, NULL, NULL);
	if (error) {
		sofree(so);
	}
	return error;
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
		    NULL, nam, NULL, NULL);
	else
		error = ECONNABORTED;

	splx(s);
	return (error);
}

int
soconnect(struct socket *so, struct mbuf *nam, struct lwp *l)
{
	int		s, error;

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
		    NULL, nam, NULL, l);
	splx(s);
	return (error);
}

int
soconnect2(struct socket *so1, struct socket *so2)
{
	int	s, error;

	s = splsoftnet();
	error = (*so1->so_proto->pr_usrreq)(so1, PRU_CONNECT2,
	    NULL, (struct mbuf *)so2, NULL, NULL);
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
	    NULL, NULL, NULL, NULL);
 bad:
	splx(s);
	sodopendfree();
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
	struct mbuf *control, int flags, struct lwp *l)
{
	struct mbuf	**mp, *m;
	struct proc	*p;
	long		space, len, resid, clen, mlen;
	int		error, s, dontroute, atomic;

	p = l->l_proc;
	sodopendfree();

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
	if (p)
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
				if (top == NULL) {
					m = m_gethdr(M_WAIT, MT_DATA);
					mlen = MHLEN;
					m->m_pkthdr.len = 0;
					m->m_pkthdr.rcvif = NULL;
				} else {
					m = m_get(M_WAIT, MT_DATA);
					mlen = MLEN;
				}
				MCLAIM(m, so->so_snd.sb_mowner);
				if (sock_loan_thresh >= 0 &&
				    uio->uio_iov->iov_len >= sock_loan_thresh &&
				    space >= sock_loan_thresh &&
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
				error = uiomove(mtod(m, void *), (int)len, uio);
 have_data:
				resid = uio->uio_resid;
				m->m_len = len;
				*mp = m;
				top->m_pkthdr.len += len;
				if (error != 0)
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
			    top, addr, control, curlwp);	/* XXX */
			if (dontroute)
				so->so_options &= ~SO_DONTROUTE;
			if (resid > 0)
				so->so_state &= ~SS_MORETOCOME;
			splx(s);

			clen = 0;
			control = NULL;
			top = NULL;
			mp = &top;
			if (error != 0)
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
	struct lwp *l = curlwp;
	struct mbuf	*m, **mp;
	int		flags, len, error, s, offset, moff, type, orig_resid;
	const struct protosw	*pr;
	struct mbuf	*nextrecord;
	int		mbuf_removed = 0;

	pr = so->so_proto;
	mp = mp0;
	type = 0;
	orig_resid = uio->uio_resid;

	if (paddr != NULL)
		*paddr = NULL;
	if (controlp != NULL)
		*controlp = NULL;
	if (flagsp != NULL)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;

	if ((flags & MSG_DONTWAIT) == 0)
		sodopendfree();

	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB, m,
		    (struct mbuf *)(long)(flags & MSG_PEEK), NULL, l);
		if (error)
			goto bad;
		do {
			error = uiomove(mtod(m, void *),
			    (int) min(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid > 0 && error == 0 && m);
 bad:
		if (m != NULL)
			m_freem(m);
		return error;
	}
	if (mp != NULL)
		*mp = NULL;
	if (so->so_state & SS_ISCONFIRMING && uio->uio_resid)
		(*pr->pr_usrreq)(so, PRU_RCVD, NULL, NULL, NULL, l);

 restart:
	if ((error = sblock(&so->so_rcv, SBLOCKWAIT(flags))) != 0)
		return error;
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
	if (m == NULL ||
	    ((flags & MSG_DONTWAIT) == 0 &&
	     so->so_rcv.sb_cc < uio->uio_resid &&
	     (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	      ((flags & MSG_WAITALL) &&
	       uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	     m->m_nextpkt == NULL &&
	     (pr->pr_flags & PR_ATOMIC) == 0)) {
#ifdef DIAGNOSTIC
		if (m == NULL && so->so_rcv.sb_cc)
			panic("receive 1");
#endif
		if (so->so_error) {
			if (m != NULL)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			if (m != NULL)
				goto dontblock;
			else
				goto release;
		}
		for (; m != NULL; m = m->m_next)
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
		if (error != 0)
			return error;
		goto restart;
	}
 dontblock:
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * While we process the initial mbufs containing address and control
	 * info, we save a copy of m->m_nextpkt into nextrecord.
	 */
	if (l != NULL)
		l->l_proc->p_stats->p_ru.ru_msgrcv++;
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
			if (paddr != NULL) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = NULL;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
	}
	while (m != NULL && m->m_type == MT_CONTROL && error == 0) {
		if (flags & MSG_PEEK) {
			if (controlp != NULL)
				*controlp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			mbuf_removed = 1;
			if (controlp != NULL) {
				struct domain *dom = pr->pr_domain;
				if (dom->dom_externalize && l &&
				    mtod(m, struct cmsghdr *)->cmsg_type ==
				    SCM_RIGHTS)
					error = (*dom->dom_externalize)(m, l);
				*controlp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = NULL;
				m = so->so_rcv.sb_mb;
			} else {
				/*
				 * Dispose of any SCM_RIGHTS message that went
				 * through the read path rather than recv.
				 */
				if (pr->pr_domain->dom_dispose &&
				    mtod(m, struct cmsghdr *)->cmsg_type == SCM_RIGHTS)
					(*pr->pr_domain->dom_dispose)(m);
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
		if (controlp != NULL) {
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
	if (m != NULL) {
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
	while (m != NULL && uio->uio_resid > 0 && error == 0) {
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
		if (mp == NULL) {
			SBLASTRECORDCHK(&so->so_rcv, "soreceive uiomove");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive uiomove");
			splx(s);
			error = uiomove(mtod(m, char *) + moff, (int)len, uio);
			s = splsoftnet();
			if (error != 0) {
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
					*mp = NULL;
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
		} else if (flags & MSG_PEEK)
			moff += len;
		else {
			if (mp != NULL)
				*mp = m_copym(m, 0, len, M_WAIT);
			m->m_data += len;
			m->m_len -= len;
			so->so_rcv.sb_cc -= len;
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
		while (flags & MSG_WAITALL && m == NULL && uio->uio_resid > 0 &&
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
				    NULL, (struct mbuf *)(long)flags, NULL, l);
			SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 2");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 2");
			error = sbwait(&so->so_rcv);
			if (error != 0) {
				sbunlock(&so->so_rcv);
				splx(s);
				return 0;
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
		if (m == NULL) {
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
			(*pr->pr_usrreq)(so, PRU_RCVD, NULL,
			    (struct mbuf *)(long)flags, NULL, l);
	}
	if (orig_resid == uio->uio_resid && orig_resid &&
	    (flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		splx(s);
		goto restart;
	}

	if (flagsp != NULL)
		*flagsp |= flags;
 release:
	sbunlock(&so->so_rcv);
	splx(s);
	return error;
}

int
soshutdown(struct socket *so, int how)
{
	const struct protosw	*pr;

	pr = so->so_proto;
	if (!(how == SHUT_RD || how == SHUT_WR || how == SHUT_RDWR))
		return (EINVAL);

	if (how == SHUT_RD || how == SHUT_RDWR)
		sorflush(so);
	if (how == SHUT_WR || how == SHUT_RDWR)
		return (*pr->pr_usrreq)(so, PRU_SHUTDOWN, NULL,
		    NULL, NULL, NULL);
	return 0;
}

void
sorflush(struct socket *so)
{
	struct sockbuf	*sb, asb;
	const struct protosw	*pr;
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
	sbrelease(&asb, so);
}

static int
sosetopt1(struct socket *so, int level, int optname, struct mbuf *m)
{
	int optval, val;
	struct linger	*l;
	struct sockbuf	*sb;
	struct timeval *tv;

	switch (optname) {

	case SO_LINGER:
		if (m == NULL || m->m_len != sizeof(struct linger))
			return EINVAL;
		l = mtod(m, struct linger *);
		if (l->l_linger < 0 || l->l_linger > USHRT_MAX ||
		    l->l_linger > (INT_MAX / hz))
			return EDOM;
		so->so_linger = l->l_linger;
		if (l->l_onoff)
			so->so_options |= SO_LINGER;
		else
			so->so_options &= ~SO_LINGER;
		break;

	case SO_DEBUG:
	case SO_KEEPALIVE:
	case SO_DONTROUTE:
	case SO_USELOOPBACK:
	case SO_BROADCAST:
	case SO_REUSEADDR:
	case SO_REUSEPORT:
	case SO_OOBINLINE:
	case SO_TIMESTAMP:
		if (m == NULL || m->m_len < sizeof(int))
			return EINVAL;
		if (*mtod(m, int *))
			so->so_options |= optname;
		else
			so->so_options &= ~optname;
		break;

	case SO_SNDBUF:
	case SO_RCVBUF:
	case SO_SNDLOWAT:
	case SO_RCVLOWAT:
		if (m == NULL || m->m_len < sizeof(int))
			return EINVAL;

		/*
		 * Values < 1 make no sense for any of these
		 * options, so disallow them.
		 */
		optval = *mtod(m, int *);
		if (optval < 1)
			return EINVAL;

		switch (optname) {

		case SO_SNDBUF:
		case SO_RCVBUF:
			sb = (optname == SO_SNDBUF) ?
			    &so->so_snd : &so->so_rcv;
			if (sbreserve(sb, (u_long)optval, so) == 0)
				return ENOBUFS;
			sb->sb_flags &= ~SB_AUTOSIZE;
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

	case SO_SNDTIMEO:
	case SO_RCVTIMEO:
		if (m == NULL || m->m_len < sizeof(*tv))
			return EINVAL;
		tv = mtod(m, struct timeval *);
		if (tv->tv_sec > (INT_MAX - tv->tv_usec / tick) / hz)
			return EDOM;
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

	default:
		return ENOPROTOOPT;
	}
	return 0;
}

int
sosetopt(struct socket *so, int level, int optname, struct mbuf *m)
{
	int error, prerr;

	if (level == SOL_SOCKET)
		error = sosetopt1(so, level, optname, m);
	else
		error = ENOPROTOOPT;

	if ((error == 0 || error == ENOPROTOOPT) &&
	    so->so_proto != NULL && so->so_proto->pr_ctloutput != NULL) {
		/* give the protocol stack a shot */
		prerr = (*so->so_proto->pr_ctloutput)(PRCO_SETOPT, so, level,
		    optname, &m);
		if (prerr == 0)
			error = 0;
		else if (prerr != ENOPROTOOPT)
			error = prerr;
	} else if (m != NULL)
		(void)m_free(m);
	return error;
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
			    (so->so_options & SO_LINGER) ? 1 : 0;
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
			*mtod(m, int *) = (so->so_options & optname) ? 1 : 0;
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

		case SO_OVERFLOWED:
			*mtod(m, int *) = so->so_rcv.sb_overflowed;
			break;

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

	new_somaxkva = somaxkva;
	node = *rnode;
	node.sysctl_data = &new_somaxkva;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (new_somaxkva < (16 * 1024 * 1024)) /* sanity */
		return (EINVAL);

	mutex_enter(&so_pendfree_lock);
	somaxkva = new_somaxkva;
	cv_broadcast(&socurkva_cv);
	mutex_exit(&so_pendfree_lock);

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
