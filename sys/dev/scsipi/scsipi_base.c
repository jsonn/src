/*	$NetBSD: scsipi_base.c,v 1.26.2.7 2000/02/04 23:01:54 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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

#include "opt_scsi.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_base.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_message.h>

int	scsipi_complete __P((struct scsipi_xfer *));
int	scsipi_enqueue __P((struct scsipi_xfer *));
void	scsipi_run_queue __P((struct scsipi_channel *chan));

void	scsipi_completion_thread __P((void *));

void	scsipi_get_tag __P((struct scsipi_xfer *));
void	scsipi_put_tag __P((struct scsipi_xfer *));

int	scsipi_get_resource __P((struct scsipi_channel *));
void	scsipi_put_resource __P((struct scsipi_channel *));
__inline int scsipi_grow_resources __P((struct scsipi_channel *));

void	scsipi_async_event_max_openings __P((struct scsipi_channel *,
	    struct scsipi_max_openings *));
void	scsipi_async_event_xfer_mode __P((struct scsipi_channel *,
	    struct scsipi_xfer_mode *));

struct pool scsipi_xfer_pool;

/*
 * scsipi_init:
 *
 *	Called when a scsibus or atapibus is attached to the system
 *	to initialize shared data structures.
 */
void
scsipi_init()
{
	static int scsipi_init_done;

	if (scsipi_init_done)
		return;
	scsipi_init_done = 1;

	/* Initialize the scsipi_xfer pool. */
	pool_init(&scsipi_xfer_pool, sizeof(struct scsipi_xfer), 0,
	    0, 0, "scxspl", 0, NULL, NULL, M_DEVBUF);
}

/*
 * scsipi_channel_init:
 *
 *	Initialize a scsipi_channel when it is attached.
 */
void
scsipi_channel_init(chan)
	struct scsipi_channel *chan;
{
	size_t nbytes;
	int i;

	/* Initialize shared data. */
	scsipi_init();

	/* Initialize the queues. */
	TAILQ_INIT(&chan->chan_queue);
	TAILQ_INIT(&chan->chan_complete);

	nbytes = chan->chan_ntargets * sizeof(struct scsipi_link **);
	chan->chan_periphs = malloc(nbytes, M_DEVBUF, M_WAITOK);

	nbytes = chan->chan_nluns * sizeof(struct scsipi_periph *);
	for (i = 0; i < chan->chan_ntargets; i++) { 
		chan->chan_periphs[i] = malloc(nbytes, M_DEVBUF, M_WAITOK);
		memset(chan->chan_periphs[i], 0, nbytes);
	}

	/*
	 * Create the asynchronous completion thread.
	 */
	kthread_create(scsipi_create_completion_thread, chan);
}

/*
 * scsipi_channel_shutdown:
 *
 *	Shutdown a scsipi_channel.
 */
void
scsipi_channel_shutdown(chan)
	struct scsipi_channel *chan;
{

	/*
	 * Shut down the completion thread.
	 */
	chan->chan_flags |= SCSIPI_CHAN_SHUTDOWN;
	wakeup(&chan->chan_complete);

	/*
	 * Now wait for the thread to exit.
	 */
	while (chan->chan_thread != NULL)
		(void) tsleep(&chan->chan_thread, PRIBIO, "scshut", 0);
}

/*
 * scsipi_insert_periph:
 *
 *	Insert a periph into the channel.
 */
void
scsipi_insert_periph(chan, periph)
	struct scsipi_channel *chan;
	struct scsipi_periph *periph;
{
	int s;

	s = splbio();
	chan->chan_periphs[periph->periph_target][periph->periph_lun] = periph;
	splx(s);
}

/*
 * scsipi_remove_periph:
 *
 *	Remove a periph from the channel.
 */
void
scsipi_remove_periph(chan, periph)
	struct scsipi_channel *chan;
	struct scsipi_periph *periph;
{
	int s;

	s = splbio();
	chan->chan_periphs[periph->periph_target][periph->periph_lun] = NULL;
	splx(s);
}

/*
 * scsipi_lookup_periph:
 *
 *	Lookup a periph on the specified channel.
 */
struct scsipi_periph *
scsipi_lookup_periph(chan, target, lun)
	struct scsipi_channel *chan;
	int target, lun;
{
	struct scsipi_periph *periph;
	int s;

	if (target >= chan->chan_ntargets ||
	    lun >= chan->chan_nluns)
		return (NULL);

	s = splbio();
	periph = chan->chan_periphs[target][lun];
	splx(s);

	return (periph);
}

/*
 * scsipi_get_resource:
 *
 *	Allocate a single xfer `resource' from the channel.
 *
 *	NOTE: Must be called at splbio().
 */
int
scsipi_get_resource(chan)
	struct scsipi_channel *chan;
{
	struct scsipi_adapter *adapt = chan->chan_adapter;

	if (chan->chan_flags & SCSIPI_CHAN_OPENINGS) {
		if (chan->chan_openings > 0) {
			chan->chan_openings--;
			return (1);
		}
		return (0);
	}

	if (adapt->adapt_openings > 0) {
		adapt->adapt_openings--;
		return (1);
	}
	return (0);
}

/*
 * scsipi_grow_resources:
 *
 *	Attempt to grow resources for a channel.  If this succeeds,
 *	we allocate one for our caller.
 *
 *	NOTE: Must be called at splbio().
 */
__inline int
scsipi_grow_resources(chan)
	struct scsipi_channel *chan;
{

	if (chan->chan_flags & SCSIPI_CHAN_CANGROW) {
		scsipi_adapter_request(chan, ADAPTER_REQ_GROW_RESOURCES, NULL);
		return (scsipi_get_resource(chan));
	}

	return (0);
}

/*
 * scsipi_put_resource:
 *
 *	Free a single xfer `resource' to the channel.
 *
 *	NOTE: Must be called at splbio().
 */
void
scsipi_put_resource(chan)
	struct scsipi_channel *chan;
{
	struct scsipi_adapter *adapt = chan->chan_adapter;

	if (chan->chan_flags & SCSIPI_CHAN_OPENINGS)
		chan->chan_openings++;
	else
		adapt->adapt_openings++;
}

/*
 * scsipi_get_tag:
 *
 *	Get a tag ID for the specified xfer.
 *
 *	NOTE: Must be called at splbio().
 */
void
scsipi_get_tag(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_periph *periph = xs->xs_periph;
	int word, bit, tag;

	for (word = 0; word < PERIPH_NTAGWORDS; word++) {
		bit = ffs(periph->periph_freetags[word]);
		if (bit != 0)
			break;
	}
#ifdef DIAGNOSTIC
	if (word == PERIPH_NTAGWORDS) {
		scsipi_printaddr(periph);
		printf("no free tags\n");
		panic("scsipi_get_tag");
	}
#endif

	bit -= 1;
	periph->periph_freetags[word] &= ~(1 << bit);
	tag = (word << 5) | bit;

	/* XXX Should eventually disallow this completely. */
	if (tag >= periph->periph_openings) {
		scsipi_printaddr(periph);
		printf("WARNING: tag %d greater than available openings %d\n",
		    tag, periph->periph_openings);
	}

	xs->xs_tag_id = tag;
}

