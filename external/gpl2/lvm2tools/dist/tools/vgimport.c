/*	$NetBSD: vgimport.c,v 1.1.1.1.2.3 2008/12/13 14:39:38 haad Exp $	*/

/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2007 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tools.h"

static int vgimport_single(struct cmd_context *cmd __attribute((unused)),
			   const char *vg_name,
			   struct volume_group *vg, int consistent,
			   void *handle __attribute((unused)))
{
	struct pv_list *pvl;
	struct physical_volume *pv;

	if (!vg || !consistent) {
		log_error("Unable to find exported volume group \"%s\"",
			  vg_name);
		goto error;
	}

	if (!(vg_status(vg) & EXPORTED_VG)) {
		log_error("Volume group \"%s\" is not exported", vg_name);
		goto error;
	}

	if (vg_status(vg) & PARTIAL_VG) {
		log_error("Volume group \"%s\" is partially missing", vg_name);
		goto error;
	}

	if (!archive(vg))
		goto error;

	vg->status &= ~EXPORTED_VG;

	dm_list_iterate_items(pvl, &vg->pvs) {
		pv = pvl->pv;
		pv->status &= ~EXPORTED_VG;
	}

	if (!vg_write(vg) || !vg_commit(vg))
		goto error;

	backup(vg);

	log_print("Volume group \"%s\" successfully imported", vg->name);

	return ECMD_PROCESSED;

      error:
	return ECMD_FAILED;
}

int vgimport(struct cmd_context *cmd, int argc, char **argv)
{
	if (!argc && !arg_count(cmd, all_ARG)) {
		log_error("Please supply volume groups or use -a for all.");
		return ECMD_FAILED;
	}

	if (argc && arg_count(cmd, all_ARG)) {
		log_error("No arguments permitted when using -a for all.");
		return ECMD_FAILED;
	}

	return process_each_vg(cmd, argc, argv, LCK_VG_WRITE, 1, NULL,
			       &vgimport_single);
}
