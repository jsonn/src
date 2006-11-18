/* $NetBSD: onewire.c,v 1.2.2.1 2006/11/18 21:34:28 ad Exp $ */
/*	$OpenBSD: onewire.c,v 1.1 2006/03/04 16:27:03 grange Exp $	*/

/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: onewire.c,v 1.2.2.1 2006/11/18 21:34:28 ad Exp $");

/*
 * 1-Wire bus driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#ifdef ONEWIRE_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

//#define ONEWIRE_MAXDEVS		256
#define ONEWIRE_MAXDEVS		8
#define ONEWIRE_SCANTIME	3

struct onewire_softc {
	struct device			sc_dev;

	struct onewire_bus *		sc_bus;
	struct lock			sc_lock;
	struct proc *			sc_thread;
	TAILQ_HEAD(, onewire_device)	sc_devs;

	int				sc_dying;
};

struct onewire_device {
	TAILQ_ENTRY(onewire_device)	d_list;
	struct device *			d_dev;
	u_int64_t			d_rom;
	int				d_present;
};

int	onewire_match(struct device *, struct cfdata *, void *);
void	onewire_attach(struct device *, struct device *, void *);
int	onewire_detach(struct device *, int);
int	onewire_activate(struct device *, enum devact);
int	onewire_print(void *, const char *);

void	onewire_thread(void *);
void	onewire_createthread(void *);
void	onewire_scan(struct onewire_softc *);

CFATTACH_DECL(onewire, sizeof(struct onewire_softc),
	onewire_match, onewire_attach, onewire_detach, onewire_activate);

const struct cdevsw onewire_cdevsw = {
	noopen, noclose, noread, nowrite, noioctl, nostop, notty,
	nopoll, nommap, nokqfilter, D_OTHER,
};

extern struct cfdriver onewire_cd;

int
onewire_match(struct device *parent, struct cfdata *cf,
    void *aux)
{
	return 1;
}

void
onewire_attach(struct device *parent, struct device *self, void *aux)
{
	struct onewire_softc *sc = device_private(self);
	struct onewirebus_attach_args *oba = aux;

	sc->sc_bus = oba->oba_bus;
	lockinit(&sc->sc_lock, PRIBIO, "owlock", 0, 0);
	TAILQ_INIT(&sc->sc_devs);

	printf("\n");

	kthread_create(onewire_createthread, sc);
}

int
onewire_detach(struct device *self, int flags)
{
	struct onewire_softc *sc = device_private(self);
	int rv;

	sc->sc_dying = 1;
	if (sc->sc_thread != NULL) {
		wakeup(sc->sc_thread);
		tsleep(&sc->sc_dying, PWAIT, "owdt", 0);
	}

	onewire_lock(sc, 0);
	//rv = config_detach_children(self, flags);
	rv = 0;  /* XXX riz */
	onewire_unlock(sc);

	return (rv);
}

int
onewire_activate(struct device *self, enum devact act)
{
	struct onewire_softc *sc = device_private(self);
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}

	//return (config_activate_children(self, act));
	return rv;
}

int
onewire_print(void *aux, const char *pnp)
{
	struct onewire_attach_args *oa = aux;
	const char *famname;

	if (pnp == NULL)
		printf(" ");

	famname = onewire_famname(ONEWIRE_ROM_FAMILY_TYPE(oa->oa_rom));
	if (famname == NULL)
		printf("family 0x%02x", (uint)ONEWIRE_ROM_FAMILY_TYPE(oa->oa_rom));
	else
		printf("\"%s\"", famname);
	printf(" sn %012llx", ONEWIRE_ROM_SN(oa->oa_rom));

	if (pnp != NULL)
		printf(" at %s", pnp);

	return (UNCONF);
}

int
onewirebus_print(void *aux, const char *pnp)
{
	if (pnp != NULL)
		printf("onewire at %s", pnp);

	return (UNCONF);
}

int
onewire_lock(void *arg, int flags)
{
	struct onewire_softc *sc = arg;
	int lflags = LK_EXCLUSIVE;

	if (flags & ONEWIRE_NOWAIT)
		lflags |= LK_NOWAIT;

	return (lockmgr(&sc->sc_lock, lflags, NULL));
}

void
onewire_unlock(void *arg)
{
	struct onewire_softc *sc = arg;

	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
}

int
onewire_reset(void *arg)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;

	return (bus->bus_reset(bus->bus_cookie));
}

int
onewire_bit(void *arg, int value)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;

	return (bus->bus_bit(bus->bus_cookie, value));
}

int
onewire_read_byte(void *arg)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;
	u_int8_t value = 0;
	int i;

	if (bus->bus_read_byte != NULL)
		return (bus->bus_read_byte(bus->bus_cookie));

	for (i = 0; i < 8; i++)
		value |= (bus->bus_bit(bus->bus_cookie, 1) << i);

	return (value);
}

void
onewire_write_byte(void *arg, int value)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;
	int i;

	if (bus->bus_write_byte != NULL)
		return (bus->bus_write_byte(bus->bus_cookie, value));

	for (i = 0; i < 8; i++)
		bus->bus_bit(bus->bus_cookie, (value >> i) & 0x1);
}

int
onewire_triplet(void *arg, int dir)
{
	struct onewire_softc *sc = arg;
	struct onewire_bus *bus = sc->sc_bus;
	int rv;

	if (bus->bus_triplet != NULL)
		return (bus->bus_triplet(bus->bus_cookie, dir));

	rv = bus->bus_bit(bus->bus_cookie, 1);
	rv <<= 1;
	rv |= bus->bus_bit(bus->bus_cookie, 1);

	switch (rv) {
	case 0x0:
		bus->bus_bit(bus->bus_cookie, dir);
		break;
	case 0x1:
		bus->bus_bit(bus->bus_cookie, 0);
		break;
	default:
		bus->bus_bit(bus->bus_cookie, 1);
	}

	return (rv);
}

