/*	$NetBSD: apmdev.c,v 1.17.2.1 2008/05/18 12:33:38 yamt Exp $ */

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John Kohl and Christopher G. Demetriou.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * from: sys/arch/i386/i386/apm.c,v 1.49 2000/05/08
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: apmdev.c,v 1.17.2.1 2008/05/18 12:33:38 yamt Exp $");

#ifdef _KERNEL_OPT
#include "opt_apmdev.h"
#endif

#ifdef APM_NOIDLE
#error APM_NOIDLE option deprecated; use APM_NO_IDLE instead
#endif

#if defined(DEBUG) && !defined(APMDEBUG)
#define	APMDEBUG
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/conf.h>

#include <dev/hpc/apm/apmvar.h>

#include <machine/stdarg.h>

#if defined(APMDEBUG)
#define	DPRINTF(f, x)		do { if (apmdebug & (f)) printf x; } while (0)

#define	APMDEBUG_INFO		0x01
#define	APMDEBUG_APMCALLS	0x02
#define	APMDEBUG_EVENTS		0x04
#define	APMDEBUG_PROBE		0x10
#define	APMDEBUG_ATTACH		0x40
#define	APMDEBUG_DEVICE		0x20
#define	APMDEBUG_ANOM		0x40

#ifdef APMDEBUG_VALUE
int	apmdebug = APMDEBUG_VALUE;
#else
int	apmdebug = 0;
#endif
#else
#define	DPRINTF(f, x)		/**/
#endif

#define APM_NEVENTS 16

struct apm_softc {
	struct device sc_dev;
	struct selinfo sc_rsel;
	struct selinfo sc_xsel;
	int	sc_flags;
	int	event_count;
	int	event_ptr;
	int	sc_power_state;
	lwp_t	*sc_thread;
	kmutex_t sc_mutex;
	struct apm_event_info event_list[APM_NEVENTS];
	struct apm_accessops *ops;
	void *cookie;
};
#define	SCFLAG_OREAD	0x0000001
#define	SCFLAG_OWRITE	0x0000002
#define	SCFLAG_OPEN	(SCFLAG_OREAD|SCFLAG_OWRITE)

#define	APMUNIT(dev)	(minor(dev)&0xf0)
#define	APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

/*
 * A brief note on the locking protocol: it's very simple; we
 * assert an exclusive lock any time thread context enters the
 * APM module.  This is both the APM thread itself, as well as
 * user context.
 */
#define	APM_LOCK(apmsc)		mutex_enter(&(apmsc)->sc_mutex)
#define	APM_UNLOCK(apmsc)	mutex_exit(&(apmsc)->sc_mutex)

static void	apmattach(struct device *, struct device *, void *);
static int	apmmatch(struct device *, struct cfdata *, void *);

static void	apm_event_handle(struct apm_softc *, u_int, u_int);
static void	apm_periodic_check(struct apm_softc *);
static void	apm_thread(void *);
static void	apm_perror(const char *, int, ...)
		    __attribute__((__format__(__printf__,1,3)));
#ifdef APM_POWER_PRINT
static void	apm_power_print(struct apm_softc *, struct apm_power_info *);
#endif
static int	apm_record_event(struct apm_softc *, u_int);
static void	apm_set_ver(struct apm_softc *, u_long);
static void	apm_standby(struct apm_softc *);
static const char *apm_strerror(int);
static void	apm_suspend(struct apm_softc *);
static void	apm_resume(struct apm_softc *, u_int, u_int);

CFATTACH_DECL(apmdev, sizeof(struct apm_softc),
    apmmatch, apmattach, NULL, NULL);

extern struct cfdriver apmdev_cd;

dev_type_open(apmdevopen);
dev_type_close(apmdevclose);
dev_type_ioctl(apmdevioctl);
dev_type_poll(apmdevpoll);
dev_type_kqfilter(apmdevkqfilter);

