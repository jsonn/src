/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumio.c,v 1.1.1.1.4.4 2004/09/21 13:34:15 skrll Exp $
 * $FreeBSD$
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

static char *sappend(char *txt, char *s);
static int drivecmp(const void *va, const void *vb);

/* Open the device associated with the drive, and set drive's vp.
 * Return an error number */
int
open_drive(struct drive *drive, struct proc *p, int verbose)
{
    struct nameidata nd;
    struct vattr va;
    int error;

    if (drive->devicename[0] == '\0')			    /* no device name */
	sprintf(drive->devicename, "/dev/%s", drive->label.name); /* get it from the drive name */
    NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, drive->devicename, p);
    error = vn_open(&nd, FREAD | FWRITE, 0);		    /* open the device */
    if (error != 0) {					    /* can't open? */
	set_drive_state(drive->driveno, drive_down, setstate_force);
	drive->lasterror = error;
	if (verbose)
	    printf("vinum open_drive %s: failed with error %d\n", drive->devicename, error); /* XXX */
	return error;
    }
    drive->flags |= VF_OPEN;				    /* mark the drive as open */
    drive->vp = nd.ni_vp;
    drive->p = p;

    error = VOP_GETATTR(drive->vp, &va, NOCRED, drive->p);
    if (error) {
	VOP_UNLOCK(drive->vp, 0);
	close_drive(drive);
	set_drive_state(drive->driveno, drive_down, setstate_force);
	drive->lasterror = error;
	if (verbose)
	    printf("vinum open_drive %s: GETAATTR returns error %d\n", drive->devicename, error); /* XXX */
	return error;
    }
    drive->dev = va.va_rdev;				    /* device */

    if (va.va_type != VBLK) {				    /* only consider block devices */
	VOP_UNLOCK(drive->vp, 0);
	close_drive(drive);
	set_drive_state(drive->driveno, drive_down, setstate_force); /* this also closes the drive */
	drive->lasterror = ENOTBLK;
	if (verbose)
	    printf("vinum open_drive %s: Not a block device\n", drive->devicename); /* XXX */
	return ENOTBLK;
    }
    drive->vp->v_numoutput = 0;
    VOP_UNLOCK(drive->vp, 0);
    return 0;
}


/*
 * Set some variables in the drive struct in more
 * convenient form.  Return error indication.
 */
int
set_drive_parms(struct drive *drive)
{
    drive->blocksize = BLKDEV_IOSIZE;			    /* do we need this? */
    drive->secsperblock = drive->blocksize		    /* number of sectors per block */
	/ drive->partinfo.disklab->d_secsize;

    /* Now update the label part */
    bcopy(hostname, drive->label.sysname, VINUMHOSTNAMELEN); /* put in host name */
    microtime(&drive->label.date_of_birth);		    /* and current time */
    drive->label.drive_size = ((u_int64_t) drive->partinfo.part->p_size) /* size of the drive in bytes */
    *((u_int64_t) drive->partinfo.disklab->d_secsize);
#ifdef VINUMDEBUG
    if (debug & DEBUG_BIGDRIVE)				    /* pretend we're 100 times as big */
	drive->label.drive_size *= 100;
#endif

    /* number of sectors available for subdisks */
    drive->sectors_available = drive->label.drive_size / DEV_BSIZE - DATASTART;

    /*
     * Bug in 3.0 as of January 1998: you can open
     * non-existent slices.  They have a length of 0.
     */
    if (drive->label.drive_size < MINVINUMSLICE) {	    /* too small to worry about */
	set_drive_state(drive->driveno, drive_down, setstate_force);
	drive->lasterror = ENOSPC;
	return ENOSPC;
    }
    drive->freelist_size = INITIAL_DRIVE_FREELIST;	    /* initial number of entries */
    drive->freelist = (struct drive_freelist *)
	Malloc(INITIAL_DRIVE_FREELIST * sizeof(struct drive_freelist));
    if (drive->freelist == NULL)			    /* can't malloc, dammit */
	return ENOSPC;
    drive->freelist_entries = 1;			    /* just (almost) the complete drive */
    drive->freelist[0].offset = DATASTART;		    /* starts here */
    drive->freelist[0].sectors = (drive->label.drive_size >> DEV_BSHIFT) - DATASTART; /* and it's this long */
    if (drive->label.name[0] != '\0')			    /* got a name */
	set_drive_state(drive->driveno, drive_up, setstate_force); /* our drive is accessible */
    else						    /* we know about it, but that's all */
	drive->state = drive_referenced;
    return 0;
}

