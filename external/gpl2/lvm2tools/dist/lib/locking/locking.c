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

#include "lib.h"
#include "locking.h"
#include "locking_types.h"
#include "lvm-string.h"
#include "activate.h"
#include "toolcontext.h"
#include "memlock.h"
#include "defaults.h"
#include "lvmcache.h"

#include <signal.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

static struct locking_type _locking;
static sigset_t _oldset;

static int _vg_lock_count = 0;		/* Number of locks held */
static int _vg_write_lock_held = 0;	/* VG write lock held? */
static int _signals_blocked = 0;

static volatile sig_atomic_t _sigint_caught = 0;
static volatile sig_atomic_t _handler_installed;
static struct sigaction _oldhandler;
static int _oldmasked;

static void _catch_sigint(int unused __attribute__((unused)))
{
	_sigint_caught = 1;
}

int sigint_caught(void) {
	return _sigint_caught;
}

void sigint_clear(void)
{
	_sigint_caught = 0;
}

/*
 * Temporarily allow keyboard interrupts to be intercepted and noted;
 * saves interrupt handler state for sigint_restore().  Users should
 * use the sigint_caught() predicate to check whether interrupt was
 * requested and act appropriately.  Interrupt flags are never
 * cleared automatically by this code, but the tools clear the flag
 * before running each command in lvm_run_command().  All other places
 * where the flag needs to be cleared need to call sigint_clear().
 */

void sigint_allow(void)
{
	struct sigaction handler;
	sigset_t sigs;

	/*
	 * Do not overwrite the backed-up handler data -
	 * just increase nesting count.
	 */
	if (_handler_installed) {
		_handler_installed++;
		return;
	}

	/* Grab old sigaction for SIGINT: shall not fail. */
	sigaction(SIGINT, NULL, &handler);
	handler.sa_flags &= ~SA_RESTART; /* Clear restart flag */
	handler.sa_handler = _catch_sigint;

	_handler_installed = 1;

	/* Override the signal handler: shall not fail. */
	sigaction(SIGINT, &handler, &_oldhandler);

	/* Unmask SIGINT.  Remember to mask it again on restore. */
	sigprocmask(0, NULL, &sigs);
	if ((_oldmasked = sigismember(&sigs, SIGINT))) {
		sigdelset(&sigs, SIGINT);
		sigprocmask(SIG_SETMASK, &sigs, NULL);
	}
}

void sigint_restore(void)
{
	if (!_handler_installed)
		return;

	if (_handler_installed > 1) {
		_handler_installed--;
		return;
	}

	/* Nesting count went down to 0. */
	_handler_installed = 0;

	if (_oldmasked) {
		sigset_t sigs;
		sigprocmask(0, NULL, &sigs);
		sigaddset(&sigs, SIGINT);
		sigprocmask(SIG_SETMASK, &sigs, NULL);
	}

	sigaction(SIGINT, &_oldhandler, NULL);
}

static void _block_signals(uint32_t flags __attribute((unused)))
{
	sigset_t set;

	if (_signals_blocked)
		return;

	if (sigfillset(&set)) {
		log_sys_error("sigfillset", "_block_signals");
		return;
	}

	if (sigprocmask(SIG_SETMASK, &set, &_oldset)) {
		log_sys_error("sigprocmask", "_block_signals");
		return;
	}

	_signals_blocked = 1;

	return;
}

static void _unblock_signals(void)
{
	/* Don't unblock signals while any locks are held */
	if (!_signals_blocked || _vg_lock_count)
		return;

	if (sigprocmask(SIG_SETMASK, &_oldset, NULL)) {
		log_sys_error("sigprocmask", "_block_signals");
		return;
	}

	_signals_blocked = 0;

	return;
}

static void _lock_memory(uint32_t flags)
{
	if (!(_locking.flags & LCK_PRE_MEMLOCK))
		return;

	if ((flags & (LCK_SCOPE_MASK | LCK_TYPE_MASK)) == LCK_LV_SUSPEND)
		memlock_inc();
}

