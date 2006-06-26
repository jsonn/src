/*	$NetBSD: sys_pipe.c,v 1.71.2.2 2006/06/26 12:52:57 yamt Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * Copyright (c) 1996 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $FreeBSD: src/sys/kern/sys_pipe.c,v 1.95 2002/03/09 22:06:31 alfred Exp $
 */

/*
 * This file contains a high-performance replacement for the socket-based
 * pipes scheme originally used in FreeBSD/4.4Lite.  It does not support
 * all features of sockets, but does do everything that pipes normally
 * do.
 *
 * Adaption for NetBSD UVM, including uvm_loan() based direct write, was
 * written by Jaromir Dolecek.
 */

/*
 * This code has two modes of operation, a small write mode and a large
 * write mode.  The small write mode acts like conventional pipes with
 * a kernel buffer.  If the buffer is less than PIPE_MINDIRECT, then the
 * "normal" pipe buffering is done.  If the buffer is between PIPE_MINDIRECT
 * and PIPE_SIZE in size it is mapped read-only into the kernel address space
 * using the UVM page loan facility from where the receiving process can copy
 * the data directly from the pages in the sending process.
 *
 * The constant PIPE_MINDIRECT is chosen to make sure that buffering will
 * happen for small transfers so that the system will not spend all of
 * its time context switching.  PIPE_SIZE is constrained by the
 * amount of kernel virtual memory.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_pipe.c,v 1.71.2.2 2006/06/26 12:52:57 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/ttycom.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/lock.h>
#include <sys/select.h>
#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>
#include <uvm/uvm.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/kauth.h>

#include <sys/pipe.h>

/*
 * Use this define if you want to disable *fancy* VM things.  Expect an
 * approx 30% decrease in transfer rate.
 */
/* #define PIPE_NODIRECT */

/*
 * interfaces to the outside world
 */
static int pipe_read(struct file *fp, off_t *offset, struct uio *uio,
		kauth_cred_t cred, int flags);
static int pipe_write(struct file *fp, off_t *offset, struct uio *uio,
		kauth_cred_t cred, int flags);
static int pipe_close(struct file *fp, struct lwp *l);
static int pipe_poll(struct file *fp, int events, struct lwp *l);
static int pipe_kqfilter(struct file *fp, struct knote *kn);
static int pipe_stat(struct file *fp, struct stat *sb, struct lwp *l);
static int pipe_ioctl(struct file *fp, u_long cmd, void *data,
		struct lwp *l);

static const struct fileops pipeops = {
	pipe_read, pipe_write, pipe_ioctl, fnullop_fcntl, pipe_poll,
	pipe_stat, pipe_close, pipe_kqfilter
};

/*
 * Default pipe buffer size(s), this can be kind-of large now because pipe
 * space is pageable.  The pipe code will try to maintain locality of
 * reference for performance reasons, so small amounts of outstanding I/O
 * will not wipe the cache.
 */
#define MINPIPESIZE (PIPE_SIZE/3)
#define MAXPIPESIZE (2*PIPE_SIZE/3)

/*
 * Maximum amount of kva for pipes -- this is kind-of a soft limit, but
 * is there so that on large systems, we don't exhaust it.
 */
#define MAXPIPEKVA (8*1024*1024)
static int maxpipekva = MAXPIPEKVA;

/*
 * Limit for direct transfers, we cannot, of course limit
 * the amount of kva for pipes in general though.
 */
#define LIMITPIPEKVA (16*1024*1024)
static int limitpipekva = LIMITPIPEKVA;

/*
 * Limit the number of "big" pipes
 */
#define LIMITBIGPIPES  32
static int maxbigpipes = LIMITBIGPIPES;
static int nbigpipe = 0;

/*
 * Amount of KVA consumed by pipe buffers.
 */
static int amountpipekva = 0;

MALLOC_DEFINE(M_PIPE, "pipe", "Pipe structures");

static void pipeclose(struct file *fp, struct pipe *pipe);
static void pipe_free_kmem(struct pipe *pipe);
static int pipe_create(struct pipe **pipep, int allockva);
static int pipelock(struct pipe *pipe, int catch);
static inline void pipeunlock(struct pipe *pipe);
static void pipeselwakeup(struct pipe *pipe, struct pipe *sigp, int code);
#ifndef PIPE_NODIRECT
static int pipe_direct_write(struct file *fp, struct pipe *wpipe,
    struct uio *uio);
#endif
static int pipespace(struct pipe *pipe, int size);

#ifndef PIPE_NODIRECT
static int pipe_loan_alloc(struct pipe *, int);
static void pipe_loan_free(struct pipe *);
#endif /* PIPE_NODIRECT */

static POOL_INIT(pipe_pool, sizeof(struct pipe), 0, 0, 0, "pipepl",
    &pool_allocator_nointr);

/*
 * The pipe system call for the DTYPE_PIPE type of pipes
 */

