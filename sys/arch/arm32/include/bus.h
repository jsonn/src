/*	$NetBSD: bus.h,v 1.16.4.1 1999/06/21 00:47:33 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _ARM32_BUS_H_
#define _ARM32_BUS_H_

/*
 * Addresses (in bus space).
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access methods for bus space.
 */
typedef struct bus_space *bus_space_tag_t;
typedef u_long bus_space_handle_t;

/*
 *	int bus_space_map  __P((bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp));
 *
 * Map a region of bus space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02

struct bus_space {
	/* cookie */
	void		*bs_cookie;

	/* mapping/unmapping */
	int		(*bs_map) __P((void *, bus_addr_t, bus_size_t,
			    int, bus_space_handle_t *));
	void		(*bs_unmap) __P((void *, bus_space_handle_t,
			    bus_size_t));
	int		(*bs_subregion) __P((void *, bus_space_handle_t,
			    bus_size_t, bus_size_t, bus_space_handle_t *));

	/* allocation/deallocation */
	int		(*bs_alloc) __P((void *, bus_addr_t, bus_addr_t,
			    bus_size_t, bus_size_t, bus_size_t, int,
			    bus_addr_t *, bus_space_handle_t *));
	void		(*bs_free) __P((void *, bus_space_handle_t,
			    bus_size_t));

	/* barrier */
	void		(*bs_barrier) __P((void *, bus_space_handle_t,
			    bus_size_t, bus_size_t, int));

	/* read (single) */
	u_int8_t	(*bs_r_1) __P((void *, bus_space_handle_t,
			    bus_size_t));
	u_int16_t	(*bs_r_2) __P((void *, bus_space_handle_t,
			    bus_size_t));
	u_int32_t	(*bs_r_4) __P((void *, bus_space_handle_t,
			    bus_size_t));
	u_int64_t	(*bs_r_8) __P((void *, bus_space_handle_t,
			    bus_size_t));

	/* read multiple */
	void		(*bs_rm_1) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int8_t *, bus_size_t));
	void		(*bs_rm_2) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int16_t *, bus_size_t));
	void		(*bs_rm_4) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int32_t *, bus_size_t));
	void		(*bs_rm_8) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int64_t *, bus_size_t));
					
	/* read region */
	void		(*bs_rr_1) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int8_t *, bus_size_t));
	void		(*bs_rr_2) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int16_t *, bus_size_t));
	void		(*bs_rr_4) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int32_t *, bus_size_t));
	void		(*bs_rr_8) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int64_t *, bus_size_t));
					
	/* write (single) */
	void		(*bs_w_1) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int8_t));
	void		(*bs_w_2) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int16_t));
	void		(*bs_w_4) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int32_t));
	void		(*bs_w_8) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int64_t));

	/* write multiple */
	void		(*bs_wm_1) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int8_t *, bus_size_t));
	void		(*bs_wm_2) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int16_t *, bus_size_t));
	void		(*bs_wm_4) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int32_t *, bus_size_t));
	void		(*bs_wm_8) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int64_t *, bus_size_t));
					
	/* write region */
	void		(*bs_wr_1) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int8_t *, bus_size_t));
	void		(*bs_wr_2) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int16_t *, bus_size_t));
	void		(*bs_wr_4) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int32_t *, bus_size_t));
	void		(*bs_wr_8) __P((void *, bus_space_handle_t,
			    bus_size_t, const u_int64_t *, bus_size_t));

	/* set multiple */
	void		(*bs_sm_1) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int8_t, bus_size_t));
	void		(*bs_sm_2) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int16_t, bus_size_t));
	void		(*bs_sm_4) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int32_t, bus_size_t));
	void		(*bs_sm_8) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int64_t, bus_size_t));

	/* set region */
	void		(*bs_sr_1) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int8_t, bus_size_t));
	void		(*bs_sr_2) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int16_t, bus_size_t));
	void		(*bs_sr_4) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int32_t, bus_size_t));
	void		(*bs_sr_8) __P((void *, bus_space_handle_t,
			    bus_size_t, u_int64_t, bus_size_t));

	/* copy */
	void		(*bs_c_1) __P((void *, bus_space_handle_t, bus_size_t,
			    bus_space_handle_t, bus_size_t, bus_size_t));
	void		(*bs_c_2) __P((void *, bus_space_handle_t, bus_size_t,
			    bus_space_handle_t, bus_size_t, bus_size_t));
	void		(*bs_c_4) __P((void *, bus_space_handle_t, bus_size_t,
			    bus_space_handle_t, bus_size_t, bus_size_t));
	void		(*bs_c_8) __P((void *, bus_space_handle_t, bus_size_t,
			    bus_space_handle_t, bus_size_t, bus_size_t));

};


