/*     $NetBSD: bus.h,v 1.11.2.1 2001/10/01 12:39:10 fvdl Exp $   */

/*-
 * Copyright (c) 1996, 1997, 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#ifndef _HPCMIPS_BUS_H_
#define _HPCMIPS_BUS_H_

#include <mips/locore.h>

#ifdef BUS_SPACE_DEBUG
#include <sys/systm.h> /* for printf() prototype */
/*
 * Macros for checking the aligned-ness of pointers passed to bus
 * space ops.  Strict alignment is required by the MIPS architecture,
 * and a trap will occur if unaligned access is performed.  These
 * may aid in the debugging of a broken device driver by displaying
 * useful information about the problem.
 */
#define	__BUS_SPACE_ALIGNED_ADDRESS(p, t)				\
	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	__BUS_SPACE_ADDRESS_SANITY(p, t, d)				\
({									\
	if (__BUS_SPACE_ALIGNED_ADDRESS((p), t) == 0) {			\
		printf("%s 0x%lx not aligned to %d bytes %s:%d\n",	\
		    d, (u_long)(p), sizeof(t), __FILE__, __LINE__);	\
	}								\
	(void) 0;							\
})

#define BUS_SPACE_ALIGNED_POINTER(p, t) __BUS_SPACE_ALIGNED_ADDRESS(p, t)
#else
#define	__BUS_SPACE_ADDRESS_SANITY(p,t,d)	(void) 0
#define BUS_SPACE_ALIGNED_POINTER(p, t) ALIGNED_POINTER(p, t)
#endif /* BUS_SPACE_DEBUG */
/*
 * Utility macros; do not use outside this file.
 */
#define	__PB_TYPENAME_PREFIX(BITS)	___CONCAT(u_int,BITS)
#define	__PB_TYPENAME(BITS)	___CONCAT(__PB_TYPENAME_PREFIX(BITS),_t)

/*
 * Bus address and size types
 */

typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
typedef struct hpcmips_bus_space	*bus_space_tag_t;
typedef u_int32_t bus_space_handle_t;
/*
 * Initialize extent.
 */
bus_space_tag_t hpcmips_system_bus_space(void);
void hpcmips_init_bus_space_extent(bus_space_tag_t);
bus_space_tag_t hpcmips_alloc_bus_space_tag(void);

struct hpcmips_bus_space {
    char t_name[16];  /* bus name */
    u_int32_t t_base; /* extent base */
    u_int32_t t_size; /* extent size */
    void *t_extent;
};


/*
 *	int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp);
 *
 * Map a region of bus space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE		0x04

int	bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *);

/*
 *	void bus_space_unmap(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Unmap a region of bus space.
 */

void	bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);

/*
 *	int bus_space_subregion(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp);
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

int	bus_space_subregion(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_size_t, bus_space_handle_t *);

/*
 *	int bus_space_alloc(bus_space_tag_t t, bus_addr_t, rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flags, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp);
 *
 * Allocate a region of bus space.
 */

int	bus_space_alloc(bus_space_tag_t, bus_addr_t, bus_addr_t, bus_size_t,
	    bus_size_t, bus_size_t, int, bus_addr_t *, bus_space_handle_t *);

/*
 *	int bus_space_free(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size);
 *
 * Free a region of bus space.
 */

void	bus_space_free(bus_space_tag_t, bus_space_handle_t, bus_size_t);

/*
 *	u_intN_t bus_space_read_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset);
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define	bus_space_read_1(t, h, o)					\
    (wbflush(),						/* XXX */	\
     (void) t, (*(volatile u_int8_t *)((h) + (o))))

#define	bus_space_read_2(t, h, o)					\
    (wbflush(),						/* XXX */	\
     (void) t, (*(volatile u_int16_t *)((h) + (o))))

#define	bus_space_read_4(t, h, o)					\
    (wbflush(),						/* XXX */	\
     (void) t, (*(volatile u_int32_t *)((h) + (o))))

#if 0	/* Cause a link error for bus_space_read_8 */
#define	bus_space_read_8(t, h, o)	!!! bus_space_read_8 unimplemented !!!
#endif