const struct cdevsw apmdev_cdevsw = {
	apmdevopen, apmdevclose, noread, nowrite, apmdevioctl,
	nostop, notty, apmdevpoll, nommap, apmdevkqfilter, D_OTHER
};

/* configurable variables */
int	apm_bogus_bios = 0;
#ifdef APM_DISABLE
int	apm_enabled = 0;
#else
int	apm_enabled = 1;
#endif
#ifdef APM_NO_IDLE
int	apm_do_idle = 0;
#else
int	apm_do_idle = 1;
#endif
#ifdef APM_NO_STANDBY
int	apm_do_standby = 0;
#else
int	apm_do_standby = 1;
#endif
#ifdef APM_V10_ONLY
int	apm_v11_enabled = 0;
#else
int	apm_v11_enabled = 1;
#endif
#ifdef APM_NO_V12
int	apm_v12_enabled = 0;
#else
int	apm_v12_enabled = 1;
#endif

/* variables used during operation (XXX cgd) */
u_char	apm_majver, apm_minver;
int	apm_inited;
int	apm_standbys, apm_userstandbys, apm_suspends, apm_battlow;
int	apm_damn_fool_bios, apm_op_inprog;
int	apm_evindex;

static int apm_spl;		/* saved spl while suspended */

static const char *
apm_strerror(int code)
{
	switch (code) {
	case APM_ERR_PM_DISABLED:
		return ("power management disabled");
	case APM_ERR_REALALREADY:
		return ("real mode interface already connected");
	case APM_ERR_NOTCONN:
		return ("interface not connected");
	case APM_ERR_16ALREADY:
		return ("16-bit interface already connected");
	case APM_ERR_16NOTSUPP:
		return ("16-bit interface not supported");
	case APM_ERR_32ALREADY:
		return ("32-bit interface already connected");
	case APM_ERR_32NOTSUPP:
		return ("32-bit interface not supported");
	case APM_ERR_UNRECOG_DEV:
		return ("unrecognized device ID");
	case APM_ERR_ERANGE:
		return ("parameter out of range");
	case APM_ERR_NOTENGAGED:
		return ("interface not engaged");
	case APM_ERR_UNABLE:
		return ("unable to enter requested state");
	case APM_ERR_NOEVENTS:
		return ("no pending events");
	case APM_ERR_NOT_PRESENT:
		return ("no APM present");
	default:
		return ("unknown error code");
	}
}

static void
apm_perror(const char *str, int errinfo, ...) /* XXX cgd */
{
	va_list ap;

	printf("APM ");

	va_start(ap, errinfo);
	vprintf(str, ap);			/* XXX cgd */
	va_end(ap);

	printf(": %s\n", apm_strerror(errinfo));
}

#ifdef APM_POWER_PRINT
static void
apm_power_print(struct apm_softc *sc, struct apm_power_info *pi)
{

	if (pi->battery_life != APM_BATT_LIFE_UNKNOWN) {
		printf("%s: battery life expectancy: %d%%\n",
		    device_xname(&sc->sc_dev), pi->battery_life);
	}
	printf("%s: A/C state: ", device_xname(&sc->sc_dev));
	switch (pi->ac_state) {
	case APM_AC_OFF:
		printf("off\n");
		break;
	case APM_AC_ON:
		printf("on\n");
		break;
	case APM_AC_BACKUP:
		printf("backup power\n");
		break;
	default:
	case APM_AC_UNKNOWN:
		printf("unknown\n");
		break;
	}
	if (apm_major == 1 && apm_minor == 0) {
		printf("%s: battery charge state:", device_xname(&sc->sc_dev));
		switch (pi->battery_state) {
		case APM_BATT_HIGH:
			printf("high\n");
			break;
		case APM_BATT_LOW:
			printf("low\n");
			break;
		case APM_BATT_CRITICAL:
			printf("critical\n");
			break;
		case APM_BATT_CHARGING:
			printf("charging\n");
			break;
		case APM_BATT_UNKNOWN:
			printf("unknown\n");
			break;
		default:
			printf("undecoded state %x\n", pi->battery_state);
			break;
		}
	} else {
		if (pi->battery_state&APM_BATT_FLAG_CHARGING)
			printf("charging ");
		}
		if (pi->battery_state&APM_BATT_FLAG_UNKNOWN)
			printf("unknown\n");
		else if (pi->battery_state&APM_BATT_FLAG_CRITICAL)
			printf("critical\n");
		else if (pi->battery_state&APM_BATT_FLAG_LOW)
			printf("low\n");
		else if (pi->battery_state&APM_BATT_FLAG_HIGH)
			printf("high\n");
	}
	if (pi->minutes_left != 0) {
		printf("%s: estimated ", device_xname(&sc->sc_dev));
		printf("%dh ", pi->minutes_left / 60);
	}
	return;
}
#endif