/*
 * scsipi_put_tag:
 *
 *	Put the tag ID for the specified xfer back into the pool.
 *
 *	NOTE: Must be called at splbio().
 */
void
scsipi_put_tag(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_periph *periph = xs->xs_periph;
	int word, bit;

	word = xs->xs_tag_id >> 5;
	bit = xs->xs_tag_id & 0x1f;

	periph->periph_freetags[word] |= (1 << bit);
}

/*
 * scsipi_get_xs:
 *
 *	Allocate an xfer descriptor and associate it with the
 *	specified peripherial.  If the peripherial has no more
 *	available command openings, we either block waiting for
 *	one to become available, or fail.
 */
struct scsipi_xfer *
scsipi_get_xs(periph, flags)
	struct scsipi_periph *periph;
	int flags;
{
	struct scsipi_xfer *xs;
	int s;

	SC_DEBUG(periph, SCSIPI_DB3, ("scsipi_get_xs\n"));

	/*
	 * If we're cold, make sure we poll.
	 */
	if (cold)
		flags |= XS_CTL_NOSLEEP | XS_CTL_POLL;

#ifdef DIAGNOSTIC
	/*
	 * URGENT commands can never be ASYNC.
	 */
	if ((flags & (XS_CTL_URGENT|XS_CTL_ASYNC)) ==
	    (XS_CTL_URGENT|XS_CTL_ASYNC)) {
		scsipi_printaddr(periph);
		printf("URGENT and ASYNC\n");
		panic("scsipi_get_xs");
	}
#endif

	s = splbio();
	/*
	 * Wait for a command opening to become available.  Rules:
	 *
	 *	- All xfers must wait for an available opening.
	 *	  Exception: URGENT xfers can proceed when
	 *	  active == openings, because we use the opening
	 *	  of the command we're recovering for.
	 *
	 *	- If the periph is recovering, only URGENT xfers may
	 *	  proceed.
	 *
	 *	- If the periph is currently executing a recovery
	 *	  command, URGENT commands must block, because only
	 *	  one recovery command can execute at a time.
	 */
	for (;;) {
		if (flags & XS_CTL_URGENT) {
			if (periph->periph_active > periph->periph_openings ||
			    (periph->periph_flags &
			     PERIPH_RECOVERY_ACTIVE) != 0)
				goto wait_for_opening;
			periph->periph_flags |= PERIPH_RECOVERY_ACTIVE;
			break;
		}
		if (periph->periph_active >= periph->periph_openings ||
		    (periph->periph_flags & PERIPH_RECOVERING) != 0)
			goto wait_for_opening;
		periph->periph_active++;
		break;

 wait_for_opening:
		if (flags & XS_CTL_NOSLEEP) {
			splx(s);
			return (NULL);
		}
		SC_DEBUG(periph, SCSIPI_DB3, ("sleeping\n"));
		periph->periph_flags |= PERIPH_WAITING;
		(void) tsleep(periph, PRIBIO, "getxs", 0);
	}
	SC_DEBUG(periph, SCSIPI_DB3, ("calling pool_get\n"));
	xs = pool_get(&scsipi_xfer_pool,
	    ((flags & XS_CTL_NOSLEEP) != 0 ? PR_NOWAIT : PR_WAITOK));
	if (xs == NULL) {
		if (flags & XS_CTL_URGENT)
			periph->periph_flags &= ~PERIPH_RECOVERY_ACTIVE;
		else
			periph->periph_active--;
		scsipi_printaddr(periph);
		printf("unable to allocate %sscsipi_xfer\n",
		    (flags & XS_CTL_URGENT) ? "URGENT " : "");
	}
	splx(s);

	SC_DEBUG(periph, SCSIPI_DB3, ("returning\n"));

	if (xs != NULL) {
		memset(xs, 0, sizeof(*xs));
		xs->xs_periph = periph;
		xs->xs_control = flags;
		s = splbio();
		TAILQ_INSERT_TAIL(&periph->periph_xferq, xs, device_q);
		splx(s);
	}
	return (xs);
}

/*
 * scsipi_put_xs:
 *
 *	Release an xfer descriptor, decreasing the outstanding command
 *	count for the peripherial.  If there is a thread waiting for
 *	an opening, wake it up.  If not, kick any queued I/O the
 *	peripherial may have.
 *
 *	NOTE: Must be called at splbio().
 */
void
scsipi_put_xs(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_periph *periph = xs->xs_periph;
	int flags = xs->xs_control;

	SC_DEBUG(periph, SCSIPI_DB3, ("scsipi_free_xs\n"));

	TAILQ_REMOVE(&periph->periph_xferq, xs, device_q);
	pool_put(&scsipi_xfer_pool, xs);

#ifdef DIAGNOSTIC
	if ((periph->periph_flags & PERIPH_RECOVERY_ACTIVE) != 0 &&
	    periph->periph_active == 0) {
		scsipi_printaddr(periph);
		printf("recovery without a command to recovery for\n");
		panic("scsipi_put_xs");
	}
#endif

	if (flags & XS_CTL_URGENT)
		periph->periph_flags &= ~PERIPH_RECOVERY_ACTIVE;
	else
		periph->periph_active--;
	if (periph->periph_active == 0 &&
	    (periph->periph_flags & PERIPH_WAITDRAIN) != 0) {
		periph->periph_flags &= ~PERIPH_WAITDRAIN;
		wakeup(&periph->periph_active);
	}

	if (periph->periph_flags & PERIPH_WAITING) {
		periph->periph_flags &= ~PERIPH_WAITING;
		wakeup(periph);
	} else {
		if (periph->periph_switch->psw_start != NULL) {
			SC_DEBUG(periph, SCSIPI_DB2,
			    ("calling private start()\n"));
			(*periph->periph_switch->psw_start)(periph);
		}
	}
}

/*
 * scsipi_channel_freeze:
 *
 *	Freeze a channel's xfer queue.
 */
void
scsipi_channel_freeze(chan, count)
	struct scsipi_channel *chan;
	int count;
{
	int s;

	s = splbio();
	chan->chan_qfreeze += count;
	splx(s);
}

/*
 * scsipi_channel_thaw:
 *
 *	Thaw a channel's xfer queue.
 */
void
scsipi_channel_thaw(chan, count)
	struct scsipi_channel *chan;
	int count;
{
	int s;

	s = splbio();
	chan->chan_qfreeze -= count;
	splx(s);
}

/*
 * scsipi_channel_timed_thaw:
 *
 *	Thaw a channel after some time has expired.
 */
void
scsipi_channel_timed_thaw(arg)
	void *arg;
{
	struct scsipi_channel *chan = arg;

	scsipi_channel_thaw(chan, 1);

	/*
	 * Kick the channel's queue here.  Note, we're running in
	 * interrupt context (softclock), so the adapter driver
	 * had better not sleep.
	 */
	scsipi_run_queue(chan);
}

/*
 * scsipi_periph_freeze:
 *
 *	Freeze a device's xfer queue.
 */
void
scsipi_periph_freeze(periph, count)
	struct scsipi_periph *periph;
	int count;
{
	int s;

	s = splbio();
	periph->periph_qfreeze += count;
	splx(s);
}

/*
 * scsipi_periph_thaw:
 *
 *	Thaw a device's xfer queue.
 */
