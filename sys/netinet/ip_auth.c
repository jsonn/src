/*	$NetBSD: ip_auth.c,v 1.4.2.1 1997/10/30 07:13:40 mrg Exp $	*/

/*
 * Copyright (C) 1997 by Darren Reed & Guido van Rooij.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char rcsid[] = "@(#)Id: ip_auth.c,v 2.0.2.21 1997/10/29 12:14:04 darrenr Exp ";
#endif

#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdlib.h>
# include <string.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if defined(KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
#else
# include <sys/ioctl.h>
#endif
#include <sys/uio.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#ifdef _KERNEL
# include <sys/systm.h>
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# include <sys/mbuf.h>
#else
# include <sys/filio.h>
# include <sys/byteorder.h>
# include <sys/dditypes.h>
# include <sys/stream.h>
# include <sys/kmem.h>
#endif
#ifdef	__NetBSD__
# include <machine/cpu.h>
#endif
#include <net/if.h>
#ifdef sun
#include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifndef	KERNEL
#define	KERNEL
#define	NOT_KERNEL
#endif
#include <netinet/ip_var.h>
#ifdef	NOT_KERNEL
#undef	KERNEL
#endif
#ifdef __sgi
# ifdef IFF_DRVRLOCK /* IRIX6 */
#include <sys/hashing.h>
# endif
#endif
#if defined(__sgi) && !defined(IFF_DRVRLOCK) /* IRIX < 6 */
extern struct ifqueue   ipintrq;                /* ip packet input queue */
#else
#include <netinet/in_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_auth.h"
#if !SOLARIS
# include <net/netisr.h>
#endif


#if (SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern kmutex_t ipf_auth;
# if SOLARIS
extern kcondvar_t ipfauthwait;
# endif
#endif

int	fr_authsize = FR_NUMAUTH;
int	fr_authused = 0;
int	fr_defaultauthage = 600;
fr_authstat_t	fr_authstats;
frauth_t fr_auth[FR_NUMAUTH];
mb_t	*fr_authpkts[FR_NUMAUTH];
int	fr_authstart = 0, fr_authend = 0, fr_authnext = 0;
frauthent_t	*fae_list = NULL;
frentry_t	*ipauth = NULL;


/*
 * Check if a packet has authorization.  If the packet is found to match an
 * authorization result and that would result in a feedback loop (i.e. it
 * will end up returning FR_AUTH) then return FR_BLOCK instead.
 */
int fr_checkauth(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	u_short id = ip->ip_id;
	u_32_t pass;
	int i;

	MUTEX_ENTER(&ipf_auth);
	for (i = fr_authstart; i != fr_authend; ) {
		/*
		 * index becomes -2 only after an SIOCAUTHW.  Check this in
		 * case the same packet gets sent again and it hasn't yet been
		 * auth'd.
		 */
		if ((fr_auth[i].fra_index == -2) &&
		    (id == fr_auth[i].fra_info.fin_id) &&
		    !bcmp((char *)fin,(char *)&fr_auth[i].fra_info,FI_CSIZE)) {
			/*
			 * Avoid feedback loop.
			 */
			if (!(pass = fr_auth[i].fra_pass) || (pass & FR_AUTH))
				pass = FR_BLOCK;
			fr_authstats.fas_hits++;
			fr_auth[i].fra_index = -1;
			fr_authused--;
			if (i == fr_authstart) {
				while (fr_auth[i].fra_index == -1) {
					i++;
					if (i == FR_NUMAUTH)
						i = 0;
					fr_authstart = i;
					if (i == fr_authend)
						break;
				}
				if (fr_authstart == fr_authend) {
					fr_authnext = 0;
					fr_authstart = fr_authend = 0;
				}
			}
			MUTEX_EXIT(&ipf_auth);
			return pass;
		}
		i++;
		if (i == FR_NUMAUTH)
			i = 0;
	}
	fr_authstats.fas_miss++;
	MUTEX_EXIT(&ipf_auth);
	return 0;
}


/*
 * Check if we have room in the auth array to hold details for another packet.
 * If we do, store it and wake up any user programs which are waiting to
 * hear about these events.
 */
