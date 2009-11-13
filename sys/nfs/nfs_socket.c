/*	$NetBSD: nfs_socket.c,v 1.173.4.4 2009/11/13 20:34:03 sborrill Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_socket.c	8.5 (Berkeley) 3/30/95
 */

/*
 * Socket operations for use by nfs
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_socket.c,v 1.173.4.4 2009/11/13 20:34:03 sborrill Exp $");

#include "fs_nfs.h"
#include "opt_nfs.h"
#include "opt_nfsserver.h"
#include "opt_mbuftrace.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/evcnt.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/vnode.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/tprintf.h>
#include <sys/namei.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsrtt.h>
#include <nfs/nfs_var.h>

#ifdef MBUFTRACE
struct mowner nfs_mowner = MOWNER_INIT("nfs","");
#endif

/*
 * Estimate rto for an nfs rpc sent via. an unreliable datagram.
 * Use the mean and mean deviation of rtt for the appropriate type of rpc
 * for the frequent rpcs and a default for the others.
 * The justification for doing "other" this way is that these rpcs
 * happen so infrequently that timer est. would probably be stale.
 * Also, since many of these rpcs are
 * non-idempotent, a conservative timeout is desired.
 * getattr, lookup - A+2D
 * read, write     - A+4D
 * other           - nm_timeo
 */
#define	NFS_RTO(n, t) \
	((t) == 0 ? (n)->nm_timeo : \
	 ((t) < 3 ? \
	  (((((n)->nm_srtt[t-1] + 3) >> 2) + (n)->nm_sdrtt[t-1] + 1) >> 1) : \
	  ((((n)->nm_srtt[t-1] + 7) >> 3) + (n)->nm_sdrtt[t-1] + 1)))
#define	NFS_SRTT(r)	(r)->r_nmp->nm_srtt[proct[(r)->r_procnum] - 1]
#define	NFS_SDRTT(r)	(r)->r_nmp->nm_sdrtt[proct[(r)->r_procnum] - 1]
/*
 * External data, mostly RPC constants in XDR form
 */
extern u_int32_t rpc_reply, rpc_msgdenied, rpc_mismatch, rpc_vers,
	rpc_auth_unix, rpc_msgaccepted, rpc_call, rpc_autherr,
	rpc_auth_kerb;
extern u_int32_t nfs_prog;
extern const int nfsv3_procid[NFS_NPROCS];
extern int nfs_ticks;

/*
 * Defines which timer to use for the procnum.
 * 0 - default
 * 1 - getattr
 * 2 - lookup
 * 3 - read
 * 4 - write
 */
static const int proct[NFS_NPROCS] = {
	[NFSPROC_NULL] = 0,
	[NFSPROC_GETATTR] = 1,
	[NFSPROC_SETATTR] = 0,
	[NFSPROC_LOOKUP] = 2,
	[NFSPROC_ACCESS] = 1,
	[NFSPROC_READLINK] = 3,
	[NFSPROC_READ] = 3,
	[NFSPROC_WRITE] = 4,
	[NFSPROC_CREATE] = 0,
	[NFSPROC_MKDIR] = 0,
	[NFSPROC_SYMLINK] = 0,
	[NFSPROC_MKNOD] = 0,
	[NFSPROC_REMOVE] = 0,
	[NFSPROC_RMDIR] = 0,
	[NFSPROC_RENAME] = 0,
	[NFSPROC_LINK] = 0,
	[NFSPROC_READDIR] = 3,
	[NFSPROC_READDIRPLUS] = 3,
	[NFSPROC_FSSTAT] = 0,
	[NFSPROC_FSINFO] = 0,
	[NFSPROC_PATHCONF] = 0,
	[NFSPROC_COMMIT] = 0,
	[NFSPROC_NOOP] = 0,
};

/*
 * There is a congestion window for outstanding rpcs maintained per mount
 * point. The cwnd size is adjusted in roughly the way that:
 * Van Jacobson, Congestion avoidance and Control, In "Proceedings of
 * SIGCOMM '88". ACM, August 1988.
 * describes for TCP. The cwnd size is chopped in half on a retransmit timeout
 * and incremented by 1/cwnd when each rpc reply is received and a full cwnd
 * of rpcs is in progress.
 * (The sent count and cwnd are scaled for integer arith.)
 * Variants of "slow start" were tried and were found to be too much of a
 * performance hit (ave. rtt 3 times larger),
 * I suspect due to the large rtt that nfs rpcs have.
 */
#define	NFS_CWNDSCALE	256
#define	NFS_MAXCWND	(NFS_CWNDSCALE * 32)
static const int nfs_backoff[8] = { 2, 4, 8, 16, 32, 64, 128, 256, };
int nfsrtton = 0;
struct nfsrtt nfsrtt;
struct nfsreqhead nfs_reqq;
static callout_t nfs_timer_ch;
static struct evcnt nfs_timer_ev;
static struct evcnt nfs_timer_start_ev;
static struct evcnt nfs_timer_stop_ev;

#ifdef NFS
static int nfs_sndlock(struct nfsmount *, struct nfsreq *);
static void nfs_sndunlock(struct nfsmount *);
#endif
static int nfs_rcvlock(struct nfsmount *, struct nfsreq *);
static void nfs_rcvunlock(struct nfsmount *);

#if defined(NFSSERVER)
static void nfsrv_wakenfsd_locked(struct nfssvc_sock *);
#endif /* defined(NFSSERVER) */

/*
 * Initialize sockets and congestion for a new NFS connection.
 * We do not free the sockaddr if error.
 */
int
nfs_connect(nmp, rep, l)
	struct nfsmount *nmp;
	struct nfsreq *rep;
	struct lwp *l;
{
	struct socket *so;
	int error, rcvreserve, sndreserve;
	struct sockaddr *saddr;
	struct sockaddr_in *sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
	struct mbuf *m;
	int val;

	nmp->nm_so = (struct socket *)0;
	saddr = mtod(nmp->nm_nam, struct sockaddr *);
	error = socreate(saddr->sa_family, &nmp->nm_so,
		nmp->nm_sotype, nmp->nm_soproto, l, NULL);
	if (error)
		goto bad;
	so = nmp->nm_so;
#ifdef MBUFTRACE
	so->so_mowner = &nfs_mowner;
	so->so_rcv.sb_mowner = &nfs_mowner;
	so->so_snd.sb_mowner = &nfs_mowner;
#endif
	nmp->nm_soflags = so->so_proto->pr_flags;

	/*
	 * Some servers require that the client port be a reserved port number.
	 */
	if (saddr->sa_family == AF_INET && (nmp->nm_flag & NFSMNT_RESVPORT)) {
		val = IP_PORTRANGE_LOW;

		if ((error = so_setsockopt(NULL, so, IPPROTO_IP, IP_PORTRANGE,
		    &val, sizeof(val))))
			goto bad;
		m = m_get(M_WAIT, MT_SONAME);
		MCLAIM(m, so->so_mowner);
		sin = mtod(m, struct sockaddr_in *);
		sin->sin_len = m->m_len = sizeof (struct sockaddr_in);
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = INADDR_ANY;
		sin->sin_port = 0;
		error = sobind(so, m, &lwp0);
		m_freem(m);
		if (error)
			goto bad;
	}
#ifdef INET6
	if (saddr->sa_family == AF_INET6 && (nmp->nm_flag & NFSMNT_RESVPORT)) {
		val = IPV6_PORTRANGE_LOW;

		if ((error = so_setsockopt(NULL, so, IPPROTO_IPV6,
		    IPV6_PORTRANGE, &val, sizeof(val))))
			goto bad;
		m = m_get(M_WAIT, MT_SONAME);
		MCLAIM(m, so->so_mowner);
		sin6 = mtod(m, struct sockaddr_in6 *);
		sin6->sin6_len = m->m_len = sizeof (struct sockaddr_in6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_addr = in6addr_any;
		sin6->sin6_port = 0;
		error = sobind(so, m, &lwp0);
		m_freem(m);
		if (error)
			goto bad;
	}
#endif

	/*
	 * Protocols that do not require connections may be optionally left
	 * unconnected for servers that reply from a port other than NFS_PORT.
	 */
	solock(so);
	if (nmp->nm_flag & NFSMNT_NOCONN) {
		if (nmp->nm_soflags & PR_CONNREQUIRED) {
			sounlock(so);
			error = ENOTCONN;
			goto bad;
		}
	} else {
		error = soconnect(so, nmp->nm_nam, l);
		if (error) {
			sounlock(so);
			goto bad;
		}

		/*
		 * Wait for the connection to complete. Cribbed from the
		 * connect system call but with the wait timing out so
		 * that interruptible mounts don't hang here for a long time.
		 */
		while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
			(void)sowait(so, false, 2 * hz);
			if ((so->so_state & SS_ISCONNECTING) &&
			    so->so_error == 0 && rep &&
			    (error = nfs_sigintr(nmp, rep, rep->r_lwp)) != 0){
				so->so_state &= ~SS_ISCONNECTING;
				sounlock(so);
				goto bad;
			}
		}
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			sounlock(so);
			goto bad;
		}
	}
	if (nmp->nm_flag & (NFSMNT_SOFT | NFSMNT_INT)) {
		so->so_rcv.sb_timeo = (5 * hz);
		so->so_snd.sb_timeo = (5 * hz);
	} else {
		/*
		 * enable receive timeout to detect server crash and reconnect.
		 * otherwise, we can be stuck in soreceive forever.
		 */
		so->so_rcv.sb_timeo = (5 * hz);
		so->so_snd.sb_timeo = 0;
	}
	if (nmp->nm_sotype == SOCK_DGRAM) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * 2;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * 2;
	} else if (nmp->nm_sotype == SOCK_SEQPACKET) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * 2;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * 2;
	} else {
		sounlock(so);
		if (nmp->nm_sotype != SOCK_STREAM)
			panic("nfscon sotype");
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
			val = 1;
			so_setsockopt(NULL, so, SOL_SOCKET, SO_KEEPALIVE, &val,
			    sizeof(val));
		}
		if (so->so_proto->pr_protocol == IPPROTO_TCP) {
			val = 1;
			so_setsockopt(NULL, so, IPPROTO_TCP, TCP_NODELAY, &val,
			    sizeof(val));
		}
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * 2;
		rcvreserve = (nmp->nm_rsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * 2;
		solock(so);
	}
	error = soreserve(so, sndreserve, rcvreserve);
	if (error) {
		sounlock(so);
		goto bad;
	}
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;
	sounlock(so);

	/* Initialize other non-zero congestion variables */
	nmp->nm_srtt[0] = nmp->nm_srtt[1] = nmp->nm_srtt[2] = nmp->nm_srtt[3] =
		NFS_TIMEO << 3;
	nmp->nm_sdrtt[0] = nmp->nm_sdrtt[1] = nmp->nm_sdrtt[2] =
		nmp->nm_sdrtt[3] = 0;
	nmp->nm_cwnd = NFS_MAXCWND / 2;	    /* Initial send window */
	nmp->nm_sent = 0;
	nmp->nm_timeouts = 0;
	return (0);