void
scsipi_periph_thaw(periph, count)
	struct scsipi_periph *periph;
	int count;
{
	int s;

	s = splbio();
	periph->periph_qfreeze -= count;
	if (periph->periph_qfreeze == 0 &&
	    (periph->periph_flags & PERIPH_WAITING) != 0)
		wakeup(periph);
	splx(s);
}

/*
 * scsipi_periph_timed_thaw:
 *
 *	Thaw a device after some time has expired.
 */
void
scsipi_periph_timed_thaw(arg)
	void *arg;
{
	struct scsipi_periph *periph = arg;

	scsipi_periph_thaw(periph, 1);

	/*
	 * Kick the channel's queue here.  Note, we're running in
	 * interrupt context (softclock), so the adapter driver
	 * had better not sleep.
	 */
	scsipi_run_queue(periph->periph_channel);
}

/*
 * scsipi_wait_drain:
 *
 *	Wait for a periph's pending xfers to drain.
 */
void
scsipi_wait_drain(periph)
	struct scsipi_periph *periph;
{
	int s;

	s = splbio();
	while (periph->periph_active != 0) {
		periph->periph_flags |= PERIPH_WAITDRAIN;
		(void) tsleep(&periph->periph_active, PRIBIO, "sxdrn", 0);
	}
	splx(s);
}

/*
 * scsipi_kill_pending:
 *
 *	Kill off all pending xfers for a periph.
 *
 *	NOTE: Must be called at splbio().
 */
void
scsipi_kill_pending(periph)
	struct scsipi_periph *periph;
{

	(*periph->periph_channel->chan_bustype->bustype_kill_pending)(periph);
#ifdef DIAGNOSTIC
	if (TAILQ_FIRST(&periph->periph_xferq) != NULL)
		panic("scsipi_kill_pending");
#endif
}

/*
 * scsipi_interpret_sense:
 *
 *	Look at the returned sense and act on the error, determining
 *	the unix error number to pass back.  (0 = report no error)
 *
 *	NOTE: If we return ERESTART, we are expected to haved
 *	thawed the device!
 *
 *	THIS IS THE DEFAULT ERROR HANDLER FOR SCSI DEVICES.
 */
int
scsipi_interpret_sense(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_sense_data *sense;
	struct scsipi_periph *periph = xs->xs_periph;
	u_int8_t key;
	u_int32_t info;
	int error;
#ifndef	SCSIVERBOSE
	static char *error_mes[] = {
		"soft error (corrected)",
		"not ready", "medium error",
		"non-media hardware failure", "illegal request",
		"unit attention", "readonly device",
		"no data found", "vendor unique",
		"copy aborted", "command aborted",
		"search returned equal", "volume overflow",
		"verify miscompare", "unknown error key"
	};
#endif

	sense = &xs->sense.scsi_sense;
#ifdef SCSIPI_DEBUG
	if (periph->periph_flags & SCSIPI_DB1) {
		int count;
		scsipi_printaddr(periph);
		printf(" sense debug information:\n");
		printf("\tcode 0x%x valid 0x%x\n",
			sense->error_code & SSD_ERRCODE,
			sense->error_code & SSD_ERRCODE_VALID ? 1 : 0);
		printf("\tseg 0x%x key 0x%x ili 0x%x eom 0x%x fmark 0x%x\n",
			sense->segment,
			sense->flags & SSD_KEY,
			sense->flags & SSD_ILI ? 1 : 0,
			sense->flags & SSD_EOM ? 1 : 0,
			sense->flags & SSD_FILEMARK ? 1 : 0);
		printf("\ninfo: 0x%x 0x%x 0x%x 0x%x followed by %d "
			"extra bytes\n",
			sense->info[0],
			sense->info[1],
			sense->info[2],
			sense->info[3],
			sense->extra_len);
		printf("\textra: ");
		for (count = 0; count < ADD_BYTES_LIM(sense); count++)
			printf("0x%x ", sense->cmd_spec_info[count]);
		printf("\n");
	}
#endif

	/*
	 * If the periph has it's own error handler, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal error processing.
	 */
	if (periph->periph_switch->psw_error != NULL) {
		SC_DEBUG(periph, SCSIPI_DB2,
		    ("calling private err_handler()\n"));
		error = (*periph->periph_switch->psw_error)(xs);
		if (error != EJUSTRETURN)
			return (error);
	}
	/* otherwise use the default */
	switch (sense->error_code & SSD_ERRCODE) {
		/*
		 * If it's code 70, use the extended stuff and
		 * interpret the key
		 */
	case 0x71:		/* delayed error */
		scsipi_printaddr(periph);
		key = sense->flags & SSD_KEY;
		printf(" DEFERRED ERROR, key = 0x%x\n", key);
		/* FALLTHROUGH */
	case 0x70:
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0)
			info = _4btol(sense->info);
		else
			info = 0;
		key = sense->flags & SSD_KEY;

		switch (key) {
		case SKEY_NO_SENSE:
		case SKEY_RECOVERED_ERROR:
			if (xs->resid == xs->datalen && xs->datalen) {
				/*
				 * Why is this here?
				 */
				xs->resid = 0;	/* not short read */
			}
		case SKEY_EQUAL:
			error = 0;
			break;
		case SKEY_NOT_READY:
			if ((periph->periph_flags & PERIPH_REMOVABLE) != 0)
				periph->periph_flags &= ~PERIPH_MEDIA_LOADED;
			if ((xs->xs_control & XS_CTL_IGNORE_NOT_READY) != 0)
				return (0);
			if (sense->add_sense_code == 0x3A &&
			    sense->add_sense_code_qual == 0x00)
				error = ENODEV; /* Medium not present */
			else
				error = EIO;
			if ((xs->xs_control & XS_CTL_SILENT) != 0)
				return (error);
			break;
		case SKEY_ILLEGAL_REQUEST:
			if ((xs->xs_control &
			     XS_CTL_IGNORE_ILLEGAL_REQUEST) != 0)
				return (0);
			/*
			 * Handle the case where a device reports
			 * Logical Unit Not Supported during discovery.
			 */
			if ((xs->xs_control & XS_CTL_DISCOVERY) != 0 &&
			    sense->add_sense_code == 0x25 &&
			    sense->add_sense_code_qual == 0x00)
				return (EINVAL);
			if ((xs->xs_control & XS_CTL_SILENT) != 0)
				return (EIO);
			error = EINVAL;
			break;
		case SKEY_UNIT_ATTENTION:
			if (sense->add_sense_code == 0x29 &&
			    sense->add_sense_code_qual == 0x00) {
				/* device or bus reset */
				return (ERESTART);
			}
			if ((periph->periph_flags & PERIPH_REMOVABLE) != 0)
				periph->periph_flags &= ~PERIPH_MEDIA_LOADED;
			if ((xs->xs_control &
			     XS_CTL_IGNORE_MEDIA_CHANGE) != 0 ||
				/* XXX Should reupload any transient state. */
				(periph->periph_flags &
				 PERIPH_REMOVABLE) == 0) {
				return (ERESTART);
			}
			if ((xs->xs_control & XS_CTL_SILENT) != 0)
				return (EIO);
			error = EIO;
			break;
		case SKEY_WRITE_PROTECT:
			error = EROFS;
			break;
		case SKEY_BLANK_CHECK:
			error = 0;
			break;
		case SKEY_ABORTED_COMMAND:
			error = ERESTART;
			break;
		case SKEY_VOLUME_OVERFLOW:
			error = ENOSPC;
			break;
		default:
			error = EIO;
			break;
		}

#ifdef SCSIVERBOSE
		if ((xs->xs_control & XS_CTL_SILENT) == 0)
			scsipi_print_sense(xs, 0);
#else
		if (key) {
			scsipi_printaddr(periph);
			printf("%s", error_mes[key - 1]);
			if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
				switch (key) {
				case SKEY_NOT_READY:
				case SKEY_ILLEGAL_REQUEST:
				case SKEY_UNIT_ATTENTION:
				case SKEY_WRITE_PROTECT:
					break;
				case SKEY_BLANK_CHECK:
					printf(", requested size: %d (decimal)",
					    info);
					break;
				case SKEY_ABORTED_COMMAND:
					if (xs->xs_retries)
						printf(", retrying");
					printf(", cmd 0x%x, info 0x%x",
					    xs->cmd->opcode, info);
					break;
				default:
					printf(", info = %d (decimal)", info);
				}
			}
			if (sense->extra_len != 0) {
				int n;
				printf(", data =");
				for (n = 0; n < sense->extra_len; n++)
					printf(" %02x",
					    sense->cmd_spec_info[n]);
			}
			printf("\n");
		}