/* ARGSUSED */
int
sys_pipe(struct lwp *l, void *v, register_t *retval)
{
	struct file *rf, *wf;
	struct pipe *rpipe, *wpipe;
	int fd, error;
	struct proc *p;

	p = l->l_proc;
	rpipe = wpipe = NULL;
	if (pipe_create(&rpipe, 1) || pipe_create(&wpipe, 0)) {
		pipeclose(NULL, rpipe);
		pipeclose(NULL, wpipe);
		return (ENFILE);
	}

	/*
	 * Note: the file structure returned from falloc() is marked
	 * as 'larval' initially. Unless we mark it as 'mature' by
	 * FILE_SET_MATURE(), any attempt to do anything with it would
	 * return EBADF, including e.g. dup(2) or close(2). This avoids
	 * file descriptor races if we block in the second falloc().
	 */

	error = falloc(p, &rf, &fd);
	if (error)
		goto free2;
	retval[0] = fd;
	rf->f_flag = FREAD;
	rf->f_type = DTYPE_PIPE;
	rf->f_data = (caddr_t)rpipe;
	rf->f_ops = &pipeops;

	error = falloc(p, &wf, &fd);
	if (error)
		goto free3;
	retval[1] = fd;
	wf->f_flag = FWRITE;
	wf->f_type = DTYPE_PIPE;
	wf->f_data = (caddr_t)wpipe;
	wf->f_ops = &pipeops;

	rpipe->pipe_peer = wpipe;
	wpipe->pipe_peer = rpipe;

	FILE_SET_MATURE(rf);
	FILE_SET_MATURE(wf);
	FILE_UNUSE(rf, l);
	FILE_UNUSE(wf, l);
	return (0);
free3:
	FILE_UNUSE(rf, l);
	ffree(rf);
	fdremove(p->p_fd, retval[0]);
free2:
	pipeclose(NULL, wpipe);
	pipeclose(NULL, rpipe);

	return (error);
}

/*
 * Allocate kva for pipe circular buffer, the space is pageable
 * This routine will 'realloc' the size of a pipe safely, if it fails
 * it will retain the old buffer.
 * If it fails it will return ENOMEM.
 */
static int
pipespace(struct pipe *pipe, int size)
{
	caddr_t buffer;
	/*
	 * Allocate pageable virtual address space. Physical memory is
	 * allocated on demand.
	 */
	buffer = (caddr_t) uvm_km_alloc(kernel_map, round_page(size), 0,
	    UVM_KMF_PAGEABLE);
	if (buffer == NULL)
		return (ENOMEM);

	/* free old resources if we're resizing */
	pipe_free_kmem(pipe);
	pipe->pipe_buffer.buffer = buffer;
	pipe->pipe_buffer.size = size;
	pipe->pipe_buffer.in = 0;
	pipe->pipe_buffer.out = 0;
	pipe->pipe_buffer.cnt = 0;
	amountpipekva += pipe->pipe_buffer.size;
	return (0);
}

/*
 * Initialize and allocate VM and memory for pipe.
 */
static int
pipe_create(struct pipe **pipep, int allockva)
{
	struct pipe *pipe;
	int error;

	pipe = *pipep = pool_get(&pipe_pool, PR_WAITOK);

	/* Initialize */
	memset(pipe, 0, sizeof(struct pipe));
	pipe->pipe_state = PIPE_SIGNALR;

	getmicrotime(&pipe->pipe_ctime);
	pipe->pipe_atime = pipe->pipe_ctime;
	pipe->pipe_mtime = pipe->pipe_ctime;
	simple_lock_init(&pipe->pipe_slock);

	if (allockva && (error = pipespace(pipe, PIPE_SIZE)))
		return (error);

	return (0);
}


/*
 * Lock a pipe for I/O, blocking other access
 * Called with pipe spin lock held.
 * Return with pipe spin lock released on success.
 */
static int
pipelock(struct pipe *pipe, int catch)
{

	LOCK_ASSERT(simple_lock_held(&pipe->pipe_slock));

	while (pipe->pipe_state & PIPE_LOCKFL) {
		int error;
		const int pcatch = catch ? PCATCH : 0;

		pipe->pipe_state |= PIPE_LWANT;
		error = ltsleep(pipe, PSOCK | pcatch, "pipelk", 0,
		    &pipe->pipe_slock);
		if (error != 0)
			return error;
	}

	pipe->pipe_state |= PIPE_LOCKFL;
	simple_unlock(&pipe->pipe_slock);

	return 0;
}

/*
 * unlock a pipe I/O lock
 */
static inline void
pipeunlock(struct pipe *pipe)
{

	KASSERT(pipe->pipe_state & PIPE_LOCKFL);

	pipe->pipe_state &= ~PIPE_LOCKFL;
	if (pipe->pipe_state & PIPE_LWANT) {
		pipe->pipe_state &= ~PIPE_LWANT;
		wakeup(pipe);
	}
}

/*
 * Select/poll wakup. This also sends SIGIO to peer connected to
 * 'sigpipe' side of pipe.
 */
static void
pipeselwakeup(struct pipe *selp, struct pipe *sigp, int code)
{
	int band;

	selnotify(&selp->pipe_sel, NOTE_SUBMIT);

	if (sigp == NULL || (sigp->pipe_state & PIPE_ASYNC) == 0)
		return;

	switch (code) {
	case POLL_IN:
		band = POLLIN|POLLRDNORM;
		break;
	case POLL_OUT:
		band = POLLOUT|POLLWRNORM;
		break;
	case POLL_HUP:
		band = POLLHUP;
		break;
#if POLL_HUP != POLL_ERR
	case POLL_ERR:
		band = POLLERR;
		break;
#endif
	default:
		band = 0;
#ifdef DIAGNOSTIC
		printf("bad siginfo code %d in pipe notification.\n", code);
#endif
		break;
	}

	fownsignal(sigp->pipe_pgid, SIGIO, code, band, selp);
}