bad:
	nfs_disconnect(nmp);
	return (error);
}

/*
 * Reconnect routine:
 * Called when a connection is broken on a reliable protocol.
 * - clean up the old socket
 * - nfs_connect() again
 * - set R_MUSTRESEND for all outstanding requests on mount point
 * If this fails the mount point is DEAD!
 * nb: Must be called with the nfs_sndlock() set on the mount point.
 */
int
nfs_reconnect(struct nfsreq *rep)
{
	struct nfsreq *rp;
	struct nfsmount *nmp = rep->r_nmp;
	int error;

	nfs_disconnect(nmp);
	while ((error = nfs_connect(nmp, rep, &lwp0)) != 0) {
		if (error == EINTR || error == ERESTART)
			return (EINTR);
		kpause("nfscn2", false, hz, NULL);
	}

	/*
	 * Loop through outstanding request list and fix up all requests
	 * on old socket.
	 */
	TAILQ_FOREACH(rp, &nfs_reqq, r_chain) {
		if (rp->r_nmp == nmp) {
			if ((rp->r_flags & R_MUSTRESEND) == 0)
				rp->r_flags |= R_MUSTRESEND | R_REXMITTED;
			rp->r_rexmit = 0;
		}
	}
	return (0);
}

/*
 * NFS disconnect. Clean up and unlink.
 */
void
nfs_disconnect(nmp)
	struct nfsmount *nmp;
{
	struct socket *so;
	int drain = 0;

	if (nmp->nm_so) {
		so = nmp->nm_so;
		nmp->nm_so = (struct socket *)0;
		solock(so);
		soshutdown(so, SHUT_RDWR);
		sounlock(so);
		drain = (nmp->nm_iflag & NFSMNT_DISMNT) != 0;
		if (drain) {
			/*
			 * soshutdown() above should wake up the current
			 * listener.
			 * Now wake up those waiting for the receive lock, and
			 * wait for them to go away unhappy, to prevent *nmp
			 * from evaporating while they're sleeping.
			 */
			mutex_enter(&nmp->nm_lock);
			while (nmp->nm_waiters > 0) {
				cv_broadcast(&nmp->nm_rcvcv);
				cv_broadcast(&nmp->nm_sndcv);
				cv_wait(&nmp->nm_disconcv, &nmp->nm_lock);
			}
			mutex_exit(&nmp->nm_lock);
		}
		soclose(so);
	}
#ifdef DIAGNOSTIC
	if (drain && (nmp->nm_waiters > 0))
		panic("nfs_disconnect: waiters left after drain?");
#endif
}

void
nfs_safedisconnect(nmp)
	struct nfsmount *nmp;
{
	struct nfsreq dummyreq;

	memset(&dummyreq, 0, sizeof(dummyreq));
	dummyreq.r_nmp = nmp;
	nfs_rcvlock(nmp, &dummyreq); /* XXX ignored error return */
	nfs_disconnect(nmp);
	nfs_rcvunlock(nmp);
}

/*
 * This is the nfs send routine. For connection based socket types, it
 * must be called with an nfs_sndlock() on the socket.
 * "rep == NULL" indicates that it has been called from a server.
 * For the client side:
 * - return EINTR if the RPC is terminated, 0 otherwise
 * - set R_MUSTRESEND if the send fails for any reason
 * - do any cleanup required by recoverable socket errors (? ? ?)
 * For the server side:
 * - return EINTR or ERESTART if interrupted by a signal
 * - return EPIPE if a connection is lost for connection based sockets (TCP...)
 * - do any cleanup required by recoverable socket errors (? ? ?)
 */
int
nfs_send(so, nam, top, rep, l)
	struct socket *so;
	struct mbuf *nam;
	struct mbuf *top;
	struct nfsreq *rep;
	struct lwp *l;
{
	struct mbuf *sendnam;
	int error, soflags, flags;

	/* XXX nfs_doio()/nfs_request() calls with  rep->r_lwp == NULL */
	if (l == NULL && rep->r_lwp == NULL)
		l = curlwp;

	if (rep) {
		if (rep->r_flags & R_SOFTTERM) {
			m_freem(top);
			return (EINTR);
		}
		if ((so = rep->r_nmp->nm_so) == NULL) {
			rep->r_flags |= R_MUSTRESEND;
			m_freem(top);
			return (0);
		}
		rep->r_flags &= ~R_MUSTRESEND;
		soflags = rep->r_nmp->nm_soflags;
	} else
		soflags = so->so_proto->pr_flags;
	if ((soflags & PR_CONNREQUIRED) || (so->so_state & SS_ISCONNECTED))
		sendnam = (struct mbuf *)0;
	else
		sendnam = nam;
	if (so->so_type == SOCK_SEQPACKET)
		flags = MSG_EOR;
	else
		flags = 0;

	error = (*so->so_send)(so, sendnam, NULL, top, NULL, flags,  l);
	if (error) {
		if (rep) {
			if (error == ENOBUFS && so->so_type == SOCK_DGRAM) {
				/*
				 * We're too fast for the network/driver,
				 * and UDP isn't flowcontrolled.
				 * We need to resend. This is not fatal,
				 * just try again.
				 *
				 * Could be smarter here by doing some sort
				 * of a backoff, but this is rare.
				 */
				rep->r_flags |= R_MUSTRESEND;
			} else {
				if (error != EPIPE)
					log(LOG_INFO,
					    "nfs send error %d for %s\n",
					    error,
					    rep->r_nmp->nm_mountp->
						    mnt_stat.f_mntfromname);
				/*
				 * Deal with errors for the client side.
				 */
				if (rep->r_flags & R_SOFTTERM)
					error = EINTR;
				else
					rep->r_flags |= R_MUSTRESEND;
			}
		} else {
			/*
			 * See above. This error can happen under normal
			 * circumstances and the log is too noisy.
			 * The error will still show up in nfsstat.
			 */
			if (error != ENOBUFS || so->so_type != SOCK_DGRAM)
				log(LOG_INFO, "nfsd send error %d\n", error);
		}

		/*
		 * Handle any recoverable (soft) socket errors here. (? ? ?)
		 */
		if (error != EINTR && error != ERESTART &&
			error != EWOULDBLOCK && error != EPIPE)
			error = 0;
	}
	return (error);
}

#ifdef NFS
/*
 * Receive a Sun RPC Request/Reply. For SOCK_DGRAM, the work is all
 * done by soreceive(), but for SOCK_STREAM we must deal with the Record
 * Mark and consolidate the data into a new mbuf list.
 * nb: Sometimes TCP passes the data up to soreceive() in long lists of
 *     small mbufs.
 * For SOCK_STREAM we must be very careful to read an entire record once
 * we have read any of it, even if the system call has been interrupted.
 */
static int
nfs_receive(struct nfsreq *rep, struct mbuf **aname, struct mbuf **mp,
    struct lwp *l)
{
	struct socket *so;
	struct uio auio;
	struct iovec aio;
	struct mbuf *m;
	struct mbuf *control;
	u_int32_t len;
	struct mbuf **getnam;
	int error, sotype, rcvflg;

	/*
	 * Set up arguments for soreceive()
	 */
	*mp = (struct mbuf *)0;
	*aname = (struct mbuf *)0;
	sotype = rep->r_nmp->nm_sotype;