#endif
		return (error);

	/*
	 * Not code 70, just report it
	 */
	default:
		scsipi_printaddr(periph);
		printf("Sense Error Code 0x%x",
			sense->error_code & SSD_ERRCODE);
		if ((sense->error_code & SSD_ERRCODE_VALID) != 0) {
			struct scsipi_sense_data_unextended *usense =
			    (struct scsipi_sense_data_unextended *)sense;
			printf(" at block no. %d (decimal)",
			    _3btol(usense->block));
		}
		printf("\n");
		return (EIO);
	}
}

/*
 * scsipi_size:
 *
 *	Find out from the device what its capacity is.
 */
u_long
scsipi_size(periph, flags)
	struct scsipi_periph *periph;
	int flags;
{
	struct scsipi_read_cap_data rdcap;
	struct scsipi_read_capacity scsipi_cmd;

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = READ_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks
	 */
	if (scsipi_command(periph, (struct scsipi_generic *)&scsipi_cmd,
	    sizeof(scsipi_cmd), (u_char *)&rdcap, sizeof(rdcap),
	    2, 20000, NULL, flags | XS_CTL_DATA_IN) != 0) {
		scsipi_printaddr(periph);
		printf("could not get size\n");
		return (0);
	}

	return (_4btol(rdcap.addr) + 1);
}

/*
 * scsipi_test_unit_ready:
 *
 *	Issue a `test unit ready' request.
 */
int
scsipi_test_unit_ready(periph, flags)
	struct scsipi_periph *periph;
	int flags;
{
	struct scsipi_test_unit_ready scsipi_cmd;

	/* some ATAPI drives don't support TEST_UNIT_READY. Sigh */
	if (periph->periph_quirks & PQUIRK_NOTUR)
		return (0);

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = TEST_UNIT_READY;

	return (scsipi_command(periph,
	    (struct scsipi_generic *)&scsipi_cmd, sizeof(scsipi_cmd),
	    0, 0, 2, 10000, NULL, flags));
}

/*
 * scsipi_inquire:
 *
 *	Ask the device about itself.
 */
int
scsipi_inquire(periph, inqbuf, flags)
	struct scsipi_periph *periph;
	struct scsipi_inquiry_data *inqbuf;
	int flags;
{
	struct scsipi_inquiry scsipi_cmd;

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = INQUIRY;
	scsipi_cmd.length = sizeof(struct scsipi_inquiry_data);

	return (scsipi_command(periph,
	    (struct scsipi_generic *) &scsipi_cmd, sizeof(scsipi_cmd),
	    (u_char *) inqbuf, sizeof(struct scsipi_inquiry_data),
	    2, 10000, NULL, XS_CTL_DATA_IN | flags));
}

/*
 * scsipi_prevent:
 *
 *	Prevent or allow the user to remove the media
 */
int
scsipi_prevent(periph, type, flags)
	struct scsipi_periph *periph;
	int type, flags;
{
	struct scsipi_prevent scsipi_cmd;

	if (periph->periph_quirks & PQUIRK_NODOORLOCK)
		return (0);

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = PREVENT_ALLOW;
	scsipi_cmd.how = type;

	return (scsipi_command(periph,
	    (struct scsipi_generic *) &scsipi_cmd, sizeof(scsipi_cmd),
	    0, 0, 2, 5000, NULL, flags));
}

/*
 * scsipi_start:
 *
 *	Send a START UNIT.
 */
int
scsipi_start(periph, type, flags)
	struct scsipi_periph *periph;
	int type, flags;
{
	struct scsipi_start_stop scsipi_cmd;

	if (periph->periph_quirks & PQUIRK_NOSTARTUNIT)
		return 0;

	bzero(&scsipi_cmd, sizeof(scsipi_cmd));
	scsipi_cmd.opcode = START_STOP;
	scsipi_cmd.byte2 = 0x00;
	scsipi_cmd.how = type;

	return (scsipi_command(periph,
	    (struct scsipi_generic *) &scsipi_cmd, sizeof(scsipi_cmd),
	    0, 0, 2, (type & SSS_START) ? 60000 : 10000, NULL, flags));
}

/*
 * scsipi_done:
 *
 *	This routine is called by an adapter's interrupt handler when
 *	an xfer is completed.
 */