/*
 * Initialize a drive: open the device and add
 * device information.
 */
int
init_drive(struct drive *drive, int verbose)
{
    int error;
    struct disklabel label;
    const struct bdevsw *bdev;

    if (drive->devicename[0] != '/') {
	drive->lasterror = EINVAL;
	log(LOG_ERR, "vinum: Can't open drive without drive name\n");
	return EINVAL;
    }
    drive->lasterror = open_drive(drive, curproc, verbose); /* open the drive */
    if (drive->lasterror)
	return drive->lasterror;

    bdev = bdevsw_lookup(drive->dev);
    if (bdev == NULL)
	return ENXIO;
    drive->lasterror = (*bdev->d_ioctl) (drive->dev,
	DIOCGDINFO,
	(caddr_t) & label,
	FREAD,
	curproc);
    if (drive->lasterror == 0) {
	if (label.d_type == DTYPE_FLOPPY) {
	    drive->partinfo.disklab->d_secsize = label.d_secsize;
	    drive->partinfo.part->p_size = label.d_secsize * label.d_secperunit;
	    drive->partinfo.part->p_fstype = FS_VINUM;	    /* force Vinum */
	} else
	    /* XXX should use VOP interface, like FreeBSD */
	    drive->lasterror = (*bdev->d_ioctl) (drive->dev, /* get the partition information */
		DIOCGPART,
		(caddr_t) & drive->partinfo,
		FREAD,
		curproc);
    }
    if (drive->lasterror) {
	if (verbose)
	    log(LOG_WARNING,
		"vinum open_drive %s: Can't get partition information, drive->lasterror %d\n",
		drive->devicename,
		drive->lasterror);
	error = drive->lasterror;			    /* clobbered by close_drive */
	close_drive(drive);
	return error;
    }
    if (drive->partinfo.part->p_fstype != FS_VINUM) {	    /* not Vinum */
	drive->lasterror = EFTYPE;
	if (verbose)
	    log(LOG_WARNING,
		"vinum open_drive %s: Wrong partition type for vinum\n",
		drive->devicename);
	close_drive(drive);
	return EFTYPE;
    }
    return set_drive_parms(drive);			    /* set various odds and ends */
}

/* Close a drive if it's open. */
void
close_drive(struct drive *drive)
{
    LOCKDRIVE(drive);					    /* keep the daemon out */
    if (drive->flags & VF_OPEN)
	close_locked_drive(drive);			    /* and close it */
    if (drive->state > drive_down)			    /* if it's up */
	drive->state = drive_down;			    /* make sure it's down */
    unlockdrive(drive);
}

/*
 * Real drive close code, called with drive already locked.
 * We have also checked that the drive is open.  No errors.
 */
void
close_locked_drive(struct drive *drive)
{
    /*
     * If we can't access the drive, we can't flush
     * the queues, which spec_close() will try to
     * do.  Get rid of them here first.
     */

    const struct bdevsw *bdev = bdevsw_lookup(drive->dev);

    if (bdev == NULL)
	panic("no bdevsw");				    /* XXX */
    drive->lasterror = (*bdev->d_close) (drive->dev, 0, 0, NULL);
    drive->flags &= ~VF_OPEN;				    /* no longer open */
}

/*
 * Remove drive from the configuration.
 * Caller must ensure that it isn't active.
 */
void
remove_drive(int driveno)
{
    struct drive *drive = &vinum_conf.drive[driveno];
    struct vinum_hdr *vhdr;				    /* buffer for header */
    int error;

    if (drive->state > drive_referenced) {		    /* real drive */
	if (drive->state == drive_up) {
	    vhdr = (struct vinum_hdr *) Malloc(VINUMHEADERLEN);	/* allocate buffer */
	    CHECKALLOC(vhdr, "Can't allocate memory");
	    error = read_drive(drive, (void *) vhdr, VINUMHEADERLEN, VINUM_LABEL_OFFSET);
	    if (error)
		drive->lasterror = error;
	    else {
		vhdr->magic = VINUM_NOMAGIC;		    /* obliterate the magic, but leave the rest */
		write_drive(drive, (void *) vhdr, VINUMHEADERLEN, VINUM_LABEL_OFFSET);
	    }
	    Free(vhdr);
	}
	free_drive(drive);				    /* close it and free resources */
	save_config();					    /* and save the updated configuration */
    }
}