int fr_newauth(m, fin, ip
#if defined(_KERNEL) && SOLARIS
, qif)
qif_t *qif;
#else
)
#endif
mb_t *m;
fr_info_t *fin;
ip_t *ip;
{
	int i;

	MUTEX_ENTER(&ipf_auth);
	if ((fr_authstart > fr_authend) && (fr_authstart - fr_authend == -1)) {
		fr_authstats.fas_nospace++;
		MUTEX_EXIT(&ipf_auth);
		return 0;
	}
	if (fr_authend - fr_authstart == FR_NUMAUTH - 1) {
		fr_authstats.fas_nospace++;
		MUTEX_EXIT(&ipf_auth);
		return 0;
	}

	fr_authstats.fas_added++;
	fr_authused++;
	i = fr_authend++;
	if (fr_authend == FR_NUMAUTH)
		fr_authend = 0;
	MUTEX_EXIT(&ipf_auth);
	fr_auth[i].fra_index = i;
	fr_auth[i].fra_pass = 0;
	fr_auth[i].fra_age = fr_defaultauthage;
	bcopy((char *)fin, (char *)&fr_auth[i].fra_info, sizeof(*fin));
#if !defined(sparc) && !defined(m68k)
	/*
	 * No need to copyback here as we want to undo the changes, not keep
	 * them.
	 */
# if SOLARIS && defined(_KERNEL)
	if (ip == (ip_t *)m->b_rptr)
# endif
	{
		register u_short bo;

		bo = ip->ip_len;
		ip->ip_len = htons(bo);
# if !SOLARIS	/* 4.4BSD converts this ip_input.c, but I don't in solaris.c */
		bo = ip->ip_id;
		ip->ip_id = htons(bo);
# endif
		bo = ip->ip_off;
		ip->ip_off = htons(bo);
	}
#endif
#if SOLARIS && defined(_KERNEL)
	m->b_rptr -= qif->qf_off;
	fr_authpkts[i] = *(mblk_t **)fin->fin_mp;
	fr_auth[i].fra_q = qif->qf_q;
	cv_signal(&ipfauthwait);
#else
	fr_authpkts[i] = m;
	WAKEUP(&fr_authnext);
#endif
	return 1;
}