void
scsipi_done(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_periph *periph = xs->xs_periph;
	struct scsipi_channel *chan = periph->periph_channel;
	int s, freezecnt;

	SC_DEBUG(periph, SCSIPI_DB2, ("scsipi_done\n"));
#ifdef SCSIPI_DEBUG
	if (periph->periph_dbflags & SCSIPI_DB1)
		show_scsipi_cmd(xs);
#endif

	s = splbio();
	/*
	 * The resource this command was using is now free.
	 */
	scsipi_put_resource(chan);

	/*
	 * If the command was tagged, free the tag.
	 */
	if (XS_CTL_TAGTYPE(xs) != 0)
		scsipi_put_tag(xs);

	/* Mark the command as `done'. */
	xs->xs_status |= XS_STS_DONE;

	/*
	 * If it's a user level request, bypass all usual completion
	 * processing, let the user work it out..  We take reponsibility
	 * for freeing the xs (and restarting the device's queue) when
	 * the user returns.
	 */
	if ((xs->xs_control & XS_CTL_USERCMD) != 0) {
		splx(s);
		SC_DEBUG(periph, SCSIPI_DB3, ("calling user done()\n"));
		scsipi_user_done(xs);
		SC_DEBUG(periph, SCSIPI_DB3, ("returned from user done()\n "));
		goto out;
	}

#ifdef DIAGNOSTIC
	if ((xs->xs_control & (XS_CTL_ASYNC|XS_CTL_POLL)) ==
	    (XS_CTL_ASYNC|XS_CTL_POLL))
		panic("scsipi_done: ASYNC and POLL");
#endif

	/*
	 * If the xfer had an error of any sort, freeze the
	 * periph's queue.  Freeze it again if we were requested
	 * to do so in the xfer.
	 */
	freezecnt = 0;
	if (xs->error != XS_NOERROR)
		freezecnt++;
	if (xs->xs_control & XS_CTL_FREEZE_PERIPH)
		freezecnt++;
	if (freezecnt != 0)
		scsipi_periph_freeze(periph, freezecnt);

	/*
	 * If this was an xfer that was not to complete asynchrnously,
	 * let the requesting thread perform error checking/handling
	 * in its context.
	 */
	if ((xs->xs_control & XS_CTL_ASYNC) == 0) {
		splx(s);
		/*
		 * If it's a polling job, just return, to unwind the
		 * call graph.  We don't need to restart the queue,
		 * because pollings jobs are treated specially, and
		 * are really only used during crash dumps anyway
		 * (XXX or during boot-time autconfiguration of
		 * ATAPI devices).
		 */
		if (xs->xs_control & XS_CTL_POLL)
			return;
		wakeup(xs);
		goto out;
	}

	/*
	 * Catch the extremely common case of I/O completing
	 * without error; no use in taking a context switch
	 * if we can handle it in interrupt context.
	 */
	if (xs->error == XS_NOERROR) {
		splx(s);
		(void) scsipi_complete(xs);
		goto out;
	}

	/*
	 * There is an error on this xfer.  Put it on the channel's
	 * completion queue, and wake up the completion thread.
	 */
	TAILQ_INSERT_TAIL(&chan->chan_complete, xs, channel_q);
	splx(s);
	wakeup(&chan->chan_complete);

 out:
	/*
	 * If there are more xfers on the channel's queue, attempt to
	 * run them.
	 */
	scsipi_run_queue(chan);
}

/*
 * scsipi_complete:
 *
 *	Completion of a scsipi_xfer.  This is the guts of scsipi_done().
 *
 *	NOTE: This routine MUST be called with valid thread context
 *	except for the case where the following two conditions are
 *	true:
 *
 *		xs->error == XS_NOERROR
 *		XS_CTL_ASYNC is set in xs->xs_control
 *
 *	The semantics of this routine can be tricky, so here is an
 *	explanation:
 *
 *		0		Xfer completed successfully.
 *
 *		ERESTART	Xfer had an error, but was restarted.
 *
 *		anything else	Xfer had an error, return value is Unix
 *				errno.
 *
 *	If the return value is anything but ERESTART:
 *
 *		- If XS_CTL_ASYNC is set, `xs' has been freed back to
 *		  the pool.
 *		- If there is a buf associated with the xfer,
 *		  it has been biodone()'d.
 */
int
scsipi_complete(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_periph *periph = xs->xs_periph;
	struct scsipi_channel *chan = periph->periph_channel;
	struct buf *bp;
	int error, s;

#ifdef DIAGNOSTIC
	if ((xs->xs_control & XS_CTL_ASYNC) != 0 && xs->bp == NULL)
		panic("scsipi_complete: XS_CTL_ASYNC but no buf");
#endif

	switch (xs->error) {
	case XS_NOERROR:
		error = 0;
		break;

	case XS_SENSE:
	case XS_SHORTSENSE:
		error = (*chan->chan_bustype->bustype_interpret_sense)(xs);
		break;

	case XS_RESOURCE_SHORTAGE:
		/*
		 * XXX Should freeze channel's queue.
		 */
		scsipi_printaddr(periph);
		printf("adapter resource shortage\n");
		/* FALLTHROUGH */

	case XS_BUSY:
		if (xs->error == XS_BUSY && xs->status == SCSI_QUEUE_FULL) {
			struct scsipi_max_openings mo;

			/*
			 * We set the openings to active - 1, assuming that
			 * the command that got us here is the first one that
			 * can't fit into the device's queue.  If that's not
			 * the case, I guess we'll find out soon enough.
			 */
			mo.mo_target = periph->periph_target;
			mo.mo_lun = periph->periph_lun;
			mo.mo_openings = periph->periph_active - 1;
#ifdef DIAGNOSTIC
			if (mo.mo_openings < 0) {
				scsipi_printaddr(periph);
				printf("QUEUE FULL resulted in < 0 openings\n");
				panic("scsipi_done");
			}
#endif
			if (mo.mo_openings == 0) {
				scsipi_printaddr(periph);
				printf("QUEUE FULL resulted in 0 openings\n");
				mo.mo_openings = 1;
			}
			scsipi_async_event(chan, ASYNC_EVENT_MAX_OPENINGS, &mo);
			error = ERESTART;
		} else if (xs->xs_retries != 0) {
			xs->xs_retries--;
			/*
			 * Wait one second, and try again.
			 */
			if (xs->xs_control & XS_CTL_POLL)
				delay(1000000);
			else {
				scsipi_periph_freeze(periph, 1);
				timeout(scsipi_periph_timed_thaw, periph, hz);
			}
			error = ERESTART;
		} else
			error = EBUSY;
		break;

	case XS_TIMEOUT:
		if (xs->xs_retries != 0) {
			xs->xs_retries--;
			error = ERESTART;
		} else
			error = EIO;
		break;

	case XS_SELTIMEOUT:
		/* XXX Disable device? */
		error = EIO;
		break;

	case XS_RESET:
		if (xs->xs_retries != 0) {
			xs->xs_retries--;
			error = ERESTART;
		} else
			error = EIO;
		break;

	default:
		scsipi_printaddr(periph);
		printf("invalid return code from adapter: %d\n", xs->error);
		error = EIO;
		break;
	}

	s = splbio();
	if (error == ERESTART) {
		/*
		 * If we get here, the periph has been thawed and frozen
		 * again if we had to issue recovery commands.  Alternatively,
		 * it may have been frozen again and in a timed thaw.  In
		 * any case, we thaw the periph once we re-enqueue the
		 * command.  Once the periph is fully thawed, it will begin
		 * operation again.
		 */
		xs->error = XS_NOERROR;
		xs->status = SCSI_OK;
		xs->xs_status &= ~XS_STS_DONE;
		xs->xs_requeuecnt++;
		error = scsipi_enqueue(xs);
		if (error == 0) {
			scsipi_periph_thaw(periph, 1);
			splx(s);
			return (ERESTART);
		}
	}

	/*
	 * scsipi_done() freezes the queue if not XS_NOERROR.
	 * Thaw it here.
	 */
	if (xs->error != XS_NOERROR)
		scsipi_periph_thaw(periph, 1);

	if ((bp = xs->bp) != NULL) {
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
		} else {
			bp->b_error = 0;
			bp->b_resid = xs->resid;
		}
		biodone(bp);
	}

	if (xs->xs_control & XS_CTL_ASYNC)
		scsipi_put_xs(xs);
	splx(s);

	return (error);
}

/*
 * scsipi_enqueue:
 *
 *	Enqueue an xfer on a channel.
 */
