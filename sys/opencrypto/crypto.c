/*	$NetBSD: crypto.c,v 1.12.4.1 2006/02/04 14:26:06 simonb Exp $ */
/*	$FreeBSD: src/sys/opencrypto/crypto.c,v 1.4.2.5 2003/02/26 00:14:05 sam Exp $	*/
/*	$OpenBSD: crypto.c,v 1.41 2002/07/17 23:52:38 art Exp $	*/

/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000, 2001 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: crypto.c,v 1.12.4.1 2006/02/04 14:26:06 simonb Exp $");

/* XXX FIXME: should be defopt'ed */
#define CRYPTO_TIMING			/* enable cryptop timing stuff */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <opencrypto/cryptodev.h>
#include <sys/kthread.h>
#include <sys/once.h>

#include <opencrypto/xform.h>			/* XXX for M_XDATA */

#ifdef __NetBSD__
  #define splcrypto splnet
  /* below is kludges to check whats still missing */
  #define SWI_CRYPTO 17
  #define register_swi(lvl, fn)  \
  softintr_establish(IPL_SOFTNET, (void (*)(void*))fn, NULL)
  #define unregister_swi(lvl, fn)  softintr_disestablish(softintr_cookie)
  #define setsoftcrypto(x) softintr_schedule(x)
#endif

#define	SESID2HID(sid)	(((sid) >> 32) & 0xffffffff)

/*
 * Crypto drivers register themselves by allocating a slot in the
 * crypto_drivers table with crypto_get_driverid() and then registering
 * each algorithm they support with crypto_register() and crypto_kregister().
 */
static	struct cryptocap *crypto_drivers;
static	int crypto_drivers_num;
static	void* softintr_cookie;

/*
 * There are two queues for crypto requests; one for symmetric (e.g.
 * cipher) operations and one for asymmetric (e.g. MOD) operations.
 * See below for how synchronization is handled.
 */
static	TAILQ_HEAD(,cryptop) crp_q =		/* request queues */
		TAILQ_HEAD_INITIALIZER(crp_q);
static	TAILQ_HEAD(,cryptkop) crp_kq =
		TAILQ_HEAD_INITIALIZER(crp_kq);

/*
 * There are two queues for processing completed crypto requests; one
 * for the symmetric and one for the asymmetric ops.  We only need one
 * but have two to avoid type futzing (cryptop vs. cryptkop).  See below
 * for how synchronization is handled.
 */
static	TAILQ_HEAD(,cryptop) crp_ret_q =	/* callback queues */
		TAILQ_HEAD_INITIALIZER(crp_ret_q);
static	TAILQ_HEAD(,cryptkop) crp_ret_kq =
		TAILQ_HEAD_INITIALIZER(crp_ret_kq);

/*
 * Crypto op and desciptor data structures are allocated
 * from separate private zones(FreeBSD)/pools(netBSD/OpenBSD) .
 */
struct pool cryptop_pool;
struct pool cryptodesc_pool;
int crypto_pool_initialized = 0;

#ifdef __NetBSD__
static void deferred_crypto_thread(void *arg);
#endif

int	crypto_usercrypto = 1;		/* userland may open /dev/crypto */
int	crypto_userasymcrypto = 1;	/* userland may do asym crypto reqs */
/*
 * cryptodevallowsoft is (intended to be) sysctl'able, controlling
 * access to hardware versus software transforms as below:
 *
 * crypto_devallowsoft < 0:  Force userlevel requests to use software
 *                              transforms, always
 * crypto_devallowsoft = 0:  Use hardware if present, grant userlevel
 *                              requests for non-accelerated transforms
 *                              (handling the latter in software)
 * crypto_devallowsoft > 0:  Allow user requests only for transforms which
 *                               are hardware-accelerated.
 */
int	crypto_devallowsoft = 1;	/* only use hardware crypto */

#ifdef __FreeBSD__
SYSCTL_INT(_kern, OID_AUTO, usercrypto, CTLFLAG_RW,
	   &crypto_usercrypto, 0,
	   "Enable/disable user-mode access to crypto support");
SYSCTL_INT(_kern, OID_AUTO, userasymcrypto, CTLFLAG_RW,
	   &crypto_userasymcrypto, 0,
	   "Enable/disable user-mode access to asymmetric crypto support");
SYSCTL_INT(_kern, OID_AUTO, cryptodevallowsoft, CTLFLAG_RW,
	   &crypto_devallowsoft, 0,
	   "Enable/disable use of software asym crypto support");
#endif