/* ARGSUSED */
static int
pipe_read(struct file *fp, off_t *offset, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	struct pipe *rpipe = (struct pipe *) fp->f_data;
	struct pipebuf *bp = &rpipe->pipe_buffer;
	int error;
	size_t nread = 0;
	size_t size;
	size_t ocnt;

	PIPE_LOCK(rpipe);
	++rpipe->pipe_busy;
	ocnt = bp->cnt;

again:
	error = pipelock(rpipe, 1);
	if (error)
		goto unlocked_error;

	while (uio->uio_resid) {
		/*
		 * normal pipe buffer receive
		 */
		if (bp->cnt > 0) {
			size = bp->size - bp->out;
			if (size > bp->cnt)
				size = bp->cnt;
			if (size > uio->uio_resid)
				size = uio->uio_resid;

			error = uiomove(&bp->buffer[bp->out], size, uio);
			if (error)
				break;

			bp->out += size;
			if (bp->out >= bp->size)
				bp->out = 0;

			bp->cnt -= size;

			/*
			 * If there is no more to read in the pipe, reset
			 * its pointers to the beginning.  This improves
			 * cache hit stats.
			 */
			if (bp->cnt == 0) {
				bp->in = 0;
				bp->out = 0;
			}
			nread += size;
#ifndef PIPE_NODIRECT
		} else if ((rpipe->pipe_state & PIPE_DIRECTR) != 0) {
			/*
			 * Direct copy, bypassing a kernel buffer.
			 */
			caddr_t	va;

			KASSERT(rpipe->pipe_state & PIPE_DIRECTW);

			size = rpipe->pipe_map.cnt;
			if (size > uio->uio_resid)
				size = uio->uio_resid;

			va = (caddr_t) rpipe->pipe_map.kva +
			    rpipe->pipe_map.pos;
			error = uiomove(va, size, uio);
			if (error)
				break;
			nread += size;
			rpipe->pipe_map.pos += size;
			rpipe->pipe_map.cnt -= size;
			if (rpipe->pipe_map.cnt == 0) {
				PIPE_LOCK(rpipe);
				rpipe->pipe_state &= ~PIPE_DIRECTR;
				wakeup(rpipe);
				PIPE_UNLOCK(rpipe);
			}
#endif
		} else {
			/*
			 * Break if some data was read.
			 */
			if (nread > 0)
				break;

			PIPE_LOCK(rpipe);

			/*
			 * detect EOF condition
			 * read returns 0 on EOF, no need to set error
			 */
			if (rpipe->pipe_state & PIPE_EOF) {
				PIPE_UNLOCK(rpipe);
				break;
			}

			/*
			 * don't block on non-blocking I/O
			 */
			if (fp->f_flag & FNONBLOCK) {
				PIPE_UNLOCK(rpipe);
				error = EAGAIN;
				break;
			}

			/*
			 * Unlock the pipe buffer for our remaining processing.
			 * We will either break out with an error or we will
			 * sleep and relock to loop.
			 */
			pipeunlock(rpipe);

			/*
			 * The PIPE_DIRECTR flag is not under the control
			 * of the long-term lock (see pipe_direct_write()),
			 * so re-check now while holding the spin lock.
			 */
			if ((rpipe->pipe_state & PIPE_DIRECTR) != 0)
				goto again;

			/*
			 * We want to read more, wake up select/poll.
			 */
			pipeselwakeup(rpipe, rpipe->pipe_peer, POLL_IN);

			/*
			 * If the "write-side" is blocked, wake it up now.
			 */
			if (rpipe->pipe_state & PIPE_WANTW) {
				rpipe->pipe_state &= ~PIPE_WANTW;
				wakeup(rpipe);
			}

			/* Now wait until the pipe is filled */
			rpipe->pipe_state |= PIPE_WANTR;
			error = ltsleep(rpipe, PSOCK | PCATCH,
					"piperd", 0, &rpipe->pipe_slock);
			if (error != 0)
				goto unlocked_error;
			goto again;
		}
	}

	if (error == 0)
		getmicrotime(&rpipe->pipe_atime);

	PIPE_LOCK(rpipe);
	pipeunlock(rpipe);

unlocked_error:
	--rpipe->pipe_busy;

	/*
	 * PIPE_WANTCLOSE processing only makes sense if pipe_busy is 0.
	 */
	if ((rpipe->pipe_busy == 0) && (rpipe->pipe_state & PIPE_WANTCLOSE)) {
		rpipe->pipe_state &= ~(PIPE_WANTCLOSE|PIPE_WANTW);
		wakeup(rpipe);
	} else if (bp->cnt < MINPIPESIZE) {
		/*
		 * Handle write blocking hysteresis.
		 */
		if (rpipe->pipe_state & PIPE_WANTW) {
			rpipe->pipe_state &= ~PIPE_WANTW;
			wakeup(rpipe);
		}
	}

	/*
	 * If anything was read off the buffer, signal to the writer it's
	 * possible to write more data. Also send signal if we are here for the
	 * first time after last write.
	 */
	if ((bp->size - bp->cnt) >= PIPE_BUF
	    && (ocnt != bp->cnt || (rpipe->pipe_state & PIPE_SIGNALR))) {
		pipeselwakeup(rpipe, rpipe->pipe_peer, POLL_OUT);
		rpipe->pipe_state &= ~PIPE_SIGNALR;
	}

	PIPE_UNLOCK(rpipe);
	return (error);
}

#ifndef PIPE_NODIRECT
/*
 * Allocate structure for loan transfer.
 */