void
onewire_read_block(void *arg, void *buf, int len)
{
	u_int8_t *p = buf;

	while (len--)
		*p++ = onewire_read_byte(arg);
}

void
onewire_write_block(void *arg, const void *buf, int len)
{
	const u_int8_t *p = buf;

	while (len--)
		onewire_write_byte(arg, *p++);
}

void
onewire_matchrom(void *arg, u_int64_t rom)
{
	int i;

	onewire_write_byte(arg, ONEWIRE_CMD_MATCH_ROM);
	for (i = 0; i < 8; i++)
		onewire_write_byte(arg, (rom >> (i * 8)) & 0xff);
}

void
onewire_thread(void *arg)
{
	struct onewire_softc *sc = arg;

	while (!sc->sc_dying) {
		onewire_scan(sc);
		tsleep(sc->sc_thread, PWAIT, "owidle", ONEWIRE_SCANTIME * hz);
	}

	sc->sc_thread = NULL;
	wakeup(&sc->sc_dying);
	kthread_exit(0);
}

void
onewire_createthread(void *arg)
{
	struct onewire_softc *sc = arg;

	if (kthread_create1(onewire_thread, sc, &sc->sc_thread,
	    "%s", sc->sc_dev.dv_xname) != 0)
		printf("%s: can't create kernel thread\n",
		    sc->sc_dev.dv_xname);
}

void
onewire_scan(struct onewire_softc *sc)
{
	struct onewire_device *d, *next, *nd;
	struct onewire_attach_args oa;
	struct device *dev;
	int search = 1, count = 0, present;
	int dir, rv;
	u_int64_t mask, rom = 0, lastrom;
	u_int8_t data[8];
	int i, i0 = -1, lastd = -1;

	TAILQ_FOREACH(d, &sc->sc_devs, d_list)
		d->d_present = 0;

	while (search && count++ < ONEWIRE_MAXDEVS) {
		/* XXX: yield processor */
		tsleep(sc, PWAIT, "owscan", hz / 10);

		/*
		 * Reset the bus. If there's no presence pulse
		 * don't search for any devices.
		 */
		onewire_lock(sc, 0);
		if (onewire_reset(sc) != 0) {
			DPRINTF(("%s: scan: no presence pulse\n",
			    sc->sc_dev.dv_xname));
			onewire_unlock(sc);
			break;
		}

		/*
		 * Start new search. Go through the previous path to
		 * the point we made a decision last time and make an
		 * opposite decision. If we didn't make any decision
		 * stop searching.
		 */
		search = 0;
		lastrom = rom;
		rom = 0;
		onewire_write_byte(sc, ONEWIRE_CMD_SEARCH_ROM);
		for (i = 0,i0 = -1; i < 64; i++) {
			dir = (lastrom >> i) & 0x1;
			if (i == lastd)
				dir = 1;
			else if (i > lastd)
				dir = 0;
			rv = onewire_triplet(sc, dir);
			switch (rv) {
			case 0x0:
				if (i != lastd) {
					if (dir == 0)
						i0 = i;
					search = 1;
				}
				mask = dir;
				break;
			case 0x1:
				mask = 0;
				break;
			case 0x2:
				mask = 1;
				break;
			default:
				DPRINTF(("%s: scan: triplet error 0x%x, "
				    "step %d\n",
				    sc->sc_dev.dv_xname, rv, i));
				onewire_unlock(sc);
				return;
			}
			rom |= (mask << i);
		}
		lastd = i0;
		onewire_unlock(sc);

		if (rom == 0)
			continue;

		/*
		 * The last byte of the ROM code contains a CRC calculated
		 * from the first 7 bytes. Re-calculate it to make sure
		 * we found a valid device.
		 */
		for (i = 0; i < 8; i++)
			data[i] = (rom >> (i * 8)) & 0xff;
		if (onewire_crc(data, 7) != data[7])
			continue;

		/*
		 * Go through the list of attached devices to see if we
		 * found a new one.
		 */
		present = 0;
		TAILQ_FOREACH(d, &sc->sc_devs, d_list) {
			if (d->d_rom == rom) {
				d->d_present = 1;
				present = 1;
				break;
			}
		}
		if (!present) {
			bzero(&oa, sizeof(oa));
			oa.oa_onewire = sc;
			oa.oa_rom = rom;
			if ((dev = config_found(&sc->sc_dev, &oa,
			    onewire_print)) == NULL)
				continue;

			MALLOC(nd, struct onewire_device *,
			    sizeof(struct onewire_device), M_DEVBUF, M_NOWAIT);
			if (nd == NULL)
				continue;
			nd->d_dev = dev;
			nd->d_rom = rom;
			nd->d_present = 1;
			TAILQ_INSERT_TAIL(&sc->sc_devs, nd, d_list);
		}
	}

	/* Detach disappeared devices */
	onewire_lock(sc, 0);
	for (d = TAILQ_FIRST(&sc->sc_devs);
	    d != NULL; d = next) {
		next = TAILQ_NEXT(d, d_list);
		if (!d->d_present) {
			config_detach(d->d_dev, DETACH_FORCE);
			TAILQ_REMOVE(&sc->sc_devs, d, d_list);
			FREE(d, M_DEVBUF);
		}
	}
	onewire_unlock(sc);
}