/*
 * Transfer drive data.  Usually called from one of these defines;
 * #define read_drive(a, b, c, d) driveio (a, b, c, d, B_READ)
 * #define write_drive(a, b, c, d) driveio (a, b, c, d, B_WRITE)
 *
 * length and offset are in bytes, but must be multiples of sector
 * size.  The function *does not check* for this condition, and
 * truncates ruthlessly.
 * Return error number.
 */
int
driveio(struct drive *drive, char *buf, size_t length, off_t offset, int flag)
{
    int error;
    struct buf *bp;
    const struct bdevsw *bdev;

    error = 0;						    /* to keep the compiler happy */
    while (length) {					    /* divide into small enough blocks */
	int len = min(length, MAXBSIZE);		    /* maximum block device transfer is MAXBSIZE */

	bp = geteblk(len);				    /* get a buffer header */
	bp->b_flags = flag | B_BUSY;
	bp->b_dev = drive->dev;				    /* device */
	bp->b_blkno = offset / drive->partinfo.disklab->d_secsize; /* block number */
	bp->b_saveaddr = bp->b_data;
	bp->b_data = buf;
	bp->b_bcount = len;
	bdev = bdevsw_lookup(bp->b_dev);
	if (bdev == NULL)
	    return ENXIO;
	(*bdev->d_strategy) (bp);
	error = biowait(bp);
	bp->b_data = bp->b_saveaddr;
	bp->b_flags |= B_INVAL | B_AGE;
	bp->b_flags &= ~B_ERROR;
	brelse(bp);
	if (error)
	    break;
	length -= len;					    /* update pointers */
	buf += len;
	offset += len;
    }
    return error;
}

/*
 * Check a drive for a vinum header.  If found,
 * update the drive information.  We come here
 * with a partially populated drive structure
 * which includes the device name.
 *
 * Return information on what we found.
 *
 * This function is called from two places: check_drive,
 * which wants to find out whether the drive is a
 * Vinum drive, and config_drive, which asserts that
 * it is a vinum drive.  In the first case, we don't
 * print error messages (verbose==0), in the second
 * we do (verbose==1).
 */
enum drive_label_info
read_drive_label(struct drive *drive, int verbose)
{
    int error;
    int result;						    /* result of our search */
    struct vinum_hdr *vhdr;				    /* and as header */

    error = init_drive(drive, 0);			    /* find the drive */
    if (error)						    /* find the drive */
	return DL_CANT_OPEN;				    /* not ours */

    vhdr = (struct vinum_hdr *) Malloc(VINUMHEADERLEN);	    /* allocate buffers */
    CHECKALLOC(vhdr, "Can't allocate memory");

    drive->state = drive_up;				    /* be optimistic */
    error = read_drive(drive, (void *) vhdr, VINUMHEADERLEN, VINUM_LABEL_OFFSET);
    if (vhdr->magic == VINUM_MAGIC) {			    /* ours! */
	if (drive->label.name[0]			    /* we have a name for this drive */
	&&(strcmp(drive->label.name, vhdr->label.name))) {  /* but it doesn't match the real name */
	    drive->lasterror = EINVAL;
	    result = DL_WRONG_DRIVE;			    /* it's the wrong drive */
	    drive->state = drive_unallocated;		    /* put it back, it's not ours */
	} else
	    result = DL_OURS;
	/*
	 * We copy the drive anyway so that we have
	 * the correct name in the drive info.  This
	 * may not be the name specified
	 */
	drive->label = vhdr->label;			    /* put in the label information */
    } else if (vhdr->magic == VINUM_NOMAGIC)		    /* was ours, but we gave it away */
	result = DL_DELETED_LABEL;			    /* and return the info */
    else
	result = DL_NOT_OURS;				    /* we could have it, but we don't yet */
    Free(vhdr);						    /* that's all. */
    return result;
}

/*
 * Check a drive for a vinum header.  If found,
 * read configuration information from the drive and
 * incorporate the data into the configuration.
 *
 * Return drive number.
 */