static void _unlock_memory(uint32_t flags)
{
	if (!(_locking.flags & LCK_PRE_MEMLOCK))
		return;

	if ((flags & (LCK_SCOPE_MASK | LCK_TYPE_MASK)) == LCK_LV_RESUME)
		memlock_dec();
}

void reset_locking(void)
{
	int was_locked = _vg_lock_count;

	_vg_lock_count = 0;
	_vg_write_lock_held = 0;

	_locking.reset_locking();

	if (was_locked)
		_unblock_signals();
}

static void _update_vg_lock_count(uint32_t flags)
{
	if ((flags & LCK_SCOPE_MASK) != LCK_VG)
		return;

	if ((flags & LCK_TYPE_MASK) == LCK_UNLOCK)
		_vg_lock_count--;
	else
		_vg_lock_count++;

	/* We don't bother to reset this until all VG locks are dropped */
	if ((flags & LCK_TYPE_MASK) == LCK_WRITE)
		_vg_write_lock_held = 1;
	else if (!_vg_lock_count)
		_vg_write_lock_held = 0;
}

/*
 * Select a locking type
 */
int init_locking(int type, struct cmd_context *cmd)
{
	init_lockingfailed(0);

	switch (type) {
	case 0:
		init_no_locking(&_locking, cmd);
		log_warn("WARNING: Locking disabled. Be careful! "
			  "This could corrupt your metadata.");
		return 1;

	case 1:
		log_very_verbose("File-based locking selected.");
		if (!init_file_locking(&_locking, cmd))
			break;
		return 1;

#ifdef HAVE_LIBDL
	case 2:
		if (!cmd->is_static) {
			log_very_verbose("External locking selected.");
			if (init_external_locking(&_locking, cmd))
				return 1;
		}
		if (!find_config_tree_int(cmd, "locking/fallback_to_clustered_locking",
					  DEFAULT_FALLBACK_TO_CLUSTERED_LOCKING))
			break;
#endif

#ifdef CLUSTER_LOCKING_INTERNAL
		log_very_verbose("Falling back to internal clustered locking.");
		/* Fall through */

	case 3:
		log_very_verbose("Cluster locking selected.");
		if (!init_cluster_locking(&_locking, cmd))
			break;
		return 1;
#endif

	default:
		log_error("Unknown locking type requested.");
		return 0;
	}

	if ((type == 2 || type == 3) &&
	    find_config_tree_int(cmd, "locking/fallback_to_local_locking",
				 DEFAULT_FALLBACK_TO_LOCAL_LOCKING)) {
		log_warn("WARNING: Falling back to local file-based locking.");
		log_warn("Volume Groups with the clustered attribute will "
			  "be inaccessible.");
		if (init_file_locking(&_locking, cmd))
			return 1;
	}

	if (!ignorelockingfailure())
		return 0;

	/* FIXME Ensure only read ops are permitted */
	log_verbose("Locking disabled - only read operations permitted.");

	init_no_locking(&_locking, cmd);
	init_lockingfailed(1);

	return 1;
}

void fin_locking(void)
{
	_locking.fin_locking();
}

/*
 * Does the LVM1 driver know of this VG name?
 */
int check_lvm1_vg_inactive(struct cmd_context *cmd, const char *vgname)
{
	struct stat info;
	char path[PATH_MAX];

	/* We'll allow operations on orphans */
	if (is_orphan_vg(vgname))
		return 1;

	if (dm_snprintf(path, sizeof(path), "%s/lvm/VGs/%s", cmd->proc_dir,
			 vgname) < 0) {
		log_error("LVM1 proc VG pathname too long for %s", vgname);
		return 0;
	}

	if (stat(path, &info) == 0) {
		log_error("%s exists: Is the original LVM driver using "
			  "this volume group?", path);
		return 0;
	} else if (errno != ENOENT && errno != ENOTDIR) {
		log_sys_error("stat", path);
		return 0;
	}

	return 1;
}

/*
 * VG locking is by VG name.
 * FIXME This should become VG uuid.
 */
