/*	$NetBSD: errseg.c,v 1.1.1.1.2.3 2008/12/13 14:39:33 haad Exp $	*/

/*
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

#include "lib.h"
#include "toolcontext.h"
#include "segtype.h"
#include "display.h"
#include "text_export.h"
#include "text_import.h"
#include "config.h"
#include "str_list.h"
#include "targets.h"
#include "lvm-string.h"
#include "activate.h"
#include "str_list.h"
#include "metadata.h"

static const char *_errseg_name(const struct lv_segment *seg)
{
	return seg->segtype->name;
}

static int _errseg_merge_segments(struct lv_segment *seg1, struct lv_segment *seg2)
{
	seg1->len += seg2->len;
	seg1->area_len += seg2->area_len;

	return 1;
}

#ifdef DEVMAPPER_SUPPORT
static int _errseg_add_target_line(struct dev_manager *dm __attribute((unused)),
				struct dm_pool *mem __attribute((unused)),
				struct cmd_context *cmd __attribute((unused)),
				void **target_state __attribute((unused)),
				struct lv_segment *seg __attribute((unused)),
				struct dm_tree_node *node, uint64_t len,
				uint32_t *pvmove_mirror_count __attribute((unused)))
{
	return dm_tree_node_add_error_target(node, len);
}

static int _errseg_target_present(const struct lv_segment *seg __attribute((unused)),
				  unsigned *attributes __attribute((unused)))
{
	static int _errseg_checked = 0;
	static int _errseg_present = 0;

	/* Reported truncated in older kernels */
	if (!_errseg_checked &&
	    (target_present("error", 0) || target_present("erro", 0)))
		_errseg_present = 1;

	_errseg_checked = 1;
	return _errseg_present;
}
#endif

static int _errseg_modules_needed(struct dm_pool *mem,
				  const struct lv_segment *seg __attribute((unused)),
				  struct dm_list *modules)
{
	if (!str_list_add(mem, modules, "error")) {
		log_error("error module string list allocation failed");
		return 0;
	}

	return 1;
}

static void _errseg_destroy(const struct segment_type *segtype)
{
	dm_free((void *)segtype);
}

static struct segtype_handler _error_ops = {
	.name = _errseg_name,
	.merge_segments = _errseg_merge_segments,
#ifdef DEVMAPPER_SUPPORT
	.add_target_line = _errseg_add_target_line,
	.target_present = _errseg_target_present,
#endif
	.modules_needed = _errseg_modules_needed,
	.destroy = _errseg_destroy,
};

struct segment_type *init_error_segtype(struct cmd_context *cmd)
{
	struct segment_type *segtype = dm_malloc(sizeof(*segtype));

	if (!segtype)
		return_NULL;

	segtype->cmd = cmd;
	segtype->ops = &_error_ops;
	segtype->name = "error";
	segtype->private = NULL;
	segtype->flags = SEG_CAN_SPLIT | SEG_VIRTUAL | SEG_CANNOT_BE_ZEROED;

	log_very_verbose("Initialised segtype: %s", segtype->name);

	return segtype;
}