/*
 * Utility macros; INTERNAL USE ONLY.
 */
#define	__bs_c(a,b)		__CONCAT(a,b)
#define	__bs_opname(op,size)	__bs_c(__bs_c(__bs_c(bs_,op),_),size)

#define	__bs_rs(sz, t, h, o)						\
	(*(t)->__bs_opname(r,sz))((t)->bs_cookie, h, o)
#define	__bs_ws(sz, t, h, o, v)						\
	(*(t)->__bs_opname(w,sz))((t)->bs_cookie, h, o, v)
#define	__bs_nonsingle(type, sz, t, h, o, a, c)				\
	(*(t)->__bs_opname(type,sz))((t)->bs_cookie, h, o, a, c)
#define	__bs_set(type, sz, t, h, o, v, c)				\
	(*(t)->__bs_opname(type,sz))((t)->bs_cookie, h, o, v, c)
#define	__bs_copy(sz, t, h1, o1, h2, o2, cnt)				\
	(*(t)->__bs_opname(c,sz))((t)->bs_cookie, h1, o1, h2, o2, cnt)


/*
 * Mapping and unmapping operations.
 */
#define	bus_space_map(t, a, s, c, hp)					\
	(*(t)->bs_map)((t)->bs_cookie, (a), (s), (c), (hp))
#define	bus_space_unmap(t, h, s)					\
	(*(t)->bs_unmap)((t)->bs_cookie, (h), (s))
#define	bus_space_subregion(t, h, o, s, hp)				\
	(*(t)->bs_subregion)((t)->bs_cookie, (h), (o), (s), (hp))


/*
 * Allocation and deallocation operations.
 */
#define	bus_space_alloc(t, rs, re, s, a, b, c, ap, hp)			\
	(*(t)->bs_alloc)((t)->bs_cookie, (rs), (re), (s), (a), (b),	\
	    (c), (ap), (hp))
#define	bus_space_free(t, h, s)						\
	(*(t)->bs_free)((t)->bs_cookie, (h), (s))

/*
 * Bus barrier operations.
 */
#define	bus_space_barrier(t, h, o, l, f)				\
	(*(t)->bs_barrier)((t)->bs_cookie, (h), (o), (l), (f))

#define	BUS_SPACE_BARRIER_READ	0x01
#define	BUS_SPACE_BARRIER_WRITE	0x02

/*
 * Bus read (single) operations.
 */
#define	bus_space_read_1(t, h, o)	__bs_rs(1,(t),(h),(o))
#define	bus_space_read_2(t, h, o)	__bs_rs(2,(t),(h),(o))
#define	bus_space_read_4(t, h, o)	__bs_rs(4,(t),(h),(o))
#define	bus_space_read_8(t, h, o)	__bs_rs(8,(t),(h),(o))


/*
 * Bus read multiple operations.
 */