static int
pipe_loan_alloc(struct pipe *wpipe, int npages)
{
	vsize_t len;

	len = (vsize_t)npages << PAGE_SHIFT;
	wpipe->pipe_map.kva = uvm_km_alloc(kernel_map, len, 0,
	    UVM_KMF_VAONLY | UVM_KMF_WAITVA);
	if (wpipe->pipe_map.kva == 0)
		return (ENOMEM);

	amountpipekva += len;
	wpipe->pipe_map.npages = npages;
	wpipe->pipe_map.pgs = malloc(npages * sizeof(struct vm_page *), M_PIPE,
	    M_WAITOK);
	return (0);
}

/*
 * Free resources allocated for loan transfer.
 */
static void
pipe_loan_free(struct pipe *wpipe)
{
	vsize_t len;

	len = (vsize_t)wpipe->pipe_map.npages << PAGE_SHIFT;
	uvm_km_free(kernel_map, wpipe->pipe_map.kva, len, UVM_KMF_VAONLY);
	wpipe->pipe_map.kva = 0;
	amountpipekva -= len;
	free(wpipe->pipe_map.pgs, M_PIPE);
	wpipe->pipe_map.pgs = NULL;
}

/*
 * NetBSD direct write, using uvm_loan() mechanism.
 * This implements the pipe buffer write mechanism.  Note that only
 * a direct write OR a normal pipe write can be pending at any given time.
 * If there are any characters in the pipe buffer, the direct write will
 * be deferred until the receiving process grabs all of the bytes from
 * the pipe buffer.  Then the direct mapping write is set-up.
 *
 * Called with the long-term pipe lock held.
 */
static int
pipe_direct_write(struct file *fp, struct pipe *wpipe, struct uio *uio)
{
	int error, npages, j;
	struct vm_page **pgs;
	vaddr_t bbase, kva, base, bend;
	vsize_t blen, bcnt;
	voff_t bpos;

	KASSERT(wpipe->pipe_map.cnt == 0);

	/*
	 * Handle first PIPE_CHUNK_SIZE bytes of buffer. Deal with buffers
	 * not aligned to PAGE_SIZE.
	 */
	bbase = (vaddr_t)uio->uio_iov->iov_base;
	base = trunc_page(bbase);
	bend = round_page(bbase + uio->uio_iov->iov_len);
	blen = bend - base;
	bpos = bbase - base;

	if (blen > PIPE_DIRECT_CHUNK) {
		blen = PIPE_DIRECT_CHUNK;
		bend = base + blen;
		bcnt = PIPE_DIRECT_CHUNK - bpos;
	} else {
		bcnt = uio->uio_iov->iov_len;
	}
	npages = blen >> PAGE_SHIFT;

	/*
	 * Free the old kva if we need more pages than we have
	 * allocated.
	 */
	if (wpipe->pipe_map.kva != 0 && npages > wpipe->pipe_map.npages)
		pipe_loan_free(wpipe);

	/* Allocate new kva. */
	if (wpipe->pipe_map.kva == 0) {
		error = pipe_loan_alloc(wpipe, npages);
		if (error)
			return (error);
	}

	/* Loan the write buffer memory from writer process */
	pgs = wpipe->pipe_map.pgs;
	error = uvm_loan(&uio->uio_vmspace->vm_map, base, blen,
			 pgs, UVM_LOAN_TOPAGE);
	if (error) {
		pipe_loan_free(wpipe);
		return (ENOMEM); /* so that caller fallback to ordinary write */
	}

	/* Enter the loaned pages to kva */
	kva = wpipe->pipe_map.kva;
	for (j = 0; j < npages; j++, kva += PAGE_SIZE) {
		pmap_kenter_pa(kva, VM_PAGE_TO_PHYS(pgs[j]), VM_PROT_READ);
	}
	pmap_update(pmap_kernel());

	/* Now we can put the pipe in direct write mode */
	wpipe->pipe_map.pos = bpos;
	wpipe->pipe_map.cnt = bcnt;
	wpipe->pipe_state |= PIPE_DIRECTW;

	/*
	 * But before we can let someone do a direct read,
	 * we have to wait until the pipe is drained.
	 */

	/* Relase the pipe lock while we wait */
	PIPE_LOCK(wpipe);
	pipeunlock(wpipe);

	while (error == 0 && wpipe->pipe_buffer.cnt > 0) {
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}

		wpipe->pipe_state |= PIPE_WANTW;
		error = ltsleep(wpipe, PSOCK | PCATCH, "pipdwc", 0,
				&wpipe->pipe_slock);
		if (error == 0 && wpipe->pipe_state & PIPE_EOF)
			error = EPIPE;
	}

	/* Pipe is drained; next read will off the direct buffer */
	wpipe->pipe_state |= PIPE_DIRECTR;

	/* Wait until the reader is done */
	while (error == 0 && (wpipe->pipe_state & PIPE_DIRECTR)) {
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
		pipeselwakeup(wpipe, wpipe, POLL_IN);
		error = ltsleep(wpipe, PSOCK | PCATCH, "pipdwt", 0,
				&wpipe->pipe_slock);
		if (error == 0 && wpipe->pipe_state & PIPE_EOF)
			error = EPIPE;
	}

	/* Take pipe out of direct write mode */
	wpipe->pipe_state &= ~(PIPE_DIRECTW | PIPE_DIRECTR);

	/* Acquire the pipe lock and cleanup */
	(void)pipelock(wpipe, 0);
	if (pgs != NULL) {
		pmap_kremove(wpipe->pipe_map.kva, blen);
		uvm_unloan(pgs, npages, UVM_LOAN_TOPAGE);
	}
	if (error || amountpipekva > maxpipekva)
		pipe_loan_free(wpipe);

	if (error) {
		pipeselwakeup(wpipe, wpipe, POLL_ERR);

		/*
		 * If nothing was read from what we offered, return error
		 * straight on. Otherwise update uio resid first. Caller
		 * will deal with the error condition, returning short
		 * write, error, or restarting the write(2) as appropriate.
		 */
		if (wpipe->pipe_map.cnt == bcnt) {
			wpipe->pipe_map.cnt = 0;
			wakeup(wpipe);
			return (error);
		}

		bcnt -= wpipe->pipe_map.cnt;
	}

	uio->uio_resid -= bcnt;
	/* uio_offset not updated, not set/used for write(2) */
	uio->uio_iov->iov_base = (char *)uio->uio_iov->iov_base + bcnt;
	uio->uio_iov->iov_len -= bcnt;
	if (uio->uio_iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}

	wpipe->pipe_map.cnt = 0;
	return (error);
}
#endif /* !PIPE_NODIRECT */

