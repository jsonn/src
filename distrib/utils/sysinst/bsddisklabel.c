/*	$NetBSD: bsddisklabel.c,v 1.27.2.1 2004/07/14 09:00:12 tron Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Based on code written by Philip A. Nelson for Piermont Information
 * Systems Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* bsddisklabel.c -- generate standard BSD disklabel */
/* Included by appropriate arch/XXXX/md.c */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/exec.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <machine/cpu.h>
#include <stdio.h>
#include <stddef.h>
#include <util.h>
#include <dirent.h>
#include "defs.h"
#include "md.h"
#include "endian.h"
#include "msg_defs.h"
#include "menu_defs.h"

/* For the current state of this file blame abs@NetBSD.org */
/* Even though he wasn't the last to hack it, but he did admit doing so :-) */

#define	PART_ANY	-1
#define	PART_EXTRA	-2
#define	PART_TMP_MFS	-3

/* Defaults for things that might be defined in md.h */
#ifndef PART_ROOT
#define PART_ROOT	PART_A
#endif
#ifndef PART_SWAP
#define PART_SWAP	PART_B
#endif
#ifndef PART_USR
#define PART_USR	PART_ANY
#endif

#ifndef DEFSWAPRAM
#define DEFSWAPRAM	32
#endif

#ifndef DEFVARSIZE
#define DEFVARSIZE	32
#endif
#ifndef DEFROOTSIZE
#define DEFROOTSIZE	32
#endif
#ifndef DEFUSRSIZE
#define DEFUSRSIZE	128
#endif
#ifndef DEFSWAPSIZE
#define DEFSWAPSIZE	128
#endif

static int set_ptn_size(menudesc *, void *);

struct ptn_info {
	int		menu_no;
	struct ptn_size {
		int	ptn_id;
		char	mount[20];
		int	dflt_size;
		int	size;
		int	limit;
		char	changed;
	}		ptn_sizes[MAXPARTITIONS + 1];	/* +1 for delete code */
	menu_ent	ptn_menus[MAXPARTITIONS + 1];	/* +1 for unit chg */
	int		free_parts;
	int		free_space;
	struct ptn_size *pool_part;
	char		exit_msg[70];
};

static int
save_ptn(int ptn, int start, int size, int fstype, const char *mountpt)
{
	static int maxptn;
	int p;

	if (maxptn == 0)
		maxptn = getmaxpartitions();

	if (ptn < 0 || bsdlabel[ptn].pi_fstype != FS_UNUSED) {
		ptn = getrawpartition() + 1;
#ifdef PART_FIRST_FREE
		if (ptn < PART_FIRST_FREE)
			ptn = PART_FIRST_FREE;
#endif
		for (;; ptn++) {
			if (ptn >= maxptn)
				return -1;
			if (ptn == PART_USR)
				continue;
			if (bsdlabel[ptn].pi_fstype == FS_UNUSED)
				break;
		}
	}

	if (fstype == FS_UNUSED)
		return ptn;

	bsdlabel[ptn].pi_fstype = fstype;
	bsdlabel[ptn].pi_offset = start;
	bsdlabel[ptn].pi_size = size;
	if (fstype == FS_BSDFFS) {
		bsdlabel[ptn].pi_frag = 8;
		bsdlabel[ptn].pi_fsize = 1024;
		bsdlabel[ptn].pi_flags |= PIF_NEWFS;
	}

	if (mountpt != NULL) {
		for (p = 0; p < maxptn; p++) {
			if (strcmp(bsdlabel[p].pi_mount, mountpt) == 0)
				bsdlabel[p].pi_flags &= ~PIF_MOUNT;
		}
		strlcpy(bsdlabel[ptn].pi_mount, mountpt,
			sizeof bsdlabel[0].pi_mount);
		bsdlabel[ptn].pi_flags |= PIF_MOUNT;
	}

	return ptn;
}