int
scsipi_enqueue(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_channel *chan = xs->xs_periph->periph_channel;
	struct scsipi_xfer *qxs;
	int s;

	s = splbio();

	/*
	 * If the xfer is to be polled, and there are already jobs on
	 * the queue, we can't proceed.
	 */
	if ((xs->xs_control & XS_CTL_POLL) != 0 &&
	    TAILQ_FIRST(&chan->chan_queue) != NULL) {
		splx(s);
		xs->error = XS_DRIVER_STUFFUP;
		return (EAGAIN);
	}

	/*
	 * If we have an URGENT xfer, it's an error recovery command
	 * and it should just go on the head of the channel's queue.
	 */
	if (xs->xs_control & XS_CTL_URGENT) {
		TAILQ_INSERT_HEAD(&chan->chan_queue, xs, channel_q);
		goto out;
	}

	/*
	 * If this xfer has already been on the queue before, we
	 * need to reinsert it in the correct order.  That order is:
	 *
	 *	Immediately before the first xfer for this periph
	 *	with a requeuecnt less than xs->xs_requeuecnt.
	 *
	 * Failing that, at the end of the queue.  (We'll end up
	 * there naturally.)
	 */
	if (xs->xs_requeuecnt != 0) {
		for (qxs = TAILQ_FIRST(&chan->chan_queue); qxs != NULL;
		     qxs = TAILQ_NEXT(qxs, channel_q)) {
			if (qxs->xs_periph == xs->xs_periph &&
			    qxs->xs_requeuecnt < xs->xs_requeuecnt)
				break;
		}
		if (qxs != NULL) {
			TAILQ_INSERT_AFTER(&chan->chan_queue, qxs, xs,
			    channel_q);
			goto out;
		}
	}
	TAILQ_INSERT_TAIL(&chan->chan_queue, xs, channel_q);
 out:
	if (xs->xs_control & XS_CTL_THAW_PERIPH)
		scsipi_periph_thaw(xs->xs_periph, 1);
	splx(s);
	return (0);
}

/*
 * scsipi_run_queue:
 *
 *	Start as many xfers as possible running on the channel.
 */
void
scsipi_run_queue(chan)
	struct scsipi_channel *chan;
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	int s;

	for (;;) {
		s = splbio();

		/*
		 * If the channel is frozen, we can't do any work right
		 * now.
		 */
		if (chan->chan_qfreeze != 0) {
			splx(s);
			return;
		}

		/*
		 * Look for work to do, and make sure we can do it.
		 */
		for (xs = TAILQ_FIRST(&chan->chan_queue); xs != NULL;
		     xs = TAILQ_NEXT(xs, channel_q)) {
			periph = xs->xs_periph;

			if ((periph->periph_active > periph->periph_openings) ||			    periph->periph_qfreeze != 0)
				continue;

			if ((periph->periph_flags & PERIPH_RECOVERING) != 0 &&
			    (xs->xs_control & XS_CTL_URGENT) == 0)
				continue;

			/*
			 * We can issue this xfer!
			 */
			goto got_one;
		}

		/*
		 * Can't find any work to do right now.
		 */
		splx(s);
		return;

 got_one:
		/*
		 * Have an xfer to run.  Allocate a resource from
		 * the adapter to run it.  If we can't allocate that
		 * resource, we don't dequeue the xfer.
		 */
		if (scsipi_get_resource(chan) == 0) {
			/*
			 * Adapter is out of resources.  If the adapter
			 * supports it, attempt to grow them.
			 */
			if (scsipi_grow_resources(chan) == 0) {
				/*
				 * Wasn't able to grow resources,
				 * nothing more we can do.
				 */
				if (xs->xs_control & XS_CTL_POLL) {
					scsipi_printaddr(xs->xs_periph);
					printf("polling command but no "
					    "adapter resources");
					/* We'll panic shortly... */
				}
				splx(s);
				return;
			}
			/*
			 * scsipi_grow_resources() allocated the resource
			 * for us.
			 */
		}

		/*
		 * We have a resource to run this xfer, do it!
		 */
		TAILQ_REMOVE(&chan->chan_queue, xs, channel_q);

		/*
		 * If the command is to be tagged, allocate a tag ID
		 * for it.
		 */
		if (XS_CTL_TAGTYPE(xs) != 0)
			scsipi_get_tag(xs);
		splx(s);

		scsipi_adapter_request(chan, ADAPTER_REQ_RUN_XFER, xs);
	}
#ifdef DIAGNOSTIC
	panic("scsipi_run_queue: impossible");
#endif
}

/*
 * scsipi_execute_xs:
 *
 *	Begin execution of an xfer, waiting for it to complete, if necessary.
 */
int
scsipi_execute_xs(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_periph *periph = xs->xs_periph;
	struct scsipi_channel *chan = periph->periph_channel;
	int async, poll, retries, error, s;

	xs->xs_status &= ~XS_STS_DONE;
	xs->error = XS_NOERROR;
	xs->resid = xs->datalen;
	xs->status = SCSI_OK;

#ifdef SCSIPI_DEBUG
	if (xs->xs_periph->periph_dbflags & SCSIPI_DB3) {
		printf("scsipi_execute_xs: ");
		show_scsipi_xs(xs);
		printf("\n");
	}
#endif

	/*
	 * Deal with command tagging:
	 *
	 *	- If the device's current operating mode doesn't
	 *	  include tagged queueing, clear the tag mask.
	 *
	 *	- If the device's current operating mode *does*
	 *	  include tagged queueing, set the tag_type in
	 *	  the xfer to the appropriate byte for the tag
	 *	  message.
	 */
	if ((PERIPH_XFER_MODE(periph) & PERIPH_CAP_TQING) == 0) {
		xs->xs_control &= ~XS_CTL_TAGMASK;
		xs->xs_tag_type = 0;
	} else {
		/*
		 * If the request doesn't specify a tag, give Head
		 * tags to URGENT operations and Ordered tags to
		 * everything else.
		 */
		if (XS_CTL_TAGTYPE(xs) == 0) {
			if (xs->xs_control & XS_CTL_URGENT)
				xs->xs_control |= XS_CTL_HEAD_TAG;
			else
				xs->xs_control |= XS_CTL_ORDERED_TAG;
		}

		switch (XS_CTL_TAGTYPE(xs)) {
		case XS_CTL_ORDERED_TAG:
			xs->xs_tag_type = MSG_ORDERED_Q_TAG;
			break;

		case XS_CTL_SIMPLE_TAG:
			xs->xs_tag_type = MSG_SIMPLE_Q_TAG;
			break;

		case XS_CTL_HEAD_TAG:
			xs->xs_tag_type = MSG_HEAD_OF_Q_TAG;
			break;

		default:
			scsipi_printaddr(periph);
			printf("invalid tag mask 0x%08x\n",
			    XS_CTL_TAGTYPE(xs));
			panic("scsipi_execute_xs");
		}
	}

	/*
	 * If we don't yet have a completion thread, or we are to poll for
	 * completion, clear the ASYNC flag.
	 */
	if (chan->chan_thread == NULL || (xs->xs_control & XS_CTL_POLL) != 0)
		xs->xs_control &= ~XS_CTL_ASYNC;

	async = (xs->xs_control & XS_CTL_ASYNC);
	poll = (xs->xs_control & XS_CTL_POLL);
	retries = xs->xs_retries;		/* for polling commands */

#ifdef DIAGNOSTIC
	if (async != 0 && xs->bp == NULL)
		panic("scsipi_execute_xs: XS_CTL_ASYNC but no buf");
#endif

	/*
	 * Enqueue the transfer.  If we're not polling for completion, this
	 * should ALWAYS return `no error'.
	 */
 try_again:
	error = scsipi_enqueue(xs);
	if (error) {
		if (poll == 0) {
			scsipi_printaddr(periph);
			printf("not polling, but enqueue failed with %d\n",
			    error);
			panic("scsipi_execute_xs");
		}
		
		scsipi_printaddr(periph);
		printf("failed to enqueue polling command");
		if (retries != 0) {
			printf(", retrying...\n");
			delay(1000000);
			retries--;
			goto try_again;
		}
		printf("\n");
		goto free_xs;
	}

 restarted:
	scsipi_run_queue(chan);

	/*
	 * The xfer is enqueued, and possibly running.  If it's to be
	 * completed asynchronously, just return now.
	 */
	if (async)
		return (EJUSTRETURN);

	/*
	 * Not an asynchronous command; wait for it to complete.
	 */
	while ((xs->xs_status & XS_STS_DONE) == 0) {
		if (poll) {
			scsipi_printaddr(periph);
			printf("polling command not done\n");
			panic("scsipi_execute_xs");
		}
		(void) tsleep(xs, PRIBIO, "xscmd", 0);
	}

	/*
	 * Command is complete.  scsipi_done() has awakened us to perform
	 * the error handling.
	 */
	error = scsipi_complete(xs);
	if (error == ERESTART)
		goto restarted;

	/*
	 * Command completed successfully or fatal error occurred.  Fall
	 * into....
	 */
 free_xs:
	s = splbio();
	scsipi_put_xs(xs);
	splx(s);

	/*
	 * Kick the queue, keep it running in case it stopped for some
	 * reason.
	 */
	scsipi_run_queue(chan);

	return (error);
}