struct drive *
check_drive(char *devicename)
{
    int driveno;
    int i;
    struct drive *drive;

    driveno = find_drive_by_name(devicename, 1);	    /* if entry doesn't exist, create it */
    drive = &vinum_conf.drive[driveno];			    /* and get a pointer */

    if (read_drive_label(drive, 0) == DL_OURS) {	    /* one of ours */
	for (i = 0; i < vinum_conf.drives_allocated; i++) { /* see if the name already exists */
	    if ((i != driveno)				    /* not this drive */
	    &&(DRIVE[i].state != drive_unallocated)	    /* and it's allocated */
	    &&(strcmp(DRIVE[i].label.name,
			DRIVE[driveno].label.name) == 0)) { /* and it has the same name */
		struct drive *mydrive = &DRIVE[i];

		if (mydrive->devicename[0] == '/') {	    /* we know a device name for it */
		    /*
		     * set an error, but don't take the
		     * drive down: that would cause unneeded
		     * error messages.
		     */
		    drive->lasterror = EEXIST;
		    break;
		} else {				    /* it's just a place holder, */
		    int sdno;

		    for (sdno = 0; sdno < vinum_conf.subdisks_allocated; sdno++) { /* look at each subdisk */
			if ((SD[sdno].driveno == i)	    /* it's pointing to this one, */
			&&(SD[sdno].state != sd_unallocated)) {	/* and it's a real subdisk */
			    SD[sdno].driveno = drive->driveno; /* point to the one we found */
			    update_sd_state(sdno);	    /* and update its state */
			}
		    }
		    bzero(mydrive, sizeof(struct drive));   /* don't deallocate it, just remove it */
		}
	    }
	}
    } else {
	if (drive->lasterror == 0)
	    drive->lasterror = ENODEV;
	close_drive(drive);
	drive->state = drive_down;
    }
    return drive;
}

static char *
sappend(char *txt, char *s)
{
    while ((*s++ = *txt++) != 0);
    return s - 1;
}

/* Kludge: kernel printf doesn't handle quads correctly XXX */
static char *lltoa(long long l, char *s);

static char *
lltoa(long long l, char *s)
{
    if (l < 0) {
	*s++ = '-';
	l = -l;
    }
    if (l > 9) {
	s = lltoa(l / 10, s);
	l %= 10;
    }
    *s++ = l + '0';
    return s;
}
/*
 * Format the configuration in text form into the buffer
 * at config.  Don't go beyond len bytes
 * XXX this stinks.  Fix soon.
 */
void
format_config(char *config, int len)
{
    int i;
    int j;
    char *s = config;

    bzero(config, len);

    /* First, the volume configuration */
    for (i = 0; i < vinum_conf.volumes_allocated; i++) {
	struct volume *vol;

	vol = &vinum_conf.volume[i];
	if ((vol->state > volume_uninit)
	    && (vol->name[0] != '\0')) {		    /* paranoia */
	    if (vol->preferred_plex >= 0)		    /* preferences, */
		sprintf(s,
		    "volume %s state %s readpol prefer %s",
		    vol->name,
		    volume_state(vol->state),
		    vinum_conf.plex[vol->preferred_plex].name);
	    else					    /* default round-robin */
		sprintf(s,
		    "volume %s state %s",
		    vol->name,
		    volume_state(vol->state));
	    while (*s)
		s++;					    /* find the end */
	    s = sappend("\n", s);
	    if (s > &config[len - 80]) {
		log(LOG_ERR, "vinum: configuration data overflow\n");
		return;
	    }
	}
    }

    /* Then the plex configuration */
    for (i = 0; i < vinum_conf.plexes_allocated; i++) {
	struct plex *plex;

	plex = &vinum_conf.plex[i];
	if ((plex->state != plex_referenced)
	    && (plex->name[0] != '\0')) {		    /* paranoia */
	    sprintf(s, "plex name %s state %s org %s ",
		plex->name,
		plex_state(plex->state),
		plex_org(plex->organization));
	    while (*s)
		s++;					    /* find the end */
	    if ((plex->organization == plex_striped)
		|| (plex->organization == plex_raid5)) {
		sprintf(s, "%db ", (int) plex->stripesize);
		while (*s)
		    s++;				    /* find the end */
	    }
	    if (plex->volno >= 0)			    /* we have a volume */
		sprintf(s, "vol %s ", vinum_conf.volume[plex->volno].name);
	    while (*s)
		s++;					    /* find the end */
	    for (j = 0; j < plex->subdisks; j++) {
		sprintf(s, " sd %s", vinum_conf.sd[plex->sdnos[j]].name);
	    }
	    s = sappend("\n", s);
	    if (s > &config[len - 80]) {
		log(LOG_ERR, "vinum: configuration data overflow\n");
		return;
	    }
	}
    }

    /* And finally the subdisk configuration */
    for (i = 0; i < vinum_conf.subdisks_allocated; i++) {
	struct sd *sd;

	sd = &SD[i];
	if ((sd->state != sd_referenced)
	    && (sd->name[0] != '\0')) {			    /* paranoia */
	    sprintf(s,
		"sd name %s drive %s plex %s state %s len ",
		sd->name,
		vinum_conf.drive[sd->driveno].label.name,
		vinum_conf.plex[sd->plexno].name,
		sd_state(sd->state));
	    while (*s)
		s++;					    /* find the end */
	    s = lltoa(sd->sectors, s);
	    s = sappend("b driveoffset ", s);
	    s = lltoa(sd->driveoffset, s);
	    s = sappend("b plexoffset ", s);
	    s = lltoa(sd->plexoffset, s);
	    s = sappend("b\n", s);
	    if (s > &config[len - 80]) {
		log(LOG_ERR, "vinum: configuration data overflow\n");
		return;
	    }
	}
    }
}

