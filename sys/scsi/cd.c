/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 *      $Id: cd.c,v 1.18.2.6 1994/02/01 20:05:16 mycroft Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/cdio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_cd.h>
#include <scsi/scsi_disk.h>	/* rw_big and start_stop come from there */
#include <scsi/scsiconf.h>

#ifdef	DDB
int	Debugger();
#else	/* DDB */
#define Debugger()
#endif	/* DDB */

#define	CDOUTSTANDING	2
#define	CDRETRIES	1

#define	MAKECDDEV(maj, unit, part)	(makedev(maj,(unit<<3)|part))
#define CDPART(z)	(minor(z) & 0x07)
#define CDUNIT(z)	(minor(z) >> 3)
#define	RAW_PART	3

struct cd_data {
	struct device sc_dev;
	struct dkdevice sc_dk;

	u_int32 flags;
#define	CDINIT		0x04	/* device has been init'd */
	struct scsi_link *sc_link;	/* address of scsi low level switch */
	u_int32 cmdscount;	/* cmds allowed outstanding by board */
	struct cd_parms {
		u_int32 blksize;
		u_long disksize;	/* total number sectors */
	} params;
	u_int32 partflags[MAXPARTITIONS];	/* per partition flags */
#define CDOPEN	0x01
	u_int32 openparts;	/* one bit for each open partition */
	u_int32 xfer_block_wait;
	struct buf buf_queue;
};

void cdattach __P((struct device *, struct device *, void *));

struct cfdriver cdcd =
{ NULL, "cd", scsi_targmatch, cdattach, DV_DISK, sizeof(struct cd_data) };

int cdgetdisklabel __P((struct cd_data *));
int cd_get_parms __P((struct cd_data *, int));
void cdstrategy __P((struct buf *));
void cdstart __P((int));

struct dkdriver cddkdriver = { cdstrategy };

struct scsi_device cd_switch =
{
    NULL,			/* use default error handler */
    cdstart,			/* we have a queue, which is started by this */
    NULL,			/* we do not have an async handler */
    NULL,			/* use default 'done' routine */
    "cd",			/* we are to be refered to by this name */
    0				/* no device specific flags */
};

