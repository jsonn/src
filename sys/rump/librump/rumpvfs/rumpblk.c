/*	$NetBSD: rumpblk.c,v 1.2.2.1 2009/05/13 17:22:58 jym Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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

/*
 * Block device emulation.  Presents a block device interface and
 * uses rumpuser system calls to satisfy I/O requests.
 *
 * We provide fault injection.  The driver can be made to fail
 * I/O occasionally.
 *
 * The driver also provides an optimization for regular files by
 * using memory-mapped I/O.  This avoids kernel access for every
 * I/O operation.  It also gives finer-grained control of how to
 * flush data.  Additionally, in case the rump kernel dumps core,
 * we get way less carnage.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rumpblk.c,v 1.2.2.1 2009/05/13 17:22:58 jym Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/condvar.h>
#include <sys/disklabel.h>
#include <sys/evcnt.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <rump/rumpuser.h>

#include "rump_private.h"
#include "rump_vfs_private.h"

#if 0
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

/* Default: 16 x 1MB windows */
unsigned memwinsize = (1<<20);
unsigned memwincnt = 16;

#define STARTWIN(off)		((off) & ~(memwinsize-1))
#define INWIN(win,off)		((win)->win_off == STARTWIN(off))
#define WINSIZE(rblk, win)	(MIN((rblk->rblk_size-win->win_off),memwinsize))
#define WINVALID(win)		((win)->win_off != (off_t)-1)
#define WINVALIDATE(win)	((win)->win_off = (off_t)-1)
struct blkwin {
	off_t win_off;
	void *win_mem;
	int win_refcnt;

	TAILQ_ENTRY(blkwin) win_lru;
};

#define RUMPBLK_SIZE 16
static struct rblkdev {
	char *rblk_path;
	int rblk_fd;
#ifdef HAS_ODIRECT
	int rblk_dfd;
#endif

	/* for mmap */
	int rblk_mmflags;
	kmutex_t rblk_memmtx;
	kcondvar_t rblk_memcv;
	TAILQ_HEAD(winlru, blkwin) rblk_lruq;
	size_t rblk_size;
	bool rblk_waiting;

	struct partition *rblk_curpi;
	struct partition rblk_pi;
	struct disklabel rblk_dl;
} minors[RUMPBLK_SIZE];

static struct evcnt ev_io_total;
static struct evcnt ev_io_async;

static struct evcnt ev_memblk_hits;
static struct evcnt ev_memblk_busy;

static struct evcnt ev_bwrite_total;
static struct evcnt ev_bwrite_async;
static struct evcnt ev_bread_total;

dev_type_open(rumpblk_open);
dev_type_close(rumpblk_close);
dev_type_read(rumpblk_read);
dev_type_write(rumpblk_write);
dev_type_ioctl(rumpblk_ioctl);
dev_type_strategy(rumpblk_strategy);
dev_type_strategy(rumpblk_strategy_fail);
dev_type_dump(rumpblk_dump);
dev_type_size(rumpblk_size);

static const struct bdevsw rumpblk_bdevsw = {
	rumpblk_open, rumpblk_close, rumpblk_strategy, rumpblk_ioctl,
	nodump, nosize, D_DISK
};

static const struct bdevsw rumpblk_bdevsw_fail = {
	rumpblk_open, rumpblk_close, rumpblk_strategy_fail, rumpblk_ioctl,
	nodump, nosize, D_DISK
};

