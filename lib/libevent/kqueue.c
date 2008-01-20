/*	$NetBSD: kqueue.c,v 1.4.6.1 2008/01/20 21:07:40 bouyer Exp $	*/
/*	$OpenBSD: kqueue.c,v 1.5 2002/07/10 14:41:31 art Exp $	*/

/*
 * Copyright 2000-2002 Niels Provos <provos@citi.umich.edu>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <sys/_time.h>
#endif
#include <sys/queue.h>
#include <sys/event.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef USE_LOG
#include "log.h"
#else
#define LOG_DBG(x)
#define log_error	warn
#endif

#ifdef HAVE_INTTYPES_H
#define INTPTR(x)	(intptr_t)x
#else
#define INTPTR(x)	x
#endif

#include "event.h"

extern struct event_list timequeue;
extern struct event_list eventqueue;
extern struct event_list addqueue;

#define EVLIST_X_KQINKERNEL	0x1000

#define NEVENT		64

struct kqop {
	struct kevent *changes;
	int nchanges;
	struct kevent *events;
	int nevents;
	int kq;
} kqueueop;

void *kq_init	(void);
int kq_add	(void *, struct event *);
int kq_del	(void *, struct event *);
int kq_recalc	(void *, int);
int kq_dispatch	(void *, struct timeval *);
int kq_insert	(struct kqop *, struct kevent *);

struct eventop kqops = {
	"kqueue",
	kq_init,
	kq_add,
	kq_del,
	kq_recalc,
	kq_dispatch
};

void *
kq_init(void)
{
	int kq;

	/* Disable kqueue when this environment variable is set */
	if (!issetugid() && getenv("EVENT_NOKQUEUE"))
		return (NULL);

	memset(&kqueueop, 0, sizeof(kqueueop));

	/* Initalize the kernel queue */
	
	if ((kq = kqueue()) == -1) {
		log_error("kqueue");
		return (NULL);
	}

	kqueueop.kq = kq;

	/* Initalize fields */
	kqueueop.changes = malloc(NEVENT * sizeof(struct kevent));
	if (kqueueop.changes == NULL)
		return (NULL);
	kqueueop.events = malloc(NEVENT * sizeof(struct kevent));
	if (kqueueop.events == NULL) {
		free (kqueueop.changes);
		return (NULL);
	}
	kqueueop.nevents = NEVENT;

	return (&kqueueop);
}

int
kq_recalc(void *arg, int max)
{
	return (0);
}

int
kq_insert(struct kqop *kqop, struct kevent *kev)
{
	int nevents = kqop->nevents;

	if (kqop->nchanges == nevents) {
		struct kevent *newchange;
		struct kevent *newresult;

		nevents *= 2;

		newchange = realloc(kqop->changes,
				    nevents * sizeof(struct kevent));
		if (newchange == NULL) {
			log_error("%s: malloc", __func__);
			return (-1);
		}
		kqop->changes = newchange;

		newresult = realloc(kqop->events,
				    nevents * sizeof(struct kevent));

		/*
		 * If we fail, we don't have to worry about freeing,
		 * the next realloc will pick it up.
		 */
		if (newresult == NULL) {
			log_error("%s: malloc", __func__);
			return (-1);
		}
		kqop->events = newresult;

		kqop->nevents = nevents;
	}

	memcpy(&kqop->changes[kqop->nchanges++], kev, sizeof(struct kevent));

	LOG_DBG((LOG_MISC, 70, "%s: fd %d %s%s",
		 __func__, kev->ident, 
		 kev->filter == EVFILT_READ ? "EVFILT_READ" : "EVFILT_WRITE",
		 kev->flags == EV_DELETE ? " (del)" : ""));

	return (0);
}

static void
kq_sighandler(int sig)
{
	/* Do nothing here */
}