/*
 * issue a save config request to the d�mon.  The actual work
 * is done in process context by daemon_save_config.
 */
void
save_config(void)
{
    queue_daemon_request(daemonrq_saveconfig, (union daemoninfo) 0);
}

/*
 * Write the configuration to all vinum slices.  This
 * is performed by the daemon only.
 */
void
daemon_save_config(void)
{
    int error;
    int written_config;					    /* set when we first write the config to disk */
    int driveno;
    struct drive *drive;				    /* point to current drive info */
    struct vinum_hdr *vhdr;				    /* and as header */
    char *config;					    /* point to config data */
    int wlabel_on;					    /* to set writing label on/off */
    const struct bdevsw *bdev;

    /* don't save the configuration while we're still working on it */
    if (vinum_conf.flags & VF_CONFIGURING)
	return;
    written_config = 0;					    /* no config written yet */
    /* Build a volume header */
    vhdr = (struct vinum_hdr *) Malloc(VINUMHEADERLEN);	    /* get space for the config data */
    CHECKALLOC(vhdr, "Can't allocate config data");
    vhdr->magic = VINUM_MAGIC;				    /* magic number */
    vhdr->config_length = MAXCONFIG;			    /* length of following config info */

    config = Malloc(MAXCONFIG);				    /* get space for the config data */
    CHECKALLOC(config, "Can't allocate config data");

    format_config(config, MAXCONFIG);
    error = 0;						    /* no errors yet */
    for (driveno = 0; driveno < vinum_conf.drives_allocated; driveno++) {
	drive = &vinum_conf.drive[driveno];		    /* point to drive */
	if (drive->state > drive_referenced) {
	    LOCKDRIVE(drive);				    /* don't let it change */

	    /*
	     * First, do some drive consistency checks.  Some
	     * of these are kludges, others require a process
	     * context and couldn't be done before.
	     */
	    if ((drive->devicename[0] == '\0')
		|| (drive->label.name[0] == '\0')) {
		unlockdrive(drive);
		free_drive(drive);			    /* get rid of it */
		break;
	    }
	    if (((drive->flags & VF_OPEN) == 0)		    /* drive not open */
	    &&(drive->state > drive_down)) {		    /* and it thinks it's not down */
		unlockdrive(drive);
		set_drive_state(driveno, drive_down, setstate_force); /* tell it what's what */
		continue;
	    }
	    if ((drive->state == drive_down)		    /* it's down */
	    &&(drive->flags & VF_OPEN)) {		    /* but open, */
		unlockdrive(drive);
		close_drive(drive);			    /* close it */
	    } else if (drive->state > drive_down) {
		microtime(&drive->label.last_update);	    /* time of last update is now */
		bcopy((char *) &drive->label,		    /* and the label info from the drive structure */
		    (char *) &vhdr->label,
		    sizeof(vhdr->label));
		if ((drive->state != drive_unallocated)
		    && (drive->state != drive_referenced)) { /* and it's a real drive */
		    wlabel_on = 1;			    /* enable writing the label */
		    bdev = bdevsw_lookup(drive->dev);
		    if (bdev == NULL)
							    /* XXX log this */
			error = ENXIO;
		    else
			error = (*bdev->d_ioctl) (drive->dev, /* make the label writeable */
			    DIOCWLABEL,
			    (caddr_t) & wlabel_on,
			    FWRITE,
			    curproc);
		    if (error == 0)
			error = write_drive(drive, (char *) vhdr, VINUMHEADERLEN, VINUM_LABEL_OFFSET);
		    if (error == 0)
			error = write_drive(drive, config, MAXCONFIG, VINUM_CONFIG_OFFSET); /* first config copy */
		    if (error == 0)
			error = write_drive(drive, config, MAXCONFIG, VINUM_CONFIG_OFFSET + MAXCONFIG);	/* second copy */
		    wlabel_on = 0;			    /* enable writing the label */
		    if (error == 0)
							    /* make the label non-writeable again */
			error = (*bdev->d_ioctl) (drive->dev,
			    DIOCWLABEL,
			    (caddr_t) & wlabel_on,
			    FWRITE,
			    curproc);
		    unlockdrive(drive);
		    if (error) {
			log(LOG_ERR,
			    "vinum: Can't write config to %s, error %d\n",
			    drive->devicename,
			    error);
			set_drive_state(drive->driveno, drive_down, setstate_force);
		    } else
			written_config = 1;		    /* we've written it on at least one drive */
		}
	    } else					    /* not worth looking at, */
		unlockdrive(drive);			    /* just unlock it again */
	}
    }
    Free(vhdr);
    Free(config);
}