/*
 *	void bus_space_read_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

#define __HPCMIPS_bus_space_read_multi(BYTES,BITS)			\
static __inline void __CONCAT(bus_space_read_multi_,BYTES)		\
	(bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	__PB_TYPENAME(BITS) *, size_t);					\
									\
static __inline void							\
__CONCAT(bus_space_read_multi_,BYTES)(bus_space_tag_t t,		\
    bus_space_handle_t h, bus_size_t o, __PB_TYPENAME(BITS) *a,		\
    size_t c)								\
{									\
									\
	while (c--)							\
		*a++ = __CONCAT(bus_space_read_,BYTES)(t, h, o);	\
}

__HPCMIPS_bus_space_read_multi(1,8)
__HPCMIPS_bus_space_read_multi(2,16)
__HPCMIPS_bus_space_read_multi(4,32)

#if 0	/* Cause a link error for bus_space_read_multi_8 */
#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 unimplemented !!!
#endif

#undef __HPCMIPS_bus_space_read_multi

/*
 *	void bus_space_read_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count);
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

#define __HPCMIPS_bus_space_read_region(BYTES,BITS)			\
static __inline void __CONCAT(bus_space_read_region_,BYTES)		\
	(bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	__PB_TYPENAME(BITS) *, size_t);			       		\
									\
static __inline void							\
__CONCAT(bus_space_read_region_,BYTES)(bus_space_tag_t t,		\
    bus_space_handle_t h, bus_size_t o, __PB_TYPENAME(BITS) *a,		\
    size_t c)								\
{									\
									\
	while (c--) {							\
		*a++ = __CONCAT(bus_space_read_,BYTES)(t, h, o);	\
		o += BYTES;						\
	}								\
}

__HPCMIPS_bus_space_read_region(1,8)
__HPCMIPS_bus_space_read_region(2,16)
__HPCMIPS_bus_space_read_region(4,32)

#if 0	/* Cause a link error for bus_space_read_region_8 */
#define	bus_space_read_region_8	!!! bus_space_read_region_8 unimplemented !!!
#endif

#undef __HPCMIPS_bus_space_read_region

/*
 *	void bus_space_write_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value);
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

#define	bus_space_write_1(t, h, o, v)					\
do {									\
	(void) t;							\
	*(volatile u_int8_t *)((h) + (o)) = (v);			\
	wbflush();					/* XXX */	\
} while (0)

#define	bus_space_write_2(t, h, o, v)					\
do {									\
	(void) t;							\
	*(volatile u_int16_t *)((h) + (o)) = (v);			\
	wbflush();					/* XXX */	\
} while (0)

#define	bus_space_write_4(t, h, o, v)					\
do {									\
	(void) t;							\
	*(volatile u_int32_t *)((h) + (o)) = (v);			\
	wbflush();					/* XXX */	\
} while (0)

#if 0	/* Cause a link error for bus_space_write_8 */
#define	bus_space_write_8	!!! bus_space_write_8 not implemented !!!
#endif

/*
 *	void bus_space_write_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

#define __HPCMIPS_bus_space_write_multi(BYTES,BITS)			\
static __inline void __CONCAT(bus_space_write_multi_,BYTES)		\
	(bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	__PB_TYPENAME(BITS) *, size_t);					\
									\
static __inline void							\
__CONCAT(bus_space_write_multi_,BYTES)(bus_space_tag_t t,		\
    bus_space_handle_t h, bus_size_t o, __PB_TYPENAME(BITS) *a,		\
    size_t c)								\
{									\
									\
	while (c--)							\
		__CONCAT(bus_space_write_,BYTES)(t, h, o, *a++);	\
}

__HPCMIPS_bus_space_write_multi(1,8)
__HPCMIPS_bus_space_write_multi(2,16)
__HPCMIPS_bus_space_write_multi(4,32)

#if 0	/* Cause a link error for bus_space_write_8 */
#define	bus_space_write_multi_8(t, h, o, a, c)				\
			!!! bus_space_write_multi_8 unimplimented !!!
#endif

#undef __HPCMIPS_bus_space_write_multi

/*
 *	void bus_space_write_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */

#define __HPCMIPS_bus_space_write_region(BYTES,BITS)			\
static __inline void __CONCAT(bus_space_write_region_,BYTES)		\
	(bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	__PB_TYPENAME(BITS) *, size_t);					\
									\