static int
pipe_write(struct file *fp, off_t *offset, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	struct pipe *wpipe, *rpipe;
	struct pipebuf *bp;
	int error;

	/* We want to write to our peer */
	rpipe = (struct pipe *) fp->f_data;

retry:
	error = 0;
	PIPE_LOCK(rpipe);
	wpipe = rpipe->pipe_peer;

	/*
	 * Detect loss of pipe read side, issue SIGPIPE if lost.
	 */
	if (wpipe == NULL)
		error = EPIPE;
	else if (simple_lock_try(&wpipe->pipe_slock) == 0) {
		/* Deal with race for peer */
		PIPE_UNLOCK(rpipe);
		goto retry;
	} else if ((wpipe->pipe_state & PIPE_EOF) != 0) {
		PIPE_UNLOCK(wpipe);
		error = EPIPE;
	}

	PIPE_UNLOCK(rpipe);
	if (error != 0)
		return (error);

	++wpipe->pipe_busy;

	/* Aquire the long-term pipe lock */
	if ((error = pipelock(wpipe,1)) != 0) {
		--wpipe->pipe_busy;
		if (wpipe->pipe_busy == 0
		    && (wpipe->pipe_state & PIPE_WANTCLOSE)) {
			wpipe->pipe_state &= ~(PIPE_WANTCLOSE | PIPE_WANTR);
			wakeup(wpipe);
		}
		PIPE_UNLOCK(wpipe);
		return (error);
	}

	bp = &wpipe->pipe_buffer;

	/*
	 * If it is advantageous to resize the pipe buffer, do so.
	 */
	if ((uio->uio_resid > PIPE_SIZE) &&
	    (nbigpipe < maxbigpipes) &&
#ifndef PIPE_NODIRECT
	    (wpipe->pipe_state & PIPE_DIRECTW) == 0 &&
#endif
	    (bp->size <= PIPE_SIZE) && (bp->cnt == 0)) {

		if (pipespace(wpipe, BIG_PIPE_SIZE) == 0)
			nbigpipe++;
	}

	while (uio->uio_resid) {
		size_t space;

#ifndef PIPE_NODIRECT
		/*
		 * Pipe buffered writes cannot be coincidental with
		 * direct writes.  Also, only one direct write can be
		 * in progress at any one time.  We wait until the currently
		 * executing direct write is completed before continuing.
		 *
		 * We break out if a signal occurs or the reader goes away.
		 */
		while (error == 0 && wpipe->pipe_state & PIPE_DIRECTW) {
			PIPE_LOCK(wpipe);
			if (wpipe->pipe_state & PIPE_WANTR) {
				wpipe->pipe_state &= ~PIPE_WANTR;
				wakeup(wpipe);
			}
			pipeunlock(wpipe);
			error = ltsleep(wpipe, PSOCK | PCATCH,
					"pipbww", 0, &wpipe->pipe_slock);

			(void)pipelock(wpipe, 0);
			if (wpipe->pipe_state & PIPE_EOF)
				error = EPIPE;
		}
		if (error)
			break;

		/*
		 * If the transfer is large, we can gain performance if
		 * we do process-to-process copies directly.
		 * If the write is non-blocking, we don't use the
		 * direct write mechanism.
		 *
		 * The direct write mechanism will detect the reader going
		 * away on us.
		 */
		if ((uio->uio_iov->iov_len >= PIPE_MINDIRECT) &&
		    (fp->f_flag & FNONBLOCK) == 0 &&
		    (wpipe->pipe_map.kva || (amountpipekva < limitpipekva))) {
			error = pipe_direct_write(fp, wpipe, uio);

			/*
			 * Break out if error occurred, unless it's ENOMEM.
			 * ENOMEM means we failed to allocate some resources
			 * for direct write, so we just fallback to ordinary
			 * write. If the direct write was successful,
			 * process rest of data via ordinary write.
			 */
			if (error == 0)
				continue;

			if (error != ENOMEM)
				break;
		}
#endif /* PIPE_NODIRECT */

		space = bp->size - bp->cnt;

		/* Writes of size <= PIPE_BUF must be atomic. */
		if ((space < uio->uio_resid) && (uio->uio_resid <= PIPE_BUF))
			space = 0;

		if (space > 0) {
			int size;	/* Transfer size */
			int segsize;	/* first segment to transfer */

			/*
			 * Transfer size is minimum of uio transfer
			 * and free space in pipe buffer.
			 */
			if (space > uio->uio_resid)
				size = uio->uio_resid;
			else
				size = space;
			/*
			 * First segment to transfer is minimum of
			 * transfer size and contiguous space in
			 * pipe buffer.  If first segment to transfer
			 * is less than the transfer size, we've got
			 * a wraparound in the buffer.
			 */
			segsize = bp->size - bp->in;
			if (segsize > size)
				segsize = size;

			/* Transfer first segment */
			error = uiomove(&bp->buffer[bp->in], segsize, uio);

			if (error == 0 && segsize < size) {
				/*
				 * Transfer remaining part now, to
				 * support atomic writes.  Wraparound
				 * happened.
				 */
#ifdef DEBUG
				if (bp->in + segsize != bp->size)
					panic("Expected pipe buffer wraparound disappeared");
#endif

				error = uiomove(&bp->buffer[0],
						size - segsize, uio);
			}
			if (error)
				break;

			bp->in += size;
			if (bp->in >= bp->size) {
#ifdef DEBUG
				if (bp->in != size - segsize + bp->size)
					panic("Expected wraparound bad");
#endif
				bp->in = size - segsize;
			}

			bp->cnt += size;
#ifdef DEBUG
			if (bp->cnt > bp->size)
				panic("Pipe buffer overflow");
#endif
		} else {
			/*
			 * If the "read-side" has been blocked, wake it up now.
			 */
			PIPE_LOCK(wpipe);
			if (wpipe->pipe_state & PIPE_WANTR) {
				wpipe->pipe_state &= ~PIPE_WANTR;
				wakeup(wpipe);
			}
			PIPE_UNLOCK(wpipe);

			/*
			 * don't block on non-blocking I/O
			 */
			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
				break;
			}

			/*
			 * We have no more space and have something to offer,
			 * wake up select/poll.
			 */
			if (bp->cnt)
				pipeselwakeup(wpipe, wpipe, POLL_OUT);

			PIPE_LOCK(wpipe);
			pipeunlock(wpipe);
			wpipe->pipe_state |= PIPE_WANTW;
			error = ltsleep(wpipe, PSOCK | PCATCH, "pipewr", 0,
					&wpipe->pipe_slock);
			(void)pipelock(wpipe, 0);
			if (error != 0)
				break;
			/*
			 * If read side wants to go away, we just issue a signal
			 * to ourselves.
			 */
			if (wpipe->pipe_state & PIPE_EOF) {
				error = EPIPE;
				break;
			}
		}
	}

	PIPE_LOCK(wpipe);
	--wpipe->pipe_busy;
	if ((wpipe->pipe_busy == 0) && (wpipe->pipe_state & PIPE_WANTCLOSE)) {
		wpipe->pipe_state &= ~(PIPE_WANTCLOSE | PIPE_WANTR);
		wakeup(wpipe);
	} else if (bp->cnt > 0) {
		/*
		 * If we have put any characters in the buffer, we wake up
		 * the reader.
		 */
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
	}

	/*
	 * Don't return EPIPE if I/O was successful
	 */
	if (error == EPIPE && bp->cnt == 0 && uio->uio_resid == 0)
		error = 0;

	if (error == 0)
		getmicrotime(&wpipe->pipe_mtime);

	/*
	 * We have something to offer, wake up select/poll.
	 * wpipe->pipe_map.cnt is always 0 in this point (direct write
	 * is only done synchronously), so check only wpipe->pipe_buffer.cnt
	 */
	if (bp->cnt)
		pipeselwakeup(wpipe, wpipe, POLL_OUT);

	/*
	 * Arrange for next read(2) to do a signal.
	 */
	wpipe->pipe_state |= PIPE_SIGNALR;

	pipeunlock(wpipe);
	PIPE_UNLOCK(wpipe);
	return (error);
}