MALLOC_DEFINE(M_CRYPTO_DATA, "crypto", "crypto session records");

/*
 * Synchronization: read carefully, this is non-trivial.
 *
 * Crypto requests are submitted via crypto_dispatch.  Typically
 * these come in from network protocols at spl0 (output path) or
 * spl[,soft]net (input path).
 *
 * Requests are typically passed on the driver directly, but they
 * may also be queued for processing by a software interrupt thread,
 * cryptointr, that runs at splsoftcrypto.  This thread dispatches
 * the requests to crypto drivers (h/w or s/w) who call crypto_done
 * when a request is complete.  Hardware crypto drivers are assumed
 * to register their IRQ's as network devices so their interrupt handlers
 * and subsequent "done callbacks" happen at spl[imp,net].
 *
 * Completed crypto ops are queued for a separate kernel thread that
 * handles the callbacks at spl0.  This decoupling insures the crypto
 * driver interrupt service routine is not delayed while the callback
 * takes place and that callbacks are delivered after a context switch
 * (as opposed to a software interrupt that clients must block).
 *
 * This scheme is not intended for SMP machines.
 */
static	void cryptointr(void);		/* swi thread to dispatch ops */
static	void cryptoret(void);		/* kernel thread for callbacks*/
static	struct proc *cryptoproc;
static	void crypto_destroy(void);
static	int crypto_invoke(struct cryptop *crp, int hint);
static	int crypto_kinvoke(struct cryptkop *krp, int hint);

static struct cryptostats cryptostats;
static	int crypto_timing = 0;

#ifdef __FreeBSD__
SYSCTL_STRUCT(_kern, OID_AUTO, crypto_stats, CTLFLAG_RW, &cryptostats,
	    cryptostats, "Crypto system statistics");

SYSCTL_INT(_debug, OID_AUTO, crypto_timing, CTLFLAG_RW,
	   &crypto_timing, 0, "Enable/disable crypto timing support");
SYSCTL_STRUCT(_kern, OID_AUTO, crypto_stats, CTLFLAG_RW, &cryptostats,
	    cryptostats, "Crypto system statistics");
#endif /* __FreeBSD__ */

static int
crypto_init0(void)
{
#ifdef __FreeBSD__
	int error;

	cryptop_zone = zinit("cryptop", sizeof (struct cryptop), 0, 0, 1);
	cryptodesc_zone = zinit("cryptodesc", sizeof (struct cryptodesc),
				0, 0, 1);
	if (cryptodesc_zone == NULL || cryptop_zone == NULL) {
		printf("crypto_init: cannot setup crypto zones\n");
		return;
	}
#endif

	crypto_drivers = malloc(CRYPTO_DRIVERS_INITIAL *
	    sizeof(struct cryptocap), M_CRYPTO_DATA, M_NOWAIT | M_ZERO);
	if (crypto_drivers == NULL) {
		printf("crypto_init: cannot malloc driver table\n");
		return 0;
	}
	crypto_drivers_num = CRYPTO_DRIVERS_INITIAL;

	softintr_cookie = register_swi(SWI_CRYPTO, cryptointr);
#ifdef __FreeBSD__
	error = kthread_create((void (*)(void *)) cryptoret, NULL,
		    &cryptoproc, "cryptoret");
	if (error) {
		printf("crypto_init: cannot start cryptoret thread; error %d",
			error);
		crypto_destroy();
	}
#else
	/* defer thread creation until after boot */
	kthread_create( deferred_crypto_thread, NULL);
#endif
	return 0;
}

void
crypto_init(void)
{
	ONCE_DECL(crypto_init_once);

	RUN_ONCE(&crypto_init_once, crypto_init0);
}

static void
crypto_destroy(void)
{
	/* XXX no wait to reclaim zones */
	if (crypto_drivers != NULL)
		free(crypto_drivers, M_CRYPTO_DATA);
	unregister_swi(SWI_CRYPTO, cryptointr);
}

/*
 * Create a new session.
 */