#define CD_STOP		0
#define CD_START	1
#define CD_EJECT	-2

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
void
cdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cd_data *cd = (struct cd_data *)self;
	struct cd_parms *dp = &cd->params;
	struct scsi_link *sc_link = aux;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdattach: "));

	/*
	 * Store information needed to contact our base driver
	 */
	cd->sc_link = sc_link;
	sc_link->device = &cd_switch;
	sc_link->dev_unit = cd->sc_dev.dv_unit;

	cd->sc_dk.dk_driver = &cddkdriver;
	dk_establish(&cd->sc_dk, &cd->sc_dev);

	sc_link->opennings = cd->cmdscount = CDOUTSTANDING;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	cd_get_parms(cd, SCSI_NOSLEEP | SCSI_NOMASK | SCSI_SILENT);
	if (dp->disksize)
		printf(": cd present, %d x %d byte records\n",
		       cd->params.disksize, cd->params.blksize);
	else
		printf(": drive empty\n");
	cd->flags |= CDINIT;
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
int 
cdopen(dev)
	dev_t dev;
{
	int error = 0;
	int unit, part;
	struct cd_data *cd;
	struct scsi_link *sc_link;

	unit = CDUNIT(dev);
	part = CDPART(dev);

	if (unit >= cdcd.cd_ndevs)
		return ENXIO;
	cd = cdcd.cd_devs[unit];
	/*
	 * Make sure the device has been initialised
	 */
	if (!cd || !(cd->flags & CDINIT))
		return ENXIO;

	sc_link = cd->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("cdopen: dev=0x%x (unit %d (of %d),partition %d)\n",
		dev, unit, cdcd.cd_ndevs, part));

	/*
	 * If it's been invalidated, and not everybody has closed it then
	 * forbid re-entry.  (may have changed media)
	 */
	if (!(sc_link->flags & SDEV_MEDIA_LOADED) && cd->openparts)
		return ENXIO;

	/*
	 * Check that it is still responding and ok.
	 * if the media has been changed this will result in a
	 * "unit attention" error which the error code will
	 * disregard because the SDEV_MEDIA_LOADED flag is not yet set
	 */
	scsi_test_unit_ready(sc_link, SCSI_SILENT);

	/*
	 * In case it is a funny one, tell it to start
	 * not needed for some drives
	 */
	scsi_start_unit(sc_link, SCSI_ERR_OK | SCSI_SILENT);

	/*
	 * Next time actually take notice of error returns
	 */
	sc_link->flags |= SDEV_OPEN;	/* unit attn errors are now errors */
	if (scsi_test_unit_ready(sc_link, 0) != 0) {
		SC_DEBUG(sc_link, SDEV_DB3, ("device not responding\n"));
		error = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("device ok\n"));

	/* Lock the pack in. */
	scsi_prevent(sc_link, PR_PREVENT, SCSI_ERR_OK | SCSI_SILENT);

	/*
	 * Load the physical device parameters 
	 */
	if (cd_get_parms(cd, 0)) {
		error = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));

	/*
	 * Make up some partition information
	 */
	cdgetdisklabel(cd);
	SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel fabricated "));

	/*
	 * Check the partition is legal
	 */
	if (part >= cd->sc_dk.dk_label.d_npartitions &&
	    part != RAW_PART) {
		error = ENXIO;
		goto bad;
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("partition ok"));

	/*
	 *  Check that the partition exists
	 */
	if (cd->sc_dk.dk_label.d_partitions[part].p_fstype == FS_UNUSED &&
	    part != RAW_PART) {
		error = ENXIO;
		goto bad;
	}
	cd->partflags[part] |= CDOPEN;
	cd->openparts |= (1 << part);
	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	sc_link->flags |= SDEV_MEDIA_LOADED;
	return 0;

bad:
	if (!cd->openparts) {
		scsi_prevent(sc_link, PR_ALLOW, SCSI_ERR_OK | SCSI_SILENT);
		sc_link->flags &= ~SDEV_OPEN;
	}
	return error;
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
int 
cdclose(dev)
	dev_t dev;
{
	int unit, part;
	struct cd_data *cd;

	unit = CDUNIT(dev);
	part = CDPART(dev);
	cd = cdcd.cd_devs[unit];
	cd->partflags[part] &= ~CDOPEN;
	cd->openparts &= ~(1 << part);
	if (!cd->openparts) {
		scsi_prevent(cd->sc_link, PR_ALLOW, SCSI_ERR_OK | SCSI_SILENT);
		cd->sc_link->flags &= ~SDEV_OPEN;
	}
	return 0;
}

/*
 * trim the size of the transfer if needed,
 * called by physio
 * basically the smaller of our max and the scsi driver's
 * minphys (note we have no max ourselves)
 *
 * Trim buffer length if buffer-size is bigger than page size
 */
void 
cdminphys(bp)
	struct buf *bp;
{
	register struct cd_data *cd = cdcd.cd_devs[CDUNIT(bp->b_dev)];

	(cd->sc_link->adapter->scsi_minphys) (bp);
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand.  The transfer is described by a buf and will include only one
 * physical transfer.
 */
void
cdstrategy(bp)
	struct buf *bp;
{
	struct buf *dp;
	int opri;
	struct cd_data *cd;
	int unit;