/*
 * we implement a very minimal set of ioctls for compatibility with sockets.
 */
int
pipe_ioctl(struct file *fp, u_long cmd, void *data, struct lwp *l)
{
	struct pipe *pipe = (struct pipe *)fp->f_data;
	struct proc *p = l->l_proc;

	switch (cmd) {

	case FIONBIO:
		return (0);

	case FIOASYNC:
		PIPE_LOCK(pipe);
		if (*(int *)data) {
			pipe->pipe_state |= PIPE_ASYNC;
		} else {
			pipe->pipe_state &= ~PIPE_ASYNC;
		}
		PIPE_UNLOCK(pipe);
		return (0);

	case FIONREAD:
		PIPE_LOCK(pipe);
#ifndef PIPE_NODIRECT
		if (pipe->pipe_state & PIPE_DIRECTW)
			*(int *)data = pipe->pipe_map.cnt;
		else
#endif
			*(int *)data = pipe->pipe_buffer.cnt;
		PIPE_UNLOCK(pipe);
		return (0);

	case FIONWRITE:
		/* Look at other side */
		pipe = pipe->pipe_peer;
		PIPE_LOCK(pipe);
#ifndef PIPE_NODIRECT
		if (pipe->pipe_state & PIPE_DIRECTW)
			*(int *)data = pipe->pipe_map.cnt;
		else
#endif
			*(int *)data = pipe->pipe_buffer.cnt;
		PIPE_UNLOCK(pipe);
		return (0);

	case FIONSPACE:
		/* Look at other side */
		pipe = pipe->pipe_peer;
		PIPE_LOCK(pipe);
#ifndef PIPE_NODIRECT
		/*
		 * If we're in direct-mode, we don't really have a
		 * send queue, and any other write will block. Thus
		 * zero seems like the best answer.
		 */
		if (pipe->pipe_state & PIPE_DIRECTW)
			*(int *)data = 0;
		else
#endif
			*(int *)data = pipe->pipe_buffer.size -
					pipe->pipe_buffer.cnt;
		PIPE_UNLOCK(pipe);
		return (0);

	case TIOCSPGRP:
	case FIOSETOWN:
		return fsetown(p, &pipe->pipe_pgid, cmd, data);

	case TIOCGPGRP:
	case FIOGETOWN:
		return fgetown(p, pipe->pipe_pgid, cmd, data);

	}
	return (EPASSTHROUGH);
}

