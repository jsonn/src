/* $NetBSD: isafcns_jensen.c,v 1.6.4.1 1997/09/04 00:53:14 thorpej Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: isafcns_jensen.c,v 1.6.4.1 1997/09/04 00:53:14 thorpej Exp $");

#include <sys/types.h>
#include <machine/pio.h>
#include <machine/pte.h>

static u_int8_t		jensen_inb __P((int port));
/* static void		jensen_insb __P((int port, void *addr, int cnt)); */
static u_int16_t	jensen_inw __P((int port));
/* static void		jensen_insw __P((int port, void *addr, int cnt)); */
u_int32_t		jensen_inl __P((int port));
/* static void		jensen_insl __P((int port, void *addr, int cnt)); */

static void		jensen_outb __P((int port, u_int8_t datum));
/* static void		jensen_outsb __P((int port, void *addr, int cnt)); */
static void		jensen_outw __P((int port, u_int16_t datum));
/* static void		jensen_outsw __P((int port, void *addr, int cnt)); */
static void		jensen_outl __P((int port, u_int32_t datum));
/* static void		jensen_outsl __P((int port, void *addr, int cnt)); */

struct alpha_isafcndesc jensen_isafcns = {
	jensen_inb,	0 /* jensen_insb */,
	jensen_inw,	0 /* jensen_insw */,
	jensen_inl,	0 /* jensen_insl */,
	jensen_outb,	0 /* jensen_outsb */,
	jensen_outw,	0 /* jensen_outsw */,
	jensen_outl,	0 /* jensen_outsl */,
};

u_int8_t
jensen_inb(ioaddr)
	int ioaddr;
{
	u_int32_t *port, val;
	u_int8_t rval;
	int offset;

	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(0x300000000L | 0 << 5 | ioaddr << 7);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xff;
	rval = val & 0xff;

printf("inb(0x%x) => 0x%x @ %p => 0x%x\n", ioaddr, val, port, rval);

	return rval;
}

u_int16_t
jensen_inw(ioaddr)
	int ioaddr;
{
	u_int32_t *port, val;
	u_int16_t rval;
	int offset;

	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(0x300000000L | 1 << 5 | ioaddr << 7);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffff;
	rval = val & 0xffff;

panic("inw(0x%x) => 0x%x @ %p => 0x%x\n", ioaddr, val, port, rval);

	return rval;
}

u_int32_t
jensen_inl(ioaddr)
	int ioaddr;
{
	u_int32_t *port, val;
	u_int32_t rval;
	int offset;

	offset = ioaddr & 3;
	port = (int32_t *)phystok0seg(0x300000000L | 3 << 5 | ioaddr << 7);
	val = *port;
	rval = ((val) >> (8 * offset)) & 0xffffffff;
	rval = val & 0xffffffff;

printf("inl(0x%x) => 0x%x @ %p => 0x%x\n", ioaddr, val, port, rval);

	return rval;
}

void
jensen_outb(ioaddr, val)
	int ioaddr;
	u_int8_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(0x300000000L | 0 << 5 | ioaddr << 7);

printf("outb(0x%x, 0x%x) => 0x%x @ %p\n", ioaddr, val, nval, port);

	*port = nval;
}

void
jensen_outw(ioaddr, val)
	int ioaddr;
	u_int16_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(0x300000000L | 1 << 5 | ioaddr << 7);

printf("outb(0x%x, 0x%x) => 0x%x @ %p\n", ioaddr, val, nval, port);

	*port = nval;
}

void
jensen_outl(ioaddr, val)
	int ioaddr;
	u_int32_t val;
{
	u_int32_t *port, nval;
	int offset;

	offset = ioaddr & 3;
	nval = val /*<< (8 * offset)*/;
	port = (int32_t *)phystok0seg(0x300000000L | 3 << 5 | ioaddr << 7);

printf("outb(0x%x, 0x%x) => 0x%x @ %p\n", ioaddr, val, nval, port);

	*port = nval;
}