	unit = CDUNIT(bp->b_dev);
	cd = cdcd.cd_devs[unit];
	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdstrategy "));
	SC_DEBUG(cd->sc_link, SDEV_DB1,
	    (" %d bytes @ blk%d\n", bp->b_bcount, bp->b_blkno));
	cdminphys(bp);
	/*
	 * If the device has been made invalid, error out
	 * maybe the media changed
	 */
	if (!(cd->sc_link->flags & SDEV_MEDIA_LOADED)) {
		bp->b_error = EIO;
		goto bad;
	}
	/*
	 * can't ever write to a CD
	 */
	if ((bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_bcount == 0)
		goto done;
	/*
	 * Decide which unit and partition we are talking about
	 */
	if (CDPART(bp->b_dev) != RAW_PART) {
		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp, &cd->sc_dk.dk_label, 1) <= 0)
			goto done;
		/* otherwise, process transfer request */
	}
	opri = splbio();
	dp = &cd->buf_queue;

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	disksort(dp, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	cdstart(unit);

	splx(opri);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * cdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It deques the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (cdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * cdstart() is called at splbio from cdstrategy and scsi_done
 */
void 
cdstart(unit)
	int unit;
{
	register struct cd_data *cd = cdcd.cd_devs[unit];
	register struct scsi_link *sc_link = cd->sc_link;
	struct buf *bp = 0;
	struct buf *dp;
	struct scsi_rw_big cmd;
	int blkno, nblks;
	struct partition *p;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdstart%d ", unit));
	/*
	 * See if there is a buf to do and we are not already
	 * doing one
	 */
	while (sc_link->opennings) {
		/*
		 * there is excess capacity, but a special waits
		 * It'll need the adapter as soon as we clear out of the
		 * way and let it run (user level wait).
		 */
		if (sc_link->flags & SDEV_WAITING)
			return;

		/*
		 * See if there is a buf with work for us to do..
		 */
		dp = &cd->buf_queue;
		if ((bp = dp->b_actf) == NULL)	/* yes, an assign */
			return;
		dp->b_actf = bp->av_forw;

		/*
		 * If the deivce has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-openned
		 */
		if (!(sc_link->flags & SDEV_MEDIA_LOADED))
			goto bad;

		/*
		 * We have a buf, now we should make a command 
		 *
		 * First, translate the block to absolute and put it in terms
		 * of the logical blocksize of the device.  Really a bit silly
		 * until we have real partitions, but.
		 */
		blkno = bp->b_blkno / (cd->params.blksize / DEV_BSIZE);
		if (CDPART(bp->b_dev) != RAW_PART) {
			p = &cd->sc_dk.dk_label.d_partitions[CDPART(bp->b_dev)];
			blkno += p->p_offset;
		}
		nblks = (bp->b_bcount + (cd->params.blksize - 1)) / (cd->params.blksize);

		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.op_code = (bp->b_flags & B_READ) ? READ_BIG : WRITE_BIG;
		cmd.addr_3 = (blkno & 0xff000000) >> 24;
		cmd.addr_2 = (blkno & 0xff0000) >> 16;
		cmd.addr_1 = (blkno & 0xff00) >> 8;
		cmd.addr_0 = blkno & 0xff;
		cmd.length2 = (nblks & 0xff00) >> 8;
		cmd.length1 = (nblks & 0xff);

		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		if (scsi_scsi_cmd(sc_link, (struct scsi_generic *) &cmd,
				  sizeof(cmd), (u_char *) bp->b_un.b_addr,
				  bp->b_bcount, CDRETRIES, 30000, bp,
				  SCSI_NOSLEEP | ((bp->b_flags & B_READ) ?
				  SCSI_DATA_IN : SCSI_DATA_OUT))
		    != SUCCESSFULLY_QUEUED) {
bad:
			printf("%s: not queued", cd->sc_dev.dv_xname);
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			biodone(bp);
		}
	}
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
int 
cdioctl(dev, cmd, addr, flag)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
{
	int error;
	int unit, part;
	register struct cd_data *cd;

	/*
	 * Find the device that the user is talking about
	 */
	unit = CDUNIT(dev);
	part = CDPART(dev);
	cd = cdcd.cd_devs[unit];
	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdioctl 0x%x ", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if (!(cd->sc_link->flags & SDEV_MEDIA_LOADED))
		return EIO;
	switch (cmd) {

	case DIOCSBAD:
		return EINVAL;

	case DIOCGDINFO:
		*(struct disklabel *) addr = cd->sc_dk.dk_label;
		return 0;

	case DIOCGPART:
		((struct partinfo *) addr)->disklab = &cd->sc_dk.dk_label;
		((struct partinfo *) addr)->part =
		    &cd->sc_dk.dk_label.d_partitions[CDPART(dev)];
		return 0;

		/*
		 * a bit silly, but someone might want to test something on a 
		 * section of cdrom.
		 */
	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(&cd->sc_dk.dk_label,
				     (struct disklabel *) addr,
		    		     0, 0);
		return error;

	case DIOCWLABEL:
		return EBADF;

	case CDIOCPLAYTRACKS: {
		struct ioc_play_track *args
		= (struct ioc_play_track *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.flags &= ~CD_PA_SOTC;
		data.page.audio.flags |= CD_PA_IMMED;
		if (error = cd_set_mode(cd, &data))
			return error;
		return cd_play_tracks(cd,
				      args->start_track, args->start_index,
				      args->end_track, args->end_index);
	}
	case CDIOCPLAYMSF: {
		struct ioc_play_msf *args
		= (struct ioc_play_msf *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.flags &= ~CD_PA_SOTC;
		data.page.audio.flags |= CD_PA_IMMED;
		if (error = cd_set_mode(cd, &data))
			return error;
		return cd_play_msf(cd,
				   args->start_m, args->start_s, args->start_f,
				   args->end_m, args->end_s, args->end_f);
	}
	case CDIOCPLAYBLOCKS: {
		struct ioc_play_blocks *args
		= (struct ioc_play_blocks *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.flags &= ~CD_PA_SOTC;
		data.page.audio.flags |= CD_PA_IMMED;
		if (error = cd_set_mode(cd, &data))
			return error;
		return cd_play(cd, args->blk, args->len);
	}
	case CDIOCREADSUBCHANNEL: {
		struct ioc_read_subchannel *args
		= (struct ioc_read_subchannel *) addr;
		struct cd_sub_channel_info data;
		u_int32 len = args->data_len;
		if (len > sizeof(data) ||
		    len < sizeof(struct cd_sub_channel_header))
			return EINVAL;
		if (error = cd_read_subchannel(cd, args->address_format,
					       args->data_format, args->track,
					       &data, len))
			return error;
		len = MIN(len, ((data.header.data_len[0] << 8) + data.header.data_len[1] +
			sizeof(struct cd_sub_channel_header)));
		return copyout(&data, args->data, len);
	}
	case CDIOREADTOCHEADER: {
		struct ioc_toc_header th;
		if (error = cd_read_toc(cd, 0, 0, &th, sizeof(th)))
			return error;
		th.len = (th.len & 0xff) << 8 + ((th.len >> 8) & 0xff);
		bcopy(&th, addr, sizeof(th));
		return 0;
	}
	case CDIOREADTOCENTRYS: {
		struct ioc_read_toc_entry *te =
		(struct ioc_read_toc_entry *) addr;
		struct cd_toc_entry data[65];
		struct ioc_toc_header *th;
		u_int32 len = te->data_len;
		th = (struct ioc_toc_header *) data;

		if (len > sizeof(data) || len < sizeof(struct cd_toc_entry))
			return EINVAL;
		if (error = cd_read_toc(cd, te->address_format,
					te->starting_track, data, len))
			return error;
		len = MIN(len, ((((th->len & 0xff) << 8) + ((th->len >> 8))) +
			sizeof(*th)));
		return copyout(th, te->data, len);
	}
	case CDIOCSETPATCH: {
		struct ioc_patch *arg = (struct ioc_patch *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = arg->patch[0];
		data.page.audio.port[RIGHT_PORT].channels = arg->patch[1];
		data.page.audio.port[2].channels = arg->patch[2];
		data.page.audio.port[3].channels = arg->patch[3];
		return cd_set_mode(cd, &data);
	}
	case CDIOCGETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		arg->vol[LEFT_PORT] = data.page.audio.port[LEFT_PORT].volume;
		arg->vol[RIGHT_PORT] = data.page.audio.port[RIGHT_PORT].volume;
		arg->vol[2] = data.page.audio.port[2].volume;
		arg->vol[3] = data.page.audio.port[3].volume;
		return 0;
	}
	case CDIOCSETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].volume = arg->vol[LEFT_PORT];
		data.page.audio.port[RIGHT_PORT].volume = arg->vol[RIGHT_PORT];
		data.page.audio.port[2].volume = arg->vol[2];
		data.page.audio.port[3].volume = arg->vol[3];
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETMONO: {
		struct ioc_vol *arg = (struct ioc_vol *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL | RIGHT_CHANNEL | 4 | 8;
		data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL | RIGHT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETSTERIO: {
		struct ioc_vol *arg = (struct ioc_vol *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
		data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETMUTE: {
		struct ioc_vol *arg = (struct ioc_vol *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = 0;
		data.page.audio.port[RIGHT_PORT].channels = 0;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETLEFT: {
		struct ioc_vol *arg = (struct ioc_vol *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
		data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETRIGHT: {
		struct ioc_vol *arg = (struct ioc_vol *) addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = RIGHT_CHANNEL;
		data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCRESUME:
		return cd_pause(cd, 1);
	case CDIOCPAUSE:
		return cd_pause(cd, 0);
	case CDIOCSTART:
		return scsi_start_unit(cd->sc_link, 0);
	case CDIOCSTOP:
		return scsi_stop_unit(cd->sc_link, 0, 0);
	case CDIOCEJECT:
		return scsi_stop_unit(cd->sc_link, 1, 0);
	case CDIOCALLOW:
		return scsi_prevent(cd->sc_link, PR_ALLOW, 0);
	case CDIOCPREVENT:
		return scsi_prevent(cd->sc_link, PR_PREVENT, 0);
	case CDIOCSETDEBUG:
		cd->sc_link->flags |= (SDEV_DB1 | SDEV_DB2);
		return 0;
	case CDIOCCLRDEBUG:
		cd->sc_link->flags &= ~(SDEV_DB1 | SDEV_DB2);
		return 0;
	case CDIOCRESET:
		return cd_reset(cd);
	default:
		if (part != RAW_PART)
			return ENOTTY;
		return scsi_do_ioctl(cd->sc_link, cmd, addr, flag);
	}
#ifdef DIAGNOSTIC
	panic("cdioctl: impossible");
#endif
}

/*
 * Load the label information on the named device
 * Actually fabricate a disklabel
 * 
 * EVENTUALLY take information about different
 * data tracks from the TOC and put it in the disklabel
 */
int 
cdgetdisklabel(cd)
	struct cd_data *cd;
{
	char *errstring;

	bzero(&cd->sc_dk.dk_label, sizeof(struct disklabel));
	bzero(&cd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));
	/*
	 * make partition 0 the whole disk
	 * remember that comparisons with the partition are done
	 * assuming the blocks are 512 bytes so fudge it.
	 */
	cd->sc_dk.dk_label.d_partitions[0].p_offset = 0;
	cd->sc_dk.dk_label.d_partitions[0].p_size
	    = cd->params.disksize * (cd->params.blksize / DEV_BSIZE);
	cd->sc_dk.dk_label.d_partitions[0].p_fstype = 9;	/* XXXX */
	cd->sc_dk.dk_label.d_npartitions = 1;

	cd->sc_dk.dk_label.d_secsize = cd->params.blksize;
	cd->sc_dk.dk_label.d_ntracks = 1;
	cd->sc_dk.dk_label.d_nsectors = 100;
	cd->sc_dk.dk_label.d_ncylinders = (cd->params.disksize / 100) + 1;
	cd->sc_dk.dk_label.d_secpercyl = 100;

	strncpy(cd->sc_dk.dk_label.d_typename, "scsi cd_rom", 16);
	strncpy(cd->sc_dk.dk_label.d_packname, "ficticious", 16);
	cd->sc_dk.dk_label.d_secperunit = cd->params.disksize;
	cd->sc_dk.dk_label.d_rpm = 300;
	cd->sc_dk.dk_label.d_interleave = 1;
	cd->sc_dk.dk_label.d_flags = D_REMOVABLE;
	cd->sc_dk.dk_label.d_magic = DISKMAGIC;
	cd->sc_dk.dk_label.d_magic2 = DISKMAGIC;
	cd->sc_dk.dk_label.d_checksum = dkcksum(&cd->sc_dk.dk_label);

	/*
	 * Signal to other users and routines that we now have a
	 * disklabel that represents the media (maybe)
	 */
	return 0;
}

/*
 * Find out from the device what it's capacity is
 */
u_int32 
cd_size(cd, flags)
	struct cd_data *cd;
	int flags;
{
	struct scsi_read_cd_cap_data rdcap;
	struct scsi_read_cd_capacity scsi_cmd;
	u_int32 size, blksize;

	/*
	 * make up a scsi command and ask the scsi driver to do
	 * it for you.
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_CD_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks and a blocksize
	 */
	if (scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			  sizeof(scsi_cmd), (u_char *) &rdcap, sizeof(rdcap),
			  CDRETRIES, 20000, NULL, SCSI_DATA_IN | flags) != 0) {
		if (!(flags & SCSI_SILENT))
			printf("%s: could not get size\n",
			       cd->sc_dev.dv_xname);
		return 0;
	} else {
		size = rdcap.addr_0 + 1;
		size += rdcap.addr_1 << 8;
		size += rdcap.addr_2 << 16;
		size += rdcap.addr_3 << 24;
		blksize = rdcap.length_0;
		blksize += rdcap.length_1 << 8;
		blksize += rdcap.length_2 << 16;
		blksize += rdcap.length_3 << 24;
	}
	if (blksize < 512)
		blksize = 2048;	/* some drives lie ! */
	if (size < 100)
		size = 400000;	/* ditto */
	cd->params.disksize = size;
	cd->params.blksize = blksize;
	return size;
}

/*
 * Get the requested page into the buffer given
 */
int 
cd_get_mode(cd, data, page)
	struct cd_data *cd;
	struct cd_mode_data *data;
	int page;
{
	struct scsi_mode_sense scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(data, sizeof(*data));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.page = page;
	scsi_cmd.length = sizeof(*data) & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), (u_char *) data, sizeof(*data),
	    		     CDRETRIES, 20000, NULL, SCSI_DATA_IN);
}

/*
 * Get the requested page into the buffer given
 */
int 
cd_set_mode(cd, data)
	struct cd_data *cd;
	struct cd_mode_data *data;
{
	struct scsi_mode_select scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = sizeof(*data) & 0xff;
	data->header.data_length = 0;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), (u_char *) data, sizeof(*data),
			     CDRETRIES, 20000, NULL, SCSI_DATA_OUT);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int 
cd_play(cd, blkno, nblks)
	struct cd_data *cd;
	int blkno, nblks;
{
	struct scsi_play scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY;
	scsi_cmd.blk_addr[0] = (blkno >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blkno >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blkno >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blkno & 0xff;
	scsi_cmd.xfer_len[0] = (nblks >> 8) & 0xff;
	scsi_cmd.xfer_len[1] = nblks & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, CDRETRIES, 200000, NULL,
			     0);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int 
cd_play_big(cd, blkno, nblks)
	struct cd_data *cd;
	int blkno, nblks;
{
	struct scsi_play_big scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_BIG;
	scsi_cmd.blk_addr[0] = (blkno >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blkno >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blkno >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blkno & 0xff;
	scsi_cmd.xfer_len[0] = (nblks >> 24) & 0xff;
	scsi_cmd.xfer_len[1] = (nblks >> 16) & 0xff;
	scsi_cmd.xfer_len[2] = (nblks >> 8) & 0xff;
	scsi_cmd.xfer_len[3] = nblks & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, CDRETRIES, 20000, NULL,
			     0);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int 
cd_play_tracks(cd, strack, sindex, etrack, eindex)
	struct cd_data *cd;
	int strack, sindex, etrack, eindex;
{
	struct scsi_play_track scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_TRACK;
	scsi_cmd.start_track = strack;
	scsi_cmd.start_index = sindex;
	scsi_cmd.end_track = etrack;
	scsi_cmd.end_index = eindex;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, CDRETRIES, 20000, NULL,
			     0);
}

/*
 * Get scsi driver to send a "play msf" command
 */
int 
cd_play_msf(cd, startm, starts, startf, endm, ends, endf)
	struct cd_data *cd;
	int startm, starts, startf, endm, ends, endf;
{
	struct scsi_play_msf scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PLAY_MSF;
	scsi_cmd.start_m = startm;
	scsi_cmd.start_s = starts;
	scsi_cmd.start_f = startf;
	scsi_cmd.end_m = endm;
	scsi_cmd.end_s = ends;
	scsi_cmd.end_f = endf;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, CDRETRIES, 2000, NULL,
			     0);
}

/*
 * Get scsi driver to send a "start up" command
 */
int 
cd_pause(cd, go)
	struct cd_data *cd;
	int go;
{
	struct scsi_pause scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PAUSE;
	scsi_cmd.resume = go;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(scsi_cmd), 0, 0, CDRETRIES, 2000, NULL,
			     0);
}

/*
 * Get scsi driver to send a "RESET" command
 */
int 
cd_reset(cd)
	struct cd_data *cd;
{

	return scsi_scsi_cmd(cd->sc_link, 0, 0, 0, 0, CDRETRIES, 2000, NULL,
			     SCSI_RESET);
}

/*
 * Read subchannel
 */
int 
cd_read_subchannel(cd, mode, format, track, data, len)
	struct cd_data *cd;
	int mode, format, len;
	struct cd_sub_channel_info *data;
{
	struct scsi_read_subchannel scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_SUBCHANNEL;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.byte3 = SRS_SUBQ;
	scsi_cmd.subchan_format = format;
	scsi_cmd.track = track;
	scsi_cmd.data_len[0] = (len) >> 8;
	scsi_cmd.data_len[1] = (len) & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(struct scsi_read_subchannel),
		             (u_char *) data, len, CDRETRIES, 5000, NULL,
			     SCSI_DATA_IN);
}

/*
 * Read table of contents
 */
int 
cd_read_toc(cd, mode, start, data, len)
	struct cd_data *cd;
	int mode, start, len;
	struct cd_toc_entry *data;
{
	struct scsi_read_toc scsi_cmd;
	int ntoc;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	/*if (len!=sizeof(struct ioc_toc_header))
	 * ntoc=((len)-sizeof(struct ioc_toc_header))/sizeof(struct cd_toc_entry);
	 * else */
	ntoc = len;
	scsi_cmd.op_code = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track = start;
	scsi_cmd.data_len[0] = (ntoc) >> 8;
	scsi_cmd.data_len[1] = (ntoc) & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *) &scsi_cmd,
			     sizeof(struct scsi_read_toc), (u_char *) data,
			     len, CDRETRIES, 5000, NULL, SCSI_DATA_IN);
}

#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0)

/*
 * Get the scsi driver to send a full inquiry to the device and use the
 * results to fill out the disk parameter structure.
 */
int 
cd_get_parms(cd, flags)
	struct cd_data *cd;
	int flags;
{

	/*
	 * First check if we have it all loaded
	 */
	if (cd->sc_link->flags & SDEV_MEDIA_LOADED)
		return 0;

	/*
	 * give a number of sectors so that sec * trks * cyls
	 * is <= disk_size 
	 */
	if (!cd_size(cd, flags))
		return ENXIO;

	cd->sc_link->flags |= SDEV_MEDIA_LOADED;
	return 0;
}

int
cdsize(dev_t dev)
{

	return -1;
}