static void
set_ptn_titles(menudesc *m, int opt, void *arg)
{
	struct ptn_info *pi = arg;
	struct ptn_size *p;
	int sm = MEG / sectorsize;
	int size;
	char inc_free[12];

	p = &pi->ptn_sizes[opt];
	if (p->mount[0] == 0) {
		wprintw(m->mw, msg_string(MSG_add_another_ptn));
		return;
	}
	size = p->size;
	if (p == pi->pool_part)
		snprintf(inc_free, sizeof inc_free, "(%u)", 
		    (size + pi->free_space) / sm);
	else
		inc_free[0] = 0;
	wprintw(m->mw, "%6u%8s%10u%10u %c %s",
		size / sm, inc_free, size / dlcylsize, size,
		p == pi->pool_part ? '+' : ' ', p->mount);
}

static void
set_ptn_menu(struct ptn_info *pi)
{
	struct ptn_size *p;
	menu_ent *m;

	for (m = pi->ptn_menus, p = pi->ptn_sizes;; m++, p++) {
		m->opt_name = NULL;
		m->opt_menu = OPT_NOMENU;
		m->opt_flags = 0;
		m->opt_action = set_ptn_size;
		if (p->mount[0] == 0)
			break;
	}
	if (pi->free_parts != 0)
		m++;
	m->opt_name = MSG_askunits;
	m->opt_menu = MENU_sizechoice;
	m->opt_flags = OPT_SUB;
	m->opt_action = NULL;
	m++;

	if (pi->free_space >= 0)
		snprintf(pi->exit_msg, sizeof pi->exit_msg,
			msg_string(MSG_fssizesok),
			pi->free_space / sizemult, multname, pi->free_parts);
	else
		snprintf(pi->exit_msg, sizeof pi->exit_msg,
			msg_string(MSG_fssizesbad),
			-pi->free_space / sizemult, multname, -pi->free_space);

	set_menu_numopts(pi->menu_no, m - pi->ptn_menus);
}

static int
set_ptn_size(menudesc *m, void *arg)
{
	struct ptn_info *pi = arg;
	struct ptn_size *p;
	char answer[10];
	char dflt[10];
	char *cp;
	int size, old_size;
	int mult;

	p = pi->ptn_sizes + m->cursel;

	if (pi->free_parts == 0 && p->size == 0)
		/* Don't allow 'free_parts' to go negative */
		return 0;

	if (p->mount[0] == 0) {
		msg_prompt_win(MSG_askfsmount, -1, 18, 0, 0,
			NULL, p->mount, sizeof p->mount);
		if (p->mount[0] == 0)
			return 0;
	}

	size = p->size;
	old_size = size;
	if (size == 0)
		size = p->dflt_size;
	size /= sizemult;
	snprintf(dflt, sizeof dflt, "%d%s",
	    size, p == pi->pool_part ? "+" : "");

	for (;;) {
		mult = sizemult;
		msg_prompt_win(MSG_askfssize, -1, 18, 0, 0,
			dflt, answer, sizeof answer,
			p->mount, multname);
		/* Some special cases when /usr is first given a size */
		if (old_size == 0 && !strcmp(p->mount, "/usr")) {
			/* Remove space for /usr from / */
			if (!pi->ptn_sizes[0].changed) {
				pi->ptn_sizes[0].size -= p->dflt_size;
				pi->free_space += p->dflt_size;
			}
			/* hack to add free space to default sized /usr */
			if (!strcmp(answer, dflt)) {
				size = p->dflt_size;
				pi->pool_part = p;
				goto adjust_free;
			}
		}
		size = strtoul(answer, &cp, 0);
		switch (*cp++) {
		default:
			continue;
		case 's':
			mult = 1;
			break;
		case 'c':
			mult = dlcylsize;
			break;
		case 'm':
		case 'M':
			mult = MEG / sectorsize;
			break;
		case 'g':
		case 'G':
			mult = 1024 * MEG / sectorsize;
			break;
		case '+':
			cp--;
			if (cp != answer)
				break;
			mult = 1;
			size = old_size;
			break;
		case 0:
			cp--;
			break;
		}
		if (*cp == 0 || *cp == '+')
			break;
	}

	size = NUMSEC(size, mult, dlcylsize);
	if (p->ptn_id == PART_TMP_MFS) {
		p->size = size;
		return 0;
	}
	if (p == pi->pool_part)
		pi->pool_part = NULL;
	if (*cp == '+' && p->limit == 0) {
		pi->pool_part = p;
		if (size == 0)
			size = dlcylsize;
	}
	if (p->limit != 0 && size > p->limit)
		size = p->limit;
    adjust_free:
	if (size != old_size)
		p->changed = 1;
	pi->free_space += old_size - size;
	p->size = size;
	if (size == 0) {
		if (old_size != 0)
			pi->free_parts++;
		if (p->ptn_id == PART_EXTRA)
			memmove(p, p + 1,
				(char *)&pi->ptn_sizes[MAXPARTITIONS]
				- (char *)p);
	} else {
		int f = pi->free_space;
		if (old_size == 0)
			pi->free_parts--;
		if (f < mult && -f < mult) {
			/*
			 * Round size to end of available space,
			 * but keep cylinder alignment
			 */
			if (f < 0)
				f = -ROUNDUP(-f, dlcylsize);
			else
				f = ROUNDDOWN(f, dlcylsize);
			size += f;
			if (size != 0) {
				pi->free_space -= f;
				p->size += f;
			}
		}
	}

	set_ptn_menu(pi);

	return 0;
}

