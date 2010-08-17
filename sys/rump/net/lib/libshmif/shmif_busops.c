/*	$NetBSD: shmif_busops.c,v 1.5.2.2 2010/08/17 06:48:05 uebayasi Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by The Nokia Foundation.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: shmif_busops.c,v 1.5.2.2 2010/08/17 06:48:05 uebayasi Exp $");

#include <sys/param.h>
#include <sys/atomic.h>

#include "shmifvar.h"

#ifndef _KERNEL
#include <assert.h>
#define KASSERT(a) assert(a)
#else
#include <rump/rumpuser.h>
#endif

uint32_t
shmif_advance(uint32_t oldoff, uint32_t delta)
{
	uint32_t newoff;

	newoff = oldoff + delta;
	if (newoff >= BUSMEM_DATASIZE)
		newoff -= (BUSMEM_DATASIZE);
	return newoff;

}

uint32_t
shmif_busread(struct shmif_mem *busmem, void *dest, uint32_t off, size_t len,
	bool *wrap)
{
	size_t chunk;

	KASSERT(len < (BUSMEM_DATASIZE) && off <= BUSMEM_DATASIZE);
	chunk = MIN(len, BUSMEM_DATASIZE - off);
	memcpy(dest, busmem->shm_data + off, chunk);
	len -= chunk;

	if (len == 0)
		return off + chunk;

	/* else, wraps around */
	off = 0;
	*wrap = true;

	/* finish reading */
	memcpy((uint8_t *)dest + chunk, busmem->shm_data + off, len);
	return off + len;
}

void
shmif_advancefirst(struct shmif_mem *busmem, uint32_t off, size_t len)
{

	while (off <= busmem->shm_first + sizeof(struct shmif_pkthdr)
	    && off+len > busmem->shm_first) {
		DPRINTF(("advancefirst: old offset %d, ", busmem->shm_first));
		busmem->shm_first = shmif_nextpktoff(busmem, busmem->shm_first);
		DPRINTF(("new offset: %d\n", busmem->shm_first));
	}
}

uint32_t
shmif_buswrite(struct shmif_mem *busmem, uint32_t off, void *data, size_t len,
	bool *wrap)
{
	size_t chunk;

	KASSERT(len < (BUSMEM_DATASIZE) && off <= BUSMEM_DATASIZE);

	chunk = MIN(len, BUSMEM_DATASIZE - off);
	len -= chunk;

	shmif_advancefirst(busmem, off, chunk + (len ? 1 : 0));

	memcpy(busmem->shm_data + off, data, chunk);

	DPRINTF(("buswrite: wrote %d bytes to %d", chunk, off));

	if (len == 0) {
		DPRINTF(("\n"));
		return off + chunk;
	}

	DPRINTF((", wrapped bytes %d to 0\n", len));

	/* else, wraps around */
	off = 0;
	*wrap = true;

	shmif_advancefirst(busmem, off, len);

	/* finish writing */
	memcpy(busmem->shm_data + off, (uint8_t *)data + chunk, len);
	return off + len;
}

uint32_t
shmif_nextpktoff(struct shmif_mem *busmem, uint32_t oldoff)
{
	struct shmif_pkthdr sp;
	bool dummy;

	shmif_busread(busmem, &sp, oldoff, sizeof(sp), &dummy);
	KASSERT(sp.sp_len < BUSMEM_DATASIZE);

	return shmif_advance(oldoff, sizeof(sp) + sp.sp_len);
}
