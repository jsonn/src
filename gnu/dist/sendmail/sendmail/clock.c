/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)Id: clock.c,v 8.52.18.3 2000/09/17 17:04:26 gshapiro Exp";
#endif /* ! lint */

#include <sendmail.h>

#ifndef sigmask
# define sigmask(s)	(1 << ((s) - 1))
#endif /* ! sigmask */

static void	endsleep __P((void));


/*
**  SETEVENT -- set an event to happen at a specific time.
**
**	Events are stored in a sorted list for fast processing.
**	An event only applies to the process that set it.
**
**	Parameters:
**		intvl -- intvl until next event occurs.
**		func -- function to call on event.
**		arg -- argument to func on event.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

EVENT	*FreeEventList;		/* list of free events */

EVENT *
setevent(intvl, func, arg)
	time_t intvl;
	void (*func)();
	int arg;
{
	register EVENT **evp;
	register EVENT *ev;
	auto time_t now;
	int wasblocked;

	if (intvl <= 0)
	{
		syserr("554 5.3.0 setevent: intvl=%ld\n", intvl);
		return NULL;
	}

	wasblocked = blocksignal(SIGALRM);
	now = curtime();

	/* search event queue for correct position */
	for (evp = &EventQueue; (ev = *evp) != NULL; evp = &ev->ev_link)
	{
		if (ev->ev_time >= now + intvl)
			break;
	}

	/* insert new event */
	ev = FreeEventList;
	if (ev == NULL)
		ev = (EVENT *) xalloc(sizeof *ev);
	else
		FreeEventList = ev->ev_link;
	ev->ev_time = now + intvl;
	ev->ev_func = func;
	ev->ev_arg = arg;
	ev->ev_pid = getpid();
	ev->ev_link = *evp;
	*evp = ev;

	if (tTd(5, 5))
		dprintf("setevent: intvl=%ld, for=%ld, func=%lx, arg=%d, ev=%lx\n",
			(long) intvl, (long)(now + intvl), (u_long) func,
			arg, (u_long) ev);

	(void) setsignal(SIGALRM, tick);
	intvl = EventQueue->ev_time - now;
	(void) alarm((unsigned) intvl < 1 ? 1 : intvl);
	if (wasblocked == 0)
		(void) releasesignal(SIGALRM);
	return ev;
}
/*
**  CLREVENT -- remove an event from the event queue.
**
**	Parameters:
**		ev -- pointer to event to remove.
**
**	Returns:
**		none.
**
**	Side Effects:
**		arranges for event ev to not happen.
*/

void
clrevent(ev)
	register EVENT *ev;
{
	register EVENT **evp;
	int wasblocked;

	if (tTd(5, 5))
		dprintf("clrevent: ev=%lx\n", (u_long) ev);
	if (ev == NULL)
		return;

	/* find the parent event */
	wasblocked = blocksignal(SIGALRM);
	for (evp = &EventQueue; *evp != NULL; evp = &(*evp)->ev_link)
	{
		if (*evp == ev)
			break;
	}

	/* now remove it */
	if (*evp != NULL)
	{
		*evp = ev->ev_link;
		ev->ev_link = FreeEventList;
		FreeEventList = ev;
	}

	/* restore clocks and pick up anything spare */
	if (wasblocked == 0)
		(void) releasesignal(SIGALRM);
	if (EventQueue != NULL)
		(void) kill(getpid(), SIGALRM);
	else
	{
		/* nothing left in event queue, no need for an alarm */
		(void) alarm(0);
	}
}
/*
**  CLEAR_EVENTS -- remove all events from the event queue.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
clear_events()
{
	register EVENT *ev;
	int wasblocked;

	if (tTd(5, 5))
		dprintf("clear_events: EventQueue=%lx\n", (u_long) EventQueue);

	if (EventQueue == NULL)
		return;

	/* nothing will be left in event queue, no need for an alarm */
	(void) alarm(0);
	wasblocked = blocksignal(SIGALRM);

	/* find the end of the EventQueue */
	for (ev = EventQueue; ev->ev_link != NULL; ev = ev->ev_link)
		continue;

	ev->ev_link = FreeEventList;
	FreeEventList = EventQueue;
	EventQueue = NULL;

	/* restore clocks and pick up anything spare */
	if (wasblocked == 0)
		(void) releasesignal(SIGALRM);
}
/*
**  TICK -- take a clock tick
**
**	Called by the alarm clock.  This routine runs events as needed.
**	Always called as a signal handler, so we assume that SIGALRM
**	has been blocked.
**
**	Parameters:
**		One that is ignored; for compatibility with signal handlers.
**
**	Returns:
**		none.
**
**	Side Effects:
**		calls the next function in EventQueue.
*/

/* ARGSUSED */
SIGFUNC_DECL
tick(sig)
	int sig;
{
	register time_t now;
	register EVENT *ev;
	int mypid = getpid();
	int save_errno = errno;

	(void) alarm(0);
	now = curtime();

	if (tTd(5, 4))
		dprintf("tick: now=%ld\n", (long) now);

	/* reset signal in case System V semantics */
	(void) setsignal(SIGALRM, tick);
	while ((ev = EventQueue) != NULL &&
	       (ev->ev_time <= now || ev->ev_pid != mypid))
	{
		void (*f)();
		int arg;
		int pid;

		/* process the event on the top of the queue */
		ev = EventQueue;
		EventQueue = EventQueue->ev_link;
		if (tTd(5, 6))
			dprintf("tick: ev=%lx, func=%lx, arg=%d, pid=%d\n",
				(u_long) ev, (u_long) ev->ev_func,
				ev->ev_arg, ev->ev_pid);

		/* we must be careful in here because ev_func may not return */
		f = ev->ev_func;
		arg = ev->ev_arg;
		pid = ev->ev_pid;
		ev->ev_link = FreeEventList;
		FreeEventList = ev;
		if (pid != getpid())
			continue;
		if (EventQueue != NULL)
		{
			if (EventQueue->ev_time > now)
				(void) alarm((unsigned) (EventQueue->ev_time - now));
			else
				(void) alarm(3);
		}

		/* call ev_func */
		errno = save_errno;
		(*f)(arg);
		(void) alarm(0);
		now = curtime();
	}
	if (EventQueue != NULL)
		(void) alarm((unsigned) (EventQueue->ev_time - now));
	errno = save_errno;
	return SIGFUNC_RETURN;
}
/*
**  SLEEP -- a version of sleep that works with this stuff
**
**	Because sleep uses the alarm facility, I must reimplement
**	it here.
**
**	Parameters:
**		intvl -- time to sleep.
**
**	Returns:
**		none.
**
**	Side Effects:
**		waits for intvl time.  However, other events can
**		be run during that interval.
*/


static bool	SleepDone;

#ifndef SLEEP_T
# define SLEEP_T	unsigned int
#endif /* ! SLEEP_T */

SLEEP_T
sleep(intvl)
	unsigned int intvl;
{
	int was_held;

	if (intvl == 0)
		return (SLEEP_T) 0;
	SleepDone = FALSE;
	(void) setevent((time_t) intvl, endsleep, 0);
	was_held = releasesignal(SIGALRM);
	while (!SleepDone)
		(void) pause();
	if (was_held > 0)
		(void) blocksignal(SIGALRM);
	return (SLEEP_T) 0;
}

static void
endsleep()
{
	SleepDone = TRUE;
}