/*
 * Disk labels are a mess.  The correct way to
 * access them is with the DIOC[GSW]DINFO ioctls,
 * but some programs, such as newfs, access the
 * disk directly, so we have to write things
 * there.  We do this only on request.  If a user
 * request tries to read it directly, we fake up
 * one on the fly.
 */

/*
 * get_volume_label returns a label structure to
 * lp, which is allocated by the caller.
 */
void
get_volume_label(char *name, int plexes, u_int64_t size, struct disklabel *lp)
{
    bzero(lp, sizeof(struct disklabel));

    strncpy(lp->d_typename, "vinum", sizeof(lp->d_typename));
    lp->d_type = DTYPE_VINUM;
    strncpy(lp->d_packname, name, min(sizeof(lp->d_packname), sizeof(name)));
    lp->d_rpm = 14400 * plexes;				    /* to keep them guessing */
    lp->d_interleave = 1;
    lp->d_flags = 0;

    /*
     * A Vinum volume has a single track with all
     * its sectors.
     */
    lp->d_secsize = DEV_BSIZE;				    /* bytes per sector */
    lp->d_nsectors = size;				    /* data sectors per track */
    lp->d_ntracks = 1;					    /* tracks per cylinder */
    lp->d_ncylinders = 1;				    /* data cylinders per unit */
    lp->d_secpercyl = size;				    /* data sectors per cylinder */
    lp->d_secperunit = size;				    /* data sectors per unit */

    lp->d_bbsize = 8192;				    /* fake boot block size */
    lp->d_sbsize = 1024;				    /* NetBSD doesn't seem to have an SBSIZE XXX */
    lp->d_magic = DISKMAGIC;
    lp->d_magic2 = DISKMAGIC;

    /*
     * Set up partitions a, b and c to be identical
     * and the size of the volume.  a is UFS, b is
     * swap, c is nothing.
     */
    lp->d_partitions[0].p_size = size;
    lp->d_partitions[0].p_fsize = 1024;
    lp->d_partitions[0].p_fstype = FS_BSDFFS;		    /* FreeBSD File System :-) */
    lp->d_partitions[0].p_fsize = 1024;			    /* FS fragment size */
    lp->d_partitions[0].p_frag = 8;			    /* and fragments per block */
    lp->d_partitions[SWAP_PART].p_size = size;
    lp->d_partitions[SWAP_PART].p_fstype = FS_SWAP;	    /* swap partition */
    lp->d_partitions[LABEL_PART].p_size = size;
    lp->d_npartitions = LABEL_PART + 1;
    strncpy(lp->d_packname, name, min(sizeof(lp->d_packname), sizeof(name)));
    lp->d_checksum = dkcksum(lp);
}

/* Write a volume label.  This implements the VINUM_LABEL ioctl. */
int
write_volume_label(int volno)
{
    struct disklabel *lp;
    struct buf *bp;
    struct disklabel *dlp;
    struct volume *vol;
    int error;

    lp = (struct disklabel *) Malloc((sizeof(struct disklabel) + (DEV_BSIZE - 1)) & (DEV_BSIZE - 1));
    if (lp == 0)
	return ENOMEM;

    if ((unsigned) (volno) >= (unsigned) vinum_conf.volumes_allocated) /* invalid volume */
	return ENOENT;

    vol = &VOL[volno];					    /* volume in question */
    if (vol->state <= volume_uninit)			    /* nothing there */
	return ENXIO;
    else if (vol->state < volume_up)			    /* not accessible */
	return EIO;					    /* I/O error */

    get_volume_label(vol->name, vol->plexes, vol->size, lp); /* get the label */

    /*
     * Now write to disk.  This code is derived from the
     * system writedisklabel (), which does silly things
     * like reading the label and refusing to write
     * unless it's already there.
     */
    bp = geteblk((int) lp->d_secsize);			    /* get a buffer */
    bp->b_dev = makedev(VINUM_CDEV_MAJOR, vol->volno);	    /* our own raw volume */
    bp->b_blkno = LABELSECTOR * ((int) lp->d_secsize / DEV_BSIZE);
    bp->b_bcount = lp->d_secsize;
    bzero(bp->b_data, lp->d_secsize);
    dlp = (struct disklabel *) bp->b_data;
    *dlp = *lp;
    bp->b_flags &= ~B_INVAL;
    bp->b_flags |= B_BUSY | B_WRITE;
    vinumstrategy(bp);
    error = biowait(bp);
    bp->b_flags |= B_INVAL | B_AGE;
    bp->b_flags &= ~B_ERROR;

    brelse(bp);
    return error;
}