#define	bus_space_read_multi_1(t, h, o, a, c)				\
	__bs_nonsingle(rm,1,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_2(t, h, o, a, c)				\
	__bs_nonsingle(rm,2,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_4(t, h, o, a, c)				\
	__bs_nonsingle(rm,4,(t),(h),(o),(a),(c))
#define	bus_space_read_multi_8(t, h, o, a, c)				\
	__bs_nonsingle(rm,8,(t),(h),(o),(a),(c))


/*
 * Bus read region operations.
 */
#define	bus_space_read_region_1(t, h, o, a, c)				\
	__bs_nonsingle(rr,1,(t),(h),(o),(a),(c))
#define	bus_space_read_region_2(t, h, o, a, c)				\
	__bs_nonsingle(rr,2,(t),(h),(o),(a),(c))
#define	bus_space_read_region_4(t, h, o, a, c)				\
	__bs_nonsingle(rr,4,(t),(h),(o),(a),(c))
#define	bus_space_read_region_8(t, h, o, a, c)				\
	__bs_nonsingle(rr,8,(t),(h),(o),(a),(c))


/*
 * Bus write (single) operations.
 */
#define	bus_space_write_1(t, h, o, v)	__bs_ws(1,(t),(h),(o),(v))
#define	bus_space_write_2(t, h, o, v)	__bs_ws(2,(t),(h),(o),(v))
#define	bus_space_write_4(t, h, o, v)	__bs_ws(4,(t),(h),(o),(v))
#define	bus_space_write_8(t, h, o, v)	__bs_ws(8,(t),(h),(o),(v))


/*
 * Bus write multiple operations.
 */
#define	bus_space_write_multi_1(t, h, o, a, c)				\
	__bs_nonsingle(wm,1,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_2(t, h, o, a, c)				\
	__bs_nonsingle(wm,2,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_4(t, h, o, a, c)				\
	__bs_nonsingle(wm,4,(t),(h),(o),(a),(c))
#define	bus_space_write_multi_8(t, h, o, a, c)				\
	__bs_nonsingle(wm,8,(t),(h),(o),(a),(c))


/*
 * Bus write region operations.
 */
#define	bus_space_write_region_1(t, h, o, a, c)				\
	__bs_nonsingle(wr,1,(t),(h),(o),(a),(c))
#define	bus_space_write_region_2(t, h, o, a, c)				\
	__bs_nonsingle(wr,2,(t),(h),(o),(a),(c))
#define	bus_space_write_region_4(t, h, o, a, c)				\
	__bs_nonsingle(wr,4,(t),(h),(o),(a),(c))
#define	bus_space_write_region_8(t, h, o, a, c)				\
	__bs_nonsingle(wr,8,(t),(h),(o),(a),(c))


/*
 * Set multiple operations.
 */
#define	bus_space_set_multi_1(t, h, o, v, c)				\
	__bs_set(sm,1,(t),(h),(o),(v),(c))
#define	bus_space_set_multi_2(t, h, o, v, c)				\
	__bs_set(sm,2,(t),(h),(o),(v),(c))
#define	bus_space_set_multi_4(t, h, o, v, c)				\
	__bs_set(sm,4,(t),(h),(o),(v),(c))
#define	bus_space_set_multi_8(t, h, o, v, c)				\
	__bs_set(sm,8,(t),(h),(o),(v),(c))


/*
 * Set region operations.
 */
#define	bus_space_set_region_1(t, h, o, v, c)				\
	__bs_set(sr,1,(t),(h),(o),(v),(c))
#define	bus_space_set_region_2(t, h, o, v, c)				\
	__bs_set(sr,2,(t),(h),(o),(v),(c))
#define	bus_space_set_region_4(t, h, o, v, c)				\
	__bs_set(sr,4,(t),(h),(o),(v),(c))
#define	bus_space_set_region_8(t, h, o, v, c)				\
	__bs_set(sr,8,(t),(h),(o),(v),(c))


/*
 * Copy operations.
 */
#define	bus_space_copy_region_1(t, h1, o1, h2, o2, c)				\
	__bs_copy(1, t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_2(t, h1, o1, h2, o2, c)				\
	__bs_copy(2, t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_4(t, h1, o1, h2, o2, c)				\
	__bs_copy(4, t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_8(t, h1, o1, h2, o2, c)				\
	__bs_copy(8, t, h1, o1, h2, o2, c)

/*
 * Macros to provide prototypes for all the functions used in the
 * bus_space structure
 */

#define bs_map_proto(f)							\
int	__bs_c(f,_bs_map) __P((void *t, bus_addr_t addr,		\
	    bus_size_t size, int cacheable, bus_space_handle_t *bshp));

#define bs_unmap_proto(f)						\
void	__bs_c(f,_bs_unmap) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t size));

#define bs_subregion_proto(f)						\
int	__bs_c(f,_bs_subregion) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, bus_size_t size, 			\
	    bus_space_handle_t *nbshp));

#define bs_alloc_proto(f)						\
int	__bs_c(f,_bs_alloc) __P((void *t, bus_addr_t rstart,		\
	    bus_addr_t rend, bus_size_t size, bus_size_t align,		\
	    bus_size_t boundary, int cacheable, bus_addr_t *addrp,	\
	    bus_space_handle_t *bshp));

#define bs_free_proto(f)						\
void	__bs_c(f,_bs_free) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t size));

#define bs_barrier_proto(f)						\
void	__bs_c(f,_bs_barrier) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, bus_size_t len, int flags));

