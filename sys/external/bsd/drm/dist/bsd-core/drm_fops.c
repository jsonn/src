/*-
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Daryll Strauss <daryll@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

/** @file drm_fops.c
 * Support code for dealing with the file privates associated with each
 * open of the DRM device.
 */

#include "drmP.h"

drm_file_t *drm_find_file_by_proc(struct drm_device *dev, DRM_STRUCTPROC *p)
{
#if defined(__NetBSD__)
	int restart = 1;
	uid_t uid = kauth_cred_getsvuid(p->p_cred);
	pid_t pid = p->p_pid;
	drm_file_t *priv;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	while (restart) {
		restart = 0;
		TAILQ_FOREACH(priv, &dev->files, link) {

	/* if the process disappeared, free the resources 
	 * NetBSD only calls drm_close once, so this frees
	 * resources earlier.
	 */
			if (pfind(priv->pid) == NULL) {
				/*drm_close_pid(dev, priv, priv->pid);*/
				restart = 1;
				break;
			}
			else
			if (priv->pid == pid && priv->uid == uid)
				return priv;
		}
	}
#else
#if __FreeBSD_version >= 500021
	uid_t uid = p->td_ucred->cr_svuid;
	pid_t pid = p->td_proc->p_pid;
#else
	uid_t uid = p->p_cred->p_svuid;
	pid_t pid = p->p_pid;
#endif
	drm_file_t *priv;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);

	TAILQ_FOREACH(priv, &dev->files, link)
		if (priv->pid == pid && priv->uid == uid)
			return priv;
#endif /* !__NetBSD__ */
	return NULL;
}

/* drm_open_helper is called whenever a process opens /dev/drm. */
int drm_open_helper(DRM_CDEV kdev, int flags, int fmt, DRM_STRUCTPROC *p,
		    drm_device_t *dev)
{
	int	     m = minor(kdev);
	drm_file_t   *priv;
	int retcode;

	if (flags & O_EXCL)
		return EBUSY; /* No exclusive opens */
	dev->flags = flags;

	DRM_DEBUG("pid = %d, minor = %d\n", DRM_CURRENTPID, m);

	DRM_LOCK();
	priv = drm_find_file_by_proc(dev, p);
	if (priv) {
		priv->refs++;
	} else {
		priv = malloc(sizeof(*priv), M_DRM, M_NOWAIT | M_ZERO);
		if (priv == NULL) {
			DRM_UNLOCK();
			return ENOMEM;
		}
#if __FreeBSD_version >= 500000
		priv->uid		= p->td_ucred->cr_svuid;
		priv->pid		= p->td_proc->p_pid;
#elif defined(__NetBSD__)
		priv->uid		= kauth_cred_getsvuid(p->p_cred);
		priv->pid		= p->p_pid;
#else
		priv->uid		= p->p_cred->p_svuid;
		priv->pid		= p->p_pid;
#endif

		priv->refs		= 1;
		priv->minor		= m;
		priv->ioctl_count 	= 0;

		/* for compatibility root is always authenticated */
		priv->authenticated	= DRM_SUSER(p);

		if (dev->driver.open) {
			/* shared code returns -errno */
			retcode = -dev->driver.open(dev, priv);
			if (retcode != 0) {
				free(priv, M_DRM);
				DRM_UNLOCK();
				return retcode;
			}
		}

		/* first opener automatically becomes master */
		priv->master = TAILQ_EMPTY(&dev->files);

		TAILQ_INSERT_TAIL(&dev->files, priv, link);
	}
	DRM_UNLOCK();
#ifdef __FreeBSD__
	kdev->si_drv1 = dev;
#endif
	return 0;
}


/* The drm_read and drm_poll are stubs to prevent spurious errors
 * on older X Servers (4.3.0 and earlier) */

int drm_read(DRM_CDEV kdev, struct uio *uio, int ioflag)
{
	return 0;
}

int drm_poll(DRM_CDEV kdev, int events, DRM_STRUCTCDEVPROC *p)
{
	return 0;
}