static int _lock_vol(struct cmd_context *cmd, const char *resource, uint32_t flags)
{
	int ret = 0;

	_block_signals(flags);
	_lock_memory(flags);

	assert(resource);

	if (!*resource) {
		log_error("Internal error: Use of P_orphans is deprecated.");
		return 0;
	}

	if (*resource == '#' && (flags & LCK_CACHE)) {
		log_error("Internal error: P_%s referenced", resource);
		return 0;
	}

	if ((ret = _locking.lock_resource(cmd, resource, flags))) {
		if ((flags & LCK_SCOPE_MASK) == LCK_VG &&
		    !(flags & LCK_CACHE)) {
			if ((flags & LCK_TYPE_MASK) == LCK_UNLOCK)
				lvmcache_unlock_vgname(resource);
			else
				lvmcache_lock_vgname(resource, (flags & LCK_TYPE_MASK)
								== LCK_READ);
		}

		_update_vg_lock_count(flags);
	}

	_unlock_memory(flags);
	_unblock_signals();

	return ret;
}

int lock_vol(struct cmd_context *cmd, const char *vol, uint32_t flags)
{
	char resource[258] __attribute((aligned(8)));

	if (flags == LCK_NONE) {
		log_debug("Internal error: %s: LCK_NONE lock requested", vol);
		return 1;
	}

	switch (flags & LCK_SCOPE_MASK) {
	case LCK_VG:
		/* Lock VG to change on-disk metadata. */
		/* If LVM1 driver knows about the VG, it can't be accessed. */
		if (!check_lvm1_vg_inactive(cmd, vol))
			return 0;
	case LCK_LV:
		/* Suspend LV if it's active. */
		strncpy(resource, vol, sizeof(resource));
		break;
	default:
		log_error("Unrecognised lock scope: %d",
			  flags & LCK_SCOPE_MASK);
		return 0;
	}

	if (!_lock_vol(cmd, resource, flags))
		return 0;

	/*
	 * If a real lock was acquired (i.e. not LCK_CACHE),
	 * perform an immediate unlock unless LCK_HOLD was requested.
	 */
	if (!(flags & LCK_CACHE) && !(flags & LCK_HOLD) &&
	    ((flags & LCK_TYPE_MASK) != LCK_UNLOCK)) {
		if (!_lock_vol(cmd, resource,
			       (flags & ~LCK_TYPE_MASK) | LCK_UNLOCK))
			return 0;
	}

	return 1;
}

/* Unlock list of LVs */
int resume_lvs(struct cmd_context *cmd, struct list *lvs)
{
	struct lv_list *lvl;

	list_iterate_items(lvl, lvs)
		resume_lv(cmd, lvl->lv);

	return 1;
}

/* Lock a list of LVs */
int suspend_lvs(struct cmd_context *cmd, struct list *lvs)
{
	struct list *lvh;
	struct lv_list *lvl;

	list_iterate_items(lvl, lvs) {
		if (!suspend_lv(cmd, lvl->lv)) {
			log_error("Failed to suspend %s", lvl->lv->name);
			list_uniterate(lvh, lvs, &lvl->list) {
				lvl = list_item(lvh, struct lv_list);
				resume_lv(cmd, lvl->lv);
			}

			return 0;
		}
	}

	return 1;
}

/* Lock a list of LVs */
int activate_lvs(struct cmd_context *cmd, struct list *lvs, unsigned exclusive)
{
	struct list *lvh;
	struct lv_list *lvl;

	list_iterate_items(lvl, lvs) {
		if (!exclusive) {
			if (!activate_lv(cmd, lvl->lv)) {
				log_error("Failed to activate %s", lvl->lv->name);
				return 0;
			}
		} else if (!activate_lv_excl(cmd, lvl->lv)) {
			log_error("Failed to activate %s", lvl->lv->name);
			list_uniterate(lvh, lvs, &lvl->list) {
				lvl = list_item(lvh, struct lv_list);
				activate_lv(cmd, lvl->lv);
			}
			return 0;
		}
	}

	return 1;
}

int vg_write_lock_held(void)
{
	return _vg_write_lock_held;
}

int locking_is_clustered(void)
{
	return (_locking.flags & LCK_CLUSTERED) ? 1 : 0;
}