static void
get_ptn_sizes(int part_start, int sectors, int no_swap)
{
	int i;
	int maxpart = getmaxpartitions();
	int sm;				/* sectors in 1MB */
	struct ptn_size *p;
	int size;

	static struct ptn_info pi = { -1, {
#define PI_ROOT 0
		{ PART_ROOT,	"/",	DEFROOTSIZE,	DEFROOTSIZE },
#define PI_SWAP 1
		{ PART_SWAP,	"swap",	DEFSWAPSIZE,	DEFSWAPSIZE },
		{ PART_TMP_MFS,	"tmp (mfs)",	64 },
#define PI_USR 3
		{ PART_USR,	"/usr",	DEFUSRSIZE },
		{ PART_ANY,	"/var",	DEFVARSIZE },
		{ PART_ANY,	"/home",	0 },
	}, {
		{ NULL, OPT_NOMENU, 0, set_ptn_size },
		{ MSG_askunits, MENU_sizechoice, OPT_SUB, NULL },
	}, };

	if (maxpart > MAXPARTITIONS)
		maxpart = MAXPARTITIONS;	/* sanity */

	msg_display(MSG_ptnsizes);
	msg_table_add(MSG_ptnheaders);

	if (pi.menu_no < 0) {
		/* If there is a swap partition elsewhere, don't add one here.*/		if (no_swap)
			pi.ptn_sizes[PI_SWAP].size = 0;
		/* If installing X increase default size of /usr */
		if (sets_selected & SET_X11)
			pi.ptn_sizes[PI_USR].dflt_size += XNEEDMB;

		if (root_limit != 0 && part_start + sectors > root_limit) {
			/* root can't have all the space... */
			pi.ptn_sizes[PI_ROOT].limit = root_limit - part_start;
			pi.ptn_sizes[PI_ROOT].changed = 1;
			/* Give free space to /usr */
			pi.ptn_sizes[PI_USR].size =
						pi.ptn_sizes[PI_USR].dflt_size;
			pi.pool_part = &pi.ptn_sizes[PI_USR];
		} else {
			/* Make size of root include default size of /usr */
			pi.ptn_sizes[PI_ROOT].size +=
						pi.ptn_sizes[PI_USR].dflt_size;
			/* Give free space to / */
			pi.pool_part = &pi.ptn_sizes[PI_ROOT];
		}

		/* Change preset sizes from MB to sectors */
		sm = MEG / sectorsize;
		pi.free_space = sectors;
		for (p = pi.ptn_sizes; p->mount[0]; p++) {
			p->size = NUMSEC(p->size, sm, dlcylsize);
			p->dflt_size = NUMSEC(p->dflt_size, sm, dlcylsize);
			pi.free_space -= p->size;
		}

		/* Steal space from swap to make things fit.. */
		if (pi.free_space < 0) {
			i = ROUNDUP(-pi.free_space, dlcylsize);
			if (i > pi.ptn_sizes[PI_SWAP].size)
				i = pi.ptn_sizes[PI_SWAP].size;
			pi.ptn_sizes[PI_SWAP].size -= i;
			pi.free_space += i;
		}

		/* Add space for 2 system dumps to / (traditional) */
		i = rammb * sm;
		i = ROUNDUP(i, dlcylsize);
		if (pi.free_space > i * 2)
			i *= 2;
		if (pi.free_space > i) {
			pi.ptn_sizes[PI_ROOT].size += i;
			pi.free_space -= i;
		}

		/* Ensure all of / is readable by the system boot code */
		i = pi.ptn_sizes[PI_ROOT].limit;
		if (i != 0 && (i -= pi.ptn_sizes[PI_ROOT].size) < 0) {
			pi.ptn_sizes[PI_ROOT].size += i;
			pi.free_space -= i;
		}

		/* Count free partition slots */
		pi.free_parts = 0;
		for (i = 0; i < maxpart; i++) {
			if (bsdlabel[i].pi_size == 0)
				pi.free_parts++;
		}
		for (i = 0; i < MAXPARTITIONS; i++) {
			p = &pi.ptn_sizes[i];
			if (i != 0 && p->ptn_id == 0)
				p->ptn_id = PART_EXTRA;
			if (p->size != 0)
				pi.free_parts--;
		}

		pi.menu_no = new_menu(0, pi.ptn_menus, nelem(pi.ptn_menus),
			3, -1, 12, 70,
			MC_NOSHORTCUT |
			MC_ALWAYS_SCROLL | MC_NOBOX | MC_NOCLEAR,
			NULL, set_ptn_titles, NULL,
			"help", pi.exit_msg);

		if (pi.menu_no < 0)
			return;
	}

	do {
		set_ptn_menu(&pi);
		current_cylsize = dlcylsize;
		process_menu(pi.menu_no, &pi);
	} while (pi.free_space < 0 || pi.free_parts < 0);

	/* Give any cylinder fragment to last partition */
	if (pi.pool_part != NULL || pi.free_space < dlcylsize) {
		for (p = pi.ptn_sizes + nelem(pi.ptn_sizes) - 1; ;p--) {
			if (p->size == 0) {
				if (p == pi.ptn_sizes)
					break;
				continue;
			}
			if (p->ptn_id == PART_TMP_MFS)
				continue;
			p->size += pi.free_space % dlcylsize;
			break;
		}
	}

	for (p = pi.ptn_sizes; p->mount[0]; p++, part_start += size) {
		size = p->size;
		if (p == pi.pool_part)
			size += ROUNDDOWN(pi.free_space, dlcylsize);
		i = p->ptn_id;
		if (i == PART_TMP_MFS) {
			tmp_mfs_size = size;
			size = 0;
			continue;
		}
		if (size == 0)
			continue;
		if (i == PART_SWAP) {
			save_ptn(i, part_start, size, FS_SWAP, NULL);
			continue;
		}
		save_ptn(i, part_start, size, FS_BSDFFS, p->mount);
	}
}