static __inline void							\
__CONCAT(bus_space_write_region_,BYTES)(bus_space_tag_t t,		\
    bus_space_handle_t h, bus_size_t o, __PB_TYPENAME(BITS) *a,		\
    size_t c)								\
{									\
									\
	while (c--) {							\
		__CONCAT(bus_space_write_,BYTES)(t, h, o, *a++);	\
		o += BYTES;						\
	}								\
}

__HPCMIPS_bus_space_write_region(1,8)
__HPCMIPS_bus_space_write_region(2,16)
__HPCMIPS_bus_space_write_region(4,32)

#if 0	/* Cause a link error for bus_space_write_region_8 */
#define	bus_space_write_region_8					\
			!!! bus_space_write_region_8 unimplemented !!!
#endif

#undef __HPCMIPS_bus_space_write_region

/*
 *	void bus_space_set_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

#define __HPCMIPS_bus_space_set_multi(BYTES,BITS)			\
static __inline void __CONCAT(bus_space_set_multi_,BYTES)		\
	(bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	__PB_TYPENAME(BITS), size_t);					\
									\
static __inline void							\
__CONCAT(bus_space_set_multi_,BYTES)(bus_space_tag_t t,			\
    bus_space_handle_t h, bus_size_t o, __PB_TYPENAME(BITS) v, size_t c)\
{									\
									\
	while (c--)							\
		__CONCAT(bus_space_write_,BYTES)(t, h, o, v);		\
}

__HPCMIPS_bus_space_set_multi(1,8)
__HPCMIPS_bus_space_set_multi(2,16)
__HPCMIPS_bus_space_set_multi(4,32)

#if 0	/* Cause a link error for bus_space_set_multi_8 */
#define	bus_space_set_multi_8						\
			!!! bus_space_set_multi_8 unimplemented !!!
#endif

#undef __HPCMIPS_bus_space_set_multi

/*
 *	void bus_space_set_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count);
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

#define __HPCMIPS_bus_space_set_region(BYTES,BITS)			\
static __inline void __CONCAT(bus_space_set_region_,BYTES)		\
	(bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	__PB_TYPENAME(BITS), size_t);					\
									\
static __inline void							\
__CONCAT(bus_space_set_region_,BYTES)(bus_space_tag_t t,		\
    bus_space_handle_t h, bus_size_t o, __PB_TYPENAME(BITS) v, size_t c)\
{									\
									\
	while (c--) {							\
		__CONCAT(bus_space_write_,BYTES)(t, h, o, v);		\
		o += BYTES;						\
	}								\
}

__HPCMIPS_bus_space_set_region(1,8)
__HPCMIPS_bus_space_set_region(2,16)
__HPCMIPS_bus_space_set_region(4,32)

#if 0	/* Cause a link error for bus_space_set_region_8 */
#define	bus_space_set_region_8						\
			!!! bus_space_set_region_8 unimplemented !!!
#endif

#undef __HPCMIPS_bus_space_set_region

/*
 *	void bus_space_copy_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh1, bus_size_t off1,
 *	    bus_space_handle_t bsh2, bus_size_t off2,
 *	    bus_size_t count);
 *
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

#define	__HPCMIPS_copy_region(BYTES)					\
static __inline void __CONCAT(bus_space_copy_region_,BYTES)		\
	(bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	    bus_space_handle_t, bus_size_t, bus_size_t);		\
									\
static __inline void							\
__CONCAT(bus_space_copy_region_,BYTES)(bus_space_tag_t t,		\
    bus_space_handle_t h1, bus_size_t o1, bus_space_handle_t h2, 	\
    bus_size_t o2, bus_size_t c)					\
{									\
	bus_size_t o;							\
									\
	if ((h1 + o1) >= (h2 + o2)) {					\
		/* src after dest: copy forward */			\
		for (o = 0; c != 0; c--, o += BYTES)			\
			__CONCAT(bus_space_write_,BYTES)(t, h2, o2 + o,	\
			    __CONCAT(bus_space_read_,BYTES)(t, h1, o1 + o)); \
	} else {							\
		/* dest after src: copy backwards */			\
		for (o = (c - 1) * BYTES; c != 0; c--, o -= BYTES)	\
			__CONCAT(bus_space_write_,BYTES)(t, h2, o2 + o,	\
			    __CONCAT(bus_space_read_,BYTES)(t, h1, o1 + o)); \
	}								\
}