/*
 * scsipi_completion_thread:
 *
 *	This is the completion thread.  We wait for errors on
 *	asynchronous xfers, and perform the error handling
 *	function, restarting the command, if necessary.
 */
void
scsipi_completion_thread(arg)
	void *arg;
{
	struct scsipi_channel *chan = arg;
	struct scsipi_xfer *xs;
	int s;

	for (;;) {
		s = splbio();
		xs = TAILQ_FIRST(&chan->chan_complete);
		if (xs == NULL &&
		    (chan->chan_flags & SCSIPI_CHAN_SHUTDOWN) == 0) {
			splx(s);
			(void) tsleep(&chan->chan_complete, PRIBIO,
			    "sccomp", 0);
			continue;
		}
		if (chan->chan_flags & SCSIPI_CHAN_SHUTDOWN) {
			splx(s);
			break;
		}
		TAILQ_REMOVE(&chan->chan_complete, xs, channel_q);
		splx(s);

		/*
		 * Have an xfer with an error; process it.
		 */
		(void) scsipi_complete(xs);

		/*
		 * Kick the queue; keep it running if it was stopped
		 * for some reason.
		 */
		scsipi_run_queue(chan);
	}

	chan->chan_thread = NULL;

	/* In case parent is waiting for us to exit. */
	wakeup(&chan->chan_thread);

	kthread_exit(0);
}

/*
 * scsipi_create_completion_thread:
 *
 *	Callback to actually create the completion thread.
 */
void
scsipi_create_completion_thread(arg)
	void *arg;
{
	struct scsipi_channel *chan = arg;
	struct scsipi_adapter *adapt = chan->chan_adapter;

	if (kthread_create1(scsipi_completion_thread, chan,
	    &chan->chan_thread, "%s:%d", adapt->adapt_dev->dv_xname,
	    chan->chan_channel)) {
		printf("%s: unable to create completion thread for "
		    "channel %d\n", adapt->adapt_dev->dv_xname,
		    chan->chan_channel);
		panic("scsipi_create_completion_thread");
	}
}

/*
 * scsipi_async_event:
 *
 *	Handle an asynchronous event from an adapter.
 */
void
scsipi_async_event(chan, event, arg)
	struct scsipi_channel *chan;
	scsipi_async_event_t event;
	void *arg;
{
	int s;

	s = splbio();
	switch (event) {
	case ASYNC_EVENT_MAX_OPENINGS:
		scsipi_async_event_max_openings(chan,
		    (struct scsipi_max_openings *)arg);
		break;

	case ASYNC_EVENT_XFER_MODE:
		scsipi_async_event_xfer_mode(chan,
		    (struct scsipi_xfer_mode *)arg);
		break;
	}
	splx(s);
}

/*
 * scsipi_print_xfer_mode:
 *
 *	Print a periph's capabilities.
 */
void
scsipi_print_xfer_mode(periph)
	struct scsipi_periph *periph;
{
	int period, freq, speed, mbs;

	if ((periph->periph_flags & PERIPH_MODE_VALID) == 0)
		return;

	printf("%s: ", periph->periph_dev->dv_xname);
	if (periph->periph_mode & PERIPH_CAP_SYNC) {
		period = scsipi_sync_factor_to_period(periph->periph_period);
		printf("Sync (%d.%dns offset %d)",
		    period / 10, period % 10, periph->periph_offset);
	} else
		printf("Async");

	if (periph->periph_mode & PERIPH_CAP_WIDE32)
		printf(", 32-bit");
	else if (periph->periph_mode & PERIPH_CAP_WIDE16)
		printf(", 16-bit");
	else
		printf(", 8-bit");

	if (periph->periph_mode & PERIPH_CAP_SYNC) {
		freq = scsipi_sync_factor_to_freq(periph->periph_period);
		speed = freq;
		if (periph->periph_mode & PERIPH_CAP_WIDE32)
			speed *= 4;
		else if (periph->periph_mode & PERIPH_CAP_WIDE16)
			speed *= 2;
		mbs = speed / 1000;
		if (mbs > 0)
			printf(" (%d.%03dMB/s)", mbs, speed % 1000);
		else
			printf(" (%dKB/s)", speed % 1000);
	}

	printf(" transfers");

	if (periph->periph_mode & PERIPH_CAP_TQING)
		printf(", tagged queueing");

	printf("\n");
}

/*
 * scsipi_async_event_max_openings:
 *
 *	Update the maximum number of outstanding commands a
 *	device may have.
 */
void
scsipi_async_event_max_openings(chan, mo)
	struct scsipi_channel *chan;
	struct scsipi_max_openings *mo;
{
	struct scsipi_periph *periph;
	int minlun, maxlun;

	if (mo->mo_lun == -1) {
		/*
		 * Wildcarded; apply it to all LUNs.
		 */
		minlun = 0;
		maxlun = chan->chan_nluns - 1;
	} else
		minlun = maxlun = mo->mo_lun;

	for (; minlun <= maxlun; minlun++) {
		periph = scsipi_lookup_periph(chan, mo->mo_target, minlun);
		if (periph == NULL)
			continue;

		if (mo->mo_openings < periph->periph_openings)
			periph->periph_openings = mo->mo_openings;
		else if (mo->mo_openings > periph->periph_openings &&
		    (periph->periph_flags & PERIPH_GROW_OPENINGS) != 0)
			periph->periph_openings = mo->mo_openings;
	}
}

/*
 * scsipi_async_event_xfer_mode:
 *
 *	Update the xfer mode for all periphs sharing the
 *	specified I_T Nexus.
 */