int
crypto_newsession(u_int64_t *sid, struct cryptoini *cri, int hard)
{
	struct cryptoini *cr;
	u_int32_t hid, lid;
	int err = EINVAL;
	int s;

	s = splcrypto();

	if (crypto_drivers == NULL)
		goto done;

	/*
	 * The algorithm we use here is pretty stupid; just use the
	 * first driver that supports all the algorithms we need.
	 *
	 * XXX We need more smarts here (in real life too, but that's
	 * XXX another story altogether).
	 */

	for (hid = 0; hid < crypto_drivers_num; hid++) {
		/*
		 * If it's not initialized or has remaining sessions
		 * referencing it, skip.
		 */
		if (crypto_drivers[hid].cc_newsession == NULL ||
		    (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP))
			continue;

		/* Hardware required -- ignore software drivers. */
		if (hard > 0 &&
		    (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE))
			continue;
		/* Software required -- ignore hardware drivers. */
		if (hard < 0 &&
		    (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE) == 0)
			continue;

		/* See if all the algorithms are supported. */
		for (cr = cri; cr; cr = cr->cri_next)
			if (crypto_drivers[hid].cc_alg[cr->cri_alg] == 0)
				break;

		if (cr == NULL) {
			/* Ok, all algorithms are supported. */

			/*
			 * Can't do everything in one session.
			 *
			 * XXX Fix this. We need to inject a "virtual" session layer right
			 * XXX about here.
			 */

			/* Call the driver initialization routine. */
			lid = hid;		/* Pass the driver ID. */
			err = crypto_drivers[hid].cc_newsession(
					crypto_drivers[hid].cc_arg, &lid, cri);
			if (err == 0) {
				(*sid) = hid;
				(*sid) <<= 32;
				(*sid) |= (lid & 0xffffffff);
				crypto_drivers[hid].cc_sessions++;
			}
			goto done;
			/*break;*/
		}
	}
done:
	splx(s);
	return err;
}

/*
 * Delete an existing session (or a reserved session on an unregistered
 * driver).
 */
int
crypto_freesession(u_int64_t sid)
{
	u_int32_t hid;
	int err = 0;
	int s;

	s = splcrypto();

	if (crypto_drivers == NULL) {
		err = EINVAL;
		goto done;
	}

	/* Determine two IDs. */
	hid = SESID2HID(sid);

	if (hid >= crypto_drivers_num) {
		err = ENOENT;
		goto done;
	}

	if (crypto_drivers[hid].cc_sessions)
		crypto_drivers[hid].cc_sessions--;

	/* Call the driver cleanup routine, if available. */
	if (crypto_drivers[hid].cc_freesession)
		err = crypto_drivers[hid].cc_freesession(
				crypto_drivers[hid].cc_arg, sid);
	else
		err = 0;

	/*
	 * If this was the last session of a driver marked as invalid,
	 * make the entry available for reuse.
	 */
	if ((crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP) &&
	    crypto_drivers[hid].cc_sessions == 0)
		bzero(&crypto_drivers[hid], sizeof(struct cryptocap));

done:
	splx(s);
	return err;
}

/*
 * Return an unused driver id.  Used by drivers prior to registering
 * support for the algorithms they handle.
 */