/* Look at all disks on the system for vinum slices. */
int
vinum_scandisk(char *devicename[], int drives)
{
    struct drive *volatile drive;
    volatile int driveno;
    int firstdrive;					    /* first drive in this list */
    volatile int gooddrives;				    /* number of usable drives found */
    int firsttime;					    /* set if we have never configured before */
    int error;
    char *config_text;					    /* read the config info from disk into here */
    char *volatile cptr;				    /* pointer into config information */
    char *eptr;						    /* end pointer into config information */
    char *config_line;					    /* copy the config line to */
    volatile int status;
    int *volatile drivelist;				    /* list of drive indices */
#define DRIVENAMELEN 64
#define DRIVEPARTS   35					    /* max partitions per drive, excluding c */
    char partname[DRIVENAMELEN];			    /* for creating partition names */

    status = 0;						    /* success indication */
    vinum_conf.flags |= VF_READING_CONFIG;		    /* reading config from disk */

    gooddrives = 0;					    /* number of usable drives found */
    firstdrive = vinum_conf.drives_used;		    /* the first drive */
    firsttime = vinum_conf.drives_used == 0;		    /* are we a virgin? */

    /* allocate a drive pointer list */
    drivelist = (int *) Malloc(drives * DRIVEPARTS * sizeof(int));
    CHECKALLOC(drivelist, "Can't allocate memory");

    /* Open all drives and find which was modified most recently */
    for (driveno = 0; driveno < drives; driveno++) {
	char part;					    /* UNIX partition */
	int slice;
	int founddrive;					    /* flag when we find a vinum drive */

	founddrive = 0;					    /* no vinum drive found yet on this spindle */
	/* first try the partition table */
	for (slice = 1; slice < 5; slice++)
	    for (part = 'a'; part < 'i'; part++) {
		if (part != 'c') {			    /* don't do the c partition */
		    snprintf(partname,
			DRIVENAMELEN,
			"%ss%d%c",
			devicename[driveno],
			slice,
			part);
		    drive = check_drive(partname);	    /* try to open it */
		    if ((drive->lasterror != 0)		    /* didn't work, */
		    ||(drive->state != drive_up))
			free_drive(drive);		    /* get rid of it */
		    else if (drive->flags & VF_CONFIGURED)  /* already read this config, */
			log(LOG_WARNING,
			    "vinum: already read config from %s\n", /* say so */
			    drive->label.name);
		    else {
			drivelist[gooddrives] = drive->driveno;	/* keep the drive index */
			drive->flags &= ~VF_NEWBORN;	    /* which is no longer newly born */
			gooddrives++;
			founddrive++;
		    }
		}
	    }
	if (founddrive == 0) {				    /* didn't find anything, */
	    for (part = 'a'; part < 'i'; part++)	    /* try the compatibility partition */
		if (part != 'c') {			    /* don't do the c partition */
		    snprintf(partname,			    /* /dev/sd0a */
			DRIVENAMELEN,
			"%s%c",
			devicename[driveno],
			part);
		    drive = check_drive(partname);	    /* try to open it */
		    if ((drive->lasterror != 0)		    /* didn't work, */
		    ||(drive->state != drive_up))
			free_drive(drive);		    /* get rid of it */
		    else if (drive->flags & VF_CONFIGURED)  /* already read this config, */
			log(LOG_WARNING,
			    "vinum: already read config from %s\n", /* say so */
			    drive->label.name);
		    else {
			drivelist[gooddrives] = drive->driveno;	/* keep the drive index */
			drive->flags &= ~VF_NEWBORN;	    /* which is no longer newly born */
			gooddrives++;
		    }
		}
	}
    }

    if (gooddrives == 0) {
	if (firsttime)
	    log(LOG_WARNING, "vinum: no drives found\n");
	else
	    log(LOG_INFO, "vinum: no additional drives found\n");
	return ENOENT;
    }
    /*
     * We now have at least one drive open.  Sort
     * them in order of config time and merge the
     * config info with what we have already.
     */
    qsort(drivelist, gooddrives, sizeof(int), drivecmp);
    config_text = (char *) Malloc(MAXCONFIG * 2);	    /* allocate buffers */
    CHECKALLOC(config_text, "Can't allocate memory");
    config_line = (char *) Malloc(MAXCONFIGLINE * 2);	    /* allocate buffers */
    CHECKALLOC(config_line, "Can't allocate memory");
    for (driveno = 0; driveno < gooddrives; driveno++) {    /* now include the config */
	drive = &DRIVE[drivelist[driveno]];		    /* point to the drive */

	if (firsttime && (driveno == 0))		    /* we've never configured before, */
	    log(LOG_INFO, "vinum: reading configuration from %s\n", drive->devicename);
	else
	    log(LOG_INFO, "vinum: updating configuration from %s\n", drive->devicename);

	if (drive->state == drive_up)
	    /* Read in both copies of the configuration information */
	    error = read_drive(drive, config_text, MAXCONFIG * 2, VINUM_CONFIG_OFFSET);
	else {
	    error = EIO;
	    printf("vinum_scandisk: %s is %s\n", drive->devicename, drive_state(drive->state));
	}

	if (error != 0) {
	    log(LOG_ERR, "vinum: Can't read device %s, error %d\n", drive->devicename, error);
	    free_drive(drive);				    /* give it back */
	    status = error;
	}
	/*
	 * At this point, check that the two copies
	 * are the same, and do something useful if
	 * not.  In particular, consider which is
	 * newer, and what this means for the
	 * integrity of the data on the drive.
	 */
	else {
	    vinum_conf.drives_used++;			    /* another drive in use */
	    /* Parse the configuration, and add it to the global configuration */
	    for (cptr = config_text; *cptr != '\0';) {	    /* love this style(9) */
		volatile int parse_status;		    /* return value from parse_config */

		for (eptr = config_line; (*cptr != '\n') && (*cptr != '\0');) /* until the end of the line */
		    *eptr++ = *cptr++;
		*eptr = '\0';				    /* and delimit */
		if (setjmp(&command_fail) == 0) {	    /* come back here on error and continue */
		    parse_status = parse_config(config_line, &keyword_set, 1); /* parse the config line */
		    if (parse_status < 0) {		    /* error in config */
			/*
			   * This config should have been parsed
			   * in user space.  If we run into
			   * problems here, something serious is
			   * afoot.  Complain and let the user
			   * snarf the config to see what's
			   * wrong.
			 */
			log(LOG_ERR,
			    "vinum: Config error on %s, aborting integration\n",
			    drive->devicename);
			free_drive(drive);		    /* give it back */
			status = EINVAL;
		    }
		}
		while (*cptr == '\n')
		    cptr++;				    /* skip to next line */
	    }
	}
	drive->flags |= VF_CONFIGURED;			    /* read this drive's configuration */
    }

    Free(config_text);
    Free(drivelist);
    vinum_conf.flags &= ~VF_READING_CONFIG;		    /* no longer reading from disk */
    if (status != 0)
	printf("vinum: couldn't read configuration");
    else
	updateconfig(VF_READING_CONFIG);		    /* update from disk config */
    return status;
}

/*
 * Compare the modification dates of the drives, for qsort.
 * Return 1 if a < b, 0 if a == b, 01 if a > b: in other
 * words, sort backwards.
 */
int
drivecmp(const void *va, const void *vb)
{
    const struct drive *a = &DRIVE[*(const int *) va];
    const struct drive *b = &DRIVE[*(const int *) vb];

    if ((a->label.last_update.tv_sec == b->label.last_update.tv_sec)
	&& (a->label.last_update.tv_usec == b->label.last_update.tv_usec))
	return 0;
    else if ((a->label.last_update.tv_sec > b->label.last_update.tv_sec)
	    || ((a->label.last_update.tv_sec == b->label.last_update.tv_sec)
	    && (a->label.last_update.tv_usec > b->label.last_update.tv_usec)))
	return -1;
    else
	return 1;
}
/* Local Variables: */
/* fill-column: 50 */
/* End: */
