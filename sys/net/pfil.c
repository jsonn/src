/*	$NetBSD: pfil.c,v 1.9.2.2 2000/11/22 16:05:57 bouyer Exp $	*/

/*
 * Copyright (c) 1996 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/pfil.h>

static int pfil_list_add(pfil_list_t *,
    int (*)(void *, struct mbuf **, struct ifnet *, int), void *, int);

static int pfil_list_remove(pfil_list_t *,
    int (*)(void *, struct mbuf **, struct ifnet *, int), void *);

LIST_HEAD(, pfil_head) pfil_head_list =
    LIST_HEAD_INITIALIZER(&pfil_head_list);

/*
 * pfil_run_hooks() runs the specified packet filter hooks.
 */
int
pfil_run_hooks(struct pfil_head *ph, struct mbuf **mp, struct ifnet *ifp,
    int dir)
{
	struct packet_filter_hook *pfh;
	struct mbuf *m = *mp;
	int rv = 0;

	for (pfh = pfil_hook_get(dir, ph); pfh != NULL;
	     pfh = TAILQ_NEXT(pfh, pfil_link)) {
		if (pfh->pfil_func != NULL) {
			rv = (*pfh->pfil_func)(pfh->pfil_arg, &m, ifp, dir);
			if (rv != 0 || m == NULL)
				break;
		}
	}

	*mp = m;
	return (rv);
}

/*
 * pfil_head_register() registers a pfil_head with the packet filter
 * hook mechanism.
 */
int
pfil_head_register(struct pfil_head *ph)
{
	struct pfil_head *lph;

	for (lph = LIST_FIRST(&pfil_head_list); lph != NULL;
	     lph = LIST_NEXT(lph, ph_list)) {
		if (lph->ph_key == ph->ph_key &&
		    lph->ph_dlt == ph->ph_dlt)
			return EEXIST;
	}

	TAILQ_INIT(&ph->ph_in);
	TAILQ_INIT(&ph->ph_out);

	LIST_INSERT_HEAD(&pfil_head_list, ph, ph_list);

	return (0);
}

/*
 * pfil_head_unregister() removes a pfil_head from the packet filter
 * hook mechanism.
 */
int
pfil_head_unregister(struct pfil_head *pfh)
{

	LIST_REMOVE(pfh, ph_list);
	return (0);
}

/*
 * pfil_head_get() returns the pfil_head for a given key/dlt.
 */
struct pfil_head *
pfil_head_get(void *key, int dlt)
{
	struct pfil_head *ph;

	for (ph = LIST_FIRST(&pfil_head_list); ph != NULL;
	     ph = LIST_NEXT(ph, ph_list)) {
		if (ph->ph_key == key && ph->ph_dlt == dlt)
			break;
	}

	return (ph);
}

/*
 * pfil_add_hook() adds a function to the packet filter hook.  the
 * flags are:
 *	PFIL_IN		call me on incoming packets
 *	PFIL_OUT	call me on outgoing packets
 *	PFIL_ALL	call me on all of the above
 *	PFIL_WAITOK	OK to call malloc with M_WAITOK.
 */
int
pfil_add_hook(int (*func)(void *, struct mbuf **, struct ifnet *, int),
    void *arg, int flags, struct pfil_head *ph)
{
	int err = 0;

	if (flags & PFIL_IN) {
		err = pfil_list_add(&ph->ph_in, func, arg, flags & ~PFIL_OUT);
		if (err)
			return err;
	}
	if (flags & PFIL_OUT) {
		err = pfil_list_add(&ph->ph_out, func, arg, flags & ~PFIL_IN);
		if (err) {
			if (flags & PFIL_IN)
				pfil_list_remove(&ph->ph_in, func, arg);
			return err;
		}
	}
	return 0;
}

static int
pfil_list_add(pfil_list_t *list,
    int (*func)(void *, struct mbuf **, struct ifnet *, int), void *arg,
    int flags)
{
	struct packet_filter_hook *pfh;

	/*
	 * First make sure the hook is not already there.
	 */
	for (pfh = TAILQ_FIRST(list); pfh != NULL;
	     pfh = TAILQ_NEXT(pfh, pfil_link)) {
		if (pfh->pfil_func == func &&
		    pfh->pfil_arg == arg)
			return EEXIST;
	}

	pfh = (struct packet_filter_hook *)malloc(sizeof(*pfh), M_IFADDR,
	    (flags & PFIL_WAITOK) ? M_WAITOK : M_NOWAIT);
	if (pfh == NULL)
		return ENOMEM;

	pfh->pfil_func = func;
	pfh->pfil_arg  = arg;

	/*
	 * insert the input list in reverse order of the output list
	 * so that the same path is followed in or out of the kernel.
	 */
	if (flags & PFIL_IN)
		TAILQ_INSERT_HEAD(list, pfh, pfil_link);
	else
		TAILQ_INSERT_TAIL(list, pfh, pfil_link);

	return 0;
}

/*
 * pfil_remove_hook removes a specific function from the packet filter
 * hook list.
 */
int
pfil_remove_hook(int (*func)(void *, struct mbuf **, struct ifnet *, int),
    void *arg, int flags, struct pfil_head *ph)
{
	int err = 0;

	if (flags & PFIL_IN)
		err = pfil_list_remove(&ph->ph_in, func, arg);
	if ((err == 0) && (flags & PFIL_OUT))
		err = pfil_list_remove(&ph->ph_out, func, arg);
	return err;
}

/*
 * pfil_list_remove is an internal function that takes a function off the
 * specified list.
 */
static int
pfil_list_remove(pfil_list_t *list,
    int (*func)(void *, struct mbuf **, struct ifnet *, int), void *arg)
{
	struct packet_filter_hook *pfh;

	for (pfh = TAILQ_FIRST(list); pfh != NULL;
	     pfh = TAILQ_NEXT(pfh, pfil_link)) {
		if (pfh->pfil_func == func && pfh->pfil_arg == arg) {
			TAILQ_REMOVE(list, pfh, pfil_link);
			free(pfh, M_IFADDR);
			return 0;
		}
	}
	return ENOENT;
}