	/*
	 * For reliable protocols, lock against other senders/receivers
	 * in case a reconnect is necessary.
	 * For SOCK_STREAM, first get the Record Mark to find out how much
	 * more there is to get.
	 * We must lock the socket against other receivers
	 * until we have an entire rpc request/reply.
	 */
	if (sotype != SOCK_DGRAM) {
		error = nfs_sndlock(rep->r_nmp, rep);
		if (error)
			return (error);
tryagain:
		/*
		 * Check for fatal errors and resending request.
		 */
		/*
		 * Ugh: If a reconnect attempt just happened, nm_so
		 * would have changed. NULL indicates a failed
		 * attempt that has essentially shut down this
		 * mount point.
		 */
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM)) {
			nfs_sndunlock(rep->r_nmp);
			return (EINTR);
		}
		so = rep->r_nmp->nm_so;
		if (!so) {
			error = nfs_reconnect(rep);
			if (error) {
				nfs_sndunlock(rep->r_nmp);
				return (error);
			}
			goto tryagain;
		}
		while (rep->r_flags & R_MUSTRESEND) {
			m = m_copym(rep->r_mreq, 0, M_COPYALL, M_WAIT);
			nfsstats.rpcretries++;
			rep->r_rtt = 0;
			rep->r_flags &= ~R_TIMING;
			error = nfs_send(so, rep->r_nmp->nm_nam, m, rep, l);
			if (error) {
				if (error == EINTR || error == ERESTART ||
				    (error = nfs_reconnect(rep)) != 0) {
					nfs_sndunlock(rep->r_nmp);
					return (error);
				}
				goto tryagain;
			}
		}
		nfs_sndunlock(rep->r_nmp);
		if (sotype == SOCK_STREAM) {
			aio.iov_base = (void *) &len;
			aio.iov_len = sizeof(u_int32_t);
			auio.uio_iov = &aio;
			auio.uio_iovcnt = 1;
			auio.uio_rw = UIO_READ;
			auio.uio_offset = 0;
			auio.uio_resid = sizeof(u_int32_t);
			UIO_SETUP_SYSSPACE(&auio);
			do {
			   rcvflg = MSG_WAITALL;
			   error = (*so->so_receive)(so, (struct mbuf **)0, &auio,
				(struct mbuf **)0, (struct mbuf **)0, &rcvflg);
			   if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
				/*
				 * if it seems that the server died after it
				 * received our request, set EPIPE so that
				 * we'll reconnect and retransmit requests.
				 */
				if (rep->r_rexmit >= rep->r_nmp->nm_retry) {
					nfsstats.rpctimeouts++;
					error = EPIPE;
				}
			   }
			} while (error == EWOULDBLOCK);
			if (!error && auio.uio_resid > 0) {
			    /*
			     * Don't log a 0 byte receive; it means
			     * that the socket has been closed, and
			     * can happen during normal operation
			     * (forcible unmount or Solaris server).
			     */
			    if (auio.uio_resid != sizeof (u_int32_t))
			      log(LOG_INFO,
				 "short receive (%lu/%lu) from nfs server %s\n",
				 (u_long)sizeof(u_int32_t) - auio.uio_resid,
				 (u_long)sizeof(u_int32_t),
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EPIPE;
			}
			if (error)
				goto errout;
			len = ntohl(len) & ~0x80000000;
			/*
			 * This is SERIOUS! We are out of sync with the sender
			 * and forcing a disconnect/reconnect is all I can do.
			 */
			if (len > NFS_MAXPACKET) {
			    log(LOG_ERR, "%s (%d) from nfs server %s\n",
				"impossible packet length",
				len,
				rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EFBIG;
			    goto errout;
			}
			auio.uio_resid = len;
			do {
			    rcvflg = MSG_WAITALL;
			    error =  (*so->so_receive)(so, (struct mbuf **)0,
				&auio, mp, (struct mbuf **)0, &rcvflg);
			} while (error == EWOULDBLOCK || error == EINTR ||
				 error == ERESTART);
			if (!error && auio.uio_resid > 0) {
			    if (len != auio.uio_resid)
			      log(LOG_INFO,
				"short receive (%lu/%d) from nfs server %s\n",
				(u_long)len - auio.uio_resid, len,
				rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EPIPE;
			}
		} else {
			/*
			 * NB: Since uio_resid is big, MSG_WAITALL is ignored
			 * and soreceive() will return when it has either a
			 * control msg or a data msg.
			 * We have no use for control msg., but must grab them
			 * and then throw them away so we know what is going
			 * on.
			 */
			auio.uio_resid = len = 100000000; /* Anything Big */
			/* not need to setup uio_vmspace */
			do {
			    rcvflg = 0;
			    error =  (*so->so_receive)(so, (struct mbuf **)0,
				&auio, mp, &control, &rcvflg);
			    if (control)
				m_freem(control);
			    if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
			    }
			} while (error == EWOULDBLOCK ||
				 (!error && *mp == NULL && control));
			if ((rcvflg & MSG_EOR) == 0)
				printf("Egad!!\n");
			if (!error && *mp == NULL)
				error = EPIPE;
			len -= auio.uio_resid;
		}
errout:
		if (error && error != EINTR && error != ERESTART) {
			m_freem(*mp);
			*mp = (struct mbuf *)0;
			if (error != EPIPE)
				log(LOG_INFO,
				    "receive error %d from nfs server %s\n",
				    error,
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			error = nfs_sndlock(rep->r_nmp, rep);
			if (!error)
				error = nfs_reconnect(rep);
			if (!error)
				goto tryagain;
			else
				nfs_sndunlock(rep->r_nmp);
		}
	} else {
		if ((so = rep->r_nmp->nm_so) == NULL)
			return (EACCES);
		if (so->so_state & SS_ISCONNECTED)
			getnam = (struct mbuf **)0;
		else
			getnam = aname;
		auio.uio_resid = len = 1000000;
		/* not need to setup uio_vmspace */
		do {
			rcvflg = 0;
			error =  (*so->so_receive)(so, getnam, &auio, mp,
				(struct mbuf **)0, &rcvflg);
			if (error == EWOULDBLOCK &&
			    (rep->r_flags & R_SOFTTERM))
				return (EINTR);
		} while (error == EWOULDBLOCK);
		len -= auio.uio_resid;
		if (!error && *mp == NULL)
			error = EPIPE;
	}
	if (error) {
		m_freem(*mp);
		*mp = (struct mbuf *)0;
	}
	return (error);
}

/*
 * Implement receipt of reply on a socket.
 * We must search through the list of received datagrams matching them
 * with outstanding requests using the xid, until ours is found.
 */
/* ARGSUSED */
static int
nfs_reply(struct nfsreq *myrep, struct lwp *lwp)
{
	struct nfsreq *rep;
	struct nfsmount *nmp = myrep->r_nmp;
	int32_t t1;
	struct mbuf *mrep, *nam, *md;
	u_int32_t rxid, *tl;
	char *dpos, *cp2;
	int error;

	/*
	 * Loop around until we get our own reply
	 */
	for (;;) {
		/*
		 * Lock against other receivers so that I don't get stuck in
		 * sbwait() after someone else has received my reply for me.
		 * Also necessary for connection based protocols to avoid
		 * race conditions during a reconnect.
		 */
		error = nfs_rcvlock(nmp, myrep);
		if (error == EALREADY)
			return (0);
		if (error)
			return (error);
		/*
		 * Get the next Rpc reply off the socket
		 */

		mutex_enter(&nmp->nm_lock);
		nmp->nm_waiters++;
		mutex_exit(&nmp->nm_lock);

		error = nfs_receive(myrep, &nam, &mrep, lwp);

		mutex_enter(&nmp->nm_lock);
		nmp->nm_waiters--;
		cv_signal(&nmp->nm_disconcv);
		mutex_exit(&nmp->nm_lock);

		if (error) {
			nfs_rcvunlock(nmp);

			if (nmp->nm_iflag & NFSMNT_DISMNT) {
				/*
				 * Oops, we're going away now..
				 */
				return error;
			}
			/*
			 * Ignore routing errors on connectionless protocols? ?
			 */
			if (NFSIGNORE_SOERROR(nmp->nm_soflags, error)) {
				nmp->nm_so->so_error = 0;
#ifdef DEBUG
				printf("nfs_reply: ignoring error %d\n", error);
#endif
				continue;
			}
			return (error);
		}
		if (nam)
			m_freem(nam);

		/*
		 * Get the xid and check that it is an rpc reply
		 */
		md = mrep;
		dpos = mtod(md, void *);
		nfsm_dissect(tl, u_int32_t *, 2*NFSX_UNSIGNED);
		rxid = *tl++;
		if (*tl != rpc_reply) {
			nfsstats.rpcinvalid++;
			m_freem(mrep);
nfsmout:
			nfs_rcvunlock(nmp);
			continue;
		}

		/*
		 * Loop through the request list to match up the reply
		 * Iff no match, just drop the datagram
		 */
		TAILQ_FOREACH(rep, &nfs_reqq, r_chain) {
			if (rep->r_mrep == NULL && rxid == rep->r_xid) {
				/* Found it.. */
				rep->r_mrep = mrep;
				rep->r_md = md;
				rep->r_dpos = dpos;
				if (nfsrtton) {
					struct rttl *rt;

					rt = &nfsrtt.rttl[nfsrtt.pos];
					rt->proc = rep->r_procnum;
					rt->rto = NFS_RTO(nmp, proct[rep->r_procnum]);
					rt->sent = nmp->nm_sent;
					rt->cwnd = nmp->nm_cwnd;
					rt->srtt = nmp->nm_srtt[proct[rep->r_procnum] - 1];
					rt->sdrtt = nmp->nm_sdrtt[proct[rep->r_procnum] - 1];
					rt->fsid = nmp->nm_mountp->mnt_stat.f_fsidx;
					getmicrotime(&rt->tstamp);
					if (rep->r_flags & R_TIMING)
						rt->rtt = rep->r_rtt;
					else
						rt->rtt = 1000000;
					nfsrtt.pos = (nfsrtt.pos + 1) % NFSRTTLOGSIZ;
				}
				/*
				 * Update congestion window.
				 * Do the additive increase of
				 * one rpc/rtt.
				 */
				if (nmp->nm_cwnd <= nmp->nm_sent) {
					nmp->nm_cwnd +=
					   (NFS_CWNDSCALE * NFS_CWNDSCALE +
					   (nmp->nm_cwnd >> 1)) / nmp->nm_cwnd;
					if (nmp->nm_cwnd > NFS_MAXCWND)
						nmp->nm_cwnd = NFS_MAXCWND;
				}
				rep->r_flags &= ~R_SENT;
				nmp->nm_sent -= NFS_CWNDSCALE;
				/*
				 * Update rtt using a gain of 0.125 on the mean
				 * and a gain of 0.25 on the deviation.
				 */
				if (rep->r_flags & R_TIMING) {
					/*
					 * Since the timer resolution of
					 * NFS_HZ is so course, it can often
					 * result in r_rtt == 0. Since
					 * r_rtt == N means that the actual
					 * rtt is between N+dt and N+2-dt ticks,
					 * add 1.
					 */
					t1 = rep->r_rtt + 1;
					t1 -= (NFS_SRTT(rep) >> 3);
					NFS_SRTT(rep) += t1;
					if (t1 < 0)
						t1 = -t1;
					t1 -= (NFS_SDRTT(rep) >> 2);
					NFS_SDRTT(rep) += t1;
				}
				nmp->nm_timeouts = 0;
				break;
			}
		}
		nfs_rcvunlock(nmp);
		/*
		 * If not matched to a request, drop it.
		 * If it's mine, get out.
		 */
		if (rep == 0) {
			nfsstats.rpcunexpected++;
			m_freem(mrep);
		} else if (rep == myrep) {
			if (rep->r_mrep == NULL)
				panic("nfsreply nil");
			return (0);
		}
	}
}

/*
 * nfs_request - goes something like this
 *	- fill in request struct
 *	- links it into list
 *	- calls nfs_send() for first transmit
 *	- calls nfs_receive() to get reply
 *	- break down rpc header and return with nfs reply pointed to
 *	  by mrep or error
 * nb: always frees up mreq mbuf list
 */
