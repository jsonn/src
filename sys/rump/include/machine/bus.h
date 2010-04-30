/*	$NetBSD: bus.h,v 1.2.6.2 2010/04/30 14:44:29 uebayasi Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_RUMP_BUS_H_
#define _SYS_RUMP_BUS_H_

/*
 * This is a blanket header for archs which are inline/macro-happy
 * in their bus.h header.  Currently, this is anything but x86.
 * After an arch is cured from inline scurvy, the native bus.h
 * should be used.
 */

/* bus space defs */
typedef int bus_addr_t;
typedef int bus_size_t;
typedef int bus_space_tag_t;
typedef int bus_space_handle_t;

/* bus dma defs */
typedef int *bus_dma_tag_t;

typedef struct {
	bus_addr_t ds_addr;
	bus_size_t ds_len;
} bus_dma_segment_t;

typedef struct {
	bus_size_t dm_maxsegsz;
	bus_size_t dm_mapsize;
	int dm_nsegs;
	bus_dma_segment_t dm_segs[1];
} *bus_dmamap_t;

#include <sys/bus_proto.h>

#endif /* _SYS_RUMP_BUS_H_ */