int
pipe_poll(struct file *fp, int events, struct lwp *l)
{
	struct pipe *rpipe = (struct pipe *)fp->f_data;
	struct pipe *wpipe;
	int eof = 0;
	int revents = 0;

retry:
	PIPE_LOCK(rpipe);
	wpipe = rpipe->pipe_peer;
	if (wpipe != NULL && simple_lock_try(&wpipe->pipe_slock) == 0) {
		/* Deal with race for peer */
		PIPE_UNLOCK(rpipe);
		goto retry;
	}

	if (events & (POLLIN | POLLRDNORM))
		if ((rpipe->pipe_buffer.cnt > 0) ||
#ifndef PIPE_NODIRECT
		    (rpipe->pipe_state & PIPE_DIRECTR) ||
#endif
		    (rpipe->pipe_state & PIPE_EOF))
			revents |= events & (POLLIN | POLLRDNORM);

	eof |= (rpipe->pipe_state & PIPE_EOF);
	PIPE_UNLOCK(rpipe);

	if (wpipe == NULL)
		revents |= events & (POLLOUT | POLLWRNORM);
	else {
		if (events & (POLLOUT | POLLWRNORM))
			if ((wpipe->pipe_state & PIPE_EOF) || (
#ifndef PIPE_NODIRECT
			     (wpipe->pipe_state & PIPE_DIRECTW) == 0 &&
#endif
			     (wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt) >= PIPE_BUF))
				revents |= events & (POLLOUT | POLLWRNORM);

		eof |= (wpipe->pipe_state & PIPE_EOF);
		PIPE_UNLOCK(wpipe);
	}

	if (wpipe == NULL || eof)
		revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(l, &rpipe->pipe_sel);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(l, &wpipe->pipe_sel);
	}

	return (revents);
}

static int
pipe_stat(struct file *fp, struct stat *ub, struct lwp *l)
{
	struct pipe *pipe = (struct pipe *)fp->f_data;

	memset((caddr_t)ub, 0, sizeof(*ub));
	ub->st_mode = S_IFIFO | S_IRUSR | S_IWUSR;
	ub->st_blksize = pipe->pipe_buffer.size;
	if (ub->st_blksize == 0 && pipe->pipe_peer)
		ub->st_blksize = pipe->pipe_peer->pipe_buffer.size;
	ub->st_size = pipe->pipe_buffer.cnt;
	ub->st_blocks = (ub->st_size) ? 1 : 0;
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_atime, &ub->st_atimespec);
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_mtime, &ub->st_mtimespec);
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_ctime, &ub->st_ctimespec);
	ub->st_uid = kauth_cred_geteuid(fp->f_cred);
	ub->st_gid = kauth_cred_getegid(fp->f_cred);
	/*
	 * Left as 0: st_dev, st_ino, st_nlink, st_rdev, st_flags, st_gen.
	 * XXX (st_dev, st_ino) should be unique.
	 */
	return (0);
}

/* ARGSUSED */
static int
pipe_close(struct file *fp, struct lwp *l)
{
	struct pipe *pipe = (struct pipe *)fp->f_data;

	fp->f_data = NULL;
	pipeclose(fp, pipe);
	return (0);
}

static void
pipe_free_kmem(struct pipe *pipe)
{

	if (pipe->pipe_buffer.buffer != NULL) {
		if (pipe->pipe_buffer.size > PIPE_SIZE)
			--nbigpipe;
		amountpipekva -= pipe->pipe_buffer.size;
		uvm_km_free(kernel_map,
			(vaddr_t)pipe->pipe_buffer.buffer,
			pipe->pipe_buffer.size, UVM_KMF_PAGEABLE);
		pipe->pipe_buffer.buffer = NULL;
	}
#ifndef PIPE_NODIRECT
	if (pipe->pipe_map.kva != 0) {
		pipe_loan_free(pipe);
		pipe->pipe_map.cnt = 0;
		pipe->pipe_map.kva = 0;
		pipe->pipe_map.pos = 0;
		pipe->pipe_map.npages = 0;
	}
#endif /* !PIPE_NODIRECT */
}

/*
 * shutdown the pipe
 */
static void
pipeclose(struct file *fp, struct pipe *pipe)
{
	struct pipe *ppipe;

	if (pipe == NULL)
		return;

retry:
	PIPE_LOCK(pipe);

	pipeselwakeup(pipe, pipe, POLL_HUP);

	/*
	 * If the other side is blocked, wake it up saying that
	 * we want to close it down.
	 */
	pipe->pipe_state |= PIPE_EOF;
	while (pipe->pipe_busy) {
		wakeup(pipe);
		pipe->pipe_state |= PIPE_WANTCLOSE;
		ltsleep(pipe, PSOCK, "pipecl", 0, &pipe->pipe_slock);
	}

	/*
	 * Disconnect from peer
	 */
	if ((ppipe = pipe->pipe_peer) != NULL) {
		/* Deal with race for peer */
		if (simple_lock_try(&ppipe->pipe_slock) == 0) {
			PIPE_UNLOCK(pipe);
			goto retry;
		}
		pipeselwakeup(ppipe, ppipe, POLL_HUP);

		ppipe->pipe_state |= PIPE_EOF;
		wakeup(ppipe);
		ppipe->pipe_peer = NULL;
		PIPE_UNLOCK(ppipe);
	}

	KASSERT((pipe->pipe_state & PIPE_LOCKFL) == 0);

	PIPE_UNLOCK(pipe);

	/*
	 * free resources
	 */
	pipe_free_kmem(pipe);
	pool_put(&pipe_pool, pipe);
}