int32_t
crypto_get_driverid(u_int32_t flags)
{
	struct cryptocap *newdrv;
	int i, s;

	crypto_init();

	s = splcrypto();
	for (i = 0; i < crypto_drivers_num; i++)
		if (crypto_drivers[i].cc_process == NULL &&
		    (crypto_drivers[i].cc_flags & CRYPTOCAP_F_CLEANUP) == 0 &&
		    crypto_drivers[i].cc_sessions == 0)
			break;

	/* Out of entries, allocate some more. */
	if (i == crypto_drivers_num) {
		/* Be careful about wrap-around. */
		if (2 * crypto_drivers_num <= crypto_drivers_num) {
			splx(s);
			printf("crypto: driver count wraparound!\n");
			return -1;
		}

		newdrv = malloc(2 * crypto_drivers_num *
		    sizeof(struct cryptocap), M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
		if (newdrv == NULL) {
			splx(s);
			printf("crypto: no space to expand driver table!\n");
			return -1;
		}

		bcopy(crypto_drivers, newdrv,
		    crypto_drivers_num * sizeof(struct cryptocap));

		crypto_drivers_num *= 2;

		free(crypto_drivers, M_CRYPTO_DATA);
		crypto_drivers = newdrv;
	}

	/* NB: state is zero'd on free */
	crypto_drivers[i].cc_sessions = 1;	/* Mark */
	crypto_drivers[i].cc_flags = flags;

	if (bootverbose)
		printf("crypto: assign driver %u, flags %u\n", i, flags);

	splx(s);

	return i;
}

static struct cryptocap *
crypto_checkdriver(u_int32_t hid)
{
	if (crypto_drivers == NULL)
		return NULL;
	return (hid >= crypto_drivers_num ? NULL : &crypto_drivers[hid]);
}

/*
 * Register support for a key-related algorithm.  This routine
 * is called once for each algorithm supported a driver.
 */
int
crypto_kregister(u_int32_t driverid, int kalg, u_int32_t flags,
    int (*kprocess)(void*, struct cryptkop *, int),
    void *karg)
{
	int s;
	struct cryptocap *cap;
	int err;

	s = splcrypto();

	cap = crypto_checkdriver(driverid);
	if (cap != NULL &&
	    (CRK_ALGORITM_MIN <= kalg && kalg <= CRK_ALGORITHM_MAX)) {
		/*
		 * XXX Do some performance testing to determine placing.
		 * XXX We probably need an auxiliary data structure that
		 * XXX describes relative performances.
		 */

		cap->cc_kalg[kalg] = flags | CRYPTO_ALG_FLAG_SUPPORTED;
		if (bootverbose)
			printf("crypto: driver %u registers key alg %u flags %u\n"
				, driverid
				, kalg
				, flags
			);

		if (cap->cc_kprocess == NULL) {
			cap->cc_karg = karg;
			cap->cc_kprocess = kprocess;
		}
		err = 0;
	} else
		err = EINVAL;

	splx(s);
	return err;
}

/*
 * Register support for a non-key-related algorithm.  This routine
 * is called once for each such algorithm supported by a driver.
 */
int
crypto_register(u_int32_t driverid, int alg, u_int16_t maxoplen,
    u_int32_t flags,
    int (*newses)(void*, u_int32_t*, struct cryptoini*),
    int (*freeses)(void*, u_int64_t),
    int (*process)(void*, struct cryptop *, int),
    void *arg)
{
	struct cryptocap *cap;
	int s, err;

	s = splcrypto();

	cap = crypto_checkdriver(driverid);
	/* NB: algorithms are in the range [1..max] */
	if (cap != NULL &&
	    (CRYPTO_ALGORITHM_MIN <= alg && alg <= CRYPTO_ALGORITHM_MAX)) {
		/*
		 * XXX Do some performance testing to determine placing.
		 * XXX We probably need an auxiliary data structure that
		 * XXX describes relative performances.
		 */

		cap->cc_alg[alg] = flags | CRYPTO_ALG_FLAG_SUPPORTED;
		cap->cc_max_op_len[alg] = maxoplen;
		if (bootverbose)
			printf("crypto: driver %u registers alg %u flags %u maxoplen %u\n"
				, driverid
				, alg
				, flags
				, maxoplen
			);

		if (cap->cc_process == NULL) {
			cap->cc_arg = arg;
			cap->cc_newsession = newses;
			cap->cc_process = process;
			cap->cc_freesession = freeses;
			cap->cc_sessions = 0;		/* Unmark */
		}
		err = 0;
	} else
		err = EINVAL;

	splx(s);
	return err;
}

/*
 * Unregister a crypto driver. If there are pending sessions using it,
 * leave enough information around so that subsequent calls using those
 * sessions will correctly detect the driver has been unregistered and
 * reroute requests.
 */
int
crypto_unregister(u_int32_t driverid, int alg)
{
	int i, err, s;
	u_int32_t ses;
	struct cryptocap *cap;

	s = splcrypto();

	cap = crypto_checkdriver(driverid);
	if (cap != NULL &&
	    (CRYPTO_ALGORITHM_MIN <= alg && alg <= CRYPTO_ALGORITHM_MAX) &&
	    cap->cc_alg[alg] != 0) {
		cap->cc_alg[alg] = 0;
		cap->cc_max_op_len[alg] = 0;

		/* Was this the last algorithm ? */
		for (i = 1; i <= CRYPTO_ALGORITHM_MAX; i++)
			if (cap->cc_alg[i] != 0)
				break;

		if (i == CRYPTO_ALGORITHM_MAX + 1) {
			ses = cap->cc_sessions;
			bzero(cap, sizeof(struct cryptocap));
			if (ses != 0) {
				/*
				 * If there are pending sessions, just mark as invalid.
				 */
				cap->cc_flags |= CRYPTOCAP_F_CLEANUP;
				cap->cc_sessions = ses;
			}
		}
		err = 0;
	} else
		err = EINVAL;

	splx(s);
	return err;
}

/*
 * Unregister all algorithms associated with a crypto driver.
 * If there are pending sessions using it, leave enough information
 * around so that subsequent calls using those sessions will
 * correctly detect the driver has been unregistered and reroute
 * requests.
 */
int
crypto_unregister_all(u_int32_t driverid)
{
	int i, err, s = splcrypto();
	u_int32_t ses;
	struct cryptocap *cap;

	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		for (i = CRYPTO_ALGORITHM_MIN; i <= CRYPTO_ALGORITHM_MAX; i++) {
			cap->cc_alg[i] = 0;
			cap->cc_max_op_len[i] = 0;
		}
		ses = cap->cc_sessions;
		bzero(cap, sizeof(struct cryptocap));
		if (ses != 0) {
			/*
			 * If there are pending sessions, just mark as invalid.
			 */
			cap->cc_flags |= CRYPTOCAP_F_CLEANUP;
			cap->cc_sessions = ses;
		}
		err = 0;
	} else
		err = EINVAL;

	splx(s);
	return err;
}