int
nfs_request(np, mrest, procnum, lwp, cred, mrp, mdp, dposp, rexmitp)
	struct nfsnode *np;
	struct mbuf *mrest;
	int procnum;
	struct lwp *lwp;
	kauth_cred_t cred;
	struct mbuf **mrp;
	struct mbuf **mdp;
	char **dposp;
	int *rexmitp;
{
	struct mbuf *m, *mrep;
	struct nfsreq *rep;
	u_int32_t *tl;
	int i;
	struct nfsmount *nmp = VFSTONFS(np->n_vnode->v_mount);
	struct mbuf *md, *mheadend;
	char nickv[RPCX_NICKVERF];
	time_t waituntil;
	char *dpos, *cp2;
	int t1, s, error = 0, mrest_len, auth_len, auth_type;
	int trylater_delay = NFS_TRYLATERDEL, failed_auth = 0;
	int verf_len, verf_type;
	u_int32_t xid;
	char *auth_str, *verf_str;
	NFSKERBKEY_T key;		/* save session key */
	kauth_cred_t acred;
	struct mbuf *mrest_backup = NULL;
	kauth_cred_t origcred = NULL; /* XXX: gcc */
	bool retry_cred = true;
	bool use_opencred = (np->n_flag & NUSEOPENCRED) != 0;

	if (rexmitp != NULL)
		*rexmitp = 0;

	acred = kauth_cred_alloc();

tryagain_cred:
	KASSERT(cred != NULL);
	rep = kmem_alloc(sizeof(*rep), KM_SLEEP);
	rep->r_nmp = nmp;
	KASSERT(lwp == NULL || lwp == curlwp);
	rep->r_lwp = lwp;
	rep->r_procnum = procnum;
	i = 0;
	m = mrest;
	while (m) {
		i += m->m_len;
		m = m->m_next;
	}
	mrest_len = i;

	/*
	 * Get the RPC header with authorization.
	 */
kerbauth:
	verf_str = auth_str = (char *)0;
	if (nmp->nm_flag & NFSMNT_KERB) {
		verf_str = nickv;
		verf_len = sizeof (nickv);
		auth_type = RPCAUTH_KERB4;
		memset((void *)key, 0, sizeof (key));
		if (failed_auth || nfs_getnickauth(nmp, cred, &auth_str,
			&auth_len, verf_str, verf_len)) {
			error = nfs_getauth(nmp, rep, cred, &auth_str,
				&auth_len, verf_str, &verf_len, key);
			if (error) {
				kmem_free(rep, sizeof(*rep));
				m_freem(mrest);
				KASSERT(kauth_cred_getrefcnt(acred) == 1);
				kauth_cred_free(acred);
				return (error);
			}
		}
		retry_cred = false;
	} else {
		/* AUTH_UNIX */
		uid_t uid;
		gid_t gid;

		/*
		 * on the most unix filesystems, permission checks are
		 * done when the file is open(2)'ed.
		 * ie. once a file is successfully open'ed,
		 * following i/o operations never fail with EACCES.
		 * we try to follow the semantics as far as possible.
		 *
		 * note that we expect that the nfs server always grant
		 * accesses by the file's owner.
		 */
		origcred = cred;
		switch (procnum) {
		case NFSPROC_READ:
		case NFSPROC_WRITE:
		case NFSPROC_COMMIT:
			uid = np->n_vattr->va_uid;
			gid = np->n_vattr->va_gid;
			if (kauth_cred_geteuid(cred) == uid &&
			    kauth_cred_getegid(cred) == gid) {
				retry_cred = false;
				break;
			}
			if (use_opencred)
				break;
			kauth_cred_setuid(acred, uid);
			kauth_cred_seteuid(acred, uid);
			kauth_cred_setsvuid(acred, uid);
			kauth_cred_setgid(acred, gid);
			kauth_cred_setegid(acred, gid);
			kauth_cred_setsvgid(acred, gid);
			cred = acred;
			break;
		default:
			retry_cred = false;
			break;
		}
		/*
		 * backup mbuf chain if we can need it later to retry.
		 *
		 * XXX maybe we can keep a direct reference to
		 * mrest without doing m_copym, but it's ...ugly.
		 */
		if (retry_cred)
			mrest_backup = m_copym(mrest, 0, M_COPYALL, M_WAIT);
		auth_type = RPCAUTH_UNIX;
		/* XXX elad - ngroups */
		auth_len = (((kauth_cred_ngroups(cred) > nmp->nm_numgrps) ?
			nmp->nm_numgrps : kauth_cred_ngroups(cred)) << 2) +
			5 * NFSX_UNSIGNED;
	}
	m = nfsm_rpchead(cred, nmp->nm_flag, procnum, auth_type, auth_len,
	     auth_str, verf_len, verf_str, mrest, mrest_len, &mheadend, &xid);
	if (auth_str)
		free(auth_str, M_TEMP);

	/*
	 * For stream protocols, insert a Sun RPC Record Mark.
	 */
	if (nmp->nm_sotype == SOCK_STREAM) {
		M_PREPEND(m, NFSX_UNSIGNED, M_WAIT);
		*mtod(m, u_int32_t *) = htonl(0x80000000 |
			 (m->m_pkthdr.len - NFSX_UNSIGNED));
	}
	rep->r_mreq = m;
	rep->r_xid = xid;
tryagain:
	if (nmp->nm_flag & NFSMNT_SOFT)
		rep->r_retry = nmp->nm_retry;
	else
		rep->r_retry = NFS_MAXREXMIT + 1;	/* past clip limit */
	rep->r_rtt = rep->r_rexmit = 0;
	if (proct[procnum] > 0)
		rep->r_flags = R_TIMING;
	else
		rep->r_flags = 0;
	rep->r_mrep = NULL;

	/*
	 * Do the client side RPC.
	 */
	nfsstats.rpcrequests++;
	/*
	 * Chain request into list of outstanding requests. Be sure
	 * to put it LAST so timer finds oldest requests first.
	 */
	s = splsoftnet();
	TAILQ_INSERT_TAIL(&nfs_reqq, rep, r_chain);
	nfs_timer_start();

	/*
	 * If backing off another request or avoiding congestion, don't
	 * send this one now but let timer do it. If not timing a request,
	 * do it now.
	 */
	if (nmp->nm_so && (nmp->nm_sotype != SOCK_DGRAM ||
	    (nmp->nm_flag & NFSMNT_DUMBTIMR) || nmp->nm_sent < nmp->nm_cwnd)) {
		splx(s);
		if (nmp->nm_soflags & PR_CONNREQUIRED)
			error = nfs_sndlock(nmp, rep);
		if (!error) {
			m = m_copym(rep->r_mreq, 0, M_COPYALL, M_WAIT);
			error = nfs_send(nmp->nm_so, nmp->nm_nam, m, rep, lwp);
			if (nmp->nm_soflags & PR_CONNREQUIRED)
				nfs_sndunlock(nmp);
		}
		if (!error && (rep->r_flags & R_MUSTRESEND) == 0) {
			nmp->nm_sent += NFS_CWNDSCALE;
			rep->r_flags |= R_SENT;
		}
	} else {
		splx(s);
		rep->r_rtt = -1;
	}

	/*
	 * Wait for the reply from our send or the timer's.
	 */
	if (!error || error == EPIPE || error == EWOULDBLOCK)
		error = nfs_reply(rep, lwp);

	/*
	 * RPC done, unlink the request.
	 */
	s = splsoftnet();
	TAILQ_REMOVE(&nfs_reqq, rep, r_chain);
	splx(s);

	/*
	 * Decrement the outstanding request count.
	 */
	if (rep->r_flags & R_SENT) {
		rep->r_flags &= ~R_SENT;	/* paranoia */
		nmp->nm_sent -= NFS_CWNDSCALE;
	}

	if (rexmitp != NULL) {
		int rexmit;

		if (nmp->nm_sotype != SOCK_DGRAM)
			rexmit = (rep->r_flags & R_REXMITTED) != 0;
		else
			rexmit = rep->r_rexmit;
		*rexmitp = rexmit;
	}

	/*
	 * If there was a successful reply and a tprintf msg.
	 * tprintf a response.
	 */
	if (!error && (rep->r_flags & R_TPRINTFMSG))
		nfs_msg(rep->r_lwp, nmp->nm_mountp->mnt_stat.f_mntfromname,
		    "is alive again");
	mrep = rep->r_mrep;
	md = rep->r_md;
	dpos = rep->r_dpos;
	if (error)
		goto nfsmout;

	/*
	 * break down the rpc header and check if ok
	 */
	nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	if (*tl++ == rpc_msgdenied) {
		if (*tl == rpc_mismatch)
			error = EOPNOTSUPP;
		else if ((nmp->nm_flag & NFSMNT_KERB) && *tl++ == rpc_autherr) {
			if (!failed_auth) {
				failed_auth++;
				mheadend->m_next = (struct mbuf *)0;
				m_freem(mrep);
				m_freem(rep->r_mreq);
				goto kerbauth;
			} else
				error = EAUTH;
		} else
			error = EACCES;
		m_freem(mrep);
		goto nfsmout;
	}

	/*
	 * Grab any Kerberos verifier, otherwise just throw it away.
	 */
	verf_type = fxdr_unsigned(int, *tl++);
	i = fxdr_unsigned(int32_t, *tl);
	if ((nmp->nm_flag & NFSMNT_KERB) && verf_type == RPCAUTH_KERB4) {
		error = nfs_savenickauth(nmp, cred, i, key, &md, &dpos, mrep);
		if (error)
			goto nfsmout;
	} else if (i > 0)
		nfsm_adv(nfsm_rndup(i));
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	/* 0 == ok */
	if (*tl == 0) {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		if (*tl != 0) {
			error = fxdr_unsigned(int, *tl);
			switch (error) {
			case NFSERR_PERM:
				error = EPERM;
				break;

			case NFSERR_NOENT:
				error = ENOENT;
				break;

			case NFSERR_IO:
				error = EIO;
				break;

			case NFSERR_NXIO:
				error = ENXIO;
				break;

			case NFSERR_ACCES:
				error = EACCES;
				if (!retry_cred)
					break;
				m_freem(mrep);
				m_freem(rep->r_mreq);
				kmem_free(rep, sizeof(*rep));
				use_opencred = !use_opencred;
				if (mrest_backup == NULL) {
					/* m_copym failure */
					KASSERT(
					    kauth_cred_getrefcnt(acred) == 1);
					kauth_cred_free(acred);
					return ENOMEM;
				}
				mrest = mrest_backup;
				mrest_backup = NULL;
				cred = origcred;
				error = 0;
				retry_cred = false;
				goto tryagain_cred;

			case NFSERR_EXIST:
				error = EEXIST;
				break;

			case NFSERR_XDEV:
				error = EXDEV;
				break;

			case NFSERR_NODEV:
				error = ENODEV;
				break;

			case NFSERR_NOTDIR:
				error = ENOTDIR;
				break;

			case NFSERR_ISDIR:
				error = EISDIR;
				break;

			case NFSERR_INVAL:
				error = EINVAL;
				break;

			case NFSERR_FBIG:
				error = EFBIG;
				break;

			case NFSERR_NOSPC:
				error = ENOSPC;
				break;

			case NFSERR_ROFS:
				error = EROFS;
				break;

			case NFSERR_MLINK:
				error = EMLINK;
				break;

			case NFSERR_TIMEDOUT:
				error = ETIMEDOUT;
				break;

			case NFSERR_NAMETOL:
				error = ENAMETOOLONG;
				break;

			case NFSERR_NOTEMPTY:
				error = ENOTEMPTY;
				break;

			case NFSERR_DQUOT:
				error = EDQUOT;
				break;

			case NFSERR_STALE:
				/*
				 * If the File Handle was stale, invalidate the
				 * lookup cache, just in case.
				 */
				error = ESTALE;
				cache_purge(NFSTOV(np));
				break;

			case NFSERR_REMOTE:
				error = EREMOTE;
				break;

			case NFSERR_WFLUSH:
			case NFSERR_BADHANDLE:
			case NFSERR_NOT_SYNC:
			case NFSERR_BAD_COOKIE:
				error = EINVAL;
				break;

			case NFSERR_NOTSUPP:
				error = ENOTSUP;
				break;

			case NFSERR_TOOSMALL:
			case NFSERR_SERVERFAULT:
			case NFSERR_BADTYPE:
				error = EINVAL;
				break;

			case NFSERR_TRYLATER:
				if ((nmp->nm_flag & NFSMNT_NFSV3) == 0)
					break;
				m_freem(mrep);
				error = 0;
				waituntil = time_second + trylater_delay;
				while (time_second < waituntil) {
					kpause("nfstrylater", false, hz, NULL);
				}
				trylater_delay *= NFS_TRYLATERDELMUL;
				if (trylater_delay > NFS_TRYLATERDELMAX)
					trylater_delay = NFS_TRYLATERDELMAX;
				/*
				 * RFC1813:
				 * The client should wait and then try
				 * the request with a new RPC transaction ID.
				 */
				nfs_renewxid(rep);
				goto tryagain;

			default:
#ifdef DIAGNOSTIC
				printf("Invalid rpc error code %d\n", error);
#endif
				error = EINVAL;
				break;
			}

			if (nmp->nm_flag & NFSMNT_NFSV3) {
				*mrp = mrep;
				*mdp = md;
				*dposp = dpos;
				error |= NFSERR_RETERR;
			} else
				m_freem(mrep);
			goto nfsmout;
		}

		/*
		 * note which credential worked to minimize number of retries.
		 */
		if (use_opencred)
			np->n_flag |= NUSEOPENCRED;
		else
			np->n_flag &= ~NUSEOPENCRED;

		*mrp = mrep;
		*mdp = md;
		*dposp = dpos;

		KASSERT(error == 0);
		goto nfsmout;
	}
	m_freem(mrep);
	error = EPROTONOSUPPORT;
nfsmout:
	KASSERT(kauth_cred_getrefcnt(acred) == 1);
	kauth_cred_free(acred);
	m_freem(rep->r_mreq);
	kmem_free(rep, sizeof(*rep));
	m_freem(mrest_backup);
	return (error);
}
#endif /* NFS */

