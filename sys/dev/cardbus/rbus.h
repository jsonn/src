/*	$NetBSD: rbus.h,v 1.3.2.1 1999/12/27 18:34:41 wrstuden Exp $	*/
/*
 * Copyright (c) 1999
 *     HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by the author.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_CARDBUS_RBUS_H_
#define _DEV_CARDBUS_RBUS_H_

/*
 * This file defines rbus (pseudo) class
 *
 * What is rbus?
 * 
 *  Ths rbus is a recursive bus-space administrator.  This means a
 *  parent bus-space administrator, which usually belongs to a bus
 *  bridge, makes some child bus-space administorators and gives
 *  (restricted) bus-space for children.  There are a root bus-space
 *  administrator which maintains whole bus-space.
 *
 * Why recursive?
 *
 *  The recursive bus-space administration has two virtues.  The
 *  former is this modelling matches the actual memory and io space
 *  management of bridge devices well.  The latter is the rbus is
 *  distributed management system, so it matches well with
 *  multi-thread kernel.
 *
 * Abstraction
 *
 *  The rbus models bus-to-bus bridge into three way: dedicate, share
 *  and slave.  Dedicate means that the bridge has dedicate bus space.
 *  Share means that the bridge has bus space, but this bus space is
 *  shared with other bus bridges.  Slave means the bus bridge which
 *  does not have it own bus space and ask a parent bus bridge for bus
 *  space when a client requests bus space to the bridge.
 */


/* require sys/extent.h */
/* require machine/bus.h */

#define rbus 1


struct extent;


/*
 *     General rule
 *
 * 1) When a rbustag has no space for child (it means rb_extent is
 *    NULL), ask bus-space for parent through rb_parent.
 *
 * 2) When a rbustag has its own space (whether shared or dedicated),
 *    allocate from rb_ext.
 */
struct rbustag {
  bus_space_tag_t rb_bt;
  struct rbustag *rb_parent;
  struct extent *rb_ext;
  bus_addr_t rb_start;
  bus_addr_t rb_end;
  bus_addr_t rb_offset;
#if notyet
  int (*rb_space_alloc) __P((struct rbustag *,
			     bus_addr_t start, bus_addr_t end,
			     bus_addr_t addr, bus_size_t size,
			     bus_addr_t mask, bus_addr_t align,
			     int flags,
			     bus_addr_t *addrp, bus_space_handle_t *bshp));
  int (*rbus_space_free) __P((struct rbustag *, bus_space_handle_t,
			      bus_size_t size, bus_addr_t *addrp));
#endif
  int rb_flags;
#define RBUS_SPACE_INVALID   0x00
#define RBUS_SPACE_SHARE     0x01
#define RBUS_SPACE_DEDICATE  0x02
#define RBUS_SPACE_MASK      0x03
#define RBUS_SPACE_ASK_PARENT 0x04
  /* your own data below */
  void *rb_md;
};

typedef struct rbustag *rbus_tag_t;




/*
 * These functions sugarcoat rbus interface to make rbus being used
 * easier.  These functions should be member functions of rbus
 * `class'.
 */
int rbus_space_alloc __P((rbus_tag_t,
			  bus_addr_t addr, bus_size_t size, bus_addr_t mask,
			  bus_addr_t align, int flags,
			  bus_addr_t *addrp, bus_space_handle_t *bshp));

int rbus_space_alloc_subregion __P((rbus_tag_t,
				    bus_addr_t start, bus_addr_t end,
				    bus_addr_t addr, bus_size_t size,
				    bus_addr_t mask, bus_addr_t align,
				    int flags,
				    bus_addr_t *addrp, bus_space_handle_t *bshp));

int rbus_space_free __P((rbus_tag_t, bus_space_handle_t, bus_size_t size,
			 bus_addr_t *addrp));


/*
 * These functions create rbus instance.  These functions are
 * so-called-as a constructor of rbus.
 *
 * rbus_new is a constructor which make an rbus instance from a parent
 * rbus.
 */
rbus_tag_t rbus_new __P((rbus_tag_t parent, bus_addr_t start, bus_size_t size,
			 bus_addr_t offset, int flags));

rbus_tag_t rbus_new_root_delegate __P((bus_space_tag_t, bus_addr_t, bus_size_t,
				       bus_addr_t offset));
rbus_tag_t rbus_new_root_share __P((bus_space_tag_t, struct extent *,
				    bus_addr_t, bus_size_t,bus_addr_t offset));

/*
 * This function release bus-space used by the argument.  This
 * function is so-called-as a destructor.
 */
int rbus_delete __P((rbus_tag_t));


/*
 * Machine-dependent definitions.
 */
#include <machine/rbus_machdep.h>

#endif /* !_DEV_CARDBUS_RBUS_H_ */
