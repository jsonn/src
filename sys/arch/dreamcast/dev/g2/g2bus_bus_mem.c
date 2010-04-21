/*	$NetBSD: g2bus_bus_mem.c,v 1.14.18.1 2010/04/21 00:36:39 matt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Bus space implementation for the SEGA G2 bus.
 *
 * NOTE: We only implement a small subset of what the bus_space(9)
 * API specifies.  Right now, the GAPS PCI bridge is only used for
 * the Dreamcast Broadband Adatper, so we only provide what the
 * pci(4) and rtk(4) drivers need.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: g2bus_bus_mem.c,v 1.14.18.1 2010/04/21 00:36:39 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <dreamcast/dev/g2/g2busvar.h>

int	g2bus_bus_mem_map(void *, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	g2bus_bus_mem_unmap(void *, bus_space_handle_t, bus_size_t);
paddr_t	g2bus_bus_mem_mmap(void *, bus_addr_t, off_t, int, int);

uint8_t g2bus_bus_mem_read_1(void *, bus_space_handle_t, bus_size_t);
uint16_t g2bus_bus_mem_read_2(void *, bus_space_handle_t, bus_size_t);
uint32_t g2bus_bus_mem_read_4(void *, bus_space_handle_t, bus_size_t);

void	g2bus_bus_mem_write_1(void *, bus_space_handle_t, bus_size_t,
	    uint8_t);
void	g2bus_bus_mem_write_2(void *, bus_space_handle_t, bus_size_t,
	    uint16_t);
void	g2bus_bus_mem_write_4(void *, bus_space_handle_t, bus_size_t,
	    uint32_t);

void	g2bus_bus_mem_read_region_1(void *, bus_space_handle_t, bus_size_t,
	    uint8_t *, bus_size_t);
void	g2bus_bus_mem_read_region_2(void *, bus_space_handle_t, bus_size_t,
	    uint16_t *, bus_size_t);
void	g2bus_bus_mem_read_region_4(void *, bus_space_handle_t, bus_size_t,
	    uint32_t *, bus_size_t);

void	g2bus_bus_mem_write_region_1(void *, bus_space_handle_t, bus_size_t,
	    const uint8_t *, bus_size_t);
void	g2bus_bus_mem_write_region_2(void *, bus_space_handle_t, bus_size_t,
	    const uint16_t *, bus_size_t);
void	g2bus_bus_mem_write_region_4(void *, bus_space_handle_t, bus_size_t,
	    const uint32_t *, bus_size_t);

void	g2bus_bus_mem_set_region_4(void *, bus_space_handle_t, bus_size_t,
	    uint32_t, bus_size_t);

uint8_t g2bus_sparse_bus_mem_read_1(void *, bus_space_handle_t, bus_size_t);
uint16_t g2bus_sparse_bus_mem_read_2(void *, bus_space_handle_t, bus_size_t);
uint32_t g2bus_sparse_bus_mem_read_4(void *, bus_space_handle_t, bus_size_t);

void	g2bus_sparse_bus_mem_write_1(void *, bus_space_handle_t, bus_size_t,
	    uint8_t);
void	g2bus_sparse_bus_mem_write_2(void *, bus_space_handle_t, bus_size_t,
	    uint16_t);
void	g2bus_sparse_bus_mem_write_4(void *, bus_space_handle_t, bus_size_t,
	    uint32_t);

void	g2bus_sparse_bus_mem_read_region_1(void *, bus_space_handle_t,
	    bus_size_t, uint8_t *, bus_size_t);

void	g2bus_sparse_bus_mem_write_region_1(void *, bus_space_handle_t,
	    bus_size_t, const uint8_t *, bus_size_t);

void	g2bus_sparse_bus_mem_read_multi_1(void *, bus_space_handle_t,
	    bus_size_t, uint8_t *, bus_size_t);

void	g2bus_sparse_bus_mem_write_multi_1(void *, bus_space_handle_t,
	    bus_size_t, const uint8_t *, bus_size_t);

void
g2bus_bus_mem_init(struct g2bus_softc *sc)
{
	bus_space_tag_t t = &sc->sc_memt;

	memset(t, 0, sizeof(*t));

	t->dbs_map = g2bus_bus_mem_map;
	t->dbs_unmap = g2bus_bus_mem_unmap;
	t->dbs_mmap = g2bus_bus_mem_mmap;

	t->dbs_r_1 = g2bus_bus_mem_read_1;
	t->dbs_r_2 = g2bus_bus_mem_read_2;
	t->dbs_r_4 = g2bus_bus_mem_read_4;

	t->dbs_w_1 = g2bus_bus_mem_write_1;
	t->dbs_w_2 = g2bus_bus_mem_write_2;
	t->dbs_w_4 = g2bus_bus_mem_write_4;

	t->dbs_rr_1 = g2bus_bus_mem_read_region_1;
	t->dbs_rr_2 = g2bus_bus_mem_read_region_2;
	t->dbs_rr_4 = g2bus_bus_mem_read_region_4;

	t->dbs_wr_1 = g2bus_bus_mem_write_region_1;
	t->dbs_wr_2 = g2bus_bus_mem_write_region_2;
	t->dbs_wr_4 = g2bus_bus_mem_write_region_4;

	t->dbs_sr_4 = g2bus_bus_mem_set_region_4;
}

int
g2bus_bus_mem_map(void *v, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *shp)
{

	KASSERT((addr & SH3_PHYS_MASK) == addr);
	*shp = SH3_PHYS_TO_P2SEG(addr);

	return 0;
}

void
g2bus_bus_mem_unmap(void *v, bus_space_handle_t sh, bus_size_t size)
{

	KASSERT(sh >= SH3_P2SEG_BASE && sh <= SH3_P2SEG_END);
	/* Nothing to do. */
}

