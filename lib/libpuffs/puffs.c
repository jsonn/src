/*	$NetBSD: puffs.c,v 1.62.2.2 2007/11/08 11:01:01 matt Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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
#if !defined(lint)
__RCSID("$NetBSD: puffs.c,v 1.62.2.2 2007/11/08 11:01:01 matt Exp $");
#endif /* !lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <mntopts.h>
#include <paths.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "puffs_priv.h"

/* Most file systems want this for opts, so just give it to them */
const struct mntopt puffsmopts[] = {
	MOPT_STDOPTS,
	PUFFSMOPT_STD,
	MOPT_NULL,
};

#ifdef PUFFS_WITH_THREADS
#include <pthread.h>
pthread_mutex_t pu_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

#define FILLOP(lower, upper)						\
do {									\
	if (pops->puffs_node_##lower)					\
		opmask[PUFFS_VN_##upper] = 1;				\
} while (/*CONSTCOND*/0)
static void
fillvnopmask(struct puffs_ops *pops, uint8_t *opmask)
{

	memset(opmask, 0, PUFFS_VN_MAX);

	FILLOP(create,   CREATE);
	FILLOP(mknod,    MKNOD);
	FILLOP(open,     OPEN);
	FILLOP(close,    CLOSE);
	FILLOP(access,   ACCESS);
	FILLOP(getattr,  GETATTR);
	FILLOP(setattr,  SETATTR);
	FILLOP(poll,     POLL); /* XXX: not ready in kernel */
	FILLOP(mmap,     MMAP);
	FILLOP(fsync,    FSYNC);
	FILLOP(seek,     SEEK);
	FILLOP(remove,   REMOVE);
	FILLOP(link,     LINK);
	FILLOP(rename,   RENAME);
	FILLOP(mkdir,    MKDIR);
	FILLOP(rmdir,    RMDIR);
	FILLOP(symlink,  SYMLINK);
	FILLOP(readdir,  READDIR);
	FILLOP(readlink, READLINK);
	FILLOP(reclaim,  RECLAIM);
	FILLOP(inactive, INACTIVE);
	FILLOP(print,    PRINT);
	FILLOP(read,     READ);
	FILLOP(write,    WRITE);
}
#undef FILLOP

/*ARGSUSED*/
static void
puffs_defaulterror(struct puffs_usermount *pu, uint8_t type,
	int error, const char *str, void *cookie)
{

	fprintf(stderr, "%s\n", str);
	abort();
}

int
puffs_getselectable(struct puffs_usermount *pu)
{

	return pu->pu_fd;
}

int
puffs_setblockingmode(struct puffs_usermount *pu, int mode)
{
	int rv, x;

	assert(puffs_getstate(pu) == PUFFS_STATE_RUNNING);

	if (mode != PUFFSDEV_BLOCK && mode != PUFFSDEV_NONBLOCK) {
		errno = EINVAL;
		return -1;
	}

	x = mode;
	rv = ioctl(pu->pu_fd, FIONBIO, &x);

	if (rv == 0) {
		if (mode == PUFFSDEV_BLOCK)
			pu->pu_state &= ~PU_ASYNCFD;
		else
			pu->pu_state |= PU_ASYNCFD;
	}

	return rv;
}

int
puffs_getstate(struct puffs_usermount *pu)
{

	return pu->pu_state & PU_STATEMASK;
}

void
puffs_setstacksize(struct puffs_usermount *pu, size_t ss)
{
	int stackshift;
 
	assert(puffs_getstate(pu) == PUFFS_STATE_BEFOREMOUNT);
	stackshift = -1;
	while (ss) {
		ss >>= 1;
		stackshift++;
	}
	pu->pu_cc_stackshift = stackshift;
	assert(1<<stackshift == ss);
}

struct puffs_pathobj *
puffs_getrootpathobj(struct puffs_usermount *pu)
{
	struct puffs_node *pnr;

	pnr = pu->pu_pn_root;
	if (pnr == NULL) {
		errno = ENOENT;
		return NULL;
	}

	return &pnr->pn_po;
}

void
puffs_setroot(struct puffs_usermount *pu, struct puffs_node *pn)
{

	pu->pu_pn_root = pn;
}

struct puffs_node *
puffs_getroot(struct puffs_usermount *pu)
{

	return pu->pu_pn_root;
}

void
puffs_setrootinfo(struct puffs_usermount *pu, enum vtype vt,
	vsize_t vsize, dev_t rdev)
{
	struct puffs_kargs *pargs = pu->pu_kargp;

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT) {
		warnx("puffs_setrootinfo: call has effect only "
		    "before mount\n");
		return;
	}

	pargs->pa_root_vtype = vt;
	pargs->pa_root_vsize = vsize;
	pargs->pa_root_rdev = rdev;
}

void *
puffs_getspecific(struct puffs_usermount *pu)
{

	return pu->pu_privdata;
}

size_t
puffs_getmaxreqlen(struct puffs_usermount *pu)
{

	return pu->pu_maxreqlen;
}

void
puffs_setmaxreqlen(struct puffs_usermount *pu, size_t reqlen)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("puffs_setmaxreqlen: call has effect only "
		    "before mount\n");

	pu->pu_kargp->pa_maxmsglen = reqlen;
}