void
scsipi_async_event_xfer_mode(chan, xm)
	struct scsipi_channel *chan;
	struct scsipi_xfer_mode *xm;
{
	struct scsipi_periph *periph;
	int lun, announce, mode, period, offset;

	for (lun = 0; lun < chan->chan_nluns; lun++) {
		periph = scsipi_lookup_periph(chan, xm->xm_target, lun);
		if (periph == NULL)
			continue;
		announce = 0;

		/*
		 * Clamp the xfer mode down to this periph's capabilities.
		 */
		mode = xm->xm_mode & periph->periph_cap;
		if (mode & PERIPH_CAP_SYNC) {
			period = xm->xm_period;
			offset = xm->xm_offset;
		} else {
			period = 0;
			offset = 0;
		}

		/*
		 * If we do not have a valid xfer mode yet, or the parameters
		 * are different, announce them.
		 */
		if ((periph->periph_flags & PERIPH_MODE_VALID) == 0 ||
		    periph->periph_mode != mode ||
		    periph->periph_period != period ||
		    periph->periph_offset != offset)
			announce = 1;

		periph->periph_mode = mode;
		periph->periph_period = period;
		periph->periph_offset = offset;
		periph->periph_flags |= PERIPH_MODE_VALID;

		if (announce)
			scsipi_print_xfer_mode(periph);
	}
}

/*
 * scsipi_set_xfer_mode:
 *
 *	Set the xfer mode for the specified I_T Nexus.
 */
void
scsipi_set_xfer_mode(chan, target, immed)
	struct scsipi_channel *chan;
	int target, immed;
{
	struct scsipi_xfer_mode xm;
	struct scsipi_periph *itperiph;
	int lun, s;

	/*
	 * Go to the minimal xfer mode.
	 */
	xm.xm_target = target;
	xm.xm_mode = 0;
	xm.xm_period = 0;			/* ignored */
	xm.xm_offset = 0;			/* ignored */

	/*
	 * Find the first LUN we know about on this I_T Nexus.
	 */
	for (lun = 0; lun < chan->chan_nluns; lun++) {
		itperiph = scsipi_lookup_periph(chan, target, lun);
		if (itperiph != NULL)
			break;
	}
	if (itperiph != NULL)
		xm.xm_mode = itperiph->periph_cap;

	/*
	 * Now issue the request to the adapter.
	 */
	s = splbio();
	scsipi_adapter_request(chan, ADAPTER_REQ_SET_XFER_MODE, &xm);
	splx(s);

	/*
	 * If we want this to happen immediately, issue a dummy command,
	 * since most adapters can't really negotiate unless they're
	 * executing a job.
	 */
	if (immed != 0 && itperiph != NULL) {
		(void) scsipi_test_unit_ready(itperiph,
		    XS_CTL_DISCOVERY | XS_CTL_IGNORE_ILLEGAL_REQUEST |
		    XS_CTL_IGNORE_NOT_READY |
		    XS_CTL_IGNORE_MEDIA_CHANGE);
	}
}

/*
 * scsipi_adapter_addref:
 *
 *	Add a reference to the adapter pointed to by the provided
 *	link, enabling the adapter if necessary.
 */
int
scsipi_adapter_addref(adapt)
	struct scsipi_adapter *adapt;
{
	int s, error = 0;

	s = splbio();
	if (adapt->adapt_refcnt++ == 0 && adapt->adapt_enable != NULL) {
		error = (*adapt->adapt_enable)(adapt->adapt_dev, 1);
		if (error)
			adapt->adapt_refcnt--;
	}
	splx(s);
	return (error);
}

/*
 * scsipi_adapter_delref:
 *
 *	Delete a reference to the adapter pointed to by the provided
 *	link, disabling the adapter if possible.
 */
void
scsipi_adapter_delref(adapt)
	struct scsipi_adapter *adapt;
{
	int s;

	s = splbio();
	if (adapt->adapt_refcnt-- == 1 && adapt->adapt_enable != NULL)
		(void) (*adapt->adapt_enable)(adapt->adapt_dev, 0);
	splx(s);
}

struct scsipi_syncparam {
	int	ss_factor;
	int	ss_period;	/* ns * 10 */
} scsipi_syncparams[] = {
	{ 0x0a,		250 },
	{ 0x0b,		303 },
	{ 0x0c,		500 },
};
const int scsipi_nsyncparams =
    sizeof(scsipi_syncparams) / sizeof(scsipi_syncparams[0]);

int
scsipi_sync_period_to_factor(period)
	int period;		/* ns * 10 */
{
	int i;

	for (i = 0; i < scsipi_nsyncparams; i++) {
		if (period <= scsipi_syncparams[i].ss_period)
			return (scsipi_syncparams[i].ss_factor);
	}

	return ((period / 10) / 4);
}

int
scsipi_sync_factor_to_period(factor)
	int factor;
{
	int i;

	for (i = 0; i < scsipi_nsyncparams; i++) {
		if (factor == scsipi_syncparams[i].ss_factor)
			return (scsipi_syncparams[i].ss_period);
	}

	return ((factor * 4) * 10);
}

int
scsipi_sync_factor_to_freq(factor)
	int factor;
{
	int i;

	for (i = 0; i < scsipi_nsyncparams; i++) {
		if (factor == scsipi_syncparams[i].ss_factor)
			return (10000000 / scsipi_syncparams[i].ss_period);
	}

	return (10000000 / ((factor * 4) * 10));
}

#ifdef SCSIPI_DEBUG
/*
 * Given a scsipi_xfer, dump the request, in all it's glory
 */
void
show_scsipi_xs(xs)
	struct scsipi_xfer *xs;
{

	printf("xs(%p): ", xs);
	printf("xs_control(0x%08x)", xs->xs_control);
	printf("xs_status(0x%08x)", xs->xs_status);
	printf("periph(%p)", xs->xs_periph);
	printf("retr(0x%x)", xs->xs_retries);
	printf("timo(0x%x)", xs->timeout);
	printf("cmd(%p)", xs->cmd);
	printf("len(0x%x)", xs->cmdlen);
	printf("data(%p)", xs->data);
	printf("len(0x%x)", xs->datalen);
	printf("res(0x%x)", xs->resid);
	printf("err(0x%x)", xs->error);
	printf("bp(%p)", xs->bp);
	show_scsipi_cmd(xs);
}

void
show_scsipi_cmd(xs)
	struct scsipi_xfer *xs;
{
	u_char *b = (u_char *) xs->cmd;
	int i = 0;

	scsipi_printaddr(xs->xs_periph);
	printf(" command: ");

	if ((xs->xs_control & XS_CTL_RESET) == 0) {
		while (i < xs->cmdlen) {
			if (i)
				printf(",");
			printf("0x%x", b[i++]);
		}
		printf("-[%d bytes]\n", xs->datalen);
		if (xs->datalen)
			show_mem(xs->data, min(64, xs->datalen));
	} else
		printf("-RESET-\n");
}

void
show_mem(address, num)
	u_char *address;
	int num;
{
	int x;

	printf("------------------------------");
	for (x = 0; x < num; x++) {
		if ((x % 16) == 0)
			printf("\n%03d: ", x);
		printf("%02x ", *address++);
	}
	printf("\n------------------------------\n");
}
#endif /* SCSIPI_DEBUG */