static void
apm_suspend(struct apm_softc *sc)
{

	if (sc->sc_power_state == PWR_SUSPEND) {
#ifdef APMDEBUG
		printf("%s: apm_suspend: already suspended?\n",
		    device_xname(&sc->sc_dev));
#endif
		return;
	}
	sc->sc_power_state = PWR_SUSPEND;

	dopowerhooks(PWR_SOFTSUSPEND);
	(void) tsleep(sc, PWAIT, "apmsuspend",  hz/2);

	apm_spl = splhigh();

	dopowerhooks(PWR_SUSPEND);

	/* XXX cgd */
	(void)sc->ops->set_powstate(sc->cookie, APM_DEV_ALLDEVS, APM_SYS_SUSPEND);
}

static void
apm_standby(struct apm_softc *sc)
{

	if (sc->sc_power_state == PWR_STANDBY) {
#ifdef APMDEBUG
		printf("%s: apm_standby: already standing by?\n",
		    device_xname(&sc->sc_dev));
#endif
		return;
	}
	sc->sc_power_state = PWR_STANDBY;

	dopowerhooks(PWR_SOFTSTANDBY);
	(void) tsleep(sc, PWAIT, "apmstandby",  hz/2);

	apm_spl = splhigh();

	dopowerhooks(PWR_STANDBY);
	/* XXX cgd */
	(void)sc->ops->set_powstate(sc->cookie, APM_DEV_ALLDEVS, APM_SYS_STANDBY);
}

static void
apm_resume(struct apm_softc *sc, u_int event_type, u_int event_info)
{

	if (sc->sc_power_state == PWR_RESUME) {
#ifdef APMDEBUG
		printf("%s: apm_resume: already running?\n",
		    device_xname(&sc->sc_dev));
#endif
		return;
	}
	sc->sc_power_state = PWR_RESUME;

	/*
	 * Some system requires its clock to be initialized after hybernation.
	 */
/* XXX
	initrtclock();
*/

	inittodr(time_second);
	dopowerhooks(PWR_RESUME);

	splx(apm_spl);

	dopowerhooks(PWR_SOFTRESUME);

	apm_record_event(sc, event_type);
}

/*
 * return 0 if the user will notice and handle the event,
 * return 1 if the kernel driver should do so.
 */
static int
apm_record_event(struct apm_softc *sc, u_int event_type)
{
	struct apm_event_info *evp;

	if ((sc->sc_flags & SCFLAG_OPEN) == 0)
		return 1;		/* no user waiting */
	if (sc->event_count == APM_NEVENTS)
		return 1;			/* overflow */
	evp = &sc->event_list[sc->event_ptr];
	sc->event_count++;
	sc->event_ptr++;
	sc->event_ptr %= APM_NEVENTS;
	evp->type = event_type;
	evp->index = ++apm_evindex;
	selnotify(&sc->sc_rsel, 0, 0);
	return (sc->sc_flags & SCFLAG_OWRITE) ? 0 : 1; /* user may handle */
}