paddr_t
g2bus_bus_mem_mmap(void *v, bus_addr_t addr, off_t offset, int prot, int flags)
{

	/* XXX not implemented */
	return -1;
}

/*
 * G2 bus cycles must not be interrupted by IRQs or G2 DMA.
 * The following paired macros will take the necessary precautions.
 */

#define G2LOCK_DECL							\
	int __s

#define G2_LOCK()							\
	do {								\
		__s = _cpu_intr_suspend();				\
		/* suspend any G2 DMA here... */			\
		while ((*(volatile uint32_t *)0xa05f688c) & 0x20)	\
			;						\
	} while (/*CONSTCOND*/0)

#define G2_UNLOCK()							\
	do {								\
		/* resume any G2 DMA here... */				\
		_cpu_intr_resume(__s);					\
	} while (/*CONSTCOND*/0)

uint8_t
g2bus_bus_mem_read_1(void *v, bus_space_handle_t sh, bus_size_t off)
{
	G2LOCK_DECL;
	uint8_t rv;

	G2_LOCK();

	rv = *(volatile uint8_t *)(sh + off);

	G2_UNLOCK();

	return rv;
}

uint16_t
g2bus_bus_mem_read_2(void *v, bus_space_handle_t sh, bus_size_t off)
{
	G2LOCK_DECL;
	uint16_t rv;

	G2_LOCK();

	rv = *(volatile uint16_t *)(sh + off);

	G2_UNLOCK();

	return rv;
}

uint32_t
g2bus_bus_mem_read_4(void *v, bus_space_handle_t sh, bus_size_t off)
{
	G2LOCK_DECL;
	uint32_t rv;

	G2_LOCK();

	rv = *(volatile uint32_t *)(sh + off);

	G2_UNLOCK();

	return rv;
}

void
g2bus_bus_mem_write_1(void *v, bus_space_handle_t sh, bus_size_t off,
    uint8_t val)
{
	G2LOCK_DECL;

	G2_LOCK();

	*(volatile uint8_t *)(sh + off) = val;

	G2_UNLOCK();
}

void
g2bus_bus_mem_write_2(void *v, bus_space_handle_t sh, bus_size_t off,
    uint16_t val)
{
	G2LOCK_DECL;

	G2_LOCK();

	*(volatile uint16_t *)(sh + off) = val;

	G2_UNLOCK();
}

void
g2bus_bus_mem_write_4(void *v, bus_space_handle_t sh, bus_size_t off,
    uint32_t val)
{
	G2LOCK_DECL;

	G2_LOCK();

	*(volatile uint32_t *)(sh + off) = val;

	G2_UNLOCK();
}

void
g2bus_bus_mem_read_region_1(void *v, bus_space_handle_t sh, bus_size_t off,
    uint8_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile const uint8_t *baddr = (uint8_t *)(sh + off);

	G2_LOCK();

	while (len--)
		*addr++ = *baddr++;

	G2_UNLOCK();
}

void
g2bus_bus_mem_read_region_2(void *v, bus_space_handle_t sh, bus_size_t off,
    uint16_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile const uint16_t *baddr = (uint16_t *)(sh + off);

	G2_LOCK();

	while (len--)
		*addr++ = *baddr++;

	G2_UNLOCK();
}

void
g2bus_bus_mem_read_region_4(void *v, bus_space_handle_t sh, bus_size_t off,
    uint32_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile const uint32_t *baddr = (uint32_t *)(sh + off);

	G2_LOCK();

	while (len--)
		*addr++ = *baddr++;

	G2_UNLOCK();
}

void
g2bus_bus_mem_write_region_1(void *v, bus_space_handle_t sh, bus_size_t off,
    const uint8_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile uint8_t *baddr = (uint8_t *)(sh + off);

	G2_LOCK();

	while (len--)
		*baddr++ = *addr++;

	G2_UNLOCK();
}

void
g2bus_bus_mem_write_region_2(void *v, bus_space_handle_t sh, bus_size_t off,
    const uint16_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile uint16_t *baddr = (uint16_t *)(sh + off);

	G2_LOCK();

	while (len--)
		*baddr++ = *addr++;

	G2_UNLOCK();
}