__HPCMIPS_copy_region(1)
__HPCMIPS_copy_region(2)
__HPCMIPS_copy_region(4)

#if 0	/* Cause a link error for bus_space_copy_region_8 */
#define	bus_space_copy_region_8						\
			!!! bus_space_copy_region_8 unimplemented !!!
#endif

#undef __HPCMIPS_copy_region

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags);
 *
 * On the MIPS, we just flush the write buffer.
 */
#define	bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)),	\
	 wbflush())
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

#undef __PB_TYPENAME_PREFIX
#undef __PB_TYPENAME

/*
 * Bus stream operations--defined in terms of non-stream counterparts
 */
#define __BUS_SPACE_HAS_STREAM_METHODS 1
#define bus_space_read_stream_1 bus_space_read_1
#define bus_space_read_stream_2 bus_space_read_2
#define bus_space_read_stream_4 bus_space_read_4
#define	bus_space_read_stream_8 bus_space_read_8
#define bus_space_read_multi_stream_1 bus_space_read_multi_1
#define bus_space_read_multi_stream_2 bus_space_read_multi_2
#define bus_space_read_multi_stream_4 bus_space_read_multi_4
#define	bus_space_read_multi_stream_8 bus_space_read_multi_8
#define bus_space_read_region_stream_1 bus_space_read_region_1
#define bus_space_read_region_stream_2 bus_space_read_region_2
#define bus_space_read_region_stream_4 bus_space_read_region_4
#define	bus_space_read_region_stream_8 bus_space_read_region_8
#define bus_space_write_stream_1 bus_space_write_1
#define bus_space_write_stream_2 bus_space_write_2
#define bus_space_write_stream_4 bus_space_write_4
#define	bus_space_write_stream_8 bus_space_write_8
#define bus_space_write_multi_stream_1 bus_space_write_multi_1
#define bus_space_write_multi_stream_2 bus_space_write_multi_2
#define bus_space_write_multi_stream_4 bus_space_write_multi_4
#define	bus_space_write_multi_stream_8 bus_space_write_multi_8
#define bus_space_write_region_stream_1 bus_space_write_region_1
#define bus_space_write_region_stream_2 bus_space_write_region_2
#define bus_space_write_region_stream_4 bus_space_write_region_4
#define	bus_space_write_region_stream_8	bus_space_write_region_8

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x004	/* hint: map memory DMA coherent */
#define	BUS_DMA_STREAMING	0x008	/* hint: sequential, unidirectional */
#define	BUS_DMA_BUS1		0x010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x020
#define	BUS_DMA_BUS3		0x040
#define	BUS_DMA_BUS4		0x080
#define	BUS_DMA_READ		0x100	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x200	/* mapping is memory -> device only */

#define	HPCMIPS_DMAMAP_COHERENT	0x100	/* no cache flush necessary on sync */

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

/*
 * Operations performed by bus_dmamap_sync().
 */
#define	BUS_DMASYNC_PREREAD	0x01	/* pre-read synchronization */
#define	BUS_DMASYNC_POSTREAD	0x02	/* post-read synchronization */
#define	BUS_DMASYNC_PREWRITE	0x04	/* pre-write synchronization */
#define	BUS_DMASYNC_POSTWRITE	0x08	/* post-write synchronization */

typedef struct hpcmips_bus_dma_tag	*bus_dma_tag_t;
typedef struct hpcmips_bus_dmamap	*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct hpcmips_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
	bus_addr_t	_ds_vaddr;	/* virtual address, 0 if invalid */
};
typedef struct hpcmips_bus_dma_segment	bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */

struct hpcmips_bus_dma_tag {
	/*
	 * DMA mapping methods.
	 */
	int	(*_dmamap_create)(bus_dma_tag_t, bus_size_t, int,
		    bus_size_t, bus_size_t, int, bus_dmamap_t *);
	void	(*_dmamap_destroy)(bus_dma_tag_t, bus_dmamap_t);
	int	(*_dmamap_load)(bus_dma_tag_t, bus_dmamap_t, void *,
		    bus_size_t, struct proc *, int);
	int	(*_dmamap_load_mbuf)(bus_dma_tag_t, bus_dmamap_t,
		    struct mbuf *, int);
	int	(*_dmamap_load_uio)(bus_dma_tag_t, bus_dmamap_t,
		    struct uio *, int);
	int	(*_dmamap_load_raw)(bus_dma_tag_t, bus_dmamap_t,
		    bus_dma_segment_t *, int, bus_size_t, int);
	void	(*_dmamap_unload)(bus_dma_tag_t, bus_dmamap_t);
	void	(*_dmamap_sync)(bus_dma_tag_t, bus_dmamap_t,
		    bus_addr_t, bus_size_t, int);

