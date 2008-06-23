/* $NetBSD: udf_strat_sequential.c,v 1.1.10.2 2008/06/23 05:02:14 wrstuden Exp $ */

/*
 * Copyright (c) 2006, 2008 Reinoud Zandijk
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <sys/cdefs.h>
#ifndef lint
__KERNEL_RCSID(0, "$NetBSD: udf_strat_sequential.c,v 1.1.10.2 2008/06/23 05:02:14 wrstuden Exp $");
#endif /* not lint */


#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/genfs/genfs_node.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/kthread.h>
#include <dev/clock_subr.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>

#if defined(_KERNEL_OPT)
#include "opt_udf.h"
#endif

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"


#define VTOI(vnode) ((struct udf_node *) vnode->v_data)
#define PRIV(ump) ((struct strat_private *) ump->strategy_private)

/* --------------------------------------------------------------------- */

/* BUFQ's */
#define UDF_SHED_MAX 3

#define UDF_SHED_READING	0
#define UDF_SHED_WRITING	1
#define UDF_SHED_SEQWRITING	2

struct strat_private {
	struct pool		 desc_pool;	 	/* node descriptors */

	lwp_t			*queue_lwp;
	kcondvar_t		 discstrat_cv;		/* to wait on       */
	kmutex_t		 discstrat_mutex;	/* disc strategy    */

	int			 run_thread;		/* thread control */
	int			 cur_queue;

	struct disk_strategy	 old_strategy_setting;
	struct bufq_state	*queues[UDF_SHED_MAX];
	struct timespec		 last_queued[UDF_SHED_MAX];
};


/* --------------------------------------------------------------------- */

static void
udf_wr_nodedscr_callback(struct buf *buf)
{
	struct udf_node *udf_node;

	KASSERT(buf);
	KASSERT(buf->b_data);

	/* called when write action is done */
	DPRINTF(WRITE, ("udf_wr_nodedscr_callback(): node written out\n"));

	udf_node = VTOI(buf->b_vp);
	if (udf_node == NULL) {
		putiobuf(buf);
		printf("udf_wr_node_callback: NULL node?\n");
		return;
	}

	/* XXX noone is waiting on this outstanding_nodedscr */
	udf_node->outstanding_nodedscr--;
	if (udf_node->outstanding_nodedscr == 0)
		wakeup(&udf_node->outstanding_nodedscr);

	/* XXX right flags to mark dirty again on error? */
	if (buf->b_error) {
		udf_node->i_flags |= IN_MODIFIED | IN_ACCESSED;
		/* XXX TODO reshedule on error */
	}

	/* first unlock the node */
	KASSERT(udf_node->i_flags & IN_CALLBACK_ULK);
	UDF_UNLOCK_NODE(udf_node, IN_CALLBACK_ULK);

	/* unreference the vnode so it can be recycled */
	holdrele(udf_node->vnode);

	putiobuf(buf);
}

/* --------------------------------------------------------------------- */

static int
udf_create_logvol_dscr_seq(struct udf_strat_args *args)
{
	union dscrptr   **dscrptr = &args->dscr;
	struct udf_mount *ump = args->ump;
	struct strat_private *priv = PRIV(ump);
	uint32_t lb_size;

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	*dscrptr = pool_get(&priv->desc_pool, PR_WAITOK);
	memset(*dscrptr, 0, lb_size);

	return 0;
}


static void
udf_free_logvol_dscr_seq(struct udf_strat_args *args)
{
	union dscrptr    *dscr = args->dscr;
	struct udf_mount *ump  = args->ump;
	struct strat_private *priv = PRIV(ump);

	pool_put(&priv->desc_pool, dscr);
}


static int
udf_read_logvol_dscr_seq(struct udf_strat_args *args)
{
	union dscrptr   **dscrptr = &args->dscr;
	union dscrptr    *tmpdscr;
	struct udf_mount *ump = args->ump;
	struct long_ad   *icb = args->icb;
	struct strat_private *priv = PRIV(ump);
	uint32_t lb_size;
	uint32_t sector, dummy;
	int error;

	lb_size = udf_rw32(ump->logical_vol->lb_size);

	error = udf_translate_vtop(ump, icb, &sector, &dummy);
	if (error)
		return error;

	/* try to read in fe/efe */
	error = udf_read_phys_dscr(ump, sector, M_UDFTEMP, &tmpdscr);
	if (error)
		return error;

	*dscrptr = pool_get(&priv->desc_pool, PR_WAITOK);
	memcpy(*dscrptr, tmpdscr, lb_size);
	free(tmpdscr, M_UDFTEMP);

	return 0;
}