#define	bs_r_1_proto(f)							\
u_int8_t	__bs_c(f,_bs_r_1) __P((void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset));

#define	bs_r_2_proto(f)							\
u_int16_t	__bs_c(f,_bs_r_2) __P((void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset));

#define	bs_r_4_proto(f)							\
u_int32_t	__bs_c(f,_bs_r_4) __P((void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset));

#define	bs_r_8_proto(f)							\
u_int64_t	__bs_c(f,_bs_r_8) __P((void *t, bus_space_handle_t bsh,	\
		    bus_size_t offset));

#define	bs_w_1_proto(f)							\
void	__bs_c(f,_bs_w_1) __P((void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, u_int8_t value));

#define	bs_w_2_proto(f)							\
void	__bs_c(f,_bs_w_2) __P((void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, u_int16_t value));

#define	bs_w_4_proto(f)							\
void	__bs_c(f,_bs_w_4) __P((void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, u_int32_t value));

#define	bs_w_8_proto(f)							\
void	__bs_c(f,_bs_w_8) __P((void *t, bus_space_handle_t bsh,		\
	    bus_size_t offset, u_int64_t value));

#define	bs_rm_1_proto(f)						\
void	__bs_c(f,_bs_rm_1) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int8_t *addr, bus_size_t count));

#define	bs_rm_2_proto(f)						\
void	__bs_c(f,_bs_rm_2) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int16_t *addr, bus_size_t count));

#define	bs_rm_4_proto(f)						\
void	__bs_c(f,_bs_rm_4) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int32_t *addr, bus_size_t count));		

#define	bs_rm_8_proto(f)						\
void	__bs_c(f,_bs_rm_8) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int64_t *addr, bus_size_t count));

#define	bs_wm_1_proto(f)						\
void	__bs_c(f,_bs_wm_1) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int8_t *addr, bus_size_t count));

#define	bs_wm_2_proto(f)						\
void	__bs_c(f,_bs_wm_2) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int16_t *addr, bus_size_t count));

#define	bs_wm_4_proto(f)						\
void	__bs_c(f,_bs_wm_4) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int32_t *addr, bus_size_t count));

#define	bs_wm_8_proto(f)						\
void	__bs_c(f,_bs_wm_8) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int64_t *addr, bus_size_t count));

#define	bs_rr_1_proto(f)						\
void	__bs_c(f, _bs_rr_1) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int8_t *addr, bus_size_t count));

#define	bs_rr_2_proto(f)						\
void	__bs_c(f, _bs_rr_2) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int16_t *addr, bus_size_t count));

#define	bs_rr_4_proto(f)						\
void	__bs_c(f, _bs_rr_4) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int32_t *addr, bus_size_t count));

#define	bs_rr_8_proto(f)						\
void	__bs_c(f, _bs_rr_8) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int64_t *addr, bus_size_t count));

#define	bs_wr_1_proto(f)						\
void	__bs_c(f, _bs_wr_1) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int8_t *addr, bus_size_t count));

#define	bs_wr_2_proto(f)						\
void	__bs_c(f, _bs_wr_2) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int16_t *addr, bus_size_t count));

#define	bs_wr_4_proto(f)						\
void	__bs_c(f, _bs_wr_4) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int32_t *addr, bus_size_t count));