/*
 * Generate the rpc reply header
 * siz arg. is used to decide if adding a cluster is worthwhile
 */
int
nfs_rephead(siz, nd, slp, err, cache, frev, mrq, mbp, bposp)
	int siz;
	struct nfsrv_descript *nd;
	struct nfssvc_sock *slp;
	int err;
	int cache;
	u_quad_t *frev;
	struct mbuf **mrq;
	struct mbuf **mbp;
	char **bposp;
{
	u_int32_t *tl;
	struct mbuf *mreq;
	char *bpos;
	struct mbuf *mb;

	mreq = m_gethdr(M_WAIT, MT_DATA);
	MCLAIM(mreq, &nfs_mowner);
	mb = mreq;
	/*
	 * If this is a big reply, use a cluster else
	 * try and leave leading space for the lower level headers.
	 */
	siz += RPC_REPLYSIZ;
	if (siz >= max_datalen) {
		m_clget(mreq, M_WAIT);
	} else
		mreq->m_data += max_hdr;
	tl = mtod(mreq, u_int32_t *);
	mreq->m_len = 6 * NFSX_UNSIGNED;
	bpos = ((char *)tl) + mreq->m_len;
	*tl++ = txdr_unsigned(nd->nd_retxid);
	*tl++ = rpc_reply;
	if (err == ERPCMISMATCH || (err & NFSERR_AUTHERR)) {
		*tl++ = rpc_msgdenied;
		if (err & NFSERR_AUTHERR) {
			*tl++ = rpc_autherr;
			*tl = txdr_unsigned(err & ~NFSERR_AUTHERR);
			mreq->m_len -= NFSX_UNSIGNED;
			bpos -= NFSX_UNSIGNED;
		} else {
			*tl++ = rpc_mismatch;
			*tl++ = txdr_unsigned(RPC_VER2);
			*tl = txdr_unsigned(RPC_VER2);
		}
	} else {
		*tl++ = rpc_msgaccepted;

		/*
		 * For Kerberos authentication, we must send the nickname
		 * verifier back, otherwise just RPCAUTH_NULL.
		 */
		if (nd->nd_flag & ND_KERBFULL) {
			struct nfsuid *nuidp;
			struct timeval ktvin, ktvout;

			memset(&ktvout, 0, sizeof ktvout);	/* XXX gcc */

			LIST_FOREACH(nuidp,
			    NUIDHASH(slp, kauth_cred_geteuid(nd->nd_cr)),
			    nu_hash) {
				if (kauth_cred_geteuid(nuidp->nu_cr) ==
				kauth_cred_geteuid(nd->nd_cr) &&
				    (!nd->nd_nam2 || netaddr_match(
				    NU_NETFAM(nuidp), &nuidp->nu_haddr,
				    nd->nd_nam2)))
					break;
			}
			if (nuidp) {
				ktvin.tv_sec =
				    txdr_unsigned(nuidp->nu_timestamp.tv_sec
					- 1);
				ktvin.tv_usec =
				    txdr_unsigned(nuidp->nu_timestamp.tv_usec);

				/*
				 * Encrypt the timestamp in ecb mode using the
				 * session key.
				 */
#ifdef NFSKERB
				XXX
#endif

				*tl++ = rpc_auth_kerb;
				*tl++ = txdr_unsigned(3 * NFSX_UNSIGNED);
				*tl = ktvout.tv_sec;
				nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
				*tl++ = ktvout.tv_usec;
				*tl++ = txdr_unsigned(
				    kauth_cred_geteuid(nuidp->nu_cr));
			} else {
				*tl++ = 0;
				*tl++ = 0;
			}
		} else {
			*tl++ = 0;
			*tl++ = 0;
		}
		switch (err) {
		case EPROGUNAVAIL:
			*tl = txdr_unsigned(RPC_PROGUNAVAIL);
			break;
		case EPROGMISMATCH:
			*tl = txdr_unsigned(RPC_PROGMISMATCH);
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(2);
			*tl = txdr_unsigned(3);
			break;
		case EPROCUNAVAIL:
			*tl = txdr_unsigned(RPC_PROCUNAVAIL);
			break;
		case EBADRPC:
			*tl = txdr_unsigned(RPC_GARBAGE);
			break;
		default:
			*tl = 0;
			if (err != NFSERR_RETVOID) {
				nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
				if (err)
				    *tl = txdr_unsigned(nfsrv_errmap(nd, err));
				else
				    *tl = 0;
			}
			break;
		};
	}

	if (mrq != NULL)
		*mrq = mreq;
	*mbp = mb;
	*bposp = bpos;
	if (err != 0 && err != NFSERR_RETVOID)
		nfsstats.srvrpc_errs++;
	return (0);
}

static void
nfs_timer_schedule(void)
{

	callout_schedule(&nfs_timer_ch, nfs_ticks);
}

void
nfs_timer_start(void)
{

	if (callout_pending(&nfs_timer_ch))
		return;

	nfs_timer_start_ev.ev_count++;
	nfs_timer_schedule();
}

void
nfs_timer_init(void)
{

	callout_init(&nfs_timer_ch, 0);
	callout_setfunc(&nfs_timer_ch, nfs_timer, NULL);
	evcnt_attach_dynamic(&nfs_timer_ev, EVCNT_TYPE_MISC, NULL,
	    "nfs", "timer");
	evcnt_attach_dynamic(&nfs_timer_start_ev, EVCNT_TYPE_MISC, NULL,
	    "nfs", "timer start");
	evcnt_attach_dynamic(&nfs_timer_stop_ev, EVCNT_TYPE_MISC, NULL,
	    "nfs", "timer stop");
}

/*
 * Nfs timer routine
 * Scan the nfsreq list and retranmit any requests that have timed out
 * To avoid retransmission attempts on STREAM sockets (in the future) make
 * sure to set the r_retry field to 0 (implies nm_retry == 0).
 */
