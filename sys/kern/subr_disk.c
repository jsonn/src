/*	$NetBSD: subr_disk.c,v 1.65.6.1 2005/02/12 18:17:52 yamt Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_disksubr.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_disk.c,v 1.65.6.1 2005/02/12 18:17:52 yamt Exp $");

#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/syslog.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <lib/libkern/libkern.h>

/*
 * A global list of all disks attached to the system.  May grow or
 * shrink over time.
 */
struct	disklist_head disklist = TAILQ_HEAD_INITIALIZER(disklist);
int	disk_count;		/* number of drives in global disklist */
struct simplelock disklist_slock = SIMPLELOCK_INITIALIZER;

int bufq_disk_default_strat = _BUFQ_DEFAULT;

BUFQ_DEFINE(dummy, 0, NULL); /* so that bufq_strats won't be empty */

/*
 * Compute checksum for disk label.
 */
u_int
dkcksum(struct disklabel *lp)
{
	u_short *start, *end;
	u_short sum = 0;

	start = (u_short *)lp;
	end = (u_short *)&lp->d_partitions[lp->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return (sum);
}

/*
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form

hp0g: hard error reading fsbn 12345 of 12344-12347 (hp0 bn %d cn %d tn %d sn %d)

 * if the offset of the error in the transfer and a disk label
 * are both available.  blkdone should be -1 if the position of the error
 * is unknown; the disklabel pointer may be null from drivers that have not
 * been converted to use them.  The message is printed with printf
 * if pri is LOG_PRINTF, otherwise it uses log at the specified priority.
 * The message should be completed (with at least a newline) with printf
 * or addlog, respectively.  There is no trailing space.
 */
#ifndef PRIdaddr
#define PRIdaddr PRId64
#endif
void
diskerr(const struct buf *bp, const char *dname, const char *what, int pri,
    int blkdone, const struct disklabel *lp)
{
	int unit = DISKUNIT(bp->b_dev), part = DISKPART(bp->b_dev);
	void (*pr)(const char *, ...);
	char partname = 'a' + part;
	daddr_t sn;

	if (/*CONSTCOND*/0)
		/* Compiler will error this is the format is wrong... */
		printf("%" PRIdaddr, bp->b_blkno);

	if (pri != LOG_PRINTF) {
		static const char fmt[] = "";
		log(pri, fmt);
		pr = addlog;
	} else
		pr = printf;
	(*pr)("%s%d%c: %s %sing fsbn ", dname, unit, partname, what,
	    bp->b_flags & B_READ ? "read" : "writ");
	sn = bp->b_blkno;
	if (bp->b_bcount <= DEV_BSIZE)
		(*pr)("%" PRIdaddr, sn);
	else {
		if (blkdone >= 0) {
			sn += blkdone;
			(*pr)("%" PRIdaddr " of ", sn);
		}
		(*pr)("%" PRIdaddr "-%" PRIdaddr "", bp->b_blkno,
		    bp->b_blkno + (bp->b_bcount - 1) / DEV_BSIZE);
	}
	if (lp && (blkdone >= 0 || bp->b_bcount <= lp->d_secsize)) {
		sn += lp->d_partitions[part].p_offset;
		(*pr)(" (%s%d bn %" PRIdaddr "; cn %" PRIdaddr "",
		    dname, unit, sn, sn / lp->d_secpercyl);
		sn %= lp->d_secpercyl;
		(*pr)(" tn %" PRIdaddr " sn %" PRIdaddr ")",
		    sn / lp->d_nsectors, sn % lp->d_nsectors);
	}
}

/*
 * Searches the disklist for the disk corresponding to the
 * name provided.
 */
struct disk *
disk_find(char *name)
{
	struct disk *diskp;

	if ((name == NULL) || (disk_count <= 0))
		return (NULL);

	simple_lock(&disklist_slock);
	for (diskp = TAILQ_FIRST(&disklist); diskp != NULL;
	    diskp = TAILQ_NEXT(diskp, dk_link))
		if (strcmp(diskp->dk_name, name) == 0) {
			simple_unlock(&disklist_slock);
			return (diskp);
		}
	simple_unlock(&disklist_slock);

	return (NULL);
}

/*
 * Attach a disk.
 */
void
disk_attach(struct disk *diskp)
{
	int s;

	/*
	 * Allocate and initialize the disklabel structures.  Note that
	 * it's not safe to sleep here, since we're probably going to be
	 * called during autoconfiguration.
	 */
	diskp->dk_label = malloc(sizeof(struct disklabel), M_DEVBUF, M_NOWAIT);
	diskp->dk_cpulabel = malloc(sizeof(struct cpu_disklabel), M_DEVBUF,
	    M_NOWAIT);
	if ((diskp->dk_label == NULL) || (diskp->dk_cpulabel == NULL))
		panic("disk_attach: can't allocate storage for disklabel");

	memset(diskp->dk_label, 0, sizeof(struct disklabel));
	memset(diskp->dk_cpulabel, 0, sizeof(struct cpu_disklabel));

	/*
	 * Initialize the wedge-related locks and other fields.
	 */
	lockinit(&diskp->dk_rawlock, PRIBIO, "dkrawlk", 0, 0);
	lockinit(&diskp->dk_openlock, PRIBIO, "dkoplk", 0, 0);
	LIST_INIT(&diskp->dk_wedges);
	diskp->dk_nwedges = 0;

	/*
	 * Set the attached timestamp.
	 */
	s = splclock();
	diskp->dk_attachtime = mono_time;
	splx(s);

	/*
	 * Link into the disklist.
	 */
	simple_lock(&disklist_slock);
	TAILQ_INSERT_TAIL(&disklist, diskp, dk_link);
	disk_count++;
	simple_unlock(&disklist_slock);
}

/*
 * Detach a disk.
 */
void
disk_detach(struct disk *diskp)
{

	(void) lockmgr(&diskp->dk_openlock, LK_DRAIN, NULL);

	/*
	 * Remove from the disklist.
	 */
	if (disk_count == 0)
		panic("disk_detach: disk_count == 0");
	simple_lock(&disklist_slock);
	TAILQ_REMOVE(&disklist, diskp, dk_link);
	disk_count--;
	simple_unlock(&disklist_slock);

	/*
	 * Free the space used by the disklabel structures.
	 */
	free(diskp->dk_label, M_DEVBUF);
	free(diskp->dk_cpulabel, M_DEVBUF);
}

/*
 * Increment a disk's busy counter.  If the counter is going from
 * 0 to 1, set the timestamp.
 */
void
disk_busy(struct disk *diskp)
{
	int s;

	/*
	 * XXX We'd like to use something as accurate as microtime(),
	 * but that doesn't depend on the system TOD clock.
	 */
	if (diskp->dk_busy++ == 0) {
		s = splclock();
		diskp->dk_timestamp = mono_time;
		splx(s);
	}
}

/*
 * Decrement a disk's busy counter, increment the byte count, total busy
 * time, and reset the timestamp.
 */
void
disk_unbusy(struct disk *diskp, long bcount, int read)
{
	int s;
	struct timeval dv_time, diff_time;

	if (diskp->dk_busy-- == 0) {
		printf("%s: dk_busy < 0\n", diskp->dk_name);
		panic("disk_unbusy");
	}

	s = splclock();
	dv_time = mono_time;
	splx(s);

	timersub(&dv_time, &diskp->dk_timestamp, &diff_time);
	timeradd(&diskp->dk_time, &diff_time, &diskp->dk_time);

	diskp->dk_timestamp = dv_time;
	if (bcount > 0) {
		if (read) {
			diskp->dk_rbytes += bcount;
			diskp->dk_rxfer++;
		} else {
			diskp->dk_wbytes += bcount;
			diskp->dk_wxfer++;
		}
	}
}

/*
 * Reset the metrics counters on the given disk.  Note that we cannot
 * reset the busy counter, as it may case a panic in disk_unbusy().
 * We also must avoid playing with the timestamp information, as it
 * may skew any pending transfer results.
 */
void
disk_resetstat(struct disk *diskp)
{
	int s = splbio(), t;

	diskp->dk_rxfer = 0;
	diskp->dk_rbytes = 0;
	diskp->dk_wxfer = 0;
	diskp->dk_wbytes = 0;

	t = splclock();
	diskp->dk_attachtime = mono_time;
	splx(t);

	timerclear(&diskp->dk_time);

	splx(s);
}

int
sysctl_hw_disknames(SYSCTLFN_ARGS)
{
	char buf[DK_DISKNAMELEN + 1];
	char *where = oldp;
	struct disk *diskp;
	size_t needed, left, slen;
	int error, first;

	if (newp != NULL)
		return (EPERM);
	if (namelen != 0)
		return (EINVAL);

	first = 1;
	error = 0;
	needed = 0;
	left = *oldlenp;

	simple_lock(&disklist_slock);
	for (diskp = TAILQ_FIRST(&disklist); diskp != NULL;
	    diskp = TAILQ_NEXT(diskp, dk_link)) {
		if (where == NULL)
			needed += strlen(diskp->dk_name) + 1;
		else {
			memset(buf, 0, sizeof(buf));
			if (first) {
				strncpy(buf, diskp->dk_name, sizeof(buf));
				first = 0;
			} else {
				buf[0] = ' ';
				strncpy(buf + 1, diskp->dk_name,
				    sizeof(buf) - 1);
			}
			buf[DK_DISKNAMELEN] = '\0';
			slen = strlen(buf);
			if (left < slen + 1)
				break;
			/* +1 to copy out the trailing NUL byte */
			error = copyout(buf, where, slen + 1);
			if (error)
				break;
			where += slen;
			needed += slen;
			left -= slen;
		}
	}
	simple_unlock(&disklist_slock);
	*oldlenp = needed;
	return (error);
}

int
sysctl_hw_diskstats(SYSCTLFN_ARGS)
{
	struct disk_sysctl sdisk;
	struct disk *diskp;
	char *where = oldp;
	size_t tocopy, left;
	int error;

	if (newp != NULL)
		return (EPERM);

	/*
	 * The original hw.diskstats call was broken and did not require
	 * the userland to pass in it's size of struct disk_sysctl.  This
	 * was fixed after NetBSD 1.6 was released, and any applications
	 * that do not pass in the size are given an error only, unless
	 * we care about 1.6 compatibility.
	 */
	if (namelen == 0)
#ifdef COMPAT_16
		tocopy = offsetof(struct disk_sysctl, dk_rxfer);
#else
		return (EINVAL);
#endif
	else
		tocopy = name[0];

	if (where == NULL) {
		*oldlenp = disk_count * tocopy;
		return (0);
	}

	error = 0;
	left = *oldlenp;
	memset(&sdisk, 0, sizeof(sdisk));
	*oldlenp = 0;

	simple_lock(&disklist_slock);
	TAILQ_FOREACH(diskp, &disklist, dk_link) {
		if (left < tocopy)
			break;
		strncpy(sdisk.dk_name, diskp->dk_name, sizeof(sdisk.dk_name));
		sdisk.dk_xfer = diskp->dk_rxfer + diskp->dk_wxfer;
		sdisk.dk_rxfer = diskp->dk_rxfer;
		sdisk.dk_wxfer = diskp->dk_wxfer;
		sdisk.dk_seek = diskp->dk_seek;
		sdisk.dk_bytes = diskp->dk_rbytes + diskp->dk_wbytes;
		sdisk.dk_rbytes = diskp->dk_rbytes;
		sdisk.dk_wbytes = diskp->dk_wbytes;
		sdisk.dk_attachtime_sec = diskp->dk_attachtime.tv_sec;
		sdisk.dk_attachtime_usec = diskp->dk_attachtime.tv_usec;
		sdisk.dk_timestamp_sec = diskp->dk_timestamp.tv_sec;
		sdisk.dk_timestamp_usec = diskp->dk_timestamp.tv_usec;
		sdisk.dk_time_sec = diskp->dk_time.tv_sec;
		sdisk.dk_time_usec = diskp->dk_time.tv_usec;
		sdisk.dk_busy = diskp->dk_busy;

		error = copyout(&sdisk, where, min(tocopy, sizeof(sdisk)));
		if (error)
			break;
		where += tocopy;
		*oldlenp += tocopy;
		left -= tocopy;
	}
	simple_unlock(&disklist_slock);
	return (error);
}

/*
 * Create a device buffer queue.
 */
void
bufq_alloc(struct bufq_state *bufq, int flags)
{
	__link_set_decl(bufq_strats, const struct bufq_strat);
	int methodid;
	const struct bufq_strat *bsp;
	const struct bufq_strat * const *it;

	bufq->bq_flags = flags;
	methodid = flags & BUFQ_METHOD_MASK;

	switch (flags & BUFQ_SORT_MASK) {
	case BUFQ_SORT_RAWBLOCK:
	case BUFQ_SORT_CYLINDER:
		break;
	case 0:
		if (methodid == BUFQ_FCFS)
			break;
		/* FALLTHROUGH */
	default:
		panic("bufq_alloc: sort out of range");
	}

	/*
	 * select strategy.
	 * if a strategy specified by flags is found, use it.
	 * otherwise, select one with the largest id number. XXX
	 */
	bsp = NULL;
	__link_set_foreach(it, bufq_strats) {
		if ((*it) == &bufq_strat_dummy)
			continue;
		if (methodid == (*it)->bs_id) {
			bsp = *it;
			break;
		}
		if (bsp == NULL || (*it)->bs_id > bsp->bs_id)
			bsp = *it;
	}

	KASSERT(bsp != NULL);
#ifdef DEBUG
	if (bsp->bs_id != methodid && methodid != _BUFQ_DEFAULT)
		printf("bufq_alloc: method 0x%04x is not available.\n",
		    methodid);
#endif
#ifdef BUFQ_DEBUG
	/* XXX aprint? */
	printf("bufq_alloc: using %s\n", bsp->bs_name);
#endif
	(*bsp->bs_initfn)(bufq);
}

/*
 * Destroy a device buffer queue.
 */
void
bufq_free(struct bufq_state *bufq)
{

	KASSERT(bufq->bq_private != NULL);
	KASSERT(BUFQ_PEEK(bufq) == NULL);

	FREE(bufq->bq_private, M_DEVBUF);
	bufq->bq_get = NULL;
	bufq->bq_put = NULL;
}

/*
 * Bounds checking against the media size, used for the raw partition.
 * The sector size passed in should currently always be DEV_BSIZE,
 * and the media size the size of the device in DEV_BSIZE sectors.
 */
int
bounds_check_with_mediasize(struct buf *bp, int secsize, u_int64_t mediasize)
{
	int64_t sz;

	sz = howmany(bp->b_bcount, secsize);

	if (bp->b_blkno + sz > mediasize) {
		sz = mediasize - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	return 1;

bad:
	bp->b_flags |= B_ERROR;
done:
	return 0;
}
