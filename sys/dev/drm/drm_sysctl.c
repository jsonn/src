/* $NetBSD: drm_sysctl.c,v 1.2.2.1 2007/12/26 19:46:09 ad Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: drm_sysctl.c,v 1.2.2.1 2007/12/26 19:46:09 ad Exp $");
/*
__FBSDID("$FreeBSD: src/sys/dev/drm/drm_sysctl.c,v 1.2 2005/11/28 23:13:53 anholt Exp $");
*/

#include "drmP.h"
#include "drm.h"

#include <sys/sysctl.h>

static int	   drm_name_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_vm_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_clients_info DRM_SYSCTL_HANDLER_ARGS;
static int	   drm_bufs_info DRM_SYSCTL_HANDLER_ARGS;

struct drm_sysctl_list {
	const char *name;
	int	   (*f) DRM_SYSCTL_HANDLER_ARGS;
} drm_sysctl_list[] = {
	{"name",    drm_name_info},
	{"vm",	    drm_vm_info},
	{"clients", drm_clients_info},
	{"bufs",    drm_bufs_info},
};
#define DRM_SYSCTL_ENTRIES (sizeof(drm_sysctl_list)/sizeof(drm_sysctl_list[0]))

struct drm_sysctl_info {
	const struct sysctlnode *dri, *dri_card, *dri_debug;
	const struct sysctlnode *dri_rest[DRM_SYSCTL_ENTRIES];
	char		       name[7];
};

int drm_sysctl_init(drm_device_t *dev)
{
	struct drm_sysctl_info *info;
	int		  i;

	info = malloc(sizeof *info, M_DRM, M_WAITOK | M_ZERO);
	if ( !info )
		return 1;

	dev->sysctl = info;

	sysctl_createv(NULL, 0, NULL, &info->dri,
			CTLFLAG_READWRITE, CTLTYPE_NODE,
			"dri", SYSCTL_DESCR("DRI Graphics"), NULL, 0, NULL, 0,
			CTL_HW, CTL_CREATE);
	snprintf(info->name, 7, "card%d", dev->unit);
	sysctl_createv(NULL, 0, NULL, &info->dri_card,
			CTLFLAG_READWRITE, CTLTYPE_NODE,
			info->name, NULL, NULL, 0, NULL, 0,
			CTL_HW, info->dri->sysctl_num, CTL_CREATE);
	for (i = 0; i < DRM_SYSCTL_ENTRIES; i++)
		sysctl_createv(NULL, 0, NULL, &(info->dri_rest[i]),
				CTLFLAG_READONLY, CTLTYPE_STRING,
				drm_sysctl_list[i].name, NULL,
				drm_sysctl_list[i].f, 0, dev,
				sizeof(drm_device_t*),
				CTL_HW,
				info->dri->sysctl_num,
				info->dri_card->sysctl_num, CTL_CREATE);
	sysctl_createv(NULL, 0, NULL, &info->dri_debug,
			CTLFLAG_READWRITE, CTLTYPE_INT,
			"debug", SYSCTL_DESCR("Enable debugging output"),
			NULL, 0,
			&drm_debug_flag, sizeof(drm_debug_flag),
			CTL_HW, info->dri->sysctl_num, CTL_CREATE);
	return 0;
}

int drm_sysctl_cleanup(drm_device_t *dev)
{
	int i, error = 0;

	sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num,
	                              dev->sysctl->dri_debug->sysctl_num,
	                              CTL_DESTROY);
	for (i = 0; i < DRM_SYSCTL_ENTRIES; i++)
		sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num,
	       	                              dev->sysctl->dri_card->sysctl_num,
	       	                           dev->sysctl->dri_rest[i]->sysctl_num,
		                              CTL_DESTROY);
	sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num,
	                              dev->sysctl->dri_card->sysctl_num,
	                              CTL_DESTROY);
	sysctl_destroyv(NULL, CTL_HW, dev->sysctl->dri->sysctl_num, CTL_DESTROY);

	free(dev->sysctl, M_DRM);
	dev->sysctl = NULL;

	return error;
}

#define SYSCTL_OUT(x, y, z) \
	(len+=z,(len<*oldlenp)?(strcat((char*)oldp, y),0):EOVERFLOW)

