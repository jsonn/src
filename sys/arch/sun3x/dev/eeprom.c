/*	$NetBSD: eeprom.c,v 1.1.1.1.2.2 1997/01/14 20:57:03 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * Access functions for the EEPROM (Electrically Eraseable PROM)
 * The main reason for the existence of this module is to
 * handle the painful task of updating the EEPROM contents.
 * After a write, it must not be touched for 10 milliseconds.
 * (See the Sun-3 Architecture Manual sec. 5.9)
 *
 * XXX: Should just keep a copy of the EEPROM contents in RAM
 * (read it once at init time) to avoid the eeprom_uio hair.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <machine/obio.h>
#include <machine/eeprom.h>

#define HZ 100	/* XXX */
#define OBIO_EEPROM_SIZE sizeof(struct eeprom)

int ee_console;		/* for convenience of drivers */

static int ee_update(caddr_t buf, int off, int cnt);

static char *eeprom_va;
static int ee_busy, ee_want;

static int  eeprom_match __P((struct device *, struct cfdata *, void *));
static void eeprom_attach __P((struct device *, struct device *, void *));

struct cfattach eeprom_ca = {
	sizeof(struct device), eeprom_match, eeprom_attach
};

struct cfdriver eeprom_cd = {
	NULL, "eeprom", DV_DULL
};

/* Called very early by internal_configure. */
void eeprom_init()
{
	eeprom_va = obio_find_mapping(OBIO_EEPROM, OBIO_EEPROM_SIZE);
	ee_console = ((struct eeprom *)eeprom_va)->eeConsole;
}

static int
eeprom_match(parent, cf, args)
    struct device *parent;
	struct cfdata *cf;
    void *args;
{
	struct confargs *ca = args;

	/* This driver only supports one unit. */
	if (cf->cf_unit != 0)
		return (0);

	/* Validate the given address. */
	if (ca->ca_paddr != OBIO_EEPROM)
		return (0);

	if (eeprom_va == NULL)
		return (0);

	return (1);
}

static void
eeprom_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{

	printf("\n");
}


/* Take the lock. */
static int
ee_take __P((void))
{
	int error = 0;
	while (ee_busy) {
		ee_want = 1;
		error = tsleep(&ee_busy, PZERO | PCATCH, "eeprom", 0);
		ee_want = 0;
		if (error)	/* interrupted */
			goto out;
	}
	ee_busy = 1;
 out:
	return error;
}

/* Give the lock. */
static void
ee_give __P((void))
{
	ee_busy = 0;
	if (ee_want) {
		ee_want = 0;
		wakeup(&ee_busy);
	}
}

/*
 * XXX - Just keep a soft copy of the eeprom?
 */
int
eeprom_uio(struct uio *uio)
{
	int error;
	int off;	/* NOT off_t */
	u_int cnt;
	caddr_t va;
	caddr_t buf = (caddr_t)0;

	off = uio->uio_offset;
	if (off >= OBIO_EEPROM_SIZE)
		return (EFAULT);

	cnt = uio->uio_resid;
	if (cnt > (OBIO_EEPROM_SIZE - off))
		cnt = (OBIO_EEPROM_SIZE - off);

	if ((error = ee_take()) != 0)
		return (error);

	if (eeprom_va == NULL) {
		error = ENXIO;
		goto out;
	}

	va = eeprom_va;
	if (uio->uio_rw != UIO_READ) {
		/* Write requires a temporary buffer. */
		buf = malloc(OBIO_EEPROM_SIZE, M_DEVBUF, M_WAITOK);
		if (!buf) {
			error = EAGAIN;
			goto out;
		}
		va = buf;
	}

	if ((error = uiomove(va + off, (int)cnt, uio)) != 0)
		goto out;

	if (uio->uio_rw != UIO_READ)
		error = ee_update(buf, off, cnt);

 out:
	if (buf)
		free(buf, M_DEVBUF);
	ee_give();
	return (error);
}

/*
 * Update the EEPROM from the passed buf.
 */
static int
ee_update(char *buf, int off, int cnt)
{
	volatile char *ep;
	char *bp;

	if (eeprom_va == NULL)
		return (ENXIO);

	ep = eeprom_va + off;
	bp = buf + off;

	while (cnt > 0) {
		/*
		 * DO NOT WRITE IT UNLESS WE HAVE TO because the
		 * EEPROM has a limited number of write cycles.
		 * After some number of writes it just fails!
		 */
		if (*ep != *bp) {
			*ep  = *bp;
			/*
			 * We have written the EEPROM, so now we must
			 * sleep for at least 10 milliseconds while
			 * holding the lock to prevent all access to
			 * the EEPROM while it recovers.
			 */
			(void)tsleep(eeprom_va, PZERO-1, "eeprom", HZ/50);
		}
		/* Make sure the write worked. */
		if (*ep != *bp)
			return (EIO);
		ep++;
		bp++;
		cnt--;
	}
	return(0);
}

/*
 * Read a byte out of the EEPROM.  This is called from
 * things like the zs driver very early to find out
 * which device should be used as the console.
 */
int ee_get_byte(int off, int canwait)
{
	int c = -1;
	if ((off < 0) || (off >= OBIO_EEPROM_SIZE))
		goto out;
	if (eeprom_va == NULL)
		goto out;

	if (canwait) {
		if (ee_take())
			goto out;
	} else {
		if (ee_busy)
			goto out;
	}

	c = eeprom_va[off] & 0xFF;

	if (canwait)
		ee_give();
 out:
	return c;
}
