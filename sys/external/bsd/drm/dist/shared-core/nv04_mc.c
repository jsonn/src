#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"

int
nv04_mc_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	/* Power up everything, resetting each individual unit will
	 * be done later if needed.
	 */
	NV_WRITE(NV03_PMC_ENABLE, 0xFFFFFFFF);

	return 0;
}

void
nv04_mc_takedown(struct drm_device *dev)
{
}