	/*
	 * DMA memory utility functions.
	 */
	int	(*_dmamem_alloc)(bus_dma_tag_t, bus_size_t, bus_size_t,
		    bus_size_t, bus_dma_segment_t *, int, int *, int);
	void	(*_dmamem_free)(bus_dma_tag_t, bus_dma_segment_t *, int);
	int	(*_dmamem_map)(bus_dma_tag_t, bus_dma_segment_t *,
		    int, size_t, caddr_t *, int);
	void	(*_dmamem_unmap)(bus_dma_tag_t, caddr_t, size_t);
	paddr_t	(*_dmamem_mmap)(bus_dma_tag_t, bus_dma_segment_t *,
		    int, off_t, int, int);

	void	*_dmamap_chipset_v;
};

#define	bus_dmamap_create(t, s, n, m, b, f, p)			\
	(*(t)->_dmamap_create)((t), (s), (n), (m), (b), (f), (p))
#define	bus_dmamap_destroy(t, p)				\
	(*(t)->_dmamap_destroy)((t), (p))
#define	bus_dmamap_load(t, m, b, s, p, f)			\
	(*(t)->_dmamap_load)((t), (m), (b), (s), (p), (f))
#define	bus_dmamap_load_mbuf(t, m, b, f)			\
	(*(t)->_dmamap_load_mbuf)((t), (m), (b), (f))
#define	bus_dmamap_load_uio(t, m, u, f)				\
	(*(t)->_dmamap_load_uio)((t), (m), (u), (f))
#define	bus_dmamap_load_raw(t, m, sg, n, s, f)			\
	(*(t)->_dmamap_load_raw)((t), (m), (sg), (n), (s), (f))
#define	bus_dmamap_unload(t, p)					\
	(*(t)->_dmamap_unload)((t), (p))
#define	bus_dmamap_sync(t, p, o, l, ops)			\
	(*(t)->_dmamap_sync)((t), (p), (o), (l), (ops))

#define	bus_dmamem_alloc(t, s, a, b, sg, n, r, f)		\
	(*(t)->_dmamem_alloc)((t), (s), (a), (b), (sg), (n), (r), (f))
#define	bus_dmamem_free(t, sg, n)				\
	(*(t)->_dmamem_free)((t), (sg), (n))
#define	bus_dmamem_map(t, sg, n, s, k, f)			\
	(*(t)->_dmamem_map)((t), (sg), (n), (s), (k), (f))
#define	bus_dmamem_unmap(t, k, s)				\
	(*(t)->_dmamem_unmap)((t), (k), (s))
#define	bus_dmamem_mmap(t, sg, n, o, p, f)			\
	(*(t)->_dmamem_mmap)((t), (sg), (n), (o), (p), (f))

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct hpcmips_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use my machine-independent code.
	 */
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxsegsz;	/* largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */
	int		_dm_flags;	/* misc. flags */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

#ifdef _HPCMIPS_BUS_DMA_PRIVATE
int	_bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	_bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	_bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
	    struct proc *, int);
int	_bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *, int);
int	_bus_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t, struct uio *, int);
int	_bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *,
	    int, bus_size_t, int);
void	_bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	_bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t, bus_size_t,
	    int);
int	_bus_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
	    bus_dma_segment_t *, int, int *, int);
void	_bus_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int	_bus_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t,
	    caddr_t *, int);
void	_bus_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);
paddr_t	_bus_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *, int, off_t, int,
	    int);
int	_bus_dmamem_alloc_range(bus_dma_tag_t, bus_size_t, bus_size_t,
	    bus_size_t, bus_dma_segment_t *, int, int *, int, vaddr_t,
	    vaddr_t);

extern struct hpcmips_bus_dma_tag hpcmips_default_bus_dma_tag;
#endif /* _HPCMIPS_BUS_DMA_PRIVATE */

#endif /* _HPCMIPS_BUS_H_ */