int fr_auth_ioctl(data, cmd, fr, frptr)
caddr_t data;
#if defined(__NetBSD__) || defined(__OpenBSD__)
u_long cmd;
#else
int cmd;
#endif
frentry_t *fr, **frptr;
{
	mb_t *m;
#if defined(_KERNEL)
# if !SOLARIS
	struct ifqueue *ifq;
	int s;
# endif
#endif
	frauth_t auth, *au = &auth;
	frauthent_t *fae, **faep;
	int i, error = 0;

	switch (cmd)
	{
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		error = EINVAL;
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
		for (faep = &fae_list; (fae = *faep); )
			if (&fae->fae_fr == fr)
				break;
			else
				faep = &fae->fae_next;
		if (cmd == SIOCRMAFR) {
			if (!fae)
				error = ESRCH;
			else {
				*faep = fae->fae_next;
				*frptr = fr->fr_next;
				KFREE(fae);
			}
		} else {
			KMALLOC(fae, frauthent_t *, sizeof(*fae));
			if (fae != NULL) {
				IRCOPY((char *)data, (char *)&fae->fae_fr,
				       sizeof(fae->fae_fr));
				if (!fae->fae_age)
					fae->fae_age = fr_defaultauthage;
				fae->fae_fr.fr_hits = 0;
				fae->fae_fr.fr_next = *frptr;
				*frptr = &fae->fae_fr;
				fae->fae_next = *faep;
				*faep = fae;
			} else
				error = ENOMEM;
		}
		break;
	case SIOCATHST:
		IWCOPY((char *)&fr_authstats, data, sizeof(fr_authstats));
		break;
	case SIOCAUTHW:
fr_authioctlloop:
		MUTEX_ENTER(&ipf_auth);
		if ((fr_authnext != fr_authend) && fr_authpkts[fr_authnext]) {
			IWCOPY((char *)&fr_auth[fr_authnext++], data,
			       sizeof(fr_info_t));
			if (fr_authnext == FR_NUMAUTH)
				fr_authnext = 0;
			MUTEX_EXIT(&ipf_auth);
			return 0;
		}
#ifdef	_KERNEL
# if	SOLARIS
		if (!cv_wait_sig(&ipfauthwait, &ipf_auth)) {
			mutex_exit(&ipf_auth);
			return EINTR;
		}
# else
		error = SLEEP(&fr_authnext, "fr_authnext");
# endif
#endif
		MUTEX_EXIT(&ipf_auth);
		if (!error)
			goto fr_authioctlloop;
		break;
	case SIOCAUTHR:
		IRCOPY(data, (caddr_t)&auth, sizeof(auth));
		MUTEX_ENTER(&ipf_auth);
		i = au->fra_index;
		if ((i < 0) || (i > FR_NUMAUTH) ||
		    (fr_auth[i].fra_info.fin_id != au->fra_info.fin_id)) {
			MUTEX_EXIT(&ipf_auth);
			return EINVAL;
		}
		m = fr_authpkts[i];
		fr_auth[i].fra_index = -2;
		fr_auth[i].fra_pass = au->fra_pass;
		fr_authpkts[i] = NULL;
#ifdef	_KERNEL
		MUTEX_EXIT(&ipf_auth);
		SPL_NET(s);
		if (m && au->fra_info.fin_out) {
# if SOLARIS
			error = fr_qout(fr_auth[i].fra_q, m);
# else /* SOLARIS */
			error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL);
# endif /* SOLARIS */
			if (error)
				fr_authstats.fas_sendfail++;
			else
				fr_authstats.fas_sendok++;
		} else if (m) {
# if SOLARIS
			error = fr_qin(fr_auth[i].fra_q, m);
# else /* SOLARIS */
			ifq = &ipintrq;
			if (IF_QFULL(ifq)) {
				IF_DROP(ifq);
				m_freem(m);
				error = ENOBUFS;
			} else {
				IF_ENQUEUE(ifq, m);
				schednetisr(NETISR_IP);
			}
# endif /* SOLARIS */
			if (error)
				fr_authstats.fas_quefail++;
			else
				fr_authstats.fas_queok++;
		} else
			error = EINVAL;
# if SOLARIS
		if (error)
			error = EINVAL;
# else
		/*
		 * If we experience an error which will result in the packet
		 * not being processed, make sure we advance to the next one.
		 */ 
		if (error == ENOBUFS) {
			fr_authused--;
			fr_auth[i].fra_index = -1;
			fr_auth[i].fra_pass = 0;
			if (i == fr_authstart) {
				while (fr_auth[i].fra_index == -1) {
					i++;
					if (i == FR_NUMAUTH)
						i = 0;
					fr_authstart = i;
					if (i == fr_authend)
						break;
				}
				if (fr_authstart == fr_authend) {
					fr_authnext = 0;
					fr_authstart = fr_authend = 0;
				}
			}
		}
# endif
		SPL_X(s);
#endif /* _KERNEL */
		break;
	default :
		error = EINVAL;
		break;
	}
	return error;
}


#ifdef	_KERNEL
/*
 * Free all network buffer memory used to keep saved packets.
 */
void fr_authunload()
{
	register int i;
	register frauthent_t *fae, **faep;
	mb_t *m;

	MUTEX_ENTER(&ipf_auth);
	for (i = 0; i < FR_NUMAUTH; i++) {
		if ((m = fr_authpkts[i])) {
			FREE_MB_T(m);
			fr_authpkts[i] = NULL;
			fr_auth[i].fra_index = -1;
		}
	}


	for (faep = &fae_list; (fae = *faep); ) {
		*faep = fae->fae_next;
		KFREE(fae);
	}
	MUTEX_EXIT(&ipf_auth);
}


/*
 * Slowly expire held auth records.  Timeouts are set
 * in expectation of this being called twice per second.
 */
void fr_authexpire()
{
	register int i;
	register frauth_t *fra;
	register frauthent_t *fae, **faep;
	mb_t *m;
#if !SOLARIS
	int s;
#endif

	SPL_NET(s);
	MUTEX_ENTER(&ipf_auth);
	for (i = 0, fra = fr_auth; i < FR_NUMAUTH; i++, fra++) {
		if ((!--fra->fra_age) && (m = fr_authpkts[i])) {
			FREE_MB_T(m);
			fr_authpkts[i] = NULL;
			fr_auth[i].fra_index = -1;
			fr_authstats.fas_expire++;
			fr_authused--;
		}
	}

	for (faep = &fae_list; (fae = *faep); ) {
		if (!--fra->fra_age) {
			*faep = fae->fae_next;
			KFREE(fae);
			fr_authstats.fas_expire++;
		} else
			faep = &fae->fae_next;
	}
	MUTEX_EXIT(&ipf_auth);
	SPL_X(s);
}
#endif