/*
 * Clear blockage on a driver.  The what parameter indicates whether
 * the driver is now ready for cryptop's and/or cryptokop's.
 */
int
crypto_unblock(u_int32_t driverid, int what)
{
	struct cryptocap *cap;
	int needwakeup, err, s;

	s = splcrypto();
	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		needwakeup = 0;
		if (what & CRYPTO_SYMQ) {
			needwakeup |= cap->cc_qblocked;
			cap->cc_qblocked = 0;
		}
		if (what & CRYPTO_ASYMQ) {
			needwakeup |= cap->cc_kqblocked;
			cap->cc_kqblocked = 0;
		}
		if (needwakeup) {
			setsoftcrypto(softintr_cookie);
		}
		err = 0;
	} else
		err = EINVAL;
	splx(s);

	return err;
}

/*
 * Dispatch a crypto request to a driver or queue
 * it, to be processed by the kernel thread.
 */
int
crypto_dispatch(struct cryptop *crp)
{
	u_int32_t hid = SESID2HID(crp->crp_sid);
	int s, result;

	s = splcrypto();

	cryptostats.cs_ops++;

#ifdef CRYPTO_TIMING
	if (crypto_timing)
		nanouptime(&crp->crp_tstamp);
#endif
	if ((crp->crp_flags & CRYPTO_F_BATCH) == 0) {
		struct cryptocap *cap;
		/*
		 * Caller marked the request to be processed
		 * immediately; dispatch it directly to the
		 * driver unless the driver is currently blocked.
		 */
		cap = crypto_checkdriver(hid);
		if (cap && !cap->cc_qblocked) {
			result = crypto_invoke(crp, 0);
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptop's and put
				 * the op on the queue.
				 */
				crypto_drivers[hid].cc_qblocked = 1;
				TAILQ_INSERT_HEAD(&crp_q, crp, crp_next);
				cryptostats.cs_blocks++;
			}
		} else {
			/*
			 * The driver is blocked, just queue the op until
			 * it unblocks and the swi thread gets kicked.
			 */
			TAILQ_INSERT_TAIL(&crp_q, crp, crp_next);
			result = 0;
		}
	} else {
		int wasempty = TAILQ_EMPTY(&crp_q);
		/*
		 * Caller marked the request as ``ok to delay'';
		 * queue it for the swi thread.  This is desirable
		 * when the operation is low priority and/or suitable
		 * for batching.
		 */
		TAILQ_INSERT_TAIL(&crp_q, crp, crp_next);
		if (wasempty) {
			setsoftcrypto(softintr_cookie);
		}

		result = 0;
	}
	splx(s);

	return result;
}

/*
 * Add an asymetric crypto request to a queue,
 * to be processed by the kernel thread.
 */
int
crypto_kdispatch(struct cryptkop *krp)
{
	struct cryptocap *cap;
	int s, result;

	s = splcrypto();
	cryptostats.cs_kops++;

	cap = crypto_checkdriver(krp->krp_hid);
	if (cap && !cap->cc_kqblocked) {
		result = crypto_kinvoke(krp, 0);
		if (result == ERESTART) {
			/*
			 * The driver ran out of resources, mark the
			 * driver ``blocked'' for cryptop's and put
			 * the op on the queue.
			 */
			crypto_drivers[krp->krp_hid].cc_kqblocked = 1;
			TAILQ_INSERT_HEAD(&crp_kq, krp, krp_next);
			cryptostats.cs_kblocks++;
		}
	} else {
		/*
		 * The driver is blocked, just queue the op until
		 * it unblocks and the swi thread gets kicked.
		 */
		TAILQ_INSERT_TAIL(&crp_kq, krp, krp_next);
		result = 0;
	}
	splx(s);

	return result;
}

/*
 * Dispatch an assymetric crypto request to the appropriate crypto devices.
 */
