/*	$NetBSD: via_map.c,v 1.1.28.1 2007/12/13 21:56:03 bouyer Exp $	*/

/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: via_map.c,v 1.1.28.1 2007/12/13 21:56:03 bouyer Exp $");

#include <dev/drm/drmP.h>
#include <dev/pci/drm/via_drm.h>
#include <dev/pci/drm/via_drv.h>

static int via_do_init_map(drm_device_t * dev, drm_via_init_t * init)
{
	drm_via_private_t *dev_priv = dev->dev_private;
	int ret = 0;

	DRM_DEBUG("%s\n", __FUNCTION__);

	DRM_GETSAREA();
	if (!dev_priv->sarea) {
		DRM_ERROR("could not find sarea!\n");
		dev->dev_private = (void *)dev_priv;
		via_do_cleanup_map(dev);
		return -EINVAL;
	}

	dev_priv->fb = drm_core_findmap(dev, init->fb_offset);
	if (!dev_priv->fb) {
		DRM_ERROR("could not find framebuffer!\n");
		dev->dev_private = (void *)dev_priv;
		via_do_cleanup_map(dev);
		return -EINVAL;
	}
	dev_priv->mmio = drm_core_findmap(dev, init->mmio_offset);
	if (!dev_priv->mmio) {
		DRM_ERROR("could not find mmio region!\n");
		dev->dev_private = (void *)dev_priv;
		via_do_cleanup_map(dev);
		return -EINVAL;
	}

	dev_priv->sarea_priv =
	    (drm_via_sarea_t *) ((u8 *) dev_priv->sarea->handle +
				 init->sarea_priv_offset);

	dev_priv->agpAddr = init->agpAddr;

	via_init_futex( dev_priv );
#ifdef VIA_HAVE_DMABLIT
	via_init_dmablit( dev );
#endif
#ifdef VIA_HAVE_FENCE
	dev_priv->emit_0_sequence = 0;
	dev_priv->have_idlelock = 0;
	spin_lock_init(&dev_priv->fence_lock);
	init_timer(&dev_priv->fence_timer);
	dev_priv->fence_timer.function = &via_fence_timer;
	dev_priv->fence_timer.data = (unsigned long) dev;
#endif /* VIA_HAVE_FENCE */
	dev->dev_private = (void *)dev_priv;
#ifdef VIA_HAVE_BUFFER
	ret = drm_bo_driver_init(dev);
	if (ret)
		DRM_ERROR("Could not initialize buffer object driver.\n");
#endif
	return ret;

}

int via_do_cleanup_map(drm_device_t * dev)
{
	via_dma_cleanup(dev);

	return 0;
}


int via_map_init(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_via_init_t init;

	DRM_DEBUG("%s\n", __FUNCTION__);

	DRM_COPY_FROM_USER_IOCTL(init, (drm_via_init_t __user *) data,
				 sizeof(init));

	switch (init.func) {
	case VIA_INIT_MAP:
		return via_do_init_map(dev, &init);
	case VIA_CLEANUP_MAP:
		return via_do_cleanup_map(dev);
	}

	return -EINVAL;
}

int via_driver_load(drm_device_t *dev, unsigned long chipset)
{
	drm_via_private_t *dev_priv;
	int ret = 0;

	dev_priv = drm_calloc(1, sizeof(drm_via_private_t), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return DRM_ERR(ENOMEM);

	dev->dev_private = (void *)dev_priv;

	dev_priv->chipset = chipset;

#ifdef VIA_HAVE_CORE_MM
	ret = drm_sman_init(&dev_priv->sman, 2, 12, 8);
	if (ret) {
		drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);
	}
#endif
	return ret;
}

int via_driver_unload(drm_device_t *dev)
{
	drm_via_private_t *dev_priv = dev->dev_private;

#ifdef VIA_HAVE_CORE_MM
	drm_sman_takedown(&dev_priv->sman);
#endif
	drm_free(dev_priv, sizeof(drm_via_private_t), DRM_MEM_DRIVER);

	return 0;
}