static const struct cdevsw rumpblk_cdevsw = {
	rumpblk_open, rumpblk_close, rumpblk_read, rumpblk_write,
	rumpblk_ioctl, nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

/* fail every n out of BLKFAIL_MAX */
#define BLKFAIL_MAX 10000
static int blkfail;
static unsigned randstate;

static struct blkwin *
getwindow(struct rblkdev *rblk, off_t off, int *wsize, int *error)
{
	struct blkwin *win;

	mutex_enter(&rblk->rblk_memmtx);
 retry:
	/* search for window */
	TAILQ_FOREACH(win, &rblk->rblk_lruq, win_lru) {
		if (INWIN(win, off) && WINVALID(win))
			break;
	}

	/* found?  return */
	if (win) {
		ev_memblk_hits.ev_count++;
		TAILQ_REMOVE(&rblk->rblk_lruq, win, win_lru);
		goto good;
	}

	/*
	 * Else, create new window.  If the least recently used is not
	 * currently in use, reuse that.  Otherwise we need to wait.
	 */
	win = TAILQ_LAST(&rblk->rblk_lruq, winlru);
	if (win->win_refcnt == 0) {
		TAILQ_REMOVE(&rblk->rblk_lruq, win, win_lru);
		mutex_exit(&rblk->rblk_memmtx);

		if (WINVALID(win)) {
			DPRINTF(("win %p, unmap mem %p, off 0x%" PRIx64 "\n",
			    win, win->win_mem, win->win_off));
			rumpuser_unmap(win->win_mem, WINSIZE(rblk, win));
			WINVALIDATE(win);
		}

		win->win_off = STARTWIN(off);
		win->win_mem = rumpuser_filemmap(rblk->rblk_fd, win->win_off,
		    WINSIZE(rblk, win), rblk->rblk_mmflags, error);
		DPRINTF(("win %p, off 0x%" PRIx64 ", mem %p\n",
		    win, win->win_off, win->win_mem));

		mutex_enter(&rblk->rblk_memmtx);
		if (win->win_mem == NULL) {
			WINVALIDATE(win);
			TAILQ_INSERT_TAIL(&rblk->rblk_lruq, win, win_lru);
			mutex_exit(&rblk->rblk_memmtx);
			return NULL;
		}
	} else {
		DPRINTF(("memwin wait\n"));
		ev_memblk_busy.ev_count++;

		rblk->rblk_waiting = true;
		cv_wait(&rblk->rblk_memcv, &rblk->rblk_memmtx);
		goto retry;
	}

 good:
	KASSERT(win);
	win->win_refcnt++;
	TAILQ_INSERT_HEAD(&rblk->rblk_lruq, win, win_lru);
	mutex_exit(&rblk->rblk_memmtx);
	*wsize = MIN(*wsize, memwinsize - (off-win->win_off));
	KASSERT(*wsize);

	return win;
}

static void
putwindow(struct rblkdev *rblk, struct blkwin *win)
{

	mutex_enter(&rblk->rblk_memmtx);
	if (--win->win_refcnt == 0 && rblk->rblk_waiting) {
		rblk->rblk_waiting = false;
		cv_signal(&rblk->rblk_memcv);
	}
	KASSERT(win->win_refcnt >= 0);
	mutex_exit(&rblk->rblk_memmtx);
}

static void
wincleanup(struct rblkdev *rblk)
{
	struct blkwin *win;

	while ((win = TAILQ_FIRST(&rblk->rblk_lruq)) != NULL) {
		TAILQ_REMOVE(&rblk->rblk_lruq, win, win_lru);
		if (WINVALID(win)) {
			DPRINTF(("cleanup win %p addr %p\n",
			    win, win->win_mem));
			rumpuser_unmap(win->win_mem, WINSIZE(rblk, win));
		}
		kmem_free(win, sizeof(*win));
	}
	rblk->rblk_mmflags = 0;
}

int
rumpblk_init(void)
{
	char buf[64];
	int rumpblk = RUMPBLK;
	unsigned tmp;
	int error, i;

	if (rumpuser_getenv("RUMP_BLKFAIL", buf, sizeof(buf), &error) == 0) {
		blkfail = strtoul(buf, NULL, 10);
		/* fail everything */
		if (blkfail > BLKFAIL_MAX)
			blkfail = BLKFAIL_MAX;
		if (rumpuser_getenv("RUMP_BLKFAIL_SEED", buf, sizeof(buf),
		    &error) == 0) {
			randstate = strtoul(buf, NULL, 10);
		} else {
			randstate = arc4random();
		}
		printf("rumpblk: FAULT INJECTION ACTIVE! fail %d/%d. "
		    "seed %u\n", blkfail, BLKFAIL_MAX, randstate);
	} else {
		blkfail = 0;
	}

	if (rumpuser_getenv("RUMP_BLKWINSIZE", buf, sizeof(buf), &error) == 0) {
		printf("rumpblk: ");
		tmp = strtoul(buf, NULL, 10);
		if (tmp && !(tmp & (tmp-1)))
			memwinsize = tmp;
		else
			printf("invalid RUMP_BLKWINSIZE %d, ", tmp);
		printf("using %d for memwinsize\n", memwinsize);
	}
	if (rumpuser_getenv("RUMP_BLKWINCOUNT", buf, sizeof(buf), &error) == 0){
		printf("rumpblk: ");
		tmp = strtoul(buf, NULL, 10);
		if (tmp)
			memwincnt = tmp;
		else
			printf("invalid RUMP_BLKWINCOUNT %d, ", tmp);
		printf("using %d for memwincount\n", memwincnt);
	}

	memset(minors, 0, sizeof(minors));
	for (i = 0; i < RUMPBLK_SIZE; i++) {
		mutex_init(&minors[i].rblk_memmtx, MUTEX_DEFAULT, IPL_NONE);
		cv_init(&minors[i].rblk_memcv, "rblkmcv");
	}

	evcnt_attach_dynamic(&ev_io_total, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "rumpblk I/O reqs");
	evcnt_attach_dynamic(&ev_io_async, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "rumpblk async I/O");

	evcnt_attach_dynamic(&ev_bread_total, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "rumpblk bytes read");
	evcnt_attach_dynamic(&ev_bwrite_total, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "rumpblk bytes written");
	evcnt_attach_dynamic(&ev_bwrite_async, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "rumpblk bytes written async");

	evcnt_attach_dynamic(&ev_memblk_hits, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "memblk window hits");
	evcnt_attach_dynamic(&ev_memblk_busy, EVCNT_TYPE_MISC, NULL,
	    "rumpblk", "memblk all windows busy");

	if (blkfail) {
		return devsw_attach("rumpblk", &rumpblk_bdevsw_fail, &rumpblk,
		    &rumpblk_cdevsw, &rumpblk);
	} else {
		return devsw_attach("rumpblk", &rumpblk_bdevsw, &rumpblk,
		    &rumpblk_cdevsw, &rumpblk);
	}
}

int
rumpblk_register(const char *path)
{
	size_t len;
	int i;

	for (i = 0; i < RUMPBLK_SIZE; i++)
		if (minors[i].rblk_path && strcmp(minors[i].rblk_path, path)==0)
			return i;

	for (i = 0; i < RUMPBLK_SIZE; i++)
		if (minors[i].rblk_path == NULL)
			break;
	if (i == RUMPBLK_SIZE)
		return -1;

	len = strlen(path);
	minors[i].rblk_path = malloc(len + 1, M_TEMP, M_WAITOK);
	strcpy(minors[i].rblk_path, path);
	minors[i].rblk_fd = -1;
	return i;
}

int
rumpblk_open(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct rblkdev *rblk = &minors[minor(dev)];
	uint64_t fsize;
	int ft, dummy;
	int error, fd;

	KASSERT(rblk->rblk_fd == -1);
	fd = rumpuser_open(rblk->rblk_path, OFLAGS(flag), &error);
	if (error)
		return error;

	if (rumpuser_getfileinfo(rblk->rblk_path, &fsize, &ft, &error) == -1) {
		rumpuser_close(fd, &dummy);
		return error;
	}

#ifdef HAS_ODIRECT
	rblk->rblk_dfd = rumpuser_open(rblk->rblk_path,
	    OFLAGS(flag) | O_DIRECT, &error);
	if (error)
		return error;
#endif

	if (ft == RUMPUSER_FT_REG) {
		struct blkwin *win;
		int i, winsize;

		/*
		 * Use mmap to access a regular file.  Allocate and
		 * cache initial windows here.  Failure to allocate one
		 * means fallback to read/write i/o.
		 */

		rblk->rblk_mmflags = 0;
		if (flag & FREAD)
			rblk->rblk_mmflags |= RUMPUSER_FILEMMAP_READ;
		if (flag & FWRITE) {
			rblk->rblk_mmflags |= RUMPUSER_FILEMMAP_WRITE;
			rblk->rblk_mmflags |= RUMPUSER_FILEMMAP_SHARED;
		}

		TAILQ_INIT(&rblk->rblk_lruq);
		rblk->rblk_size = fsize;
		rblk->rblk_fd = fd;

		for (i = 0; i < memwincnt && i * memwinsize < fsize; i++) {
			win = kmem_zalloc(sizeof(*win), KM_SLEEP);
			WINVALIDATE(win);
			TAILQ_INSERT_TAIL(&rblk->rblk_lruq, win, win_lru);

			/*
			 * Allocate first windows.  Here we just generally
			 * make sure a) we can mmap at all b) we have the
			 * necessary VA available
			 */
			winsize = 1;
			win = getwindow(rblk, i*memwinsize, &winsize, &error); 
			if (win) {
				putwindow(rblk, win);
			} else {
				wincleanup(rblk);
				break;
			}
		}

		memset(&rblk->rblk_dl, 0, sizeof(rblk->rblk_dl));
		rblk->rblk_pi.p_size = fsize >> DEV_BSHIFT;
		rblk->rblk_dl.d_secsize = DEV_BSIZE;
		rblk->rblk_curpi = &rblk->rblk_pi;
	} else {
		if (rumpuser_ioctl(fd, DIOCGDINFO, &rblk->rblk_dl,
		    &error) == -1) {
			KASSERT(error);
			rumpuser_close(fd, &dummy);
			return error;
		}

		rblk->rblk_fd = fd;
		rblk->rblk_curpi = &rblk->rblk_dl.d_partitions[0];
	}

	KASSERT(rblk->rblk_fd != -1);
	return 0;
}

int
rumpblk_close(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct rblkdev *rblk = &minors[minor(dev)];
	int dummy;

	if (rblk->rblk_mmflags)
		wincleanup(rblk);
	rumpuser_fsync(rblk->rblk_fd, &dummy);
	rumpuser_close(rblk->rblk_fd, &dummy);
	rblk->rblk_fd = -1;

	return 0;
}

int
rumpblk_ioctl(dev_t dev, u_long xfer, void *addr, int flag, struct lwp *l)
{
	struct rblkdev *rblk = &minors[minor(dev)];
	int rv, error;

	if (xfer == DIOCGPART) {
		struct partinfo *pi = (struct partinfo *)addr;

		pi->part = rblk->rblk_curpi;
		pi->disklab = &rblk->rblk_dl;

		return 0;
	}

	rv = rumpuser_ioctl(rblk->rblk_fd, xfer, addr, &error);
	if (rv == -1)
		return error;

	return 0;
}

int
rumpblk_read(dev_t dev, struct uio *uio, int flags)
{

	panic("%s: unimplemented", __func__);
}

int
rumpblk_write(dev_t dev, struct uio *uio, int flags)
{

	panic("%s: unimplemented", __func__);
}

static void
dostrategy(struct buf *bp)
{
	struct rblkdev *rblk = &minors[minor(bp->b_dev)];
	off_t off;
	int async = bp->b_flags & B_ASYNC;
	int error;

	/* collect statistics */
	ev_io_total.ev_count++;
	if (async)
		ev_io_async.ev_count++;
	if (BUF_ISWRITE(bp)) {
		ev_bwrite_total.ev_count += bp->b_bcount;
		if (async)
			ev_bwrite_async.ev_count += bp->b_bcount;
	} else {
		ev_bread_total.ev_count++;
	}

	off = bp->b_blkno << DEV_BSHIFT;
	/*
	 * Do bounds checking if we're working on a file.  Otherwise
	 * invalid file systems might attempt to read beyond EOF.  This
	 * is bad(tm) especially on mmapped images.  This is essentially
	 * the kernel bounds_check() routines.
	 */
	if (rblk->rblk_size && off + bp->b_bcount > rblk->rblk_size) {
		int64_t sz = rblk->rblk_size - off;

		/* EOF */
		if (sz == 0) {
			rump_biodone(bp, 0, 0);
			return;
		}
		/* beyond EOF ==> error */
		if (sz < 0) {
			rump_biodone(bp, 0, EINVAL);
			return;
		}

		/* truncate to device size */
		bp->b_bcount = sz;
	}

	DPRINTF(("rumpblk_strategy: 0x%x bytes %s off 0x%" PRIx64
	    " (0x%" PRIx64 " - 0x%" PRIx64 "), %ssync\n",
	    bp->b_bcount, BUF_ISREAD(bp) ? "READ" : "WRITE",
	    off, off, (off + bp->b_bcount), async ? "a" : ""));

	/* mmap?  handle here and return */
	if (rblk->rblk_mmflags) {
		struct blkwin *win;
		int winsize, iodone;
		uint8_t *ioaddr, *bufaddr;

		for (iodone = 0; iodone < bp->b_bcount;
		    iodone += winsize, off += winsize) {
			winsize = bp->b_bcount - iodone;
			win = getwindow(rblk, off, &winsize, &error); 
			if (win == NULL) {
				rump_biodone(bp, iodone, error);
				return;
			}

			ioaddr = (uint8_t *)win->win_mem + (off-STARTWIN(off));
			bufaddr = (uint8_t *)bp->b_data + iodone;

			DPRINTF(("strat: %p off 0x%" PRIx64
			    ", ioaddr %p (%p)/buf %p\n", win,
			    win->win_off, ioaddr, win->win_mem, bufaddr));
			if (BUF_ISREAD(bp)) {
				memcpy(bufaddr, ioaddr, winsize);
			} else {
				memcpy(ioaddr, bufaddr, winsize);
			}

			/* synchronous write, sync bits back to disk */
			if (BUF_ISWRITE(bp) && !async) {
				rumpuser_memsync(ioaddr, winsize, &error);
			}
			putwindow(rblk, win);
		}

		rump_biodone(bp, bp->b_bcount, 0);
		return;
	}

	/*
	 * Do I/O.  We have different paths for async and sync I/O.
	 * Async I/O is done by passing a request to rumpuser where
	 * it is executed.  The rumpuser routine then calls
	 * biodone() to signal any waiters in the kernel.  I/O's are
	 * executed in series.  Technically executing them in parallel
	 * would produce better results, but then we'd need either
	 * more threads or posix aio.  Maybe worth investigating
	 * this later.
	 * 
	 * Using bufq here might be a good idea.
	 */

	if (rump_threads) {
		struct rumpuser_aio *rua;
		int op, fd;

		fd = rblk->rblk_fd;
		if (BUF_ISREAD(bp)) {
			op = RUA_OP_READ;
		} else {
			op = RUA_OP_WRITE;
			if (!async) {
				/* O_DIRECT not fully automatic yet */
#ifdef HAS_ODIRECT
				if ((off & (DEV_BSIZE-1)) == 0
				    && ((intptr_t)bp->b_data&(DEV_BSIZE-1)) == 0
				    && (bp->b_bcount & (DEV_BSIZE-1)) == 0)
					fd = rblk->rblk_dfd;
				else
#endif
					op |= RUA_OP_SYNC;
			}
		}

		rumpuser_mutex_enter(&rumpuser_aio_mtx);
		while ((rumpuser_aio_head+1) % N_AIOS == rumpuser_aio_tail) {
			rumpuser_cv_wait(&rumpuser_aio_cv, &rumpuser_aio_mtx);
		}

		rua = &rumpuser_aios[rumpuser_aio_head];
		KASSERT(rua->rua_bp == NULL);
		rua->rua_fd = fd;
		rua->rua_data = bp->b_data;
		rua->rua_dlen = bp->b_bcount;
		rua->rua_off = off;
		rua->rua_bp = bp;
		rua->rua_op = op;

		/* insert into queue & signal */
		rumpuser_aio_head = (rumpuser_aio_head+1) % N_AIOS;
		rumpuser_cv_signal(&rumpuser_aio_cv);
		rumpuser_mutex_exit(&rumpuser_aio_mtx);
	} else {
		if (BUF_ISREAD(bp)) {
			rumpuser_read_bio(rblk->rblk_fd, bp->b_data,
			    bp->b_bcount, off, rump_biodone, bp);
		} else {
			rumpuser_write_bio(rblk->rblk_fd, bp->b_data,
			    bp->b_bcount, off, rump_biodone, bp);
		}
		if (BUF_ISWRITE(bp) && !async)
			rumpuser_fsync(rblk->rblk_fd, &error);
	}
}

void
rumpblk_strategy(struct buf *bp)
{

	dostrategy(bp);
}

/*
 * Simple random number generator.  This is private so that we can
 * very repeatedly control which blocks will fail.
 *
 * <mlelstv> pooka, rand()
 * <mlelstv> [paste]
 */
static unsigned
gimmerand(void)
{

	return (randstate = randstate * 1103515245 + 12345) % (0x80000000L);
}

/*
 * Block device with very simple fault injection.  Fails every
 * n out of BLKFAIL_MAX I/O with EIO.  n is determined by the env
 * variable RUMP_BLKFAIL.
 */
void
rumpblk_strategy_fail(struct buf *bp)
{

	if (gimmerand() % BLKFAIL_MAX >= blkfail) {
		dostrategy(bp);
	} else { 
		printf("block fault injection: failing I/O on block %lld\n",
		    (long long)bp->b_blkno);
		bp->b_error = EIO;
		biodone(bp);
	}
}