static void
apm_event_handle(struct apm_softc *sc, u_int event_code, u_int event_info)
{
	int error;
	const char *code;
	struct apm_power_info pi;

	switch (event_code) {
	case APM_USER_STANDBY_REQ:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: user standby request\n"));
		if (apm_do_standby) {
			if (apm_record_event(sc, event_code))
				apm_userstandbys++;
			apm_op_inprog++;
			(void)sc->ops->set_powstate(sc->cookie,
						    APM_DEV_ALLDEVS,
						    APM_LASTREQ_INPROG);
		} else {
			(void)sc->ops->set_powstate(sc->cookie,
						    APM_DEV_ALLDEVS,
						    APM_LASTREQ_REJECTED);
			/* in case BIOS hates being spurned */
			sc->ops->enable(sc->cookie, 1);
		}
		break;

	case APM_STANDBY_REQ:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: system standby request\n"));
		if (apm_standbys || apm_suspends) {
			DPRINTF(APMDEBUG_EVENTS | APMDEBUG_ANOM,
			    ("damn fool BIOS did not wait for answer\n"));
			/* just give up the fight */
			apm_damn_fool_bios = 1;
		}
		if (apm_do_standby) {
			if (apm_record_event(sc, event_code))
				apm_standbys++;
			apm_op_inprog++;
			(void)sc->ops->set_powstate(sc->cookie,
						    APM_DEV_ALLDEVS,
						    APM_LASTREQ_INPROG);
		} else {
			(void)sc->ops->set_powstate(sc->cookie,
						    APM_DEV_ALLDEVS,
						    APM_LASTREQ_REJECTED);
			/* in case BIOS hates being spurned */
			sc->ops->enable(sc->cookie, 1);
		}
		break;

	case APM_USER_SUSPEND_REQ:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: user suspend request\n"));
		if (apm_record_event(sc, event_code))
			apm_suspends++;
		apm_op_inprog++;
		(void)sc->ops->set_powstate(sc->cookie,
					    APM_DEV_ALLDEVS,
					    APM_LASTREQ_INPROG);
		break;

	case APM_SUSPEND_REQ:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: system suspend request\n"));
		if (apm_standbys || apm_suspends) {
			DPRINTF(APMDEBUG_EVENTS | APMDEBUG_ANOM,
			    ("damn fool BIOS did not wait for answer\n"));
			/* just give up the fight */
			apm_damn_fool_bios = 1;
		}
		if (apm_record_event(sc, event_code))
			apm_suspends++;
		apm_op_inprog++;
		(void)sc->ops->set_powstate(sc->cookie,
					    APM_DEV_ALLDEVS,
					    APM_LASTREQ_INPROG);
		break;

	case APM_POWER_CHANGE:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: power status change\n"));
		error = sc->ops->get_powstat(sc->cookie, &pi);
#ifdef APM_POWER_PRINT
		/* only print if nobody is catching events. */
		if (error == 0 &&
		    (sc->sc_flags & (SCFLAG_OREAD|SCFLAG_OWRITE)) == 0)
			apm_power_print(sc, &pi);
#endif
		apm_record_event(sc, event_code);
		break;

	case APM_NORMAL_RESUME:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: resume system\n"));
		apm_resume(sc, event_code, event_info);
		break;

	case APM_CRIT_RESUME:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: critical resume system"));
		apm_resume(sc, event_code, event_info);
		break;

	case APM_SYS_STANDBY_RESUME:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: system standby resume\n"));
		apm_resume(sc, event_code, event_info);
		break;

	case APM_UPDATE_TIME:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: update time\n"));
		apm_resume(sc, event_code, event_info);
		break;

	case APM_CRIT_SUSPEND_REQ:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: critical system suspend\n"));
		apm_record_event(sc, event_code);
		apm_suspend(sc);
		break;

	case APM_BATTERY_LOW:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: battery low\n"));
		apm_battlow++;
		apm_record_event(sc, event_code);
		break;

	case APM_CAP_CHANGE:
		DPRINTF(APMDEBUG_EVENTS, ("apmev: capability change\n"));
		if (apm_minver < 2) {
			DPRINTF(APMDEBUG_EVENTS, ("apm: unexpected event\n"));
		} else {
			u_int numbatts, capflags;
			sc->ops->get_capabilities(sc->cookie,
						  &numbatts, &capflags);
			sc->ops->get_powstat(sc->cookie, &pi); /* XXX */
		}
		break;

	default:
		switch (event_code >> 8) {
			case 0:
				code = "reserved system";
				break;
			case 1:
				code = "reserved device";
				break;
			case 2:
				code = "OEM defined";
				break;
			default:
				code = "reserved";
				break;
		}
		printf("APM: %s event code %x\n", code, event_code);
	}
}