void
g2bus_bus_mem_write_region_4(void *v, bus_space_handle_t sh, bus_size_t off,
    const uint32_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile uint32_t *baddr = (uint32_t *)(sh + off);

	G2_LOCK();

	while (len--)
		*baddr++ = *addr++;

	G2_UNLOCK();
}

void
g2bus_bus_mem_set_region_4(void *v, bus_space_handle_t sh, bus_size_t off,
    uint32_t val, bus_size_t len)
{
	G2LOCK_DECL;
	volatile uint32_t *baddr = (uint32_t *)(sh + off);

	G2_LOCK();

	while (len--)
		*baddr++ = val;

	G2_UNLOCK();
}

void
g2bus_set_bus_mem_sparse(bus_space_tag_t memt)
{

	memt->dbs_r_1 = g2bus_sparse_bus_mem_read_1;
	memt->dbs_r_2 = g2bus_sparse_bus_mem_read_2;
	memt->dbs_r_4 = g2bus_sparse_bus_mem_read_4;

	memt->dbs_w_1 = g2bus_sparse_bus_mem_write_1;
	memt->dbs_w_2 = g2bus_sparse_bus_mem_write_2;
	memt->dbs_w_4 = g2bus_sparse_bus_mem_write_4;

	memt->dbs_rr_1 = g2bus_sparse_bus_mem_read_region_1;

	memt->dbs_wr_1 = g2bus_sparse_bus_mem_write_region_1;

	memt->dbs_rm_1 = g2bus_sparse_bus_mem_read_multi_1;

	memt->dbs_wm_1 = g2bus_sparse_bus_mem_write_multi_1;
}

uint8_t
g2bus_sparse_bus_mem_read_1(void *v, bus_space_handle_t sh, bus_size_t off)
{
	G2LOCK_DECL;
	uint8_t rv;

	G2_LOCK();

	rv = *(volatile uint8_t *)(sh + (off * 4));

	G2_UNLOCK();

	return rv;
}

uint16_t
g2bus_sparse_bus_mem_read_2(void *v, bus_space_handle_t sh, bus_size_t off)
{
	G2LOCK_DECL;
	uint16_t rv;

	G2_LOCK();

	rv = *(volatile uint16_t *)(sh + (off * 4));

	G2_UNLOCK();

	return rv;
}

uint32_t
g2bus_sparse_bus_mem_read_4(void *v, bus_space_handle_t sh, bus_size_t off)
{
	G2LOCK_DECL;
	uint32_t rv;

	G2_LOCK();

	rv = *(volatile uint32_t *)(sh + (off * 4));

	G2_UNLOCK();

	return rv;
}

void
g2bus_sparse_bus_mem_write_1(void *v, bus_space_handle_t sh, bus_size_t off,
    uint8_t val)
{
	G2LOCK_DECL;

	G2_LOCK();

	*(volatile uint8_t *)(sh + (off * 4)) = val;

	G2_UNLOCK();
}

void
g2bus_sparse_bus_mem_write_2(void *v, bus_space_handle_t sh, bus_size_t off,
    uint16_t val)
{
	G2LOCK_DECL;

	G2_LOCK();

	*(volatile uint16_t *)(sh + (off * 4)) = val;

	G2_UNLOCK();
}

void
g2bus_sparse_bus_mem_write_4(void *v, bus_space_handle_t sh, bus_size_t off,
    uint32_t val)
{
	G2LOCK_DECL;

	G2_LOCK();

	*(volatile uint32_t *)(sh + (off * 4)) = val;

	G2_UNLOCK();
}

void
g2bus_sparse_bus_mem_read_region_1(void *v, bus_space_handle_t sh,
    bus_size_t off, uint8_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile const uint8_t *baddr = (uint8_t *)(sh + (off * 4));

	G2_LOCK();

	while (len--) {
		*addr++ = *baddr;
		baddr += 4;
	}

	G2_UNLOCK();
}

void
g2bus_sparse_bus_mem_write_region_1(void *v, bus_space_handle_t sh,
    bus_size_t off, const uint8_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile uint8_t *baddr = (uint8_t *)(sh + (off * 4));

	G2_LOCK();

	while (len--) {
		*baddr = *addr++;
		baddr += 4;
	}

	G2_UNLOCK();
}

void
g2bus_sparse_bus_mem_read_multi_1(void *v, bus_space_handle_t sh,
    bus_size_t off, uint8_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile const uint8_t *baddr = (uint8_t *)(sh + (off * 4));

	G2_LOCK();

	while (len--)
		*addr++ = *baddr;

	G2_UNLOCK();
}

void
g2bus_sparse_bus_mem_write_multi_1(void *v, bus_space_handle_t sh,
    bus_size_t off, const uint8_t *addr, bus_size_t len)
{
	G2LOCK_DECL;
	volatile uint8_t *baddr = (uint8_t *)(sh + (off * 4));

	G2_LOCK();

	while (len--)
		*baddr = *addr++;

	G2_UNLOCK();
}
