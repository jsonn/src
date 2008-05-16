/*	$NetBSD: darwin_iokit.h,v 1.9.74.1 2008/05/16 02:23:35 yamt Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
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

#ifndef	_DARWIN_IOKIT_H_
#define	_DARWIN_IOKIT_H_

typedef uint64_t darwin_unsignedwide;
typedef darwin_unsignedwide darwin_absolutetime;
typedef volatile int darwin_ev_lock_data_t; /* aka IOSharedLockData */
typedef uint32_t darwin_iooptionbits;

typedef struct {
	int16_t x;
	int16_t y;
} darwin_iogpoint;

typedef struct {
	int16_t width;
	int16_t height;
} darwin_iogsize;

typedef struct {
	int16_t minx;
	int16_t maxx;
	int16_t miny;
	int16_t maxy;
} darwin_iogbounds;

#include <compat/darwin/darwin_iohidsystem.h>
#include <compat/darwin/darwin_ioframebuffer.h>

#define DARWIN_IOKIT_DEVCLASSES			\
	&darwin_iohidsystem_devclass,		\
	&darwin_ioframebuffer_devclass,		\
	&darwin_iokbd_devclass,			\
	&darwin_iomouse_devclass,		\
	&darwin_ioresources_devclass,

#endif /* _DARWIN_IOKIT_H_ */