static void
apm_periodic_check(struct apm_softc *sc)
{
	int error;
	u_int event_code, event_info;


	/*
	 * tell the BIOS we're working on it, if asked to do a
	 * suspend/standby
	 */
	if (apm_op_inprog)
		sc->ops->set_powstate(sc->cookie, APM_DEV_ALLDEVS,
				      APM_LASTREQ_INPROG);

	while ((error = sc->ops->get_event(sc->cookie, &event_code,
					   &event_info)) == 0
	       && !apm_damn_fool_bios)
		apm_event_handle(sc, event_code, event_info);

	if (error != APM_ERR_NOEVENTS)
		apm_perror("get event", error);
	if (apm_suspends) {
		apm_op_inprog = 0;
		apm_suspend(sc);
	} else if (apm_standbys || apm_userstandbys) {
		apm_op_inprog = 0;
		apm_standby(sc);
	}
	apm_suspends = apm_standbys = apm_battlow = apm_userstandbys = 0;
	apm_damn_fool_bios = 0;
}

static void
apm_set_ver(struct apm_softc *self, u_long detail)
{

	if (apm_v12_enabled &&
	    APM_MAJOR_VERS(detail) == 1 &&
	    APM_MINOR_VERS(detail) == 2) {
		apm_majver = 1;
		apm_minver = 2;
		goto ok;
	}

	if (apm_v11_enabled &&
	    APM_MAJOR_VERS(detail) == 1 &&
	    APM_MINOR_VERS(detail) == 1) {
		apm_majver = 1;
		apm_minver = 1;
	} else {
		apm_majver = 1;
		apm_minver = 0;
	}
ok:
	printf("Power Management spec V%d.%d", apm_majver, apm_minver);
	apm_inited = 1;
	if (detail & APM_IDLE_SLOWS) {
#ifdef DIAGNOSTIC
		/* not relevant often */
		printf(" (slowidle)");
#endif
		/* leave apm_do_idle at its user-configured setting */
	} else
		apm_do_idle = 0;
#ifdef DIAGNOSTIC
	if (detail & APM_BIOS_PM_DISABLED)
		printf(" (BIOS mgmt disabled)");
	if (detail & APM_BIOS_PM_DISENGAGED)
		printf(" (BIOS managing devices)");
#endif
}

static int
apmmatch(struct device *parent,
	 struct cfdata *match, void *aux)
{

	/* There can be only one! */
	if (apm_inited)
		return 0;

	return (1);
}

