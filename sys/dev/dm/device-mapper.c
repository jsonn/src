/*        $NetBSD: device-mapper.c,v 1.1.2.15 2008/11/02 00:02:32 haad Exp $ */

/*
 * Copyright (c) 1996, 1997, 1998, 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Hamsik.
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
 * I want to say thank you to all people who helped me with this project.
 */

#include <sys/types.h>
#include <sys/param.h>

#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/ioccom.h>
#include <sys/kmem.h>
#include <sys/module.h>

#include "netbsd-dm.h"
#include "dm.h"

static dev_type_open(dmopen);
static dev_type_close(dmclose);
static dev_type_read(dmread);
static dev_type_write(dmwrite);
static dev_type_ioctl(dmioctl);
static dev_type_strategy(dmstrategy);
static dev_type_dump(dmdump);
static dev_type_size(dmsize);

/* attach and detach routines */
int dmattach(void);
int dmdestroy(void);

static int dm_cmd_to_fun(prop_dictionary_t);
static int disk_ioctl_switch(dev_t, u_long, void *);
static int dm_ioctl_switch(u_long);
static void dmminphys(struct buf *);

/* ***Variable-definitions*** */
const struct bdevsw dm_bdevsw = {
	dmopen, dmclose, dmstrategy, dmioctl, dmdump, dmsize, D_DISK | D_MPSAFE
};

const struct cdevsw dm_cdevsw = {
	dmopen, dmclose, dmread, dmwrite, dmioctl,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK | D_MPSAFE
};

int unload;

/*
 * This array is used to translate cmd to function pointer.
 *
 * Interface between libdevmapper and lvm2tools uses different 
 * names for one IOCTL call because libdevmapper do another thing
 * then. When I run "info" or "mknodes" libdevmapper will send same
 * ioctl to kernel but will do another things in userspace.
 *
 */
struct cmd_function cmd_fn[] = {
		{"version", dm_get_version_ioctl},
		{"targets", dm_list_versions_ioctl},
		{"create",  dm_dev_create_ioctl},
		{"info",    dm_dev_status_ioctl},
		{"mknodes", dm_dev_status_ioctl},		
		{"names",   dm_dev_list_ioctl},
		{"suspend", dm_dev_suspend_ioctl},
		{"remove",  dm_dev_remove_ioctl}, 
		{"rename",  dm_dev_rename_ioctl},
		{"resume",  dm_dev_resume_ioctl},
		{"clear",   dm_table_clear_ioctl},
		{"deps",    dm_table_deps_ioctl},
		{"reload",  dm_table_load_ioctl},
		{"status",  dm_table_status_ioctl},
		{"table",   dm_table_status_ioctl},
		{NULL, NULL}	
};


MODULE(MODULE_CLASS_MISC, dm, NULL);

/* New module handle routine */
static int
dm_modcmd(modcmd_t cmd, void *arg)
{
#ifdef _MODULE
	int bmajor = -1, cmajor = -1;

	switch (cmd) {
	case MODULE_CMD_INIT:
		dmattach();
		return devsw_attach("dm", &dm_bdevsw, &bmajor,
		    &dm_cdevsw, &cmajor);
		break;

	case MODULE_CMD_FINI:
		dmdestroy();
		return devsw_detach(&dm_bdevsw, &dm_cdevsw);
		break;

	case MODULE_CMD_STAT:
		return ENOTTY;

	default:
		return ENOTTY;
	}

	return 0;
#else

	if (cmd == MODULE_CMD_INIT)
		return 0;
	return ENOTTY;

#endif /* _MODULE */
}


/* attach routine */
int
dmattach(void)
{
	dm_target_init();
	dm_dev_init();
	dm_pdev_init();

	return 0;
}

/* Destroy routine */
int
dmdestroy(void)
{
	atomic_inc_32(&unload);

	dm_dev_destroy();
	dm_pdev_destroy();
	dm_target_destroy();

	return 0;
}

static int
dmopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	aprint_debug("open routine called %d\n", minor(dev));
	
	if (unload == 1)
		return EBUSY;
	
	return 0;
}

static int
dmclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	aprint_debug("CLOSE routine called\n");

	return 0;
}