#define	bs_wr_8_proto(f)						\
void	__bs_c(f, _bs_wr_8) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, const u_int64_t *addr, bus_size_t count));

#define	bs_sm_1_proto(f)						\
void	__bs_c(f,_bs_sm_1) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int8_t value, bus_size_t count));

#define	bs_sm_2_proto(f)						\
void	__bs_c(f,_bs_sm_2) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int16_t value, bus_size_t count));

#define	bs_sm_4_proto(f)						\
void	__bs_c(f,_bs_sm_4) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int32_t value, bus_size_t count));

#define	bs_sm_8_proto(f)						\
void	__bs_c(f,_bs_sm_8) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int64_t value, bus_size_t count));

#define	bs_sr_1_proto(f)						\
void	__bs_c(f,_bs_sr_1) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int8_t value, bus_size_t count));

#define	bs_sr_2_proto(f)						\
void	__bs_c(f,_bs_sr_2) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int16_t value, bus_size_t count));

#define	bs_sr_4_proto(f)						\
void	__bs_c(f,_bs_sr_4) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int32_t value, bus_size_t count));

#define	bs_sr_8_proto(f)						\
void	__bs_c(f,_bs_sr_8) __P((void *t, bus_space_handle_t bsh,	\
	    bus_size_t offset, u_int64_t value, bus_size_t count));

#define	bs_c_1_proto(f)							\
void	__bs_c(f,_bs_c_1) __P((void *t, bus_space_handle_t bsh1,	\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count));

#define	bs_c_2_proto(f)							\
void	__bs_c(f,_bs_c_2) __P((void *t, bus_space_handle_t bsh1,	\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count));

#define	bs_c_4_proto(f)							\
void	__bs_c(f,_bs_c_4) __P((void *t, bus_space_handle_t bsh1,	\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count));

#define	bs_c_8_proto(f)							\
void	__bs_c(f,_bs_c_8) __P((void *t, bus_space_handle_t bsh1,	\
	    bus_size_t offset1, bus_space_handle_t bsh2,		\
	    bus_size_t offset2, bus_size_t count));

#define bs_protos(f)		\
bs_map_proto(f);		\
bs_unmap_proto(f);		\
bs_subregion_proto(f);		\
bs_alloc_proto(f);		\
bs_free_proto(f);		\
bs_barrier_proto(f);		\
bs_r_1_proto(f);		\
bs_r_2_proto(f);		\
bs_r_4_proto(f);		\
bs_r_8_proto(f);		\
bs_w_1_proto(f);		\
bs_w_2_proto(f);		\
bs_w_4_proto(f);		\
bs_w_8_proto(f);		\
bs_rm_1_proto(f);		\
bs_rm_2_proto(f);		\
bs_rm_4_proto(f);		\
bs_rm_8_proto(f);		\
bs_wm_1_proto(f);		\
bs_wm_2_proto(f);		\
bs_wm_4_proto(f);		\
bs_wm_8_proto(f);		\
bs_rr_1_proto(f);		\
bs_rr_2_proto(f);		\
bs_rr_4_proto(f);		\
bs_rr_8_proto(f);		\
bs_wr_1_proto(f);		\
bs_wr_2_proto(f);		\
bs_wr_4_proto(f);		\
bs_wr_8_proto(f);		\
bs_sm_1_proto(f);		\
bs_sm_2_proto(f);		\
bs_sm_4_proto(f);		\
bs_sm_8_proto(f);		\
bs_sr_1_proto(f);		\
bs_sr_2_proto(f);		\
bs_sr_4_proto(f);		\
bs_sr_8_proto(f);		\
bs_c_1_proto(f);		\
bs_c_2_proto(f);		\
bs_c_4_proto(f);		\
bs_c_8_proto(f);

#define BUS_SPACE_ALIGNED_POINTER(p, t) ALIGNED_POINTER(p, t)

/* Bus Space DMA macros */

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x00	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x01	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x02	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x04	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x10	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x20
#define	BUS_DMA_BUS3		0x40
#define	BUS_DMA_BUS4		0x80

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