static void
apmattach(struct device *parent, struct device *self, void *aux)
{
	struct apm_softc *sc = (void *)self;
	struct apmdev_attach_args *aaa = aux;
	struct apm_power_info pinfo;
	u_int numbatts, capflags;
	int error;

	printf(": ");

	sc->ops = aaa->accessops;
	sc->cookie = aaa->accesscookie;

	switch ((APM_MAJOR_VERS(aaa->apm_detail) << 8) +
		APM_MINOR_VERS(aaa->apm_detail)) {
	case 0x0100:
		apm_v11_enabled = 0;
		apm_v12_enabled = 0;
		break;
	case 0x0101:
		apm_v12_enabled = 0;
		/* fall through */
	case 0x0102:
	default:
		break;
	}

	apm_set_ver(sc, aaa->apm_detail);	/* prints version info */
	printf("\n");
	if (apm_minver >= 2)
		sc->ops->get_capabilities(sc->cookie, &numbatts, &capflags);

	/*
	 * enable power management
	 */
	sc->ops->enable(sc->cookie, 1);

	error = sc->ops->get_powstat(sc->cookie, &pinfo);
	if (error == 0) {
#ifdef APM_POWER_PRINT
		apm_power_print(apmsc, &pinfo);
#endif
	} else
		apm_perror("get power status", error);
	sc->ops->cpu_busy(sc->cookie);

	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_NONE);
	selinit(&sc->sc_rsel);
	selinit(&sc->sc_xsel);

	/* Initial state is `resumed'. */
	sc->sc_power_state = PWR_RESUME;

	/* Do an initial check. */
	apm_periodic_check(sc);

	/*
	 * Create a kernel thread to periodically check for APM events,
	 * and notify other subsystems when they occur.
	 */
	if (kthread_create(PRI_NONE, 0, NULL, apm_thread, sc,
	    &sc->sc_thread, "%s", device_xname(&sc->sc_dev)) != 0) {
		/*
		 * We were unable to create the APM thread; bail out.
		 */
		sc->ops->disconnect(sc->cookie);
		aprint_error_dev(&sc->sc_dev, "unable to create thread, "
		    "kernel APM support disabled\n");
	}
}

/*
 * Print function (for parent devices).
 */
int
apmprint(void *aux, const char *pnp)
{
	if (pnp)
		aprint_normal("apm at %s", pnp);

	return (UNCONF);
}
void
apm_thread(void *arg)
{
	struct apm_softc *apmsc = arg;

	/*
	 * Loop forever, doing a periodic check for APM events.
	 */
	for (;;) {
		APM_LOCK(apmsc);
		apm_periodic_check(apmsc);
		APM_UNLOCK(apmsc);
		(void) tsleep(apmsc, PWAIT, "apmev",  (8 * hz) / 7);
	}
}