static void
filt_pipedetach(struct knote *kn)
{
	struct pipe *pipe = (struct pipe *)kn->kn_fp->f_data;

	switch(kn->kn_filter) {
	case EVFILT_WRITE:
		/* need the peer structure, not our own */
		pipe = pipe->pipe_peer;
		/* XXXSMP: race for peer */

		/* if reader end already closed, just return */
		if (pipe == NULL)
			return;

		break;
	default:
		/* nothing to do */
		break;
	}

#ifdef DIAGNOSTIC
	if (kn->kn_hook != pipe)
		panic("filt_pipedetach: inconsistent knote");
#endif

	PIPE_LOCK(pipe);
	SLIST_REMOVE(&pipe->pipe_sel.sel_klist, kn, knote, kn_selnext);
	PIPE_UNLOCK(pipe);
}

/*ARGSUSED*/
static int
filt_piperead(struct knote *kn, long hint)
{
	struct pipe *rpipe = (struct pipe *)kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	if ((hint & NOTE_SUBMIT) == 0)
		PIPE_LOCK(rpipe);
	kn->kn_data = rpipe->pipe_buffer.cnt;
	if ((kn->kn_data == 0) && (rpipe->pipe_state & PIPE_DIRECTW))
		kn->kn_data = rpipe->pipe_map.cnt;

	/* XXXSMP: race for peer */
	if ((rpipe->pipe_state & PIPE_EOF) ||
	    (wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_flags |= EV_EOF;
		if ((hint & NOTE_SUBMIT) == 0)
			PIPE_UNLOCK(rpipe);
		return (1);
	}
	if ((hint & NOTE_SUBMIT) == 0)
		PIPE_UNLOCK(rpipe);
	return (kn->kn_data > 0);
}

/*ARGSUSED*/
static int
filt_pipewrite(struct knote *kn, long hint)
{
	struct pipe *rpipe = (struct pipe *)kn->kn_fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;

	if ((hint & NOTE_SUBMIT) == 0)
		PIPE_LOCK(rpipe);
	/* XXXSMP: race for peer */
	if ((wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_data = 0;
		kn->kn_flags |= EV_EOF;
		if ((hint & NOTE_SUBMIT) == 0)
			PIPE_UNLOCK(rpipe);
		return (1);
	}
	kn->kn_data = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;
	if (wpipe->pipe_state & PIPE_DIRECTW)
		kn->kn_data = 0;

	if ((hint & NOTE_SUBMIT) == 0)
		PIPE_UNLOCK(rpipe);
	return (kn->kn_data >= PIPE_BUF);
}

static const struct filterops pipe_rfiltops =
	{ 1, NULL, filt_pipedetach, filt_piperead };
static const struct filterops pipe_wfiltops =
	{ 1, NULL, filt_pipedetach, filt_pipewrite };

/*ARGSUSED*/
static int
pipe_kqfilter(struct file *fp, struct knote *kn)
{
	struct pipe *pipe;

	pipe = (struct pipe *)kn->kn_fp->f_data;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &pipe_rfiltops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &pipe_wfiltops;
		/* XXXSMP: race for peer */
		pipe = pipe->pipe_peer;
		if (pipe == NULL) {
			/* other end of pipe has been closed */
			return (EBADF);
		}
		break;
	default:
		return (1);
	}
	kn->kn_hook = pipe;

	PIPE_LOCK(pipe);
	SLIST_INSERT_HEAD(&pipe->pipe_sel.sel_klist, kn, kn_selnext);
	PIPE_UNLOCK(pipe);
	return (0);
}

/*
 * Handle pipe sysctls.
 */
SYSCTL_SETUP(sysctl_kern_pipe_setup, "sysctl kern.pipe subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "kern", NULL,
		       NULL, 0, NULL, 0,
		       CTL_KERN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "pipe",
		       SYSCTL_DESCR("Pipe settings"),
		       NULL, 0, NULL, 0,
		       CTL_KERN, KERN_PIPE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxkvasz",
		       SYSCTL_DESCR("Maximum amount of kernel memory to be "
				    "used for pipes"),
		       NULL, 0, &maxpipekva, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_MAXKVASZ, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxloankvasz",
		       SYSCTL_DESCR("Limit for direct transfers via page loan"),
		       NULL, 0, &limitpipekva, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_LIMITKVA, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxbigpipes",
		       SYSCTL_DESCR("Maximum number of \"big\" pipes"),
		       NULL, 0, &maxbigpipes, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_MAXBIGPIPES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "nbigpipes",
		       SYSCTL_DESCR("Number of \"big\" pipes"),
		       NULL, 0, &nbigpipe, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_NBIGPIPES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "kvasize",
		       SYSCTL_DESCR("Amount of kernel memory consumed by pipe "
				    "buffers"),
		       NULL, 0, &amountpipekva, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_KVASIZE, CTL_EOL);
}