#define DRM_SYSCTL_PRINT(fmt, arg...)				\
do {								\
	snprintf(buf, sizeof(buf), fmt, ##arg);			\
	retcode = SYSCTL_OUT(req, buf, strlen(buf));		\
	if (retcode)						\
		goto done;					\
} while (0)

static int drm_name_info DRM_SYSCTL_HANDLER_ARGS
{
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
	char buf[128];
	int retcode;
	int hasunique = 0;

	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';

	DRM_SYSCTL_PRINT("%s", dev->driver.name);
	
	DRM_LOCK();
	if (dev->unique) {
		snprintf(buf, sizeof(buf), " %s", dev->unique);
		hasunique = 1;
	}
	DRM_UNLOCK();
	
	if (hasunique)
		SYSCTL_OUT(req, buf, strlen(buf));

	SYSCTL_OUT(req, "", 1);

done:
	return retcode;
}

static int drm_vm_info DRM_SYSCTL_HANDLER_ARGS
{
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
	drm_local_map_t *map, *tempmaps;
	const char   *types[] = { "FB", "REG", "SHM", "AGP", "SG" };
	const char *type, *yesno;
	int i, mapcount;
	char buf[128];
	int retcode;

	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';

	/* We can't hold the lock while doing SYSCTL_OUTs, so allocate a
	 * temporary copy of all the map entries and then SYSCTL_OUT that.
	 */
	DRM_LOCK();

	mapcount = 0;
	TAILQ_FOREACH(map, &dev->maplist, link)
		mapcount++;

	tempmaps = malloc(sizeof(drm_local_map_t) * mapcount, M_DRM, M_NOWAIT);
	if (tempmaps == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}

	i = 0;
	TAILQ_FOREACH(map, &dev->maplist, link)
		tempmaps[i++] = *map;

	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\nslot	 offset	      size type flags	 "
			 "address mtrr\n");

	for (i = 0; i < mapcount; i++) {
		map = &tempmaps[i];

		if (/* map->type < 0 || */ map->type > 4)
			type = "??";
		else
			type = types[map->type];

		if (!map->mtrr)
			yesno = "no";
		else
			yesno = "yes";

		DRM_SYSCTL_PRINT(
		    "%4d 0x%08lx 0x%08lx %4.4s  0x%02x 0x%08lx %s\n", i,
		    map->offset, map->size, type, map->flags,
		    (unsigned long)map->handle, yesno);
	}
	SYSCTL_OUT(req, "", 1);

done:
	free(tempmaps, M_DRM);
	return retcode;
}

static int drm_bufs_info DRM_SYSCTL_HANDLER_ARGS
{
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
	drm_device_dma_t *dma = dev->dma;
	drm_device_dma_t tempdma;
	int *templists;
	int i;
	char buf[128];
	int retcode;

	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';

	/* We can't hold the locks around DRM_SYSCTL_PRINT, so make a temporary
	 * copy of the whole structure and the relevant data from buflist.
	 */
	DRM_LOCK();
	if (dma == NULL) {
		DRM_UNLOCK();
		return 0;
	}
	DRM_SPINLOCK(&dev->dma_lock);
	tempdma = *dma;
	templists = malloc(sizeof(int) * dma->buf_count, M_DRM, M_NOWAIT);
	for (i = 0; i < dma->buf_count; i++)
		templists[i] = dma->buflist[i]->list;
	dma = &tempdma;
	DRM_SPINUNLOCK(&dev->dma_lock);
	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\n o     size count  free	 segs pages    kB\n");
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
		if (dma->bufs[i].buf_count)
			DRM_SYSCTL_PRINT("%2d %8d %5d %5d %5d %5d %5d\n",
				       i,
				       dma->bufs[i].buf_size,
				       dma->bufs[i].buf_count,
				       atomic_read(&dma->bufs[i]
						   .freelist.count),
				       dma->bufs[i].seg_count,
				       dma->bufs[i].seg_count
				       *(1 << dma->bufs[i].page_order),
				       (dma->bufs[i].seg_count
					* (1 << dma->bufs[i].page_order))
				       * PAGE_SIZE / 1024);
	}
	DRM_SYSCTL_PRINT("\n");
	for (i = 0; i < dma->buf_count; i++) {
		if (i && !(i%32)) DRM_SYSCTL_PRINT("\n");
		DRM_SYSCTL_PRINT(" %d", templists[i]);
	}
	DRM_SYSCTL_PRINT("\n");

	SYSCTL_OUT(req, "", 1);
done:
	free(templists, M_DRM);
	return retcode;
}

static int drm_clients_info DRM_SYSCTL_HANDLER_ARGS
{
	int len = 0;
	drm_device_t *dev = rnode->sysctl_data;
	drm_file_t *priv, *tempprivs;
	char buf[128];
	int retcode;
	int privcount, i;

	if(oldp == NULL) return EINVAL;
	*((char*)oldp) = '\0';

	DRM_LOCK();

	privcount = 0;
	TAILQ_FOREACH(priv, &dev->files, link)
		privcount++;

	tempprivs = malloc(sizeof(drm_file_t) * privcount, M_DRM, M_NOWAIT);
	if (tempprivs == NULL) {
		DRM_UNLOCK();
		return ENOMEM;
	}
	i = 0;
	TAILQ_FOREACH(priv, &dev->files, link)
		tempprivs[i++] = *priv;

	DRM_UNLOCK();

	DRM_SYSCTL_PRINT("\na dev	pid    uid	magic	  ioctls\n");
	for (i = 0; i < privcount; i++) {
		priv = &tempprivs[i];
		DRM_SYSCTL_PRINT("%c %3d %5d %5d %10u %10lu\n",
			       priv->authenticated ? 'y' : 'n',
			       priv->minor,
			       priv->pid,
			       priv->uid,
			       priv->magic,
			       priv->ioctl_count);
	}

	SYSCTL_OUT(req, "", 1);
done:
	free(tempprivs, M_DRM);
	return retcode;
}