static int
crypto_kinvoke(struct cryptkop *krp, int hint)
{
	u_int32_t hid;
	int error;

	/* Sanity checks. */
	if (krp == NULL)
		return EINVAL;
	if (krp->krp_callback == NULL) {
		free(krp, M_XDATA);		/* XXX allocated in cryptodev */
		return EINVAL;
	}

	for (hid = 0; hid < crypto_drivers_num; hid++) {
		if ((crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE) &&
		    crypto_devallowsoft == 0)
			continue;
		if (crypto_drivers[hid].cc_kprocess == NULL)
			continue;
		if ((crypto_drivers[hid].cc_kalg[krp->krp_op] &
		    CRYPTO_ALG_FLAG_SUPPORTED) == 0)
			continue;
		break;
	}
	if (hid < crypto_drivers_num) {
		krp->krp_hid = hid;
		error = crypto_drivers[hid].cc_kprocess(
				crypto_drivers[hid].cc_karg, krp, hint);
	} else {
		error = ENODEV;
	}

	if (error) {
		krp->krp_status = error;
		crypto_kdone(krp);
	}
	return 0;
}

#ifdef CRYPTO_TIMING
static void
crypto_tstat(struct cryptotstat *ts, struct timespec *tv)
{
	struct timespec now, t;

	nanouptime(&now);
	t.tv_sec = now.tv_sec - tv->tv_sec;
	t.tv_nsec = now.tv_nsec - tv->tv_nsec;
	if (t.tv_nsec < 0) {
		t.tv_sec--;
		t.tv_nsec += 1000000000;
	}
	timespecadd(&ts->acc, &t, &t);
	if (timespeccmp(&t, &ts->min, <))
		ts->min = t;
	if (timespeccmp(&t, &ts->max, >))
		ts->max = t;
	ts->count++;

	*tv = now;
}
#endif

/*
 * Dispatch a crypto request to the appropriate crypto devices.
 */
static int
crypto_invoke(struct cryptop *crp, int hint)
{
	u_int32_t hid;
	int (*process)(void*, struct cryptop *, int);

#ifdef CRYPTO_TIMING
	if (crypto_timing)
		crypto_tstat(&cryptostats.cs_invoke, &crp->crp_tstamp);
#endif
	/* Sanity checks. */
	if (crp == NULL)
		return EINVAL;
	if (crp->crp_callback == NULL) {
		crypto_freereq(crp);
		return EINVAL;
	}
	if (crp->crp_desc == NULL) {
		crp->crp_etype = EINVAL;
		crypto_done(crp);
		return 0;
	}

	hid = SESID2HID(crp->crp_sid);
	if (hid < crypto_drivers_num) {
		if (crypto_drivers[hid].cc_flags & CRYPTOCAP_F_CLEANUP)
			crypto_freesession(crp->crp_sid);
		process = crypto_drivers[hid].cc_process;
	} else {
		process = NULL;
	}

	if (process == NULL) {
		struct cryptodesc *crd;
		u_int64_t nid;

		/*
		 * Driver has unregistered; migrate the session and return
		 * an error to the caller so they'll resubmit the op.
		 */
		for (crd = crp->crp_desc; crd->crd_next; crd = crd->crd_next)
			crd->CRD_INI.cri_next = &(crd->crd_next->CRD_INI);

		if (crypto_newsession(&nid, &(crp->crp_desc->CRD_INI), 0) == 0)
			crp->crp_sid = nid;

		crp->crp_etype = EAGAIN;
		crypto_done(crp);
		return 0;
	} else {
		/*
		 * Invoke the driver to process the request.
		 */
		return (*process)(crypto_drivers[hid].cc_arg, crp, hint);
	}
}

/*
 * Release a set of crypto descriptors.
 */
void
crypto_freereq(struct cryptop *crp)
{
	struct cryptodesc *crd;
	int s;

	if (crp == NULL)
		return;

	s = splcrypto();

	while ((crd = crp->crp_desc) != NULL) {
		crp->crp_desc = crd->crd_next;
		pool_put(&cryptodesc_pool, crd);
	}

	pool_put(&cryptop_pool, crp);
	splx(s);
}

/*
 * Acquire a set of crypto descriptors.
 */