/*
 * md back-end code for menu-driven BSD disklabel editor.
 */
int
make_bsd_partitions(void)
{
	int i;
	int part;
	int maxpart = getmaxpartitions();
	int partstart;
	int part_raw, part_bsd;
	int ptend;
	int no_swap = 0;
	partinfo *p;

	/*
	 * Initialize global variables that track space used on this disk.
	 * Standard 4.4BSD 8-partition labels always cover whole disk.
	 */
	if (ptsize == 0)
		ptsize = dlsize - ptstart;
	if (dlsize == 0)
		dlsize = ptstart + ptsize;

	partstart = ptstart;
	ptend = ptstart + ptsize;

	/* Ask for layout type -- standard or special */
	msg_display(MSG_layout,
		    ptsize / (MEG / sectorsize),
		    DEFROOTSIZE + DEFSWAPSIZE + DEFUSRSIZE,
		    DEFROOTSIZE + DEFSWAPSIZE + DEFUSRSIZE + XNEEDMB);

	process_menu(MENU_layout, NULL);

	md_set_sizemultname();

	/* Build standard partitions */
	memset(&bsdlabel, 0, sizeof bsdlabel);

	/* Set initial partition types to unused */
	for (part = 0 ; part < maxpart ; ++part)
		bsdlabel[part].pi_fstype = FS_UNUSED;

	/* Whole disk partition */
	part_raw = getrawpartition();
	if (part_raw == -1)
		part_raw = PART_C;	/* for sanity... */
	bsdlabel[part_raw].pi_offset = 0;
	bsdlabel[part_raw].pi_size = dlsize;

	if (part_raw == PART_D) {
		/* Probably a system that expects an i386 style mbr */
		part_bsd = PART_C;
		bsdlabel[PART_C].pi_offset = ptstart;
		bsdlabel[PART_C].pi_size = ptsize;
	} else {
		part_bsd = part_raw;
	}

#if defined(PART_BOOT) && defined(BOOT_SIZE)
	i = BOOT_SIZE;
	if (i >= 1024) {
		/* Treat big numbers as a byte count */
		i = (i + dlcylsize * sectorsize - 1) / (dlcylsize * sectorsize);
		i *= dlcylsize;
	}
	bsdlabel[PART_BOOT].pi_fstype = FS_BOOT;
	bsdlabel[PART_BOOT].pi_size = i;
#ifdef BOOT_HIGH
	bsdlabel[PART_BOOT].pi_offset = ptend - i;
	ptend -= i;
#else
	bsdlabel[PART_BOOT].pi_offset = ptstart;
	partstart += i;
#endif
#endif

#ifdef PART_REST
	bsdlabel[PART_REST].pi_offset = 0;
	bsdlabel[PART_REST].pi_size = ptstart;
#endif

	/*
	 * Save any partitions that are outside the area we are
	 * going to use.
	 * In particular this saves details of the other MBR
	 * partitions on a multiboot i386 system.
	 */
	 for (i = maxpart; i--;) {
		if (bsdlabel[i].pi_size != 0)
			/* Don't overwrite special partitions */
			continue;
		p = &oldlabel[i];
		if (p->pi_fstype == FS_UNUSED || p->pi_size == 0)
			continue;
		if (layoutkind == 4) {
			if (PI_ISBSDFS(p))
				p->pi_flags |= PIF_MOUNT;
		} else {
			if (p->pi_offset < ptstart + ptsize &&			
			    p->pi_offset + p->pi_size > ptstart)
				/* Not outside area we are allocating */
				continue;
			if (p->pi_fstype == FS_SWAP)
				no_swap = 1;
		}
		bsdlabel[i] = oldlabel[i];
	 }

	if (layoutkind == 4) {
		/* XXX Check we have a sensible layout */
		;
	} else
		get_ptn_sizes(partstart, ptend - partstart, no_swap);

	/*
	 * OK, we have a partition table. Give the user the chance to
	 * edit it and verify it's OK, or abort altogether.
	 */
 edit_check:
	if (edit_and_check_label(bsdlabel, maxpart, part_raw, part_bsd) == 0) {
		msg_display(MSG_abort);
		return 0;
	}
	if (md_check_partitions() == 0)
		goto edit_check;

	/* Disk name */
	msg_prompt(MSG_packname, bsddiskname, bsddiskname, sizeof bsddiskname);

	/* save label to disk for MI code to update. */
	(void) savenewlabel(bsdlabel, maxpart);

	/* Everything looks OK. */
	return (1);
}
