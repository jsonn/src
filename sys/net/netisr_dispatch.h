/* $NetBSD: netisr_dispatch.h,v 1.10.14.1 2005/04/29 11:29:32 kent Exp $ */

/*
 * netisr_dispatch: This file is included by the
 *	machine dependant softnet function.  The
 *	DONETISR macro should be set before including
 *	this file.  i.e.:
 *
 * softintr() {
 *	...do setup stuff...
 *	#define DONETISR(bit, fn) do { ... } while (0)
 *	#include <net/netisr_dispatch.h>
 *	#undef DONETISR
 *	...do cleanup stuff.
 * }
 */

#ifndef _NET_NETISR_H_
#error <net/netisr.h> must be included before <net/netisr_dispatch.h>
#endif

/*
 * When adding functions to this list, be sure to add headers to provide
 * their prototypes in <net/netisr.h> (if necessary).
 */

#ifdef INET
#if NARP > 0
	DONETISR(NETISR_ARP,arpintr);
#endif
	DONETISR(NETISR_IP,ipintr);
#endif
#ifdef INET6
	DONETISR(NETISR_IPV6,ip6intr);
#endif
#ifdef NETATALK
	DONETISR(NETISR_ATALK,atintr);
#endif
#ifdef NS
	DONETISR(NETISR_NS,nsintr);
#endif
#ifdef ISO
	DONETISR(NETISR_ISO,clnlintr);
#endif
#ifdef CCITT
	DONETISR(NETISR_CCITT,ccittintr);
#endif
#ifdef NATM
	DONETISR(NETISR_NATM,natmintr);
#endif
#if NSL > 0 && !defined(__HAVE_GENERIC_SOFT_INTERRUPTS)
	DONETISR(NETISR_SLIP,slnetisr);
#endif
#if NSTRIP > 0 && !defined(__HAVE_GENERIC_SOFT_INTERRUPTS)
	DONETISR(NETISR_STRIP,stripnetisr);
#endif
#if NPPP > 0 && !defined(__HAVE_GENERIC_SOFT_INTERRUPTS)
	DONETISR(NETISR_PPP,pppnetisr);
#endif