void
nfs_timer(void *arg)
{
	struct nfsreq *rep;
	struct mbuf *m;
	struct socket *so;
	struct nfsmount *nmp;
	int timeo;
	int error;
	bool more = false;
#ifdef NFSSERVER
	struct timeval tv;
	struct nfssvc_sock *slp;
	u_quad_t cur_usec;
#endif

	nfs_timer_ev.ev_count++;

	mutex_enter(softnet_lock);	/* XXX PR 40491 */
	TAILQ_FOREACH(rep, &nfs_reqq, r_chain) {
		more = true;
		nmp = rep->r_nmp;
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM))
			continue;
		if (nfs_sigintr(nmp, rep, rep->r_lwp)) {
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (rep->r_rtt >= 0) {
			rep->r_rtt++;
			if (nmp->nm_flag & NFSMNT_DUMBTIMR)
				timeo = nmp->nm_timeo;
			else
				timeo = NFS_RTO(nmp, proct[rep->r_procnum]);
			if (nmp->nm_timeouts > 0)
				timeo *= nfs_backoff[nmp->nm_timeouts - 1];
			if (timeo > NFS_MAXTIMEO)
				timeo = NFS_MAXTIMEO;
			if (rep->r_rtt <= timeo)
				continue;
			if (nmp->nm_timeouts <
			    (sizeof(nfs_backoff) / sizeof(nfs_backoff[0])))
				nmp->nm_timeouts++;
		}
		/*
		 * Check for server not responding
		 */
		if ((rep->r_flags & R_TPRINTFMSG) == 0 &&
		     rep->r_rexmit > nmp->nm_deadthresh) {
			nfs_msg(rep->r_lwp,
			    nmp->nm_mountp->mnt_stat.f_mntfromname,
			    "not responding");
			rep->r_flags |= R_TPRINTFMSG;
		}
		if (rep->r_rexmit >= rep->r_retry) {	/* too many */
			nfsstats.rpctimeouts++;
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (nmp->nm_sotype != SOCK_DGRAM) {
			if (++rep->r_rexmit > NFS_MAXREXMIT)
				rep->r_rexmit = NFS_MAXREXMIT;
			continue;
		}
		if ((so = nmp->nm_so) == NULL)
			continue;

		/*
		 * If there is enough space and the window allows..
		 *	Resend it
		 * Set r_rtt to -1 in case we fail to send it now.
		 */
		/* solock(so);		XXX PR 40491 */
		rep->r_rtt = -1;
		if (sbspace(&so->so_snd) >= rep->r_mreq->m_pkthdr.len &&
		   ((nmp->nm_flag & NFSMNT_DUMBTIMR) ||
		    (rep->r_flags & R_SENT) ||
		    nmp->nm_sent < nmp->nm_cwnd) &&
		   (m = m_copym(rep->r_mreq, 0, M_COPYALL, M_DONTWAIT))){
		        if (so->so_state & SS_ISCONNECTED)
			    error = (*so->so_proto->pr_usrreq)(so, PRU_SEND, m,
			    (struct mbuf *)0, (struct mbuf *)0, (struct lwp *)0);
			else
			    error = (*so->so_proto->pr_usrreq)(so, PRU_SEND, m,
			    nmp->nm_nam, (struct mbuf *)0, (struct lwp *)0);
			if (error) {
				if (NFSIGNORE_SOERROR(nmp->nm_soflags, error)) {
#ifdef DEBUG
					printf("nfs_timer: ignoring error %d\n",
						error);
#endif
					so->so_error = 0;
				}
			} else {
				/*
				 * Iff first send, start timing
				 * else turn timing off, backoff timer
				 * and divide congestion window by 2.
				 */
				if (rep->r_flags & R_SENT) {
					rep->r_flags &= ~R_TIMING;
					if (++rep->r_rexmit > NFS_MAXREXMIT)
						rep->r_rexmit = NFS_MAXREXMIT;
					nmp->nm_cwnd >>= 1;
					if (nmp->nm_cwnd < NFS_CWNDSCALE)
						nmp->nm_cwnd = NFS_CWNDSCALE;
					nfsstats.rpcretries++;
				} else {
					rep->r_flags |= R_SENT;
					nmp->nm_sent += NFS_CWNDSCALE;
				}
				rep->r_rtt = 0;
			}
		}
		/* sounlock(so);	XXX PR 40491 */
	}
	mutex_exit(softnet_lock);	/* XXX PR 40491 */

#ifdef NFSSERVER
	/*
	 * Scan the write gathering queues for writes that need to be
	 * completed now.
	 */
	getmicrotime(&tv);
	cur_usec = (u_quad_t)tv.tv_sec * 1000000 + (u_quad_t)tv.tv_usec;
	mutex_enter(&nfsd_lock);
	TAILQ_FOREACH(slp, &nfssvc_sockhead, ns_chain) {
		struct nfsrv_descript *nd;

		nd = LIST_FIRST(&slp->ns_tq);
		if (nd != NULL) {
			if (nd->nd_time <= cur_usec) {
				nfsrv_wakenfsd_locked(slp);
			}
			more = true;
		}
	}
	mutex_exit(&nfsd_lock);
#endif /* NFSSERVER */
	if (more) {
		nfs_timer_schedule();
	} else {
		nfs_timer_stop_ev.ev_count++;
	}
}

/*
 * Test for a termination condition pending on the process.
 * This is used for NFSMNT_INT mounts.
 */
int
nfs_sigintr(nmp, rep, l)
	struct nfsmount *nmp;
	struct nfsreq *rep;
	struct lwp *l;
{
	sigset_t ss;

	if (rep && (rep->r_flags & R_SOFTTERM))
		return (EINTR);
	if (!(nmp->nm_flag & NFSMNT_INT))
		return (0);
	if (l) {
		sigpending1(l, &ss);
#if 0
		sigminusset(&l->l_proc->p_sigctx.ps_sigignore, &ss);
#endif
		if (sigismember(&ss, SIGINT) || sigismember(&ss, SIGTERM) ||
		    sigismember(&ss, SIGKILL) || sigismember(&ss, SIGHUP) ||
		    sigismember(&ss, SIGQUIT))
			return (EINTR);
	}
	return (0);
}

#ifdef NFS
/*
 * Lock a socket against others.
 * Necessary for STREAM sockets to ensure you get an entire rpc request/reply
 * and also to avoid race conditions between the processes with nfs requests
 * in progress when a reconnect is necessary.
 */
static int
nfs_sndlock(struct nfsmount *nmp, struct nfsreq *rep)
{
	struct lwp *l;
	int timeo = 0;
	bool catch = false;
	int error = 0;

	if (rep) {
		l = rep->r_lwp;
		if (rep->r_nmp->nm_flag & NFSMNT_INT)
			catch = true;
	} else
		l = NULL;
	mutex_enter(&nmp->nm_lock);
	while ((nmp->nm_iflag & NFSMNT_SNDLOCK) != 0) {
		if (rep && nfs_sigintr(rep->r_nmp, rep, l)) {
			error = EINTR;
			goto quit;
		}
		if (catch) {
			cv_timedwait_sig(&nmp->nm_sndcv, &nmp->nm_lock, timeo);
		} else {
			cv_timedwait(&nmp->nm_sndcv, &nmp->nm_lock, timeo);
		}
		if (catch) {
			catch = false;
			timeo = 2 * hz;
		}
	}
	nmp->nm_iflag |= NFSMNT_SNDLOCK;
quit:
	mutex_exit(&nmp->nm_lock);
	return error;
}

/*
 * Unlock the stream socket for others.
 */
static void
nfs_sndunlock(struct nfsmount *nmp)
{

	mutex_enter(&nmp->nm_lock);
	if ((nmp->nm_iflag & NFSMNT_SNDLOCK) == 0)
		panic("nfs sndunlock");
	nmp->nm_iflag &= ~NFSMNT_SNDLOCK;
	cv_signal(&nmp->nm_sndcv);
	mutex_exit(&nmp->nm_lock);
}
#endif /* NFS */

static int
nfs_rcvlock(struct nfsmount *nmp, struct nfsreq *rep)
{
	int *flagp = &nmp->nm_iflag;
	int slptimeo = 0;
	bool catch;
	int error = 0;

	KASSERT(nmp == rep->r_nmp);

	catch = (nmp->nm_flag & NFSMNT_INT) != 0;
	mutex_enter(&nmp->nm_lock);
	while (/* CONSTCOND */ true) {
		if (*flagp & NFSMNT_DISMNT) {
			cv_signal(&nmp->nm_disconcv);
			error = EIO;
			break;
		}
		/* If our reply was received while we were sleeping,
		 * then just return without taking the lock to avoid a
		 * situation where a single iod could 'capture' the
		 * receive lock.
		 */
		if (rep->r_mrep != NULL) {
			error = EALREADY;
			break;
		}
		if (nfs_sigintr(rep->r_nmp, rep, rep->r_lwp)) {
			error = EINTR;
			break;
		}
		if ((*flagp & NFSMNT_RCVLOCK) == 0) {
			*flagp |= NFSMNT_RCVLOCK;
			break;
		}
		if (catch) {
			cv_timedwait_sig(&nmp->nm_rcvcv, &nmp->nm_lock,
			    slptimeo);
		} else {
			cv_timedwait(&nmp->nm_rcvcv, &nmp->nm_lock,
			    slptimeo);
		}
		if (catch) {
			catch = false;
			slptimeo = 2 * hz;
		}
	}
	mutex_exit(&nmp->nm_lock);
	return error;
}

/*
 * Unlock the stream socket for others.
 */
static void
nfs_rcvunlock(struct nfsmount *nmp)
{

	mutex_enter(&nmp->nm_lock);
	if ((nmp->nm_iflag & NFSMNT_RCVLOCK) == 0)
		panic("nfs rcvunlock");
	nmp->nm_iflag &= ~NFSMNT_RCVLOCK;
	cv_broadcast(&nmp->nm_rcvcv);
	mutex_exit(&nmp->nm_lock);
}

/*
 * Parse an RPC request
 * - verify it
 * - allocate and fill in the cred.
 */