typedef struct arm32_bus_dma_tag	*bus_dma_tag_t;
typedef struct arm32_bus_dmamap		*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct arm32_bus_dma_segment {
	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
	/*
	 * PRIVATE MEMBERS: not for use by machine-independent code.
	 */
	bus_addr_t	_ds_vaddr;	/* Virtual mapped address
					 * Used by bus_dmamem_sync() */
};
typedef struct arm32_bus_dma_segment	bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */

struct arm32_bus_dma_tag {
	/*
	 * DMA range for this tag.  If the page doesn't fall within
	 * one of these ranges, an error is returned.  The caller
	 * may then decide what to do with the transfer.  If the
	 * range pointer is NULL, it is ignored.
	 */
	bus_dma_segment_t *_ranges;
	int _nranges;

	/*
	 * DMA mapping methods.
	 */
	int	(*_dmamap_create) __P((bus_dma_tag_t, bus_size_t, int,
		    bus_size_t, bus_size_t, int, bus_dmamap_t *));
	void	(*_dmamap_destroy) __P((bus_dma_tag_t, bus_dmamap_t));
	int	(*_dmamap_load) __P((bus_dma_tag_t, bus_dmamap_t, void *,
		    bus_size_t, struct proc *, int));
	int	(*_dmamap_load_mbuf) __P((bus_dma_tag_t, bus_dmamap_t,
		    struct mbuf *, int));
	int	(*_dmamap_load_uio) __P((bus_dma_tag_t, bus_dmamap_t,
		    struct uio *, int));
	int	(*_dmamap_load_raw) __P((bus_dma_tag_t, bus_dmamap_t,
		    bus_dma_segment_t *, int, bus_size_t, int));
	void	(*_dmamap_unload) __P((bus_dma_tag_t, bus_dmamap_t));
	void	(*_dmamap_sync) __P((bus_dma_tag_t, bus_dmamap_t,
		    bus_addr_t, bus_size_t, int));

	/*
	 * DMA memory utility functions.
	 */
	int	(*_dmamem_alloc) __P((bus_dma_tag_t, bus_size_t, bus_size_t,
		    bus_size_t, bus_dma_segment_t *, int, int *, int));
	void	(*_dmamem_free) __P((bus_dma_tag_t,
		    bus_dma_segment_t *, int));
	int	(*_dmamem_map) __P((bus_dma_tag_t, bus_dma_segment_t *,
		    int, size_t, caddr_t *, int));
	void	(*_dmamem_unmap) __P((bus_dma_tag_t, caddr_t, size_t));
	int	(*_dmamem_mmap) __P((bus_dma_tag_t, bus_dma_segment_t *,
		    int, int, int, int));
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
	(void)((t)->_dmamap_sync ?				\
	    (*(t)->_dmamap_sync)((t), (p), (o), (l), (ops)) : (void)0)

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
struct arm32_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use by machine-independent code.
	 */
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxsegsz;	/* largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */
	int		_dm_flags;	/* misc. flags */

	void		*_dm_cookie;	/* cookie for bus-specific functions */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

#ifdef _ARM32_BUS_DMA_PRIVATE
int	_bus_dmamap_create __P((bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *));
void	_bus_dmamap_destroy __P((bus_dma_tag_t, bus_dmamap_t));
int	_bus_dmamap_load __P((bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));
int	_bus_dmamap_load_mbuf __P((bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int));
int	_bus_dmamap_load_uio __P((bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int));
int	_bus_dmamap_load_raw __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));
void	_bus_dmamap_unload __P((bus_dma_tag_t, bus_dmamap_t));
void	_bus_dmamap_sync __P((bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int));

int	_bus_dmamem_alloc __P((bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags));
void	_bus_dmamem_free __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs));
int	_bus_dmamem_map __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags));
void	_bus_dmamem_unmap __P((bus_dma_tag_t tag, caddr_t kva,
	    size_t size));
int	_bus_dmamem_mmap __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, int off, int prot, int flags));

int	_bus_dmamem_alloc_range __P((bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags,
	    vm_offset_t low, vm_offset_t high));
#endif /* _ARM32_BUS_DMA_PRIVATE */

#endif /* _ARM32_BUS_H_ */
