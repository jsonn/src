/* $NetBSD: softintr.c,v 1.3.2.2 2004/09/18 14:39:42 skrll Exp $ */

/*
 * Copyright 2000, 2001
 * Broadcom Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and copied only
 * in accordance with the following terms and conditions.  Subject to these
 * conditions, you may download, copy, install, use, modify and distribute
 * modified or unmodified copies of this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce and
 *    retain this copyright notice and list of conditions as they appear in
 *    the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Broadcom Corporation.  The "Broadcom Corporation" name may not be
 *    used to endorse or promote products derived from this software
 *    without the prior written permission of Broadcom Corporation.
 *
 * 3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR IMPLIED
 *    WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED WARRANTIES OF
 *    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 *    NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM BE LIABLE
 *    FOR ANY DAMAGES WHATSOEVER, AND IN PARTICULAR, BROADCOM SHALL NOT BE
 *    LIABLE FOR DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *    OR OTHERWISE), EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: softintr.c,v 1.3.2.2 2004/09/18 14:39:42 skrll Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

/* XXX for uvmexp */
#include <uvm/uvm_extern.h>

#include <machine/intr.h>
#include <machine/systemsw.h>

#include <net/netisr.h>

struct sh {
	struct sh *next;
	void	(*fun)(void *);
	void	*arg;
};

static struct sh *sh_list_head = NULL;
static struct sh **sh_list_tail = &sh_list_head;

static void dosoftnet(void *unused);
static void dosoftclock(void *unused);

static struct evcnt softclock_ev =
    EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "soft", "clock");
static struct evcnt softnet_ev =
    EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "soft", "net");

EVCNT_ATTACH_STATIC(softclock_ev);
EVCNT_ATTACH_STATIC(softnet_ev);

void *
softintr_establish(int level, void (*fun)(void *), void *arg)
{
	struct sh *new, *sh;

	new = malloc(sizeof *sh, M_DEVBUF, M_WAITOK);
	new->next = NULL;
	new->fun = fun;
	new->arg = arg;

	return new;
}

void
softintr_disestablish(void *cookie)
{
	struct sh *sh, **prevnext;
	int s;

	s = splhigh();
	for (prevnext = &sh_list_head, sh = *prevnext;
	    sh != NULL;
	    prevnext = &sh->next, sh = *prevnext) {
		if (sh == cookie) {
			*prevnext = sh->next;
			if (*prevnext == NULL)
				sh_list_tail = prevnext;
			break;
		}
	}
	splx(s);
	free(sh, M_DEVBUF);
}

void
softintr_schedule(void *cookie)
{
	struct sh *sh = cookie;
	int s;

	s = splhigh();
	if (sh->next == NULL) {
		*sh_list_tail = sh;
		sh_list_tail = &sh->next;
		cpu_setsoftintr();
	}
	splx(s);
}

void
dosoftints(void)
{
	struct sh *sh;
	void (*fun)(void *), *arg;
	int s;

	while (1) {
		s = splhigh();

		sh = sh_list_head;
		if (sh == NULL)
			break;

		uvmexp.softs++;

		if ((sh_list_head = sh->next) == NULL)
			sh_list_tail = &sh_list_head;
		sh->next = NULL;
		fun = sh->fun;
		arg = sh->arg;

		splx(s);

		(*fun)(arg);
	}
	splx(s);
}

void
setsoftclock()
{
	static struct sh sh_softclock = { NULL, dosoftclock, NULL, };

	softintr_schedule(&sh_softclock);
}

static void
dosoftclock(void *unused)
{

	softclock_ev.ev_count++;
	softclock(NULL);
}

void
setsoftnet()
{
	static struct sh sh_softnet = { NULL, dosoftnet, NULL, };

	softintr_schedule(&sh_softnet);
}

/* XXX BEGIN */
#include "opt_inet.h"
#include <sys/mbuf.h>
#ifdef INET
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include "arp.h"
#if NARP > 0
#include <netinet/if_inarp.h>
#endif
#endif
#ifdef INET6
# ifndef INET
#  include <netinet/in.h>
# endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif
/* XXX END */

static void
dosoftnet(void *unused)
{
	int n, s;

	softnet_ev.ev_count++;

	/* XXX could just use netintr! */

	s = splhigh();
	n = netisr;
	netisr = 0;
	splx(s);

#define DONETISR(bit, fn)						\
	do {								\
		if (n & (1 << (bit)))					\
			fn();						\
	} while (0)

#include <net/netisr_dispatch.h>

#undef DONETISR 
}
