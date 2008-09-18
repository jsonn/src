/* $NetBSD: drm_vm.c,v 1.10.2.2 2008/09/18 04:35:03 wrstuden Exp $ */

/*-
 * Copyright 2003 Eric Anholt
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * ERIC ANHOLT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: drm_vm.c,v 1.10.2.2 2008/09/18 04:35:03 wrstuden Exp $");
/*
__FBSDID("$FreeBSD: src/sys/dev/drm/drm_vm.c,v 1.2 2005/11/28 23:13:53 anholt Exp $");
*/

#include "drmP.h"
#include "drm.h"

/* WARNING:  pure, unadulterated EVIL ahead! */

paddr_t drm_mmap(dev_t kdev, off_t offset, int prot)
{
	DRM_DEVICE;
	drm_local_map_t *map;
	drm_file_t *priv;
	drm_map_type_t type;
	paddr_t phys;
	uintptr_t roffset;

	DRM_LOCK();
	priv = drm_find_file_by_proc(dev, DRM_CURPROC);
	DRM_UNLOCK();
	if (priv == NULL) {
		DRM_ERROR("can't find authenticator\n");
		return -1;
	}

	if (!priv->authenticated)
		return -1;

	if (dev->dma && offset >= 0 && offset < ptoa(dev->dma->page_count)) {
		drm_device_dma_t *dma = dev->dma;

		DRM_SPINLOCK(&dev->dma_lock);

		if (dma->pagelist != NULL) {
			unsigned long page = offset >> PAGE_SHIFT;
			unsigned long pphys = dma->pagelist[page];

#ifdef macppc
			return pphys;
#else
			return atop(pphys);
#endif
		} else {
			DRM_SPINUNLOCK(&dev->dma_lock);
			return -1;
		}
		DRM_SPINUNLOCK(&dev->dma_lock);
	}

				/* A sequential search of a linked list is
				   fine here because: 1) there will only be
				   about 5-10 entries in the list and, 2) a
				   DRI client only has to do this mapping
				   once, so it doesn't have to be optimized
				   for performance, even if the list was a
				   bit longer. */
	DRM_LOCK();
	roffset = DRM_NETBSD_HANDLE2ADDR(offset);
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->type == _DRM_SHM) {
			if (roffset >= (uintptr_t)map->handle && roffset < (uintptr_t)map->handle + map->size)
				break;
		} else {
			if (offset >= map->offset && offset < map->offset + map->size)
				break;
		}
	}

	if (map == NULL) {
		DRM_UNLOCK();
		DRM_DEBUG("can't find map\n");
		return -1;
	}
	if (((map->flags&_DRM_RESTRICTED) && !DRM_SUSER(DRM_CURPROC))) {
		DRM_UNLOCK();
		DRM_DEBUG("restricted map\n");
		return -1;
	}
	type = map->type;
	DRM_UNLOCK();

	switch (type) {
	case _DRM_FRAME_BUFFER:
	case _DRM_REGISTERS:
	case _DRM_AGP:
		phys = offset;
		break;
	case _DRM_CONSISTENT:
		phys = vtophys((paddr_t)map->handle + (offset - map->offset));
		break;
	case _DRM_SCATTER_GATHER:
		phys = vtophys(offset);
		break;
	case _DRM_SHM:
		phys = vtophys(DRM_NETBSD_HANDLE2ADDR(offset));
		break;
	default:
		DRM_ERROR("bad map type %d\n", type);
		return -1;	/* This should never happen. */
	}

#ifdef macppc
	return phys;
#else
	return atop(phys);
#endif
}