static int
dmioctl(dev_t dev, const u_long cmd, void *data, int flag, struct lwp *l)
{
	int r;
	prop_dictionary_t dm_dict_in;

	r = 0;

	aprint_debug("dmioctl called\n");
	
	KASSERT(data != NULL);
	
	if (disk_ioctl_switch(dev, cmd, data) != 0) {
		struct plistref *pref = (struct plistref *) data;

		if((r = prop_dictionary_copyin_ioctl(pref, cmd, &dm_dict_in)) != 0)
			return r;
		
		dm_check_version(dm_dict_in);

		/* call cmd selected function */
		if ((r = dm_ioctl_switch(cmd)) != 0)
			return r;

		char *xml;
		xml = prop_dictionary_externalize(dm_dict_in);
		aprint_verbose("%s\n",xml);

		/* run ioctl routine */
		if ((r = dm_cmd_to_fun(dm_dict_in)) != 0)
			return r;
		
		r = prop_dictionary_copyout_ioctl(pref, cmd, dm_dict_in);
	}

	return r;
}

/*
 * Translate command sent from libdevmapper to func.
 */
static int
dm_cmd_to_fun(prop_dictionary_t dm_dict){
	int i,len,slen;
	int r;
	const char *command;
	
	r = 0;
		
	(void)prop_dictionary_get_cstring_nocopy(dm_dict, DM_IOCTL_COMMAND,
	    &command);

	len = strlen(command);
		
	for(i=0; cmd_fn[i].cmd != NULL; i++){
		slen = strlen(cmd_fn[i].cmd);

		if (len != slen)
			continue;

		if ((strncmp(command, cmd_fn[i].cmd, slen)) == 0) {
			aprint_normal("ioctl command: %s\n", command);
			r = cmd_fn[i].fn(dm_dict);
			break;
		}
	}
	
	return r;
}

/* Call apropriate ioctl handler function. */
static int
dm_ioctl_switch(u_long cmd)
{
	int r;
	
	r = 0;

	switch(cmd) {
		
	case NETBSD_DM_IOCTL:
		aprint_debug("NetBSD_DM_IOCTL called\n");
		break;
		
	default:
		 aprint_debug("unknown ioctl called\n");
		 return ENOTTY;
		 break; /* NOT REACHED */
	}

	 return r;
}

 /*
  * Check for disk specific ioctls.
  */

static int
disk_ioctl_switch(dev_t dev, u_long cmd, void *data)
{
	dm_dev_t *dmv;
	
	switch(cmd) {
	case DIOCGWEDGEINFO:
	{
		struct dkwedge_info *dkw = (void *) data;

		if ((dmv = dm_dev_lookup(NULL, NULL, minor(dev))) == NULL)
			return ENOENT;
			
		aprint_normal("DIOCGWEDGEINFO ioctl called\n");
		
		strlcpy(dkw->dkw_devname, dmv->name, 16);
		strlcpy(dkw->dkw_wname, dmv->name, DM_NAME_LEN);
		strlcpy(dkw->dkw_parent, dmv->name, 16);
		
		dkw->dkw_offset = 0;
		dkw->dkw_size = dm_table_size(&dmv->table_head);
		strcpy(dkw->dkw_ptype, DKW_PTYPE_FFS);

		dm_dev_unbusy(dmv);
		break;
	}
	
	case DIOCGDINFO:
	{
		
		if ((dmv = dm_dev_lookup(NULL, NULL, minor(dev))) == NULL)
			return ENOENT;

		aprint_debug("DIOCGDINFO %d\n", dmv->dk_label->d_secsize);
		 
		*(struct disklabel *)data = *(dmv->dk_label);

		dm_dev_unbusy(dmv);
		break;
	}
	 
	case DIOCGPART:
	{
		if ((dmv = dm_dev_lookup(NULL, NULL, minor(dev))) == NULL)
			return ENOENT;

		((struct partinfo *)data)->disklab = dmv->dk_label;
		((struct partinfo *)data)->part = &dmv->dk_label->d_partitions[0];

		dm_dev_unbusy(dmv);
		break;
	}
	case DIOCWDINFO:
	case DIOCSDINFO:
	case DIOCKLABEL:
	case DIOCWLABEL:
	case DIOCGDEFLABEL:
		
	default:
		aprint_debug("unknown disk_ioctl called\n");
		return 1;
		break; /* NOT REACHED */
	}
	
	return 0;
}

/*
 * Do all IO operations on dm logical devices.
 */