struct cryptop *
crypto_getreq(int num)
{
	struct cryptodesc *crd;
	struct cryptop *crp;
	int s;

	s = splcrypto();

	if (crypto_pool_initialized == 0) {
		pool_init(&cryptop_pool, sizeof(struct cryptop), 0, 0,
		    0, "cryptop", NULL);
		pool_init(&cryptodesc_pool, sizeof(struct cryptodesc), 0, 0,
		    0, "cryptodesc", NULL);
		crypto_pool_initialized = 1;
	}

	crp = pool_get(&cryptop_pool, 0);
	if (crp == NULL) {
		splx(s);
		return NULL;
	}
	bzero(crp, sizeof(struct cryptop));

	while (num--) {
		crd = pool_get(&cryptodesc_pool, 0);
		if (crd == NULL) {
			splx(s);
			crypto_freereq(crp);
			return NULL;
		}

		bzero(crd, sizeof(struct cryptodesc));
		crd->crd_next = crp->crp_desc;
		crp->crp_desc = crd;
	}

	splx(s);
	return crp;
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_done(struct cryptop *crp)
{
	if (crp->crp_etype != 0)
		cryptostats.cs_errs++;
#ifdef CRYPTO_TIMING
	if (crypto_timing)
		crypto_tstat(&cryptostats.cs_done, &crp->crp_tstamp);
#endif
	/*
	 * On netbsd 1.6O, CBIMM does its wake_one() before the requestor
	 * has done its tsleep().
	 */
#ifndef __NetBSD__
	if (crp->crp_flags & CRYPTO_F_CBIMM) {
		/*
		 * Do the callback directly.  This is ok when the
		 * callback routine does very little (e.g. the
		 * /dev/crypto callback method just does a wakeup).
		 */
#ifdef CRYPTO_TIMING
		if (crypto_timing) {
			/*
			 * NB: We must copy the timestamp before
			 * doing the callback as the cryptop is
			 * likely to be reclaimed.
			 */
			struct timespec t = crp->crp_tstamp;
			crypto_tstat(&cryptostats.cs_cb, &t);
			crp->crp_callback(crp);
			crypto_tstat(&cryptostats.cs_finis, &t);
		} else
#endif
			crp->crp_callback(crp);
	} else
#endif /* __NetBSD__ */
	{
		int s, wasempty;
		/*
		 * Normal case; queue the callback for the thread.
		 *
		 * The return queue is manipulated by the swi thread
		 * and, potentially, by crypto device drivers calling
		 * back to mark operations completed.  Thus we need
		 * to mask both while manipulating the return queue.
		 */
		s = splcrypto();
		wasempty = TAILQ_EMPTY(&crp_ret_q);
		TAILQ_INSERT_TAIL(&crp_ret_q, crp, crp_next);
		if (wasempty)
			wakeup_one(&crp_ret_q);
		splx(s);
	}
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_kdone(struct cryptkop *krp)
{
	int s, wasempty;

	if (krp->krp_status != 0)
		cryptostats.cs_kerrs++;
	/*
	 * The return queue is manipulated by the swi thread
	 * and, potentially, by crypto device drivers calling
	 * back to mark operations completed.  Thus we need
	 * to mask both while manipulating the return queue.
	 */
	s = splcrypto();
	wasempty = TAILQ_EMPTY(&crp_ret_kq);
	TAILQ_INSERT_TAIL(&crp_ret_kq, krp, krp_next);
	if (wasempty)
		wakeup_one(&crp_ret_q);
	splx(s);
}

int
crypto_getfeat(int *featp)
{
	int hid, kalg, feat = 0;
	int s;

	s = splcrypto();

	if (crypto_userasymcrypto == 0)
		goto out;

	for (hid = 0; hid < crypto_drivers_num; hid++) {
		if ((crypto_drivers[hid].cc_flags & CRYPTOCAP_F_SOFTWARE) &&
		    crypto_devallowsoft == 0) {
			continue;
		}
		if (crypto_drivers[hid].cc_kprocess == NULL)
			continue;
		for (kalg = 0; kalg < CRK_ALGORITHM_MAX; kalg++)
			if ((crypto_drivers[hid].cc_kalg[kalg] &
			    CRYPTO_ALG_FLAG_SUPPORTED) != 0)
				feat |=  1 << kalg;
	}
out:
	splx(s);
	*featp = feat;
	return (0);
}

/*
 * Software interrupt thread to dispatch crypto requests.
 */
static void
cryptointr(void)
{
	struct cryptop *crp, *submit;
	struct cryptkop *krp;
	struct cryptocap *cap;
	int result, hint, s;

	printf("crypto softint\n");
	cryptostats.cs_intrs++;
	s = splcrypto();
	do {
		/*
		 * Find the first element in the queue that can be
		 * processed and look-ahead to see if multiple ops
		 * are ready for the same driver.
		 */
		submit = NULL;
		hint = 0;
		TAILQ_FOREACH(crp, &crp_q, crp_next) {
			u_int32_t hid = SESID2HID(crp->crp_sid);
			cap = crypto_checkdriver(hid);
			if (cap == NULL || cap->cc_process == NULL) {
				/* Op needs to be migrated, process it. */
				if (submit == NULL)
					submit = crp;
				break;
			}
			if (!cap->cc_qblocked) {
				if (submit != NULL) {
					/*
					 * We stop on finding another op,
					 * regardless whether its for the same
					 * driver or not.  We could keep
					 * searching the queue but it might be
					 * better to just use a per-driver
					 * queue instead.
					 */
					if (SESID2HID(submit->crp_sid) == hid)
						hint = CRYPTO_HINT_MORE;
					break;
				} else {
					submit = crp;
					if ((submit->crp_flags & CRYPTO_F_BATCH) == 0)
						break;
					/* keep scanning for more are q'd */
				}
			}
		}
		if (submit != NULL) {
			TAILQ_REMOVE(&crp_q, submit, crp_next);
			result = crypto_invoke(submit, hint);
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				/* XXX validate sid again? */
				crypto_drivers[SESID2HID(submit->crp_sid)].cc_qblocked = 1;
				TAILQ_INSERT_HEAD(&crp_q, submit, crp_next);
				cryptostats.cs_blocks++;
			}
		}

		/* As above, but for key ops */
		TAILQ_FOREACH(krp, &crp_kq, krp_next) {
			cap = crypto_checkdriver(krp->krp_hid);
			if (cap == NULL || cap->cc_kprocess == NULL) {
				/* Op needs to be migrated, process it. */
				break;
			}
			if (!cap->cc_kqblocked)
				break;
		}
		if (krp != NULL) {
			TAILQ_REMOVE(&crp_kq, krp, krp_next);
			result = crypto_kinvoke(krp, 0);
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptkop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				/* XXX validate sid again? */
				crypto_drivers[krp->krp_hid].cc_kqblocked = 1;
				TAILQ_INSERT_HEAD(&crp_kq, krp, krp_next);
				cryptostats.cs_kblocks++;
			}
		}
	} while (submit != NULL || krp != NULL);
	splx(s);
}