static int
udf_write_logvol_dscr_seq(struct udf_strat_args *args)
{
	union dscrptr    *dscr     = args->dscr;
	struct udf_mount *ump      = args->ump;
	struct udf_node  *udf_node = args->udf_node;
	struct long_ad   *icb      = args->icb;
	int               waitfor  = args->waitfor;
	uint32_t logsectornr, sectornr, dummy;
	int error, vpart;

	/*
	 * we have to decide if we write it out sequential or at its fixed 
	 * position by examining the partition its (to be) written on.
	 */
	vpart       = udf_rw16(udf_node->loc.loc.part_num);
	logsectornr = udf_rw32(icb->loc.lb_num);
	sectornr    = 0;
	if (ump->vtop_tp[vpart] != UDF_VTOP_TYPE_VIRT) {
		error = udf_translate_vtop(ump, icb, &sectornr, &dummy);
		if (error)
			return error;
	}

	UDF_LOCK_NODE(udf_node, IN_CALLBACK_ULK);

	if (waitfor) {
		DPRINTF(WRITE, ("udf_write_logvol_dscr: sync write\n"));

		error = udf_write_phys_dscr_sync(ump, udf_node, UDF_C_NODE,
			dscr, sectornr, logsectornr);
		UDF_UNLOCK_NODE(udf_node, IN_CALLBACK_ULK);
	} else {
		DPRINTF(WRITE, ("udf_write_logvol_dscr: no wait, async write\n"));

		/* add reference to the vnode to prevent recycling */
		vhold(udf_node->vnode);

		udf_node->outstanding_nodedscr++;

		error = udf_write_phys_dscr_async(ump, udf_node, UDF_C_NODE,
			dscr, sectornr, logsectornr, udf_wr_nodedscr_callback);
		/* will be UNLOCKED in call back */
	}

	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Main file-system specific sheduler. Due to the nature of optical media
 * sheduling can't be performed in the traditional way. Most OS
 * implementations i've seen thus read or write a file atomically giving all
 * kinds of side effects.
 *
 * This implementation uses a kernel thread to shedule the queued requests in
 * such a way that is semi-optimal for optical media; this means aproximately
 * (R*|(Wr*|Ws*))* since switching between reading and writing is expensive in
 * time.
 */

static void
udf_queuebuf_seq(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct buf *nestbuf = args->nestbuf;
	struct strat_private *priv = PRIV(ump);
	int queue;
	int what;

	KASSERT(ump);
	KASSERT(nestbuf);
	KASSERT(nestbuf->b_iodone == nestiobuf_iodone);

	what = nestbuf->b_udf_c_type;
	queue = UDF_SHED_READING;
	if ((nestbuf->b_flags & B_READ) == 0) {
		/* writing */
		queue = UDF_SHED_SEQWRITING;
		if (what == UDF_C_DSCR)
			queue = UDF_SHED_WRITING;
		if (what == UDF_C_NODE) {
			if (ump->meta_alloc != UDF_ALLOC_VAT)
				queue = UDF_SHED_WRITING;
		}
#if 0
		if (queue == UDF_SHED_SEQWRITING) {
			/* TODO do add sector to uncommitted space */
		}
#endif
	}

	/* use our own sheduler lists for more complex sheduling */
	mutex_enter(&priv->discstrat_mutex);
		BUFQ_PUT(priv->queues[queue], nestbuf);
		vfs_timestamp(&priv->last_queued[queue]);
	mutex_exit(&priv->discstrat_mutex);

	/* signal our thread that there might be something to do */
	cv_signal(&priv->discstrat_cv);
}

/* --------------------------------------------------------------------- */

/* TODO convert to lb_size */
static void
udf_VAT_mapping_update(struct udf_mount *ump, struct buf *buf)
{
	union dscrptr    *fdscr = (union dscrptr *) buf->b_data;
	struct vnode     *vp = buf->b_vp;
	struct udf_node  *udf_node = VTOI(vp);
	struct part_desc *pdesc;
	uint32_t lb_size, blks;
	uint32_t lb_num, lb_map;
	uint32_t udf_rw32_lbmap;
	int c_type = buf->b_udf_c_type;
	int error;

	/* only interested when we're using a VAT */
	if (ump->meta_alloc != UDF_ALLOC_VAT)
		return;
	KASSERT(ump->vat_node);

	/* only nodes are recorded in the VAT */
	/* NOTE: and the fileset descriptor (FIXME ?) */
	if (c_type != UDF_C_NODE)
		return;

	/* we now have an UDF FE/EFE node on media with VAT (or VAT itself) */
	lb_size = udf_rw32(ump->logical_vol->lb_size);
	blks = lb_size / DEV_BSIZE;

	/* calculate offset from base partition */
	pdesc = ump->partitions[ump->vtop[ump->metadata_part]];
	lb_map  = buf->b_blkno / blks;
	lb_map -= udf_rw32(pdesc->start_loc);

	udf_rw32_lbmap = udf_rw32(lb_map);

	/* if we're the VAT itself, only update our assigned sector number */
	if (udf_node == ump->vat_node) {
		fdscr->tag.tag_loc = udf_rw32_lbmap;
		udf_validate_tag_sum(fdscr);
		DPRINTF(TRANSLATE, ("VAT assigned to sector %u\n",
			udf_rw32(udf_rw32_lbmap)));
		/* no use mapping the VAT node in the VAT */
		return;
	}

	/* check for tag location is false for allocation extents */
	KASSERT(fdscr->tag.tag_loc == udf_node->write_loc.loc.lb_num);

	/* record new position in VAT file */
	lb_num = udf_rw32(udf_node->write_loc.loc.lb_num);

	DPRINTF(TRANSLATE, ("VAT entry change (log %u -> phys %u)\n",
			lb_num, lb_map));

	/* VAT should be the longer than this write, can't go wrong */
	KASSERT(lb_num <= ump->vat_entries);

	mutex_enter(&ump->allocate_mutex);
	error = udf_vat_write(ump->vat_node,
			(uint8_t *) &udf_rw32_lbmap, 4,
			ump->vat_offset + lb_num * 4);
	mutex_exit(&ump->allocate_mutex);

	if (error)
		panic( "udf_VAT_mapping_update: HELP! i couldn't "
			"write in the VAT file ?\n");
}


static void
udf_issue_buf(struct udf_mount *ump, int queue, struct buf *buf)
{
	struct long_ad *node_ad_cpy;
	uint64_t *lmapping, *pmapping, *lmappos, blknr;
	uint32_t our_sectornr, sectornr, bpos;
	uint8_t *fidblk;
	int sector_size = ump->discinfo.sector_size;
	int blks = sector_size / DEV_BSIZE;
	int len, buf_len;

	/* if reading, just pass to the device's STRATEGY */
	if (queue == UDF_SHED_READING) {
		DPRINTF(SHEDULE, ("\nudf_issue_buf READ %p : sector %d type %d,"
			"b_resid %d, b_bcount %d, b_bufsize %d\n",
			buf, (uint32_t) buf->b_blkno / blks, buf->b_udf_c_type,
			buf->b_resid, buf->b_bcount, buf->b_bufsize));
		VOP_STRATEGY(ump->devvp, buf);
		return;
	}

	blknr        = buf->b_blkno;
	our_sectornr = blknr / blks;

	if (queue == UDF_SHED_WRITING) {
		DPRINTF(SHEDULE, ("\nudf_issue_buf WRITE %p : sector %d "
			"type %d, b_resid %d, b_bcount %d, b_bufsize %d\n",
			buf, (uint32_t) buf->b_blkno / blks, buf->b_udf_c_type,
			buf->b_resid, buf->b_bcount, buf->b_bufsize));
		/* if we have FIDs fixup using buffer's sector number(s) */
		if (buf->b_udf_c_type == UDF_C_FIDS) {
			panic("UDF_C_FIDS in SHED_WRITING!\n");
			buf_len = buf->b_bcount;
			sectornr = our_sectornr;
			bpos = 0;
			while (buf_len) {
				len = MIN(buf_len, sector_size);
				fidblk = (uint8_t *) buf->b_data + bpos;
				udf_fixup_fid_block(fidblk, sector_size,
					0, len, sectornr);
				sectornr++;
				bpos += len;
				buf_len -= len;
			}
		}
		udf_fixup_node_internals(ump, buf->b_data, buf->b_udf_c_type);
		VOP_STRATEGY(ump->devvp, buf);
		return;
	}

	KASSERT(queue == UDF_SHED_SEQWRITING);
	DPRINTF(SHEDULE, ("\nudf_issue_buf SEQWRITE %p : sector XXXX "
		"type %d, b_resid %d, b_bcount %d, b_bufsize %d\n",
		buf, buf->b_udf_c_type, buf->b_resid, buf->b_bcount,
		buf->b_bufsize));

	/*
	 * Buffers should not have been allocated to disc addresses yet on
	 * this queue. Note that a buffer can get multiple extents allocated.
	 *
	 * lmapping contains lb_num relative to base partition.
	 * pmapping contains lb_num as used for disc adressing.
	 */
	lmapping    = ump->la_lmapping;
	pmapping    = ump->la_pmapping;
	node_ad_cpy = ump->la_node_ad_cpy;

	/* allocate buf and get its logical and physical mappings */
	udf_late_allocate_buf(ump, buf, lmapping, pmapping, node_ad_cpy);
	udf_VAT_mapping_update(ump, buf);	/* XXX could pass *lmapping */

	/* if we have FIDs, fixup using the new allocation table */
	if (buf->b_udf_c_type == UDF_C_FIDS) {
		buf_len = buf->b_bcount;
		bpos = 0;
		lmappos = lmapping;
		while (buf_len) {
			sectornr = *lmappos++;
			len = MIN(buf_len, sector_size);
			fidblk = (uint8_t *) buf->b_data + bpos;
			udf_fixup_fid_block(fidblk, sector_size,
				0, len, sectornr);
			bpos += len;
			buf_len -= len;
		}
	}
	udf_fixup_node_internals(ump, buf->b_data, buf->b_udf_c_type);
	VOP_STRATEGY(ump->devvp, buf);
}


static void
udf_doshedule(struct udf_mount *ump)
{
	struct buf *buf;
	struct timespec now, *last;
	struct strat_private *priv = PRIV(ump);
	void (*b_callback)(struct buf *);
	int new_queue;
	int error;

	buf = BUFQ_GET(priv->queues[priv->cur_queue]);
	if (buf) {
		/* transfer from the current queue to the device queue */
		mutex_exit(&priv->discstrat_mutex);

		/* transform buffer to synchronous; XXX needed? */
		b_callback = buf->b_iodone;
		buf->b_iodone = NULL;
		CLR(buf->b_flags, B_ASYNC);

		/* issue and wait on completion */
		udf_issue_buf(ump, priv->cur_queue, buf);
		biowait(buf);

		mutex_enter(&priv->discstrat_mutex);

		/* if there is an error, repair this error, otherwise propagate */
		if (buf->b_error && ((buf->b_flags & B_READ) == 0)) {
			/* check what we need to do */
			panic("UDF write error, can't handle yet!\n");
		}

		/* propagate result to higher layers */
		if (b_callback) {
			buf->b_iodone = b_callback;
			(*buf->b_iodone)(buf);
		}

		return;
	}

	/* Check if we're idling in this state */
	vfs_timestamp(&now);
	last = &priv->last_queued[priv->cur_queue];
	if (ump->discinfo.mmc_class == MMC_CLASS_CD) {
		/* dont switch too fast for CD media; its expensive in time */
		if (now.tv_sec - last->tv_sec < 3)
			return;
	}

	/* check if we can/should switch */
	new_queue = priv->cur_queue;

	if (BUFQ_PEEK(priv->queues[UDF_SHED_READING]))
		new_queue = UDF_SHED_READING;
	if (BUFQ_PEEK(priv->queues[UDF_SHED_SEQWRITING]))
		new_queue = UDF_SHED_SEQWRITING;
	if (BUFQ_PEEK(priv->queues[UDF_SHED_WRITING]))		/* only for unmount */
		new_queue = UDF_SHED_WRITING;
	if (priv->cur_queue == UDF_SHED_READING) {
		if (new_queue == UDF_SHED_SEQWRITING) {
			/* TODO use flag to signal if this is needed */
			mutex_exit(&priv->discstrat_mutex);

			/* update trackinfo for data and metadata */
			error = udf_update_trackinfo(ump,
					&ump->data_track);
			assert(error == 0);
			error = udf_update_trackinfo(ump,
					&ump->metadata_track);
			assert(error == 0);
			mutex_enter(&priv->discstrat_mutex);
		}
	}

	if (new_queue != priv->cur_queue) {
		DPRINTF(SHEDULE, ("switching from %d to %d\n",
			priv->cur_queue, new_queue));
	}

	priv->cur_queue = new_queue;
}


static void
udf_discstrat_thread(void *arg)
{
	struct udf_mount *ump = (struct udf_mount *) arg;
	struct strat_private *priv = PRIV(ump);
	int empty;

	empty = 1;
	mutex_enter(&priv->discstrat_mutex);
	while (priv->run_thread || !empty) {
		/* process the current selected queue */
		udf_doshedule(ump);
		empty  = (BUFQ_PEEK(priv->queues[UDF_SHED_READING]) == NULL);
		empty &= (BUFQ_PEEK(priv->queues[UDF_SHED_WRITING]) == NULL);
		empty &= (BUFQ_PEEK(priv->queues[UDF_SHED_SEQWRITING]) == NULL);

		/* wait for more if needed */
		if (empty)
			cv_timedwait(&priv->discstrat_cv,
				&priv->discstrat_mutex, hz/8);
	}
	mutex_exit(&priv->discstrat_mutex);

	wakeup(&priv->run_thread);
	kthread_exit(0);
	/* not reached */
}

/* --------------------------------------------------------------------- */

static void
udf_discstrat_init_seq(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct strat_private *priv = PRIV(ump);
	struct disk_strategy dkstrat;
	uint32_t lb_size;

	KASSERT(ump);
	KASSERT(ump->logical_vol);
	KASSERT(priv == NULL);

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	KASSERT(lb_size > 0);

	/* initialise our memory space */
	ump->strategy_private = malloc(sizeof(struct strat_private),
		M_UDFTEMP, M_WAITOK);
	priv = ump->strategy_private;
	memset(priv, 0 , sizeof(struct strat_private));

	/* initialise locks */
	cv_init(&priv->discstrat_cv, "udfstrat");
	mutex_init(&priv->discstrat_mutex, MUTEX_DEFAULT, IPL_NONE);

	/*
	 * Initialise pool for descriptors associated with nodes. This is done
	 * in lb_size units though currently lb_size is dictated to be
	 * sector_size.
	 */
	pool_init(&priv->desc_pool, lb_size, 0, 0, 0, "udf_desc_pool", NULL,
	    IPL_NONE);

	/*
	 * remember old device strategy method and explicit set method
	 * `discsort' since we have our own more complex strategy that is not
	 * implementable on the CD device and other strategies will get in the
	 * way.
	 */
	memset(&priv->old_strategy_setting, 0,
		sizeof(struct disk_strategy));
	VOP_IOCTL(ump->devvp, DIOCGSTRATEGY, &priv->old_strategy_setting,
		FREAD | FKIOCTL, NOCRED);
	memset(&dkstrat, 0, sizeof(struct disk_strategy));
	strcpy(dkstrat.dks_name, "discsort");
	VOP_IOCTL(ump->devvp, DIOCSSTRATEGY, &dkstrat, FWRITE | FKIOCTL,
		NOCRED);

	/* initialise our internal sheduler */
	priv->cur_queue = UDF_SHED_READING;
	bufq_alloc(&priv->queues[UDF_SHED_READING], "disksort",
		BUFQ_SORT_RAWBLOCK);
	bufq_alloc(&priv->queues[UDF_SHED_WRITING], "disksort",
		BUFQ_SORT_RAWBLOCK);
	bufq_alloc(&priv->queues[UDF_SHED_SEQWRITING], "fcfs", 0);
	vfs_timestamp(&priv->last_queued[UDF_SHED_READING]);
	vfs_timestamp(&priv->last_queued[UDF_SHED_WRITING]);
	vfs_timestamp(&priv->last_queued[UDF_SHED_SEQWRITING]);

	/* create our disk strategy thread */
	priv->run_thread = 1;
	if (kthread_create(PRI_NONE, 0 /* KTHREAD_MPSAFE*/, NULL /* cpu_info*/,
		udf_discstrat_thread, ump, &priv->queue_lwp,
		"%s", "udf_rw")) {
		panic("fork udf_rw");
	}
}


static void
udf_discstrat_finish_seq(struct udf_strat_args *args)
{
	struct udf_mount *ump = args->ump;
	struct strat_private *priv = PRIV(ump);
	int error;

	if (ump == NULL)
		return;

	/* stop our sheduling thread */
	KASSERT(priv->run_thread == 1);
	priv->run_thread = 0;
	wakeup(priv->queue_lwp);
	do {
		error = tsleep(&priv->run_thread, PRIBIO+1,
			"udfshedfin", hz);
	} while (error);
	/* kthread should be finished now */

	/* set back old device strategy method */
	VOP_IOCTL(ump->devvp, DIOCSSTRATEGY, &priv->old_strategy_setting,
			FWRITE, NOCRED);

	/* destroy our pool */
	pool_destroy(&priv->desc_pool);

	/* free our private space */
	free(ump->strategy_private, M_UDFTEMP);
	ump->strategy_private = NULL;
}

/* --------------------------------------------------------------------- */

struct udf_strategy udf_strat_sequential =
{
	udf_create_logvol_dscr_seq,
	udf_free_logvol_dscr_seq,
	udf_read_logvol_dscr_seq,
	udf_write_logvol_dscr_seq,
	udf_queuebuf_seq,
	udf_discstrat_init_seq,
	udf_discstrat_finish_seq
};
	