int
apmdevopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	int unit = APMUNIT(dev);
	int ctl = APMDEV(dev);
	int error = 0;
	struct apm_softc *sc;

	if (unit >= apmdev_cd.cd_ndevs)
		return ENXIO;
	sc = apmdev_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (!apm_inited)
		return ENXIO;

	DPRINTF(APMDEBUG_DEVICE,
	    ("apmopen: pid %d flag %x mode %x\n", l->l_proc->p_pid, flag, mode));

	APM_LOCK(sc);
	switch (ctl) {
	case APMDEV_CTL:
		if (!(flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		if (sc->sc_flags & SCFLAG_OWRITE) {
			error = EBUSY;
			break;
		}
		sc->sc_flags |= SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		if (!(flag & FREAD) || (flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		sc->sc_flags |= SCFLAG_OREAD;
		break;
	default:
		error = ENXIO;
		break;
	}
	APM_UNLOCK(sc);

	return (error);
}

int
apmdevclose(dev_t dev, int flag, int mode,
	    struct lwp *l)
{
	struct apm_softc *sc = apmdev_cd.cd_devs[APMUNIT(dev)];
	int ctl = APMDEV(dev);

	DPRINTF(APMDEBUG_DEVICE,
	    ("apmclose: pid %d flag %x mode %x\n", l->l_proc->p_pid, flag, mode));

	APM_LOCK(sc);
	switch (ctl) {
	case APMDEV_CTL:
		sc->sc_flags &= ~SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		sc->sc_flags &= ~SCFLAG_OREAD;
		break;
	}
	if ((sc->sc_flags & SCFLAG_OPEN) == 0) {
		sc->event_count = 0;
		sc->event_ptr = 0;
	}
	APM_UNLOCK(sc);
	return 0;
}

int
apmdevioctl(dev_t dev, u_long cmd, void *data, int flag,
	    struct lwp *l)
{
	struct apm_softc *sc = apmdev_cd.cd_devs[APMUNIT(dev)];
	struct apm_power_info *powerp;
	struct apm_event_info *evp;
#if 0
	struct apm_ctl *actl;
#endif
	int i, error = 0;
	int batt_flags;

	APM_LOCK(sc);
	switch (cmd) {
	case APM_IOC_STANDBY:
		if (!apm_do_standby) {
			error = EOPNOTSUPP;
			break;
		}

		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		apm_userstandbys++;
		break;

	case APM_IOC_SUSPEND:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		apm_suspends++;
		break;

	case APM_IOC_NEXTEVENT:
		if (!sc->event_count)
			error = EAGAIN;
		else {
			evp = (struct apm_event_info *)data;
			i = sc->event_ptr + APM_NEVENTS - sc->event_count;
			i %= APM_NEVENTS;
			*evp = sc->event_list[i];
			sc->event_count--;
		}
		break;

	case OAPM_IOC_GETPOWER:
	case APM_IOC_GETPOWER:
		powerp = (struct apm_power_info *)data;
		if ((error = sc->ops->get_powstat(sc->cookie, powerp)) != 0) {
			apm_perror("ioctl get power status", error);
			error = EIO;
			break;
		}
		switch (apm_minver) {
		case 0:
			break;
		case 1:
		default:
			batt_flags = powerp->battery_state;
			powerp->battery_state = APM_BATT_UNKNOWN;
			if (batt_flags & APM_BATT_FLAG_HIGH)
				powerp->battery_state = APM_BATT_HIGH;
			else if (batt_flags & APM_BATT_FLAG_LOW)
				powerp->battery_state = APM_BATT_LOW;
			else if (batt_flags & APM_BATT_FLAG_CRITICAL)
				powerp->battery_state = APM_BATT_CRITICAL;
			else if (batt_flags & APM_BATT_FLAG_CHARGING)
				powerp->battery_state = APM_BATT_CHARGING;
			else if (batt_flags & APM_BATT_FLAG_NO_SYSTEM_BATTERY)
				powerp->battery_state = APM_BATT_ABSENT;
			break;
		}
		break;

	default:
		error = ENOTTY;
	}
	APM_UNLOCK(sc);

	return (error);
}

int
apmdevpoll(dev_t dev, int events, struct lwp *l)
{
	struct apm_softc *sc = apmdev_cd.cd_devs[APMUNIT(dev)];
	int revents = 0;

	APM_LOCK(sc);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->event_count)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(l, &sc->sc_rsel);
	}
	APM_UNLOCK(sc);

	return (revents);
}

static void
filt_apmrdetach(struct knote *kn)
{
	struct apm_softc *sc = kn->kn_hook;

	APM_LOCK(sc);
	SLIST_REMOVE(&sc->sc_rsel.sel_klist, kn, knote, kn_selnext);
	APM_UNLOCK(sc);
}

static int
filt_apmread(struct knote *kn, long hint)
{
	struct apm_softc *sc = kn->kn_hook;

	kn->kn_data = sc->event_count;
	return (kn->kn_data > 0);
}

static const struct filterops apmread_filtops =
	{ 1, NULL, filt_apmrdetach, filt_apmread };

int
apmdevkqfilter(dev_t dev, struct knote *kn)
{
	struct apm_softc *sc = apmdev_cd.cd_devs[APMUNIT(dev)];
	struct klist *klist;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->sc_rsel.sel_klist;
		kn->kn_fop = &apmread_filtops;
		break;

	default:
		return (EINVAL);
	}

	kn->kn_hook = sc;

	APM_LOCK(sc);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	APM_UNLOCK(sc);

	return (0);
}