/*
 * Kernel thread to do callbacks.
 */
static void
cryptoret(void)
{
	struct cryptop *crp;
	struct cryptkop *krp;
	int s;

	s = splcrypto();
	for (;;) {
		crp = TAILQ_FIRST(&crp_ret_q);
		if (crp != NULL)
			TAILQ_REMOVE(&crp_ret_q, crp, crp_next);
		krp = TAILQ_FIRST(&crp_ret_kq);
		if (krp != NULL)
			TAILQ_REMOVE(&crp_ret_kq, krp, krp_next);

		if (crp != NULL || krp != NULL) {
			splx(s);		/* lower ipl for callbacks */
			if (crp != NULL) {
#ifdef CRYPTO_TIMING
				if (crypto_timing) {
					/*
					 * NB: We must copy the timestamp before
					 * doing the callback as the cryptop is
					 * likely to be reclaimed.
					 */
					struct timespec t = crp->crp_tstamp;
					crypto_tstat(&cryptostats.cs_cb, &t);
					crp->crp_callback(crp);
					crypto_tstat(&cryptostats.cs_finis, &t);
				} else
#endif
					crp->crp_callback(crp);
			}
			if (krp != NULL)
				krp->krp_callback(krp);
			s  = splcrypto();
		} else {
			(void) tsleep(&crp_ret_q, PLOCK, "crypto_wait", 0);
			cryptostats.cs_rets++;
		}
	}
}

static void
deferred_crypto_thread(void *arg)
{
	int error;

	error = kthread_create1((void (*)(void*)) cryptoret, NULL,
				&cryptoproc, "cryptoret");
	if (error) {
		printf("crypto_init: cannot start cryptoret thread; error %d",
		    error);
		crypto_destroy();
	}
}

#ifdef __FreeBSD__
/*
 * Initialization code, both for static and dynamic loading.
 */
static int
crypto_modevent(module_t mod, int type, void *unused)
{
	int error = EINVAL;

	switch (type) {
	case MOD_LOAD:
		error = crypto_init();
		if (error == 0 && bootverbose)
			printf("crypto: <crypto core>\n");
		break;
	case MOD_UNLOAD:
		/*XXX disallow if active sessions */
		error = 0;
		crypto_destroy();
		break;
	}
	return error;
}
static moduledata_t crypto_mod = {
	"crypto",
	crypto_modevent,
	0
};

MODULE_VERSION(crypto, 1);
DECLARE_MODULE(crypto, crypto_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
#endif /* __FreeBSD__ */