static void
dmstrategy(struct buf *bp)
{
	dm_dev_t *dmv;
	dm_table_t  *tbl;
	dm_table_entry_t *table_en;
	struct buf *nestbuf;

	uint32_t dev_type;

	uint64_t buf_start, buf_len, issued_len;
	uint64_t table_start, table_end;
	uint64_t start, end;
	
	buf_start = bp->b_blkno * DEV_BSIZE;
	buf_len = bp->b_bcount;

	tbl = NULL; 

	table_end = 0;
	dev_type = 0;
	issued_len = 0;

	if ((dmv = dm_dev_lookup(NULL, NULL, minor(bp->b_dev))) == NULL) {
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return;
	} 
	
	/* Select active table */
	tbl = dm_table_get_entry(&dmv->table_head, DM_TABLE_ACTIVE);

	 /* Nested buffers count down to zero therefore I have
	    to set bp->b_resid to maximal value. */
	bp->b_resid = bp->b_bcount;
	
	/*
	 * Find out what tables I want to select.
	 */
	SLIST_FOREACH(table_en, tbl, next)
	{
		/* I need need number of bytes not blocks. */
		table_start = table_en->start * DEV_BSIZE;
		/*
		 * I have to sub 1 from table_en->length to prevent
		 * off by one error
		 */
		table_end = table_start + (table_en->length)* DEV_BSIZE;
		
		start = MAX(table_start, buf_start);

		end = MIN(table_end, buf_start + buf_len);

		aprint_debug("----------------------------------------\n");
		aprint_debug("table_start %010" PRIu64", table_end %010"
		    PRIu64 "\n", table_start, table_end);
		aprint_debug("buf_start %010" PRIu64", buf_len %010"
		    PRIu64"\n", buf_start, buf_len);
		aprint_debug("start-buf_start %010"PRIu64", end %010"
		    PRIu64"\n", start - buf_start, end);
		aprint_debug("start %010" PRIu64" , end %010"
                    PRIu64"\n", start, end);
		aprint_debug("\n----------------------------------------\n");

		if (start < end) {
			/* create nested buffer  */
			nestbuf = getiobuf(NULL, true);

			nestiobuf_setup(bp, nestbuf, start - buf_start,
			    (end - start));

			issued_len += end - start;
			
			/* I need number of blocks. */
			nestbuf->b_blkno = (start - table_start) / DEV_BSIZE;

			table_en->target->strategy(table_en, nestbuf);
		}
	}	

	if (issued_len < buf_len)
		nestiobuf_done(bp, buf_len - issued_len, EINVAL);

	dm_table_release(&dmv->table_head, DM_TABLE_ACTIVE);
	dm_dev_unbusy(dmv);

	return;
}


static int
dmread(dev_t dev, struct uio *uio, int flag)
{
	return (physio(dmstrategy, NULL, dev, B_READ, dmminphys, uio));
}

static int
dmwrite(dev_t dev, struct uio *uio, int flag)
{
	return (physio(dmstrategy, NULL, dev, B_WRITE, dmminphys, uio));
}

static int
dmdump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	return ENODEV;
}

static int
dmsize(dev_t dev)
{
	dm_dev_t *dmv;
	uint64_t size;
	
	size = 0;
	
	if ((dmv = dm_dev_lookup(NULL, NULL, minor(dev))) == NULL)
			return -ENOENT;
	
	size = dm_table_size(&dmv->table_head);
	dm_dev_unbusy(dmv);
	
  	return size;
}

static void
dmminphys(struct buf *bp)
{
	bp->b_bcount = MIN(bp->b_bcount, MAXPHYS);
}

 /*
  * Load the label information on the named device
  * Actually fabricate a disklabel.
  *
  * EVENTUALLY take information about different
  * data tracks from the TOC and put it in the disklabel
  *
  * Copied from vnd code.
  */
void
dmgetdisklabel(struct disklabel *lp, dm_table_head_t *head)
{
	struct partition *pp;
	int dmp_size;

	dmp_size = dm_table_size(head);

	/*
	 * Size must be at least 2048 DEV_BSIZE blocks
	 * (1M) in order to use this geometry.
	 */
		
	lp->d_secperunit = dmp_size;
	lp->d_secsize = DEV_BSIZE;
	lp->d_nsectors = 32;
	lp->d_ntracks = 64;
	lp->d_ncylinders = dmp_size / (lp->d_nsectors * lp->d_ntracks);
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "lvm", sizeof(lp->d_typename));
	lp->d_type = DTYPE_DM;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	pp = &lp->d_partitions[0];
	/*
	 * This is logical offset and therefore it can be 0
	 * I will consider table offsets later in dmstrategy.
	 */
	pp->p_offset = 0; 
	pp->p_size = dmp_size * DEV_BSIZE;
	pp->p_fstype = FS_BSDFFS;  /* default value */
	lp->d_npartitions = 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
	
	return;
}
