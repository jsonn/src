/*	$NetBSD: intr.h,v 1.1.22.3 2006/11/04 14:28:18 yamt Exp $	*/

/* XXX: cherry: To Be fixed when we switch on interrupts. */

#ifndef _IA64_INTR_H_
#define _IA64_INTR_H_

#define	IPL_NONE	0	/* XXX: Placeholder */
#define	IPL_BIO		0	/* XXX: Placeholder */
#define	IPL_NET		0	/* XXX: Placeholder */
#define	IPL_TTY		0	/* XXX: Placeholder */
#define	IPL_CLOCK	0	/* XXX: Placeholder */
#define	IPL_STATCLOCK	0	/* XXX: Placeholder */
#define	IPL_HIGH	0	/* XXX: Placeholder */
#define	IPL_SERIAL	0	/* XXX: Placeholder */
#define	IPL_SCHED	0	/* XXX: Placeholder */
#define	IPL_LOCK	0	/* XXX: Placeholder */
#define	IPL_VM		0	/* XXX: Placeholder */

#define IPL_SOFTCLOCK   0	/* XXX: Placeholder */
#define IPL_SOFTNET     0	/* XXX: Placeholder */
#define IPL_SOFTSERIAL  0	/* XXX: Placeholder */

static __inline int splraise(int dummy) { return 0; }
static __inline void spllower(int dummy) { }

/*
 * Hardware interrupt masks
 */
#define	splbio()	splraise(IPL_BIO)
#define	splnet()	splraise(IPL_NET)
#define	spltty()	splraise(IPL_TTY)
#define	splaudio()	splraise(IPL_AUDIO)
#define	splclock()	splraise(IPL_CLOCK)
#define	splstatclock()	splclock()
#define	splserial()	splraise(IPL_SERIAL)
#define splipi()	splraise(IPL_IPI)


/*
 * Miscellaneous
 */
#define	splvm()		splraise(IPL_VM)
#define	splhigh()	splraise(IPL_HIGH)
#define	spl0()		spllower(IPL_NONE)
#define	splsched()	splraise(IPL_SCHED)
#define spllock() 	splhigh()
#define	splx(x)		spllower(x)

/*
 * Software interrupt masks
 *
 * NOTE: spllowersoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */

#define spllowersoftclock() spllower(IPL_SOFTCLOCK)
#define	splsoftclock() splraise(IPL_SOFTCLOCK)
#define	splsoftnet()	splraise(IPL_SOFTNET)
#define	splsoftserial()	splraise(IPL_SOFTSERIAL)

typedef int ipl_t;
typedef struct {
	ipl_t _ipl;
} ipl_cookie_t;

static ipl_cookie_t
makeiplcookie(ipl_t ipl)
{

	return (ipl_cookie_t){._ipl = ipl};
}

static int
splraiseipl(ipl_cookie_t icookie)
{

	return splraise(icookie._ipl);
}

#endif /* ! _IA64_INTR_H_ */