void
puffs_setfhsize(struct puffs_usermount *pu, size_t fhsize, int flags)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("puffs_setfhsize: call has effect only before mount\n");

	pu->pu_kargp->pa_fhsize = fhsize;
	pu->pu_kargp->pa_fhflags = flags;
}

void
puffs_setncookiehash(struct puffs_usermount *pu, int nhash)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("puffs_setfhsize: call has effect only before mount\n");

	pu->pu_kargp->pa_nhashbuckets = nhash;
}

void
puffs_set_pathbuild(struct puffs_usermount *pu, pu_pathbuild_fn fn)
{

	pu->pu_pathbuild = fn;
}

void
puffs_set_pathtransform(struct puffs_usermount *pu, pu_pathtransform_fn fn)
{

	pu->pu_pathtransform = fn;
}

void
puffs_set_pathcmp(struct puffs_usermount *pu, pu_pathcmp_fn fn)
{

	pu->pu_pathcmp = fn;
}

void
puffs_set_pathfree(struct puffs_usermount *pu, pu_pathfree_fn fn)
{

	pu->pu_pathfree = fn;
}

void
puffs_set_namemod(struct puffs_usermount *pu, pu_namemod_fn fn)
{

	pu->pu_namemod = fn;
}

void
puffs_set_errnotify(struct puffs_usermount *pu, pu_errnotify_fn fn)
{

	pu->pu_errnotify = fn;
}

void
puffs_ml_setloopfn(struct puffs_usermount *pu, puffs_ml_loop_fn lfn)
{

	pu->pu_ml_lfn = lfn;
}

void
puffs_ml_settimeout(struct puffs_usermount *pu, struct timespec *ts)
{

	if (ts == NULL) {
		pu->pu_ml_timep = NULL;
	} else {
		pu->pu_ml_timeout = *ts;
		pu->pu_ml_timep = &pu->pu_ml_timeout;
	}
}

void
puffs_set_prepost(struct puffs_usermount *pu,
	pu_prepost_fn pre, pu_prepost_fn pst)
{

	pu->pu_oppre = pre;
	pu->pu_oppost = pst;
}

void
puffs_setback(struct puffs_cc *pcc, int whatback)
{
	struct puffs_req *preq = pcc->pcc_preq;

	assert(PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN && (
	    preq->preq_optype == PUFFS_VN_OPEN ||
	    preq->preq_optype == PUFFS_VN_MMAP ||
	    preq->preq_optype == PUFFS_VN_REMOVE ||
	    preq->preq_optype == PUFFS_VN_RMDIR ||
	    preq->preq_optype == PUFFS_VN_INACTIVE));

	preq->preq_setbacks |= whatback & PUFFS_SETBACK_MASK;
}

