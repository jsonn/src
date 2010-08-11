/*	$NetBSD: vm_vfs.c,v 1.5.2.5 2010/08/11 22:55:08 yamt Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vm_vfs.c,v 1.5.2.5 2010/08/11 22:55:08 yamt Exp $");

#include <sys/param.h>

#include <sys/buf.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>
#include <uvm/uvm_readahead.h>

/*
 * release resources held during async io.  this is almost the
 * same as uvm_aio_aiodone() from uvm_pager.c and only lacks the
 * call to uvm_aio_aiodone_pages(): unbusies pages directly here.
 */
void
uvm_aio_aiodone(struct buf *bp)
{
	int i, npages = bp->b_bufsize >> PAGE_SHIFT;
	struct vm_page **pgs;
	vaddr_t va;
	int pageout = 0;

	pgs = kmem_alloc(npages * sizeof(*pgs), KM_SLEEP);
	for (i = 0; i < npages; i++) {
		va = (vaddr_t)bp->b_data + (i << PAGE_SHIFT);
		pgs[i] = uvm_pageratop(va);
		if (pgs[i]->flags & PG_PAGEOUT) {
			KASSERT((pgs[i]->flags & PG_FAKE) == 0);
			pageout++;
			pgs[i]->flags &= ~PG_PAGEOUT;
		}
	}

	uvm_pagermapout((vaddr_t)bp->b_data, npages);
	uvm_pageout_done(pageout);
	uvm_page_unbusy(pgs, npages);

	if (BUF_ISWRITE(bp) && (bp->b_cflags & BC_AGE) != 0) {
		mutex_enter(bp->b_objlock);
		vwakeup(bp);
		mutex_exit(bp->b_objlock);
	}

	putiobuf(bp);

	kmem_free(pgs, npages * sizeof(*pgs));
}

void
uvm_aio_biodone(struct buf *bp)
{

	uvm_aio_aiodone(bp);
}

/*
 * UBC
 */

void
uvm_vnp_zerorange(struct vnode *vp, off_t off, size_t len)
{
	struct uvm_object *uobj = &vp->v_uobj;
	struct vm_page **pgs;
	int maxpages = MIN(32, round_page(len) >> PAGE_SHIFT);
	int rv, npages, i;

	if (maxpages == 0)
		return;

	pgs = kmem_zalloc(maxpages * sizeof(pgs), KM_SLEEP);
	while (len) {
		npages = MIN(maxpages, round_page(len) >> PAGE_SHIFT);
		memset(pgs, 0, npages * sizeof(struct vm_page *));
		mutex_enter(&uobj->vmobjlock);
		rv = uobj->pgops->pgo_get(uobj, off, pgs, &npages, 0, 
		    VM_PROT_READ | VM_PROT_WRITE, 0, PGO_SYNCIO);
		KASSERT(npages > 0);

		for (i = 0; i < npages; i++) {
			uint8_t *start;
			size_t chunkoff, chunklen;

			chunkoff = off & PAGE_MASK;
			chunklen = MIN(PAGE_SIZE - chunkoff, len);
			start = (uint8_t *)pgs[i]->uanon + chunkoff;

			memset(start, 0, chunklen);
			pgs[i]->flags &= ~PG_CLEAN;

			off += chunklen;
			len -= chunklen;
		}
		uvm_page_unbusy(pgs, npages);
	}
	kmem_free(pgs, maxpages * sizeof(pgs));

	return;
}

/* dumdidumdum */
#define len2npages(off, len)						\
  (((((len) + PAGE_MASK) & ~(PAGE_MASK)) >> PAGE_SHIFT)			\
    + (((off & PAGE_MASK) + (len & PAGE_MASK)) > PAGE_SIZE))

int
ubc_uiomove(struct uvm_object *uobj, struct uio *uio, vsize_t todo,
	int advice, int flags)
{
	struct vm_page **pgs;
	int npages = len2npages(uio->uio_offset, todo);
	size_t pgalloc;
	int i, rv, pagerflags;

	pgalloc = npages * sizeof(pgs);
	pgs = kmem_zalloc(pgalloc, KM_SLEEP);

	pagerflags = PGO_SYNCIO | PGO_NOBLOCKALLOC | PGO_NOTIMESTAMP;
	if (flags & UBC_WRITE)
		pagerflags |= PGO_PASTEOF;
	if (flags & UBC_FAULTBUSY)
		pagerflags |= PGO_OVERWRITE;

	do {
		mutex_enter(&uobj->vmobjlock);
		rv = uobj->pgops->pgo_get(uobj, uio->uio_offset & ~PAGE_MASK,
		    pgs, &npages, 0, VM_PROT_READ | VM_PROT_WRITE, 0,
		    pagerflags);
		if (rv)
			goto out;

		for (i = 0; i < npages; i++) {
			size_t xfersize;
			off_t pageoff;

			pageoff = uio->uio_offset & PAGE_MASK;
			xfersize = MIN(MIN(todo, PAGE_SIZE), PAGE_SIZE-pageoff);
			KASSERT(xfersize > 0);
			uiomove((uint8_t *)pgs[i]->uanon + pageoff,
			    xfersize, uio);
			if (uio->uio_rw == UIO_WRITE)
				pgs[i]->flags &= ~(PG_CLEAN | PG_FAKE);
			todo -= xfersize;
		}
		uvm_page_unbusy(pgs, npages);
	} while (todo);

 out:
	kmem_free(pgs, pgalloc);
	return rv;
}