int
kq_dispatch(void *arg, struct timeval *tv)
{
	struct kqop *kqop = arg;
	struct kevent *changes = kqop->changes;
	struct kevent *events = kqop->events;
	struct event *ev;
	struct timespec ts;
	int i, res;

	TIMEVAL_TO_TIMESPEC(tv, &ts);

	res = kevent(kqop->kq, changes, kqop->nchanges,
	    events, kqop->nevents, &ts);
	kqop->nchanges = 0;
	if (res == -1) {
		if (errno != EINTR) {
			log_error("kevent");
			return (-1);
		}

		return (0);
	}

	LOG_DBG((LOG_MISC, 80, "%s: kevent reports %d", __func__, res));

	for (i = 0; i < res; i++) {
		int which = 0;

		if (events[i].flags & EV_ERROR) {
			/* 
			 * Error messages that can happen, when a delete fails.
			 *   EBADF happens when the file discriptor has been
			 *   closed,
			 *   ENOENT when the file discriptor was closed and
			 *   then reopened.
			 * An error is also indicated when a callback deletes
			 * an event we are still processing.  In that case
			 * the data field is set to ENOENT.
			 */
			if (events[i].data == EBADF ||
			    events[i].data == ENOENT)
				continue;
			return (-1);
		}

		ev = (struct event *)events[i].udata;

		if (events[i].filter == EVFILT_READ) {
			which |= EV_READ;
		} else if (events[i].filter == EVFILT_WRITE) {
			which |= EV_WRITE;
		} else if (events[i].filter == EVFILT_SIGNAL) {
			which |= EV_SIGNAL;
		}

		if (!which)
			continue;

		if (!(ev->ev_events & EV_PERSIST)) {
			ev->ev_flags &= ~EVLIST_X_KQINKERNEL;
			event_del(ev);
		}

		event_active(ev, which,
		    ev->ev_events & EV_SIGNAL ? events[i].data : 1);
	}

	return (0);
}


int
kq_add(void *arg, struct event *ev)
{
	struct kqop *kqop = arg;
	struct kevent kev;

	if (ev->ev_events & EV_SIGNAL) {
		int nsignal = EVENT_SIGNAL(ev);

 		memset(&kev, 0, sizeof(kev));
		kev.ident = nsignal;
		kev.filter = EVFILT_SIGNAL;
		kev.flags = EV_ADD;
		if (!(ev->ev_events & EV_PERSIST))
			kev.flags |= EV_ONESHOT;
		kev.udata = INTPTR(ev);
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);

		if (signal(nsignal, kq_sighandler) == SIG_ERR)
			return (-1);

		ev->ev_flags |= EVLIST_X_KQINKERNEL;
		return (0);
	}

	if (ev->ev_events & EV_READ) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_READ;
		kev.flags = EV_ADD;
		if (!(ev->ev_events & EV_PERSIST))
			kev.flags |= EV_ONESHOT;
		kev.udata = INTPTR(ev);
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);

		ev->ev_flags |= EVLIST_X_KQINKERNEL;
	}

	if (ev->ev_events & EV_WRITE) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_WRITE;
		kev.flags = EV_ADD;
		if (!(ev->ev_events & EV_PERSIST))
			kev.flags |= EV_ONESHOT;
		kev.udata = INTPTR(ev);
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);

		ev->ev_flags |= EVLIST_X_KQINKERNEL;
	}

	return (0);
}

int
kq_del(void *arg, struct event *ev)
{
	struct kqop *kqop = arg;
	struct kevent kev;

	if (!(ev->ev_flags & EVLIST_X_KQINKERNEL))
		return (0);

	if (ev->ev_events & EV_SIGNAL) {
		int nsignal = EVENT_SIGNAL(ev);

 		memset(&kev, 0, sizeof(kev));
		kev.ident = (int)signal;
		kev.filter = EVFILT_SIGNAL;
		kev.flags = EV_DELETE;
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);

		if (signal(nsignal, SIG_DFL) == SIG_ERR)
			return (-1);

		ev->ev_flags &= ~EVLIST_X_KQINKERNEL;
		return (0);
	}

	if (ev->ev_events & EV_READ) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_READ;
		kev.flags = EV_DELETE;
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);

		ev->ev_flags &= ~EVLIST_X_KQINKERNEL;
	}

	if (ev->ev_events & EV_WRITE) {
 		memset(&kev, 0, sizeof(kev));
		kev.ident = ev->ev_fd;
		kev.filter = EVFILT_WRITE;
		kev.flags = EV_DELETE;
		
		if (kq_insert(kqop, &kev) == -1)
			return (-1);

		ev->ev_flags &= ~EVLIST_X_KQINKERNEL;
	}

	return (0);
}