int
puffs_mount(struct puffs_usermount *pu, const char *dir, int mntflags,
	void *cookie)
{
	char rp[MAXPATHLEN];
	int rv, fd;

#if 1
	/* XXXkludgehere */
	/* kauth doesn't provide this service any longer */
	if (geteuid() != 0)
		mntflags |= MNT_NOSUID | MNT_NODEV;
#endif

	if (realpath(dir, rp) == NULL)
		return -1;

	if (strcmp(dir, rp) != 0) {
		warnx("puffs_mount: \"%s\" is a relative path.", dir);
		warnx("puffs_mount: using \"%s\" instead.", rp);
	}

	fd = open(_PATH_PUFFS, O_RDONLY);
	if (fd == -1) {
		warnx("puffs_mount: cannot open %s", _PATH_PUFFS);
		return -1;
	}
	if (fd <= 2)
		warnx("puffs_init: device fd %d (<= 2), sure this is "
		    "what you want?", fd);

	pu->pu_kargp->pa_fd = pu->pu_fd = fd;
	pu->pu_kargp->pa_root_cookie = cookie;
	if ((rv = mount(MOUNT_PUFFS, rp, mntflags,
	    pu->pu_kargp, sizeof(struct puffs_kargs))) == -1)
		goto out;
#if 0
	if ((rv = ioctl(pu->pu_fd, PUFFSREQSIZEOP, &pu->pu_maxreqlen)) == -1)
		goto out;
#endif
	PU_SETSTATE(pu, PUFFS_STATE_RUNNING);

 out:
	free(pu->pu_kargp);
	pu->pu_kargp = NULL;

	return rv;
}

struct puffs_usermount *
_puffs_init(int develv, struct puffs_ops *pops, const char *mntfromname,
	const char *puffsname, void *priv, uint32_t pflags)
{
	struct puffs_usermount *pu;
	struct puffs_kargs *pargs;
	int sverrno;

	if (develv != PUFFS_DEVEL_LIBVERSION) {
		warnx("puffs_init: mounting with lib version %d, need %d",
		    develv, PUFFS_DEVEL_LIBVERSION);
		errno = EINVAL;
		return NULL;
	}

	pu = malloc(sizeof(struct puffs_usermount));
	if (pu == NULL)
		goto failfree;
	memset(pu, 0, sizeof(struct puffs_usermount));

	pargs = pu->pu_kargp = malloc(sizeof(struct puffs_kargs));
	if (pargs == NULL)
		goto failfree;
	memset(pargs, 0, sizeof(struct puffs_kargs));

	pargs->pa_vers = PUFFSDEVELVERS | PUFFSVERSION;
	pargs->pa_flags = PUFFS_FLAG_KERN(pflags);
	fillvnopmask(pops, pargs->pa_vnopmask);
	(void)strlcpy(pargs->pa_typename, puffsname,
	    sizeof(pargs->pa_typename));
	(void)strlcpy(pargs->pa_mntfromname, mntfromname,
	    sizeof(pargs->pa_mntfromname));

	puffs_zerostatvfs(&pargs->pa_svfsb);
	pargs->pa_root_cookie = NULL;
	pargs->pa_root_vtype = VDIR;
	pargs->pa_root_vsize = 0;
	pargs->pa_root_rdev = 0;
	pargs->pa_maxmsglen = 0;

	pu->pu_flags = pflags;
	pu->pu_ops = *pops;
	free(pops); /* XXX */

	pu->pu_privdata = priv;
	pu->pu_cc_stackshift = PUFFS_CC_STACKSHIFT_DEFAULT;
	LIST_INIT(&pu->pu_pnodelst);
	LIST_INIT(&pu->pu_framectrl.fb_ios);
	LIST_INIT(&pu->pu_ccnukelst);
	TAILQ_INIT(&pu->pu_sched);
	TAILQ_INIT(&pu->pu_exq);

	/* defaults for some user-settable translation functions */
	pu->pu_cmap = NULL; /* identity translation */

	pu->pu_pathbuild = puffs_stdpath_buildpath;
	pu->pu_pathfree = puffs_stdpath_freepath;
	pu->pu_pathcmp = puffs_stdpath_cmppath;
	pu->pu_pathtransform = NULL;
	pu->pu_namemod = NULL;

	pu->pu_errnotify = puffs_defaulterror;

	PU_SETSTATE(pu, PUFFS_STATE_BEFOREMOUNT);

	return pu;

 failfree:
	/* can't unmount() from here for obvious reasons */
	sverrno = errno;
	free(pu);
	errno = sverrno;
	return NULL;
}

