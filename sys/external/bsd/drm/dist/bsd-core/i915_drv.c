/* i915_drv.c -- Intel i915 driver -*- linux-c -*-
 * Created: Wed Feb 14 17:10:04 2001 by gareth@valinux.com
 */
/*-
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t i915_pciidlist[] = {
	i915_PCI_IDS
};

#ifdef __FreeBSD__
static int i915_suspend(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (!dev || !dev_priv) {
		DRM_ERROR("dev: 0x%lx, dev_priv: 0x%lx\n",
			(unsigned long) dev, (unsigned long) dev_priv);
		DRM_ERROR("DRM not initialized, aborting suspend.\n");
		return -ENODEV;
	}

	i915_save_state(dev);

	return (bus_generic_suspend(nbdev));
}

static int i915_resume(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);

	i915_restore_state(dev);

	return (bus_generic_resume(nbdev));
}
#endif

static void i915_configure(struct drm_device *dev)
{
	dev->driver.buf_priv_size	= sizeof(drm_i915_private_t);
	dev->driver.load		= i915_driver_load;
	dev->driver.unload		= i915_driver_unload;
	dev->driver.firstopen		= i915_driver_firstopen;
	dev->driver.preclose		= i915_driver_preclose;
	dev->driver.lastclose		= i915_driver_lastclose;
	dev->driver.device_is_agp	= i915_driver_device_is_agp;
	dev->driver.get_vblank_counter	= i915_get_vblank_counter;
	dev->driver.enable_vblank	= i915_enable_vblank;
	dev->driver.disable_vblank	= i915_disable_vblank;
	dev->driver.irq_preinstall	= i915_driver_irq_preinstall;
	dev->driver.irq_postinstall	= i915_driver_irq_postinstall;
	dev->driver.irq_uninstall	= i915_driver_irq_uninstall;
	dev->driver.irq_handler		= i915_driver_irq_handler;

	dev->driver.ioctls		= i915_ioctls;
	dev->driver.max_ioctl		= i915_max_ioctl;

	dev->driver.name		= DRIVER_NAME;
	dev->driver.desc		= DRIVER_DESC;
	dev->driver.date		= DRIVER_DATE;
	dev->driver.major		= DRIVER_MAJOR;
	dev->driver.minor		= DRIVER_MINOR;
	dev->driver.patchlevel		= DRIVER_PATCHLEVEL;

	dev->driver.use_agp		= 1;
	dev->driver.require_agp		= 1;
	dev->driver.use_mtrr		= 1;
	dev->driver.use_irq		= 1;
	dev->driver.use_vbl_irq		= 1;
	dev->driver.use_vbl_irq2	= 1;
}

#ifdef __FreeBSD__
static int
i915_probe(device_t dev)
{
	return drm_probe(dev, i915_pciidlist);
}

static int
i915_attach(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(struct drm_device));
	i915_configure(dev);
	return drm_attach(nbdev, i915_pciidlist);
}

static device_method_t i915_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		i915_probe),
	DEVMETHOD(device_attach,	i915_attach),
#ifdef __FreeBSD__
	DEVMETHOD(device_suspend,	i915_suspend),
	DEVMETHOD(device_resume,	i915_resume),
#endif
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t i915_driver = {
#if __FreeBSD_version >= 700010
	"drm",
#else
	"drmsub",
#endif
	i915_methods,
	sizeof(struct drm_device)
};

extern devclass_t drm_devclass;
#if __FreeBSD_version >= 700010
DRIVER_MODULE(i915, vgapci, i915_driver, drm_devclass, 0, 0);
#else
DRIVER_MODULE(i915, agp, i915_driver, drm_devclass, 0, 0);
#endif
MODULE_DEPEND(i915, drm, 1, 1, 1);

#elif defined(__OpenBSD__)
CFDRIVER_DECL(i915, DV_TTY, NULL);
#elif defined(__NetBSD__)
static int
i915drm_probe(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;
	return drm_probe(pa, i915_pciidlist);
}

static void
i915drm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	drm_device_t *dev = (drm_device_t *)self;

	i915_configure(dev);

	pmf_device_register(self, NULL, NULL);

	drm_attach(self, pa, i915_pciidlist);
}

CFATTACH_DECL(i915drm, sizeof(drm_device_t), i915drm_probe, i915drm_attach,
	drm_detach, drm_activate);

#ifdef _MODULE

MODULE(MODULE_CLASS_DRIVER, i915drm, "drm");

CFDRIVER_DECL(i915drm, DV_DULL, NULL);
extern struct cfattach i915drm_ca;
static int drmloc[] = { -1 };
static struct cfparent drmparent = {
	"drm", "vga", DVUNIT_ANY
};
static struct cfdata i915drm_cfdata[] = {
	{
		.cf_name = "i915drm",
		.cf_atname = "i915drm",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = drmloc,
		.cf_flags = 0,
		.cf_pspec = &drmparent,
	},
	{ NULL }
};

static int
i915drm_modcmd(modcmd_t cmd, void *arg)
{
	int err;

	switch (cmd) {
	case MODULE_CMD_INIT:
		err = config_cfdriver_attach(&i915drm_cd);
		if (err)
			return err;
		err = config_cfattach_attach("i915drm", &i915drm_ca);
		if (err) {
			config_cfdriver_detach(&i915drm_cd);
			return err;
		}
		err = config_cfdata_attach(i915drm_cfdata, 1);
		if (err) {
			config_cfattach_detach("i915drm", &i915drm_ca);
			config_cfdriver_detach(&i915drm_cd);
			return err;
		}
		return 0;
	case MODULE_CMD_FINI:
		err = config_cfdata_detach(i915drm_cfdata);
		if (err)
			return err;
		config_cfattach_detach("i915drm", &i915drm_ca);
		config_cfdriver_detach(&i915drm_cd);
		return 0;
	default:
		return ENOTTY;
	}
}
#endif /* _MODULE */
#endif /* __FreeBSD__ */