int
nfs_getreq(nd, nfsd, has_header)
	struct nfsrv_descript *nd;
	struct nfsd *nfsd;
	int has_header;
{
	int len, i;
	u_int32_t *tl;
	int32_t t1;
	struct uio uio;
	struct iovec iov;
	char *dpos, *cp2, *cp;
	u_int32_t nfsvers, auth_type;
	uid_t nickuid;
	int error = 0, ticklen;
	struct mbuf *mrep, *md;
	struct nfsuid *nuidp;
	struct timeval tvin, tvout;

	memset(&tvout, 0, sizeof tvout);	/* XXX gcc */

	KASSERT(nd->nd_cr == NULL);
	mrep = nd->nd_mrep;
	md = nd->nd_md;
	dpos = nd->nd_dpos;
	if (has_header) {
		nfsm_dissect(tl, u_int32_t *, 10 * NFSX_UNSIGNED);
		nd->nd_retxid = fxdr_unsigned(u_int32_t, *tl++);
		if (*tl++ != rpc_call) {
			m_freem(mrep);
			return (EBADRPC);
		}
	} else
		nfsm_dissect(tl, u_int32_t *, 8 * NFSX_UNSIGNED);
	nd->nd_repstat = 0;
	nd->nd_flag = 0;
	if (*tl++ != rpc_vers) {
		nd->nd_repstat = ERPCMISMATCH;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	if (*tl != nfs_prog) {
		nd->nd_repstat = EPROGUNAVAIL;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	tl++;
	nfsvers = fxdr_unsigned(u_int32_t, *tl++);
	if (nfsvers < NFS_VER2 || nfsvers > NFS_VER3) {
		nd->nd_repstat = EPROGMISMATCH;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	if (nfsvers == NFS_VER3)
		nd->nd_flag = ND_NFSV3;
	nd->nd_procnum = fxdr_unsigned(u_int32_t, *tl++);
	if (nd->nd_procnum == NFSPROC_NULL)
		return (0);
	if (nd->nd_procnum > NFSPROC_COMMIT ||
	    (!nd->nd_flag && nd->nd_procnum > NFSV2PROC_STATFS)) {
		nd->nd_repstat = EPROCUNAVAIL;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	if ((nd->nd_flag & ND_NFSV3) == 0)
		nd->nd_procnum = nfsv3_procid[nd->nd_procnum];
	auth_type = *tl++;
	len = fxdr_unsigned(int, *tl++);
	if (len < 0 || len > RPCAUTH_MAXSIZ) {
		m_freem(mrep);
		return (EBADRPC);
	}

	nd->nd_flag &= ~ND_KERBAUTH;
	/*
	 * Handle auth_unix or auth_kerb.
	 */
	if (auth_type == rpc_auth_unix) {
		uid_t uid;
		gid_t gid;

		nd->nd_cr = kauth_cred_alloc();
		len = fxdr_unsigned(int, *++tl);
		if (len < 0 || len > NFS_MAXNAMLEN) {
			m_freem(mrep);
			error = EBADRPC;
			goto errout;
		}
		nfsm_adv(nfsm_rndup(len));
		nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);

		uid = fxdr_unsigned(uid_t, *tl++);
		gid = fxdr_unsigned(gid_t, *tl++);
		kauth_cred_setuid(nd->nd_cr, uid);
		kauth_cred_seteuid(nd->nd_cr, uid);
		kauth_cred_setsvuid(nd->nd_cr, uid);
		kauth_cred_setgid(nd->nd_cr, gid);
		kauth_cred_setegid(nd->nd_cr, gid);
		kauth_cred_setsvgid(nd->nd_cr, gid);

		len = fxdr_unsigned(int, *tl);
		if (len < 0 || len > RPCAUTH_UNIXGIDS) {
			m_freem(mrep);
			error = EBADRPC;
			goto errout;
		}
		nfsm_dissect(tl, u_int32_t *, (len + 2) * NFSX_UNSIGNED);

		if (len > 0) {
			size_t grbuf_size = min(len, NGROUPS) * sizeof(gid_t);
			gid_t *grbuf = kmem_alloc(grbuf_size, KM_SLEEP);

			for (i = 0; i < len; i++) {
				if (i < NGROUPS) /* XXX elad */
					grbuf[i] = fxdr_unsigned(gid_t, *tl++);
				else
					tl++;
			}
			kauth_cred_setgroups(nd->nd_cr, grbuf,
			    min(len, NGROUPS), -1, UIO_SYSSPACE);
			kmem_free(grbuf, grbuf_size);
		}

		len = fxdr_unsigned(int, *++tl);
		if (len < 0 || len > RPCAUTH_MAXSIZ) {
			m_freem(mrep);
			error = EBADRPC;
			goto errout;
		}
		if (len > 0)
			nfsm_adv(nfsm_rndup(len));
	} else if (auth_type == rpc_auth_kerb) {
		switch (fxdr_unsigned(int, *tl++)) {
		case RPCAKN_FULLNAME:
			ticklen = fxdr_unsigned(int, *tl);
			*((u_int32_t *)nfsd->nfsd_authstr) = *tl;
			uio.uio_resid = nfsm_rndup(ticklen) + NFSX_UNSIGNED;
			nfsd->nfsd_authlen = uio.uio_resid + NFSX_UNSIGNED;
			if (uio.uio_resid > (len - 2 * NFSX_UNSIGNED)) {
				m_freem(mrep);
				error = EBADRPC;
				goto errout;
			}
			uio.uio_offset = 0;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			UIO_SETUP_SYSSPACE(&uio);
			iov.iov_base = (void *)&nfsd->nfsd_authstr[4];
			iov.iov_len = RPCAUTH_MAXSIZ - 4;
			nfsm_mtouio(&uio, uio.uio_resid);
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (*tl++ != rpc_auth_kerb ||
				fxdr_unsigned(int, *tl) != 4 * NFSX_UNSIGNED) {
				printf("Bad kerb verifier\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			nfsm_dissect(cp, void *, 4 * NFSX_UNSIGNED);
			tl = (u_int32_t *)cp;
			if (fxdr_unsigned(int, *tl) != RPCAKN_FULLNAME) {
				printf("Not fullname kerb verifier\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			cp += NFSX_UNSIGNED;
			memcpy(nfsd->nfsd_verfstr, cp, 3 * NFSX_UNSIGNED);
			nfsd->nfsd_verflen = 3 * NFSX_UNSIGNED;
			nd->nd_flag |= ND_KERBFULL;
			nfsd->nfsd_flag |= NFSD_NEEDAUTH;
			break;
		case RPCAKN_NICKNAME:
			if (len != 2 * NFSX_UNSIGNED) {
				printf("Kerb nickname short\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADCRED);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			nickuid = fxdr_unsigned(uid_t, *tl);
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (*tl++ != rpc_auth_kerb ||
				fxdr_unsigned(int, *tl) != 3 * NFSX_UNSIGNED) {
				printf("Kerb nick verifier bad\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			tvin.tv_sec = *tl++;
			tvin.tv_usec = *tl;

			LIST_FOREACH(nuidp, NUIDHASH(nfsd->nfsd_slp, nickuid),
			    nu_hash) {
				if (kauth_cred_geteuid(nuidp->nu_cr) == nickuid &&
				    (!nd->nd_nam2 ||
				     netaddr_match(NU_NETFAM(nuidp),
				      &nuidp->nu_haddr, nd->nd_nam2)))
					break;
			}
			if (!nuidp) {
				nd->nd_repstat =
					(NFSERR_AUTHERR|AUTH_REJECTCRED);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}

			/*
			 * Now, decrypt the timestamp using the session key
			 * and validate it.
			 */
#ifdef NFSKERB
			XXX
#endif

			tvout.tv_sec = fxdr_unsigned(long, tvout.tv_sec);
			tvout.tv_usec = fxdr_unsigned(long, tvout.tv_usec);
			if (nuidp->nu_expire < time_second ||
			    nuidp->nu_timestamp.tv_sec > tvout.tv_sec ||
			    (nuidp->nu_timestamp.tv_sec == tvout.tv_sec &&
			     nuidp->nu_timestamp.tv_usec > tvout.tv_usec)) {
				nuidp->nu_expire = 0;
				nd->nd_repstat =
				    (NFSERR_AUTHERR|AUTH_REJECTVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			kauth_cred_hold(nuidp->nu_cr);
			nd->nd_cr = nuidp->nu_cr;
			nd->nd_flag |= ND_KERBNICK;
		}
	} else {
		nd->nd_repstat = (NFSERR_AUTHERR | AUTH_REJECTCRED);
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}

	nd->nd_md = md;
	nd->nd_dpos = dpos;
	KASSERT((nd->nd_cr == NULL && (nfsd->nfsd_flag & NFSD_NEEDAUTH) != 0)
	     || (nd->nd_cr != NULL && (nfsd->nfsd_flag & NFSD_NEEDAUTH) == 0));
	return (0);
nfsmout:
errout:
	KASSERT(error != 0);
	if (nd->nd_cr != NULL) {
		kauth_cred_free(nd->nd_cr);
		nd->nd_cr = NULL;
	}
	return (error);
}

int
nfs_msg(l, server, msg)
	struct lwp *l;
	const char *server, *msg;
{
	tpr_t tpr;

	if (l)
		tpr = tprintf_open(l->l_proc);
	else
		tpr = NULL;
	tprintf(tpr, "nfs server %s: %s\n", server, msg);
	tprintf_close(tpr);
	return (0);
}

#ifdef NFSSERVER
int (*nfsrv3_procs[NFS_NPROCS]) __P((struct nfsrv_descript *,
				    struct nfssvc_sock *, struct lwp *,
				    struct mbuf **)) = {
	nfsrv_null,
	nfsrv_getattr,
	nfsrv_setattr,
	nfsrv_lookup,
	nfsrv3_access,
	nfsrv_readlink,
	nfsrv_read,
	nfsrv_write,
	nfsrv_create,
	nfsrv_mkdir,
	nfsrv_symlink,
	nfsrv_mknod,
	nfsrv_remove,
	nfsrv_rmdir,
	nfsrv_rename,
	nfsrv_link,
	nfsrv_readdir,
	nfsrv_readdirplus,
	nfsrv_statfs,
	nfsrv_fsinfo,
	nfsrv_pathconf,
	nfsrv_commit,
	nfsrv_noop
};

/*
 * Socket upcall routine for the nfsd sockets.
 * The void *arg is a pointer to the "struct nfssvc_sock".
 */
void
nfsrv_soupcall(struct socket *so, void *arg, int waitflag)
{
	struct nfssvc_sock *slp = (struct nfssvc_sock *)arg;

	nfsdsock_setbits(slp, SLP_A_NEEDQ);
	nfsrv_wakenfsd(slp);
}

void
nfsrv_rcv(struct nfssvc_sock *slp)
{
	struct socket *so;
	struct mbuf *m;
	struct mbuf *mp, *nam;
	struct uio auio;
	int flags;
	int error;
	int setflags = 0;

	error = nfsdsock_lock(slp, true);
	if (error) {
		setflags |= SLP_A_NEEDQ;
		goto dorecs_unlocked;
	}

	nfsdsock_clearbits(slp, SLP_A_NEEDQ);

	so = slp->ns_so;
	if (so->so_type == SOCK_STREAM) {
		/*
		 * Do soreceive().
		 */
		auio.uio_resid = 1000000000;
		/* not need to setup uio_vmspace */
		flags = MSG_DONTWAIT;
		error = (*so->so_receive)(so, &nam, &auio, &mp, NULL, &flags);
		if (error || mp == NULL) {
			if (error == EWOULDBLOCK)
				setflags |= SLP_A_NEEDQ;
			else
				setflags |= SLP_A_DISCONN;
			goto dorecs;
		}
		m = mp;
		m_claimm(m, &nfs_mowner);
		if (slp->ns_rawend) {
			slp->ns_rawend->m_next = m;
			slp->ns_cc += 1000000000 - auio.uio_resid;
		} else {
			slp->ns_raw = m;
			slp->ns_cc = 1000000000 - auio.uio_resid;
		}
		while (m->m_next)
			m = m->m_next;
		slp->ns_rawend = m;

		/*
		 * Now try and parse record(s) out of the raw stream data.
		 */
		error = nfsrv_getstream(slp, M_WAIT);
		if (error) {
			if (error == EPERM)
				setflags |= SLP_A_DISCONN;
			else
				setflags |= SLP_A_NEEDQ;
		}
	} else {
		do {
			auio.uio_resid = 1000000000;
			/* not need to setup uio_vmspace */
			flags = MSG_DONTWAIT;
			error = (*so->so_receive)(so, &nam, &auio, &mp, NULL,
			    &flags);
			if (mp) {
				if (nam) {
					m = nam;
					m->m_next = mp;
				} else
					m = mp;
				m_claimm(m, &nfs_mowner);
				if (slp->ns_recend)
					slp->ns_recend->m_nextpkt = m;
				else
					slp->ns_rec = m;
				slp->ns_recend = m;
				m->m_nextpkt = (struct mbuf *)0;
			}
			if (error) {
				if ((so->so_proto->pr_flags & PR_CONNREQUIRED)
				    && error != EWOULDBLOCK) {
					setflags |= SLP_A_DISCONN;
					goto dorecs;
				}
			}
		} while (mp);
	}
dorecs:
	nfsdsock_unlock(slp);

dorecs_unlocked:
	if (setflags) {
		nfsdsock_setbits(slp, setflags);
	}
}

int
nfsdsock_lock(struct nfssvc_sock *slp, bool waitok)
{

	mutex_enter(&slp->ns_lock);
	while ((~slp->ns_flags & (SLP_BUSY|SLP_VALID)) == 0) {
		if (!waitok) {
			mutex_exit(&slp->ns_lock);
			return EWOULDBLOCK;
		}
		cv_wait(&slp->ns_cv, &slp->ns_lock);
	}
	if ((slp->ns_flags & SLP_VALID) == 0) {
		mutex_exit(&slp->ns_lock);
		return EINVAL;
	}
	KASSERT((slp->ns_flags & SLP_BUSY) == 0);
	slp->ns_flags |= SLP_BUSY;
	mutex_exit(&slp->ns_lock);

	return 0;
}

void
nfsdsock_unlock(struct nfssvc_sock *slp)
{

	mutex_enter(&slp->ns_lock);
	KASSERT((slp->ns_flags & SLP_BUSY) != 0);
	cv_broadcast(&slp->ns_cv);
	slp->ns_flags &= ~SLP_BUSY;
	mutex_exit(&slp->ns_lock);
}

int
nfsdsock_drain(struct nfssvc_sock *slp)
{
	int error = 0;

	mutex_enter(&slp->ns_lock);
	if ((slp->ns_flags & SLP_VALID) == 0) {
		error = EINVAL;
		goto done;
	}
	slp->ns_flags &= ~SLP_VALID;
	while ((slp->ns_flags & SLP_BUSY) != 0) {
		cv_wait(&slp->ns_cv, &slp->ns_lock);
	}
done:
	mutex_exit(&slp->ns_lock);

	return error;
}

/*
 * Try and extract an RPC request from the mbuf data list received on a
 * stream socket. The "waitflag" argument indicates whether or not it
 * can sleep.
 */
int
nfsrv_getstream(slp, waitflag)
	struct nfssvc_sock *slp;
	int waitflag;
{
	struct mbuf *m, **mpp;
	struct mbuf *recm;
	u_int32_t recmark;
	int error = 0;

	KASSERT((slp->ns_flags & SLP_BUSY) != 0);
	for (;;) {
		if (slp->ns_reclen == 0) {
			if (slp->ns_cc < NFSX_UNSIGNED) {
				break;
			}
			m = slp->ns_raw;
			m_copydata(m, 0, NFSX_UNSIGNED, (void *)&recmark);
			m_adj(m, NFSX_UNSIGNED);
			slp->ns_cc -= NFSX_UNSIGNED;
			recmark = ntohl(recmark);
			slp->ns_reclen = recmark & ~0x80000000;
			if (recmark & 0x80000000)
				slp->ns_sflags |= SLP_S_LASTFRAG;
			else
				slp->ns_sflags &= ~SLP_S_LASTFRAG;
			if (slp->ns_reclen > NFS_MAXPACKET) {
				error = EPERM;
				break;
			}
		}

		/*
		 * Now get the record part.
		 *
		 * Note that slp->ns_reclen may be 0.  Linux sometimes
		 * generates 0-length records.
		 */
		if (slp->ns_cc == slp->ns_reclen) {
			recm = slp->ns_raw;
			slp->ns_raw = slp->ns_rawend = (struct mbuf *)0;
			slp->ns_cc = slp->ns_reclen = 0;
		} else if (slp->ns_cc > slp->ns_reclen) {
			recm = slp->ns_raw;
			m = m_split(recm, slp->ns_reclen, waitflag);
			if (m == NULL) {
				error = EWOULDBLOCK;
				break;
			}
			m_claimm(recm, &nfs_mowner);
			slp->ns_raw = m;
			if (m->m_next == NULL)
				slp->ns_rawend = m;
			slp->ns_cc -= slp->ns_reclen;
			slp->ns_reclen = 0;
		} else {
			break;
		}

		/*
		 * Accumulate the fragments into a record.
		 */
		mpp = &slp->ns_frag;
		while (*mpp)
			mpp = &((*mpp)->m_next);
		*mpp = recm;
		if (slp->ns_sflags & SLP_S_LASTFRAG) {
			if (slp->ns_recend)
				slp->ns_recend->m_nextpkt = slp->ns_frag;
			else
				slp->ns_rec = slp->ns_frag;
			slp->ns_recend = slp->ns_frag;
			slp->ns_frag = NULL;
		}
	}

	return error;
}

/*
 * Parse an RPC header.
 */
int
nfsrv_dorec(struct nfssvc_sock *slp, struct nfsd *nfsd,
    struct nfsrv_descript **ndp, bool *more)
{
	struct mbuf *m, *nam;
	struct nfsrv_descript *nd;
	int error;

	*ndp = NULL;
	*more = false;

	if (nfsdsock_lock(slp, true)) {
		return ENOBUFS;
	}
	m = slp->ns_rec;
	if (m == NULL) {
		nfsdsock_unlock(slp);
		return ENOBUFS;
	}
	slp->ns_rec = m->m_nextpkt;
	if (slp->ns_rec) {
		m->m_nextpkt = NULL;
		*more = true;
	} else {
		slp->ns_recend = NULL;
	}
	nfsdsock_unlock(slp);

	if (m->m_type == MT_SONAME) {
		nam = m;
		m = m->m_next;
		nam->m_next = NULL;
	} else
		nam = NULL;
	nd = nfsdreq_alloc();
	nd->nd_md = nd->nd_mrep = m;
	nd->nd_nam2 = nam;
	nd->nd_dpos = mtod(m, void *);
	error = nfs_getreq(nd, nfsd, true);
	if (error) {
		m_freem(nam);
		nfsdreq_free(nd);
		return (error);
	}
	*ndp = nd;
	nfsd->nfsd_nd = nd;
	return (0);
}

/*
 * Search for a sleeping nfsd and wake it up.
 * SIDE EFFECT: If none found, set NFSD_CHECKSLP flag, so that one of the
 * running nfsds will go look for the work in the nfssvc_sock list.
 */
static void
nfsrv_wakenfsd_locked(struct nfssvc_sock *slp)
{
	struct nfsd *nd;

	KASSERT(mutex_owned(&nfsd_lock));

	if ((slp->ns_flags & SLP_VALID) == 0)
		return;
	if (slp->ns_gflags & SLP_G_DOREC)
		return;
	nd = SLIST_FIRST(&nfsd_idle_head);
	if (nd) {
		SLIST_REMOVE_HEAD(&nfsd_idle_head, nfsd_idle);
		if (nd->nfsd_slp)
			panic("nfsd wakeup");
		slp->ns_sref++;
		KASSERT(slp->ns_sref > 0);
		nd->nfsd_slp = slp;
		cv_signal(&nd->nfsd_cv);
	} else {
		slp->ns_gflags |= SLP_G_DOREC;
		nfsd_head_flag |= NFSD_CHECKSLP;
		TAILQ_INSERT_TAIL(&nfssvc_sockpending, slp, ns_pending);
	}
}

void
nfsrv_wakenfsd(struct nfssvc_sock *slp)
{

	mutex_enter(&nfsd_lock);
	nfsrv_wakenfsd_locked(slp);
	mutex_exit(&nfsd_lock);
}

int
nfsdsock_sendreply(struct nfssvc_sock *slp, struct nfsrv_descript *nd)
{
	int error;

	if (nd->nd_mrep != NULL) {
		m_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
	}

	mutex_enter(&slp->ns_lock);
	if ((slp->ns_flags & SLP_SENDING) != 0) {
		SIMPLEQ_INSERT_TAIL(&slp->ns_sendq, nd, nd_sendq);
		mutex_exit(&slp->ns_lock);
		return 0;
	}
	KASSERT(SIMPLEQ_EMPTY(&slp->ns_sendq));
	slp->ns_flags |= SLP_SENDING;
	mutex_exit(&slp->ns_lock);

again:
	error = nfs_send(slp->ns_so, nd->nd_nam2, nd->nd_mreq, NULL, curlwp);
	if (nd->nd_nam2) {
		m_free(nd->nd_nam2);
	}
	nfsdreq_free(nd);

	mutex_enter(&slp->ns_lock);
	KASSERT((slp->ns_flags & SLP_SENDING) != 0);
	nd = SIMPLEQ_FIRST(&slp->ns_sendq);
	if (nd != NULL) {
		SIMPLEQ_REMOVE_HEAD(&slp->ns_sendq, nd_sendq);
		mutex_exit(&slp->ns_lock);
		goto again;
	}
	slp->ns_flags &= ~SLP_SENDING;
	mutex_exit(&slp->ns_lock);

	return error;
}

void
nfsdsock_setbits(struct nfssvc_sock *slp, int bits)
{

	mutex_enter(&slp->ns_alock);
	slp->ns_aflags |= bits;
	mutex_exit(&slp->ns_alock);
}

void
nfsdsock_clearbits(struct nfssvc_sock *slp, int bits)
{

	mutex_enter(&slp->ns_alock);
	slp->ns_aflags &= ~bits;
	mutex_exit(&slp->ns_alock);
}

bool
nfsdsock_testbits(struct nfssvc_sock *slp, int bits)
{

	return (slp->ns_aflags & bits);
}
#endif /* NFSSERVER */

#if defined(NFSSERVER) || (defined(NFS) && !defined(NFS_V2_ONLY))
static struct pool nfs_srvdesc_pool;

void
nfsdreq_init(void)
{

	pool_init(&nfs_srvdesc_pool, sizeof(struct nfsrv_descript),
	    0, 0, 0, "nfsrvdescpl", &pool_allocator_nointr, IPL_NONE);
}

struct nfsrv_descript *
nfsdreq_alloc(void)
{
	struct nfsrv_descript *nd;

	nd = pool_get(&nfs_srvdesc_pool, PR_WAITOK);
	nd->nd_cr = NULL;
	return nd;
}

void
nfsdreq_free(struct nfsrv_descript *nd)
{
	kauth_cred_t cr;

	cr = nd->nd_cr;
	if (cr != NULL) {
		kauth_cred_free(cr);
	}
	pool_put(&nfs_srvdesc_pool, nd);
}
#endif /* defined(NFSSERVER) || (defined(NFS) && !defined(NFS_V2_ONLY)) */