/*
 * XXX: there's currently no clean way to request unmount from
 * within the user server, so be very brutal about it.
 */
/*ARGSUSED1*/
int
puffs_exit(struct puffs_usermount *pu, int force)
{
	struct puffs_node *pn;

	force = 1; /* currently */

	if (pu->pu_fd)
		close(pu->pu_fd);

	while ((pn = LIST_FIRST(&pu->pu_pnodelst)) != NULL)
		puffs_pn_put(pn);

	puffs_framev_exit(pu);
	if (pu->pu_state & PU_HASKQ)
		close(pu->pu_kq);
	free(pu);

	return 0; /* always succesful for now, WILL CHANGE */
}

int
puffs_mainloop(struct puffs_usermount *pu)
{
	struct puffs_getreq *pgr = NULL;
	struct puffs_putreq *ppr = NULL;
	struct puffs_framectrl *pfctrl = &pu->pu_framectrl;
	struct puffs_fctrl_io *fio;
	struct puffs_cc *pcc;
	struct kevent *curev, *newevs;
	size_t nchanges;
	int puffsfd, sverrno;
	int ndone;

	assert(puffs_getstate(pu) >= PUFFS_STATE_RUNNING);

	pgr = puffs_req_makeget(pu, puffs_getmaxreqlen(pu), 0);
	if (pgr == NULL)
		goto out;

	ppr = puffs_req_makeput(pu);
	if (ppr == NULL)
		goto out;

	newevs = realloc(pfctrl->evs, (2*pfctrl->nfds+1)*sizeof(struct kevent));
	if (newevs == NULL)
		goto out;
	pfctrl->evs = newevs;

	pu->pu_state |= PU_INLOOP;

	pu->pu_kq = kqueue();
	if (pu->pu_kq == -1)
		goto out;
	pu->pu_state |= PU_HASKQ;

	curev = pfctrl->evs;
	LIST_FOREACH(fio, &pfctrl->fb_ios, fio_entries) {
		EV_SET(curev, fio->io_fd, EVFILT_READ, EV_ADD,
		    0, 0, (uintptr_t)fio);
		curev++;
		EV_SET(curev, fio->io_fd, EVFILT_WRITE, EV_ADD | EV_DISABLE,
		    0, 0, (uintptr_t)fio);
		curev++;
	}
	puffsfd = puffs_getselectable(pu);
	EV_SET(curev, puffsfd, EVFILT_READ, EV_ADD, 0, 0, 0);
	if (kevent(pu->pu_kq, pfctrl->evs, 2*pfctrl->nfds+1,
	    NULL, 0, NULL) == -1)
		goto out;

	while (puffs_getstate(pu) != PUFFS_STATE_UNMOUNTED) {
		if (pu->pu_ml_lfn)
			pu->pu_ml_lfn(pu);

		/*
		 * Do this here, because:
		 *  a) loopfunc might generate some results
		 *  b) it's still "after" event handling (except for round 1)
		 */
		if (puffs_req_putput(ppr) == -1)
			goto out;
		puffs_req_resetput(ppr);

		/* micro optimization: skip kevent syscall if possible */
		if (pfctrl->nfds == 0 && pu->pu_ml_timep == NULL
		    && (pu->pu_state & PU_ASYNCFD) == 0) {
			if (puffs_req_handle(pgr, ppr, 0) == -1)
				goto out;
			continue;
		}

		/* else: do full processing */

		/*
		 * Build list of which to enable/disable in writecheck.
		 * Don't bother worrying about O(n) for now.
		 */
		nchanges = 0;
		LIST_FOREACH(fio, &pfctrl->fb_ios, fio_entries) {
			if (fio->stat & FIO_WRGONE)
				continue;

			/*
			 * Try to write out everything to avoid the
			 * need for enabling EVFILT_WRITE.  The likely
			 * case is that we can fit everything into the
			 * socket buffer.
			 */
			if (puffs_framev_output(pu, pfctrl, fio, ppr)) {
				/* need kernel notify? (error condition) */
				if (puffs_req_putput(ppr) == -1)
					goto out;
				puffs_req_resetput(ppr);
			}

			/* en/disable write checks for kqueue as needed */
			assert((FIO_EN_WRITE(fio) && FIO_RM_WRITE(fio)) == 0);
			if (FIO_EN_WRITE(fio)) {
				EV_SET(&pfctrl->evs[nchanges], fio->io_fd,
				    EVFILT_WRITE, EV_ENABLE, 0, 0,
				    (uintptr_t)fio);
				fio->stat |= FIO_WR;
				nchanges++;
			}
			if (FIO_RM_WRITE(fio)) {
				EV_SET(&pfctrl->evs[nchanges], fio->io_fd,
				    EVFILT_WRITE, EV_DISABLE, 0, 0,
				    (uintptr_t)fio);
				fio->stat &= ~FIO_WR;
				nchanges++;
			}
			assert(nchanges <= pfctrl->nfds);
		}

		ndone = kevent(pu->pu_kq, pfctrl->evs, nchanges,
		    pfctrl->evs, pfctrl->nfds+1, pu->pu_ml_timep);

		if (ndone == -1) {
			if (errno != EINTR)
				goto out;
			else
				continue;
		}

		/* uoptimize */
		if (ndone == 0)
			continue;

		/* iterate over the results */
		for (curev = pfctrl->evs; ndone--; curev++) {
			int what;

			/* get & possibly dispatch events from kernel */
			if (curev->ident == puffsfd) {
				if (puffs_req_handle(pgr, ppr, 0) == -1)
					goto out;
				continue;
			}

			fio = (void *)curev->udata;
			if (curev->flags & EV_ERROR) {
				assert(curev->filter == EVFILT_WRITE);
				fio->stat &= ~FIO_WR;

				/* XXX: how to know if it's a transient error */
				puffs_framev_writeclose(pu, fio,
				    (int)curev->data);
				puffs_framev_notify(fio, PUFFS_FBIO_ERROR);
				continue;
			}

			what = 0;
			if (curev->filter == EVFILT_READ) {
				puffs_framev_input(pu, pfctrl, fio, ppr);
				what |= PUFFS_FBIO_READ;
			}

			else if (curev->filter == EVFILT_WRITE) {
				puffs_framev_output(pu, pfctrl, fio, ppr);
				what |= PUFFS_FBIO_WRITE;
			}
			if (what)
				puffs_framev_notify(fio, what);
		}

		/*
		 * Schedule continuations.
		 */
		while ((pcc = TAILQ_FIRST(&pu->pu_sched)) != NULL) {
			TAILQ_REMOVE(&pu->pu_sched, pcc, entries);
			puffs_goto(pcc);
		}

		/*
		 * Really free fd's now that we don't have references
		 * to them.
		 */
		while ((fio = LIST_FIRST(&pfctrl->fb_ios_rmlist)) != NULL) {
			LIST_REMOVE(fio, fio_entries);
			free(fio);
		}
	}
	errno = 0;
	puffs_req_putput(ppr);

 out:
	/* store the real error for a while */
	sverrno = errno;

	if (ppr)
		puffs_req_destroyput(ppr);
	if (pgr)
		puffs_req_destroyget(pgr);

	errno = sverrno;
	if (errno)
		return -1;
	else
		return 0;
}
