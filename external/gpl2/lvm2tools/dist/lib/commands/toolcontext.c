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
#include "toolcontext.h"
#include "metadata.h"
#include "defaults.h"
#include "lvm-string.h"
#include "activate.h"
#include "filter.h"
#include "filter-composite.h"
#include "filter-md.h"
#include "filter-persistent.h"
#include "filter-regex.h"
#include "filter-sysfs.h"
#include "label.h"
#include "lvm-file.h"
#include "format-text.h"
#include "display.h"
#include "memlock.h"
#include "str_list.h"
#include "segtype.h"
#include "lvmcache.h"
#include "dev-cache.h"
#include "archiver.h"

#ifdef HAVE_LIBDL
#include "sharedlib.h"
#endif

#ifdef LVM1_INTERNAL
#include "format1.h"
#endif

#ifdef POOL_INTERNAL
#include "format_pool.h"
#endif

#include <locale.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <syslog.h>
#include <time.h>

#ifdef linux
#  include <malloc.h>
#endif

static int _get_env_vars(struct cmd_context *cmd)
{
	const char *e;

	/* Set to "" to avoid using any system directory */
	if ((e = getenv("LVM_SYSTEM_DIR"))) {
		if (dm_snprintf(cmd->sys_dir, sizeof(cmd->sys_dir),
				 "%s", e) < 0) {
			log_error("LVM_SYSTEM_DIR environment variable "
				  "is too long.");
			return 0;
		}
	}

	return 1;
}

static void _init_logging(struct cmd_context *cmd)
{
	int append = 1;
	time_t t;

	const char *log_file;
	char timebuf[26];

	/* Syslog */
	cmd->default_settings.syslog =
	    find_config_tree_int(cmd, "log/syslog", DEFAULT_SYSLOG);
	if (cmd->default_settings.syslog != 1)
		fin_syslog();

	if (cmd->default_settings.syslog > 1)
		init_syslog(cmd->default_settings.syslog);

	/* Debug level for log file output */
	cmd->default_settings.debug =
	    find_config_tree_int(cmd, "log/level", DEFAULT_LOGLEVEL);
	init_debug(cmd->default_settings.debug);

	/* Verbose level for tty output */
	cmd->default_settings.verbose =
	    find_config_tree_int(cmd, "log/verbose", DEFAULT_VERBOSE);
	init_verbose(cmd->default_settings.verbose + VERBOSE_BASE_LEVEL);

	/* Log message formatting */
	init_indent(find_config_tree_int(cmd, "log/indent",
				    DEFAULT_INDENT));

	cmd->default_settings.msg_prefix = find_config_tree_str(cmd,
							   "log/prefix",
							   DEFAULT_MSG_PREFIX);
	init_msg_prefix(cmd->default_settings.msg_prefix);

	cmd->default_settings.cmd_name = find_config_tree_int(cmd,
							 "log/command_names",
							 DEFAULT_CMD_NAME);
	init_cmd_name(cmd->default_settings.cmd_name);

	/* Test mode */
	cmd->default_settings.test =
	    find_config_tree_int(cmd, "global/test", 0);

	/* Settings for logging to file */
	if (find_config_tree_int(cmd, "log/overwrite", DEFAULT_OVERWRITE))
		append = 0;

	log_file = find_config_tree_str(cmd, "log/file", 0);

	if (log_file) {
		release_log_memory();
		fin_log();
		init_log_file(log_file, append);
	}

	log_file = find_config_tree_str(cmd, "log/activate_file", 0);
	if (log_file)
		init_log_direct(log_file, append);

	init_log_while_suspended(find_config_tree_int(cmd,
						 "log/activation", 0));

	t = time(NULL);
	ctime_r(&t, &timebuf[0]);
	timebuf[24] = '\0';
	log_verbose("Logging initialised at %s", timebuf);

	/* Tell device-mapper about our logging */
#ifdef DEVMAPPER_SUPPORT
	dm_log_init(print_log);
#endif
}

static int _process_config(struct cmd_context *cmd)
{
	mode_t old_umask;
	const char *read_ahead;

	/* umask */
	cmd->default_settings.umask = find_config_tree_int(cmd,
						      "global/umask",
						      DEFAULT_UMASK);

	if ((old_umask = umask((mode_t) cmd->default_settings.umask)) !=
	    (mode_t) cmd->default_settings.umask)
		log_verbose("Set umask to %04o", cmd->default_settings.umask);

	/* dev dir */
	if (dm_snprintf(cmd->dev_dir, sizeof(cmd->dev_dir), "%s/",
			 find_config_tree_str(cmd, "devices/dir",
					 DEFAULT_DEV_DIR)) < 0) {
		log_error("Device directory given in config file too long");
		return 0;
	}
#ifdef DEVMAPPER_SUPPORT
	dm_set_dev_dir(cmd->dev_dir);
#endif

	/* proc dir */
	if (dm_snprintf(cmd->proc_dir, sizeof(cmd->proc_dir), "%s",
			 find_config_tree_str(cmd, "global/proc",
					 DEFAULT_PROC_DIR)) < 0) {
		log_error("Device directory given in config file too long");
		return 0;
	}
#ifdef _linux_
	if (*cmd->proc_dir && !dir_exists(cmd->proc_dir)) {
		log_error("WARNING: proc dir %s not found - some checks will be bypassed",
			  cmd->proc_dir);
		cmd->proc_dir[0] = '\0';
	}
#elseif 
	cmd->proc_dir[0] = '\0';
#endif

	/* activation? */
	cmd->default_settings.activation = find_config_tree_int(cmd,
							   "global/activation",
							   DEFAULT_ACTIVATION);
	set_activation(cmd->default_settings.activation);

	cmd->default_settings.suffix = find_config_tree_int(cmd,
						       "global/suffix",
						       DEFAULT_SUFFIX);

	if (!(cmd->default_settings.unit_factor =
	      units_to_bytes(find_config_tree_str(cmd,
					     "global/units",
					     DEFAULT_UNITS),
			     &cmd->default_settings.unit_type))) {
		log_error("Invalid units specification");
		return 0;
	}

	read_ahead = find_config_tree_str(cmd, "activation/readahead", DEFAULT_READ_AHEAD);
	if (!strcasecmp(read_ahead, "auto"))
		cmd->default_settings.read_ahead = DM_READ_AHEAD_AUTO;
	else if (!strcasecmp(read_ahead, "none"))
		cmd->default_settings.read_ahead = DM_READ_AHEAD_NONE;
	else {
		log_error("Invalid readahead specification");
		return 0;
	}

	return 1;
}

static int _set_tag(struct cmd_context *cmd, const char *tag)
{
	log_very_verbose("Setting host tag: %s", dm_pool_strdup(cmd->libmem, tag));

	if (!str_list_add(cmd->libmem, &cmd->tags, tag)) {
		log_error("_set_tag: str_list_add %s failed", tag);
		return 0;
	}

	return 1;
}

static int _check_host_filters(struct cmd_context *cmd, struct config_node *hn,
			       int *passes)
{
	struct config_node *cn;
	struct config_value *cv;

	*passes = 1;

	for (cn = hn; cn; cn = cn->sib) {
		if (!cn->v)
			continue;
		if (!strcmp(cn->key, "host_list")) {
			*passes = 0;
			if (cn->v->type == CFG_EMPTY_ARRAY)
				continue;
			for (cv = cn->v; cv; cv = cv->next) {
				if (cv->type != CFG_STRING) {
					log_error("Invalid hostname string "
						  "for tag %s", cn->key);
					return 0;
				}
				if (!strcmp(cv->v.str, cmd->hostname)) {
					*passes = 1;
					return 1;
				}
			}
		}
		if (!strcmp(cn->key, "host_filter")) {
			log_error("host_filter not supported yet");
			return 0;
		}
	}

	return 1;
}

static int _init_tags(struct cmd_context *cmd, struct config_tree *cft)
{
	const struct config_node *tn, *cn;
	const char *tag;
	int passes;

	if (!(tn = find_config_node(cft->root, "tags")) || !tn->child)
		return 1;

	/* NB hosttags 0 when already 1 intentionally does not delete the tag */
	if (!cmd->hosttags && find_config_int(cft->root, "tags/hosttags",
					      DEFAULT_HOSTTAGS)) {
		/* FIXME Strip out invalid chars: only A-Za-z0-9_+.- */
		if (!_set_tag(cmd, cmd->hostname))
			return_0;
		cmd->hosttags = 1;
	}

	for (cn = tn->child; cn; cn = cn->sib) {
		if (cn->v)
			continue;
		tag = cn->key;
		if (*tag == '@')
			tag++;
		if (!validate_name(tag)) {
			log_error("Invalid tag in config file: %s", cn->key);
			return 0;
		}
		if (cn->child) {
			passes = 0;
			if (!_check_host_filters(cmd, cn->child, &passes))
				return_0;
			if (!passes)
				continue;
		}
		if (!_set_tag(cmd, tag))
			return_0;
	}

	return 1;
}

static int _load_config_file(struct cmd_context *cmd, const char *tag)
{
	char config_file[PATH_MAX] = "";
	const char *filler = "";
	struct stat info;
	struct config_tree_list *cfl;

	if (*tag)
		filler = "_";

	if (dm_snprintf(config_file, sizeof(config_file), "%s/lvm%s%s.conf",
			 cmd->sys_dir, filler, tag) < 0) {
		log_error("LVM_SYSTEM_DIR or tag was too long");
		return 0;
	}

	if (!(cfl = dm_pool_alloc(cmd->libmem, sizeof(*cfl)))) {
		log_error("config_tree_list allocation failed");
		return 0;
	}

	if (!(cfl->cft = create_config_tree(config_file, 0))) {
		log_error("config_tree allocation failed");
		return 0;
	}

	/* Is there a config file? */
	if (stat(config_file, &info) == -1) {
		if (errno == ENOENT) {
			list_add(&cmd->config_files, &cfl->list);
			goto out;
		}
		log_sys_error("stat", config_file);
		destroy_config_tree(cfl->cft);
		return 0;
	}

	log_very_verbose("Loading config file: %s", config_file);
	if (!read_config_file(cfl->cft)) {
		log_error("Failed to load config file %s", config_file);
		destroy_config_tree(cfl->cft);
		return 0;
	}

	list_add(&cmd->config_files, &cfl->list);

      out:
	if (*tag)
		_init_tags(cmd, cfl->cft);
	else
		/* Use temporary copy of lvm.conf while loading other files */
		cmd->cft = cfl->cft;

	return 1;
}

/* Find and read first config file */
static int _init_lvm_conf(struct cmd_context *cmd)
{
	/* No config file if LVM_SYSTEM_DIR is empty */
	if (!*cmd->sys_dir) {
		if (!(cmd->cft = create_config_tree(NULL, 0))) {
			log_error("Failed to create config tree");
			return 0;
		}
		return 1;
	}

	if (!_load_config_file(cmd, ""))
		return_0;

	return 1;
}

/* Read any additional config files */
static int _init_tag_configs(struct cmd_context *cmd)
{
	struct str_list *sl;

	/* Tag list may grow while inside this loop */
	list_iterate_items(sl, &cmd->tags) {
		if (!_load_config_file(cmd, sl->str))
			return_0;
	}

	return 1;
}

static int _merge_config_files(struct cmd_context *cmd)
{
	struct config_tree_list *cfl;

	/* Replace temporary duplicate copy of lvm.conf */
	if (cmd->cft->root) {
		if (!(cmd->cft = create_config_tree(NULL, 0))) {
			log_error("Failed to create config tree");
			return 0;
		}
	}

	list_iterate_items(cfl, &cmd->config_files) {
		/* Merge all config trees into cmd->cft using merge/tag rules */
		if (!merge_config_tree(cmd, cmd->cft, cfl->cft))
			return_0;
	}

	return 1;
}

static void _destroy_tags(struct cmd_context *cmd)
{
	struct list *slh, *slht;

	list_iterate_safe(slh, slht, &cmd->tags) {
		list_del(slh);
	}
}

int config_files_changed(struct cmd_context *cmd)
{
	struct config_tree_list *cfl;

	list_iterate_items(cfl, &cmd->config_files) {
		if (config_file_changed(cfl->cft))
			return 1;
	}

	return 0;
}

static void _destroy_tag_configs(struct cmd_context *cmd)
{
	struct config_tree_list *cfl;

	if (cmd->cft && cmd->cft->root) {
		destroy_config_tree(cmd->cft);
		cmd->cft = NULL;
	}

	list_iterate_items(cfl, &cmd->config_files) {
		destroy_config_tree(cfl->cft);
	}

	list_init(&cmd->config_files);
}

static int _init_dev_cache(struct cmd_context *cmd)
{
	const struct config_node *cn;
	struct config_value *cv;

	if (!dev_cache_init(cmd))
		return_0;

	if (!(cn = find_config_tree_node(cmd, "devices/scan"))) {
		if (!dev_cache_add_dir("/dev")) {
			log_error("Failed to add /dev to internal "
				  "device cache");
			return 0;
		}
		log_verbose("device/scan not in config file: "
			    "Defaulting to /dev");
		return 1;
	}

	for (cv = cn->v; cv; cv = cv->next) {
		if (cv->type != CFG_STRING) {
			log_error("Invalid string in config file: "
				  "devices/scan");
			return 0;
		}

		if (!dev_cache_add_dir(cv->v.str)) {
			log_error("Failed to add %s to internal device cache",
				  cv->v.str);
			return 0;
		}
	}

	if (!(cn = find_config_tree_node(cmd, "devices/loopfiles")))
		return 1;

	for (cv = cn->v; cv; cv = cv->next) {
		if (cv->type != CFG_STRING) {
			log_error("Invalid string in config file: "
				  "devices/loopfiles");
			return 0;
		}

		if (!dev_cache_add_loopfile(cv->v.str)) {
			log_error("Failed to add loopfile %s to internal "
				  "device cache", cv->v.str);
			return 0;
		}
	}


	return 1;
}

#define MAX_FILTERS 4

static struct dev_filter *_init_filter_components(struct cmd_context *cmd)
{
	unsigned nr_filt = 0;
	const struct config_node *cn;
	struct dev_filter *filters[MAX_FILTERS];

	memset(filters, 0, sizeof(filters));

	/*
	 * Filters listed in order: top one gets applied first.
	 * Failure to initialise some filters is not fatal.
	 * Update MAX_FILTERS definition above when adding new filters.
	 */

	/*
	 * sysfs filter. Only available on 2.6 kernels.  Non-critical.
	 * Listed first because it's very efficient at eliminating
	 * unavailable devices.
	 */
	if (find_config_tree_bool(cmd, "devices/sysfs_scan",
			     DEFAULT_SYSFS_SCAN)) {
		if ((filters[nr_filt] = sysfs_filter_create(cmd->proc_dir)))
			nr_filt++;
	}

	/* regex filter. Optional. */
	if (!(cn = find_config_tree_node(cmd, "devices/filter")))
		log_very_verbose("devices/filter not found in config file: "
				 "no regex filter installed");

	else if (!(filters[nr_filt++] = regex_filter_create(cn->v))) {
		log_error("Failed to create regex device filter");
		return NULL;
	}

	/* device type filter. Required. */
	cn = find_config_tree_node(cmd, "devices/types");
	if (!(filters[nr_filt++] = lvm_type_filter_create(cmd->proc_dir, cn))) {
		log_error("Failed to create lvm type filter");
		return NULL;
	}

	/* md component filter. Optional, non-critical. */
	if (find_config_tree_bool(cmd, "devices/md_component_detection",
			     DEFAULT_MD_COMPONENT_DETECTION)) {
		init_md_filtering(1);
		if ((filters[nr_filt] = md_filter_create()))
			nr_filt++;
	}

	/* Only build a composite filter if we really need it. */
	return (nr_filt == 1) ?
	    filters[0] : composite_filter_create(nr_filt, filters);
}

static int _init_filters(struct cmd_context *cmd, unsigned load_persistent_cache)
{
	const char *dev_cache = NULL, *cache_dir, *cache_file_prefix;
	struct dev_filter *f3, *f4;
	struct stat st;
	char cache_file[PATH_MAX];

	cmd->dump_filter = 0;

	if (!(f3 = _init_filter_components(cmd)))
		return 0;

	init_ignore_suspended_devices(find_config_tree_int(cmd,
	    "devices/ignore_suspended_devices", DEFAULT_IGNORE_SUSPENDED_DEVICES));

	/*
	 * If 'cache_dir' or 'cache_file_prefix' is set, ignore 'cache'.
	 */
	cache_dir = find_config_tree_str(cmd, "devices/cache_dir", NULL);
	cache_file_prefix = find_config_tree_str(cmd, "devices/cache_file_prefix", NULL);

	if (cache_dir || cache_file_prefix) {
		if (dm_snprintf(cache_file, sizeof(cache_file),
		    "%s%s%s/%s.cache",
		    cache_dir ? "" : cmd->sys_dir,
		    cache_dir ? "" : "/",
		    cache_dir ? : DEFAULT_CACHE_SUBDIR,
		    cache_file_prefix ? : DEFAULT_CACHE_FILE_PREFIX) < 0) {
			log_error("Persistent cache filename too long.");
			return 0;
		}
	} else if (!(dev_cache = find_config_tree_str(cmd, "devices/cache", NULL)) &&
		   (dm_snprintf(cache_file, sizeof(cache_file),
				"%s/%s/%s.cache",
				cmd->sys_dir, DEFAULT_CACHE_SUBDIR,
				DEFAULT_CACHE_FILE_PREFIX) < 0)) {
		log_error("Persistent cache filename too long.");
		return 0;
	}

	if (!dev_cache)
		dev_cache = cache_file;

	if (!(f4 = persistent_filter_create(f3, dev_cache))) {
		log_error("Failed to create persistent device filter");
		return 0;
	}

	/* Should we ever dump persistent filter state? */
	if (find_config_tree_int(cmd, "devices/write_cache_state", 1))
		cmd->dump_filter = 1;

	if (!*cmd->sys_dir)
		cmd->dump_filter = 0;

	/*
	 * Only load persistent filter device cache on startup if it is newer
	 * than the config file and this is not a long-lived process.
	 */
	if (load_persistent_cache && !cmd->is_long_lived &&
	    !stat(dev_cache, &st) &&
	    (st.st_ctime > config_file_timestamp(cmd->cft)) &&
	    !persistent_filter_load(f4, NULL))
		log_verbose("Failed to load existing device cache from %s",
			    dev_cache);

	cmd->filter = f4;

	return 1;
}

static int _init_formats(struct cmd_context *cmd)
{
	const char *format;

	struct format_type *fmt;

#ifdef HAVE_LIBDL
	const struct config_node *cn;
#endif

	label_init();

#ifdef LVM1_INTERNAL
	if (!(fmt = init_lvm1_format(cmd)))
		return 0;
	fmt->library = NULL;
	list_add(&cmd->formats, &fmt->list);
#endif

#ifdef POOL_INTERNAL
	if (!(fmt = init_pool_format(cmd)))
		return 0;
	fmt->library = NULL;
	list_add(&cmd->formats, &fmt->list);
#endif

#ifdef HAVE_LIBDL
	/* Load any formats in shared libs if not static */
	if (!cmd->is_static &&
	    (cn = find_config_tree_node(cmd, "global/format_libraries"))) {

		struct config_value *cv;
		struct format_type *(*init_format_fn) (struct cmd_context *);
		void *lib;

		for (cv = cn->v; cv; cv = cv->next) {
			if (cv->type != CFG_STRING) {
				log_error("Invalid string in config file: "
					  "global/format_libraries");
				return 0;
			}
			if (!(lib = load_shared_library(cmd, cv->v.str,
							"format", 0)))
				return_0;

			if (!(init_format_fn = dlsym(lib, "init_format"))) {
				log_error("Shared library %s does not contain "
					  "format functions", cv->v.str);
				dlclose(lib);
				return 0;
			}

			if (!(fmt = init_format_fn(cmd)))
				return 0;
			fmt->library = lib;
			list_add(&cmd->formats, &fmt->list);
		}
	}
#endif

	if (!(fmt = create_text_format(cmd)))
		return 0;
	fmt->library = NULL;
	list_add(&cmd->formats, &fmt->list);

	cmd->fmt_backup = fmt;

	format = find_config_tree_str(cmd, "global/format",
				 DEFAULT_FORMAT);

	list_iterate_items(fmt, &cmd->formats) {
		if (!strcasecmp(fmt->name, format) ||
		    (fmt->alias && !strcasecmp(fmt->alias, format))) {
			cmd->default_settings.fmt = fmt;
			return 1;
		}
	}

	log_error("_init_formats: Default format (%s) not found", format);
	return 0;
}

int init_lvmcache_orphans(struct cmd_context *cmd)
{
	struct format_type *fmt;

	list_iterate_items(fmt, &cmd->formats)
		if (!lvmcache_add_orphan_vginfo(fmt->orphan_vg_name, fmt))
			return_0;

	return 1;
}

static int _init_segtypes(struct cmd_context *cmd)
{
	struct segment_type *segtype;

#ifdef HAVE_LIBDL
	const struct config_node *cn;
#endif

	if (!(segtype = init_striped_segtype(cmd)))
		return 0;
	segtype->library = NULL;
	list_add(&cmd->segtypes, &segtype->list);

	if (!(segtype = init_zero_segtype(cmd)))
		return 0;
	segtype->library = NULL;
	list_add(&cmd->segtypes, &segtype->list);

	if (!(segtype = init_error_segtype(cmd)))
		return 0;
	segtype->library = NULL;
	list_add(&cmd->segtypes, &segtype->list);

	if (!(segtype = init_free_segtype(cmd)))
		return 0;
	segtype->library = NULL;
	list_add(&cmd->segtypes, &segtype->list);

#ifdef SNAPSHOT_INTERNAL
	if (!(segtype = init_snapshot_segtype(cmd)))
		return 0;
	segtype->library = NULL;
	list_add(&cmd->segtypes, &segtype->list);
#endif

#ifdef MIRRORED_INTERNAL
	if (!(segtype = init_mirrored_segtype(cmd)))
		return 0;
	segtype->library = NULL;
	list_add(&cmd->segtypes, &segtype->list);
#endif

#ifdef HAVE_LIBDL
	/* Load any formats in shared libs unless static */
	if (!cmd->is_static &&
	    (cn = find_config_tree_node(cmd, "global/segment_libraries"))) {

		struct config_value *cv;
		struct segment_type *(*init_segtype_fn) (struct cmd_context *);
		void *lib;
		struct segment_type *segtype2;

		for (cv = cn->v; cv; cv = cv->next) {
			if (cv->type != CFG_STRING) {
				log_error("Invalid string in config file: "
					  "global/segment_libraries");
				return 0;
			}
			if (!(lib = load_shared_library(cmd, cv->v.str,
							"segment type", 0)))
				return_0;

			if (!(init_segtype_fn = dlsym(lib, "init_segtype"))) {
				log_error("Shared library %s does not contain "
					  "segment type functions", cv->v.str);
				dlclose(lib);
				return 0;
			}

			if (!(segtype = init_segtype_fn(cmd)))
				return 0;
			segtype->library = lib;
			list_add(&cmd->segtypes, &segtype->list);

			list_iterate_items(segtype2, &cmd->segtypes) {
				if ((segtype == segtype2) ||
				     strcmp(segtype2->name, segtype->name))
					continue;
				log_error("Duplicate segment type %s: "
					  "unloading shared library %s",
					  segtype->name, cv->v.str);
				list_del(&segtype->list);
				segtype->ops->destroy(segtype);
				dlclose(lib);
			}
		}
	}
#endif

	return 1;
}

static int _init_hostname(struct cmd_context *cmd)
{
	struct utsname uts;

	if (uname(&uts)) {
		log_sys_error("uname", "_init_hostname");
		return 0;
	}

	if (!(cmd->hostname = dm_pool_strdup(cmd->libmem, uts.nodename))) {
		log_error("_init_hostname: dm_pool_strdup failed");
		return 0;
	}

	if (!(cmd->kernel_vsn = dm_pool_strdup(cmd->libmem, uts.release))) {
		log_error("_init_hostname: dm_pool_strdup kernel_vsn failed");
		return 0;
	}

	return 1;
}

static int _init_backup(struct cmd_context *cmd)
{
	uint32_t days, min;
	char default_dir[PATH_MAX];
	const char *dir;

	if (!cmd->sys_dir) {
		log_warn("WARNING: Metadata changes will NOT be backed up");
		backup_init(cmd, "");
		archive_init(cmd, "", 0, 0);
		return 1;
	}

	/* set up archiving */
	cmd->default_settings.archive =
	    find_config_tree_bool(cmd, "backup/archive",
			     DEFAULT_ARCHIVE_ENABLED);

	days = (uint32_t) find_config_tree_int(cmd, "backup/retain_days",
					  DEFAULT_ARCHIVE_DAYS);

	min = (uint32_t) find_config_tree_int(cmd, "backup/retain_min",
					 DEFAULT_ARCHIVE_NUMBER);

	if (dm_snprintf
	    (default_dir, sizeof(default_dir), "%s/%s", cmd->sys_dir,
	     DEFAULT_ARCHIVE_SUBDIR) == -1) {
		log_err("Couldn't create default archive path '%s/%s'.",
			cmd->sys_dir, DEFAULT_ARCHIVE_SUBDIR);
		return 0;
	}

	dir = find_config_tree_str(cmd, "backup/archive_dir",
			      default_dir);

	if (!archive_init(cmd, dir, days, min)) {
		log_debug("backup_init failed.");
		return 0;
	}

	/* set up the backup */
	cmd->default_settings.backup =
	    find_config_tree_bool(cmd, "backup/backup",
			     DEFAULT_BACKUP_ENABLED);

	if (dm_snprintf
	    (default_dir, sizeof(default_dir), "%s/%s", cmd->sys_dir,
	     DEFAULT_BACKUP_SUBDIR) == -1) {
		log_err("Couldn't create default backup path '%s/%s'.",
			cmd->sys_dir, DEFAULT_BACKUP_SUBDIR);
		return 0;
	}

	dir = find_config_tree_str(cmd, "backup/backup_dir", default_dir);

	if (!backup_init(cmd, dir)) {
		log_debug("backup_init failed.");
		return 0;
	}

	return 1;
}

/* Entry point */
struct cmd_context *create_toolcontext(struct arg *the_args, unsigned is_static,
				       unsigned is_long_lived)
{
	struct cmd_context *cmd;

#ifdef M_MMAP_MAX
	mallopt(M_MMAP_MAX, 0);
#endif

	if (!setlocale(LC_ALL, ""))
		log_very_verbose("setlocale failed");

#ifdef INTL_PACKAGE
	bindtextdomain(INTL_PACKAGE, LOCALEDIR);
#endif

	init_syslog(DEFAULT_LOG_FACILITY);

	if (!(cmd = dm_malloc(sizeof(*cmd)))) {
		log_error("Failed to allocate command context");
		return NULL;
	}
	memset(cmd, 0, sizeof(*cmd));
	cmd->args = the_args;
	cmd->is_static = is_static;
	cmd->is_long_lived = is_long_lived;
	cmd->hosttags = 0;
	list_init(&cmd->formats);
	list_init(&cmd->segtypes);
	list_init(&cmd->tags);
	list_init(&cmd->config_files);

	strcpy(cmd->sys_dir, DEFAULT_SYS_DIR);

	if (!_get_env_vars(cmd))
		goto error;

	/* Create system directory if it doesn't already exist */
	if (*cmd->sys_dir && !dm_create_dir(cmd->sys_dir)) {
		log_error("Failed to create LVM2 system dir for metadata backups, config "
			  "files and internal cache.");
		log_error("Set environment variable LVM_SYSTEM_DIR to alternative location "
			  "or empty string.");
		goto error;
	}

	if (!(cmd->libmem = dm_pool_create("library", 4 * 1024))) {
		log_error("Library memory pool creation failed");
		goto error;
	}

	if (!_init_lvm_conf(cmd))
		goto error;

	_init_logging(cmd);

	if (!_init_hostname(cmd))
		goto error;

	if (!_init_tags(cmd, cmd->cft))
		goto error;

	if (!_init_tag_configs(cmd))
		goto error;

	if (!_merge_config_files(cmd))
		goto error;

	if (!_process_config(cmd))
		goto error;

	if (!_init_dev_cache(cmd))
		goto error;

	if (!_init_filters(cmd, 1))
		goto error;

	if (!(cmd->mem = dm_pool_create("command", 4 * 1024))) {
		log_error("Command memory pool creation failed");
		goto error;
	}

	memlock_init(cmd);

	if (!_init_formats(cmd))
		goto error;

	if (!init_lvmcache_orphans(cmd))
		goto error;

	if (!_init_segtypes(cmd))
		goto error;

	if (!_init_backup(cmd))
		goto error;

	cmd->default_settings.cache_vgmetadata = 1;
	cmd->current_settings = cmd->default_settings;

	cmd->config_valid = 1;
	return cmd;

      error:
	dm_free(cmd);
	return NULL;
}

static void _destroy_formats(struct list *formats)
{
	struct list *fmtl, *tmp;
	struct format_type *fmt;
	void *lib;

	list_iterate_safe(fmtl, tmp, formats) {
		fmt = list_item(fmtl, struct format_type);
		list_del(&fmt->list);
		lib = fmt->library;
		fmt->ops->destroy(fmt);
#ifdef HAVE_LIBDL
		if (lib)
			dlclose(lib);
#endif
	}
}

static void _destroy_segtypes(struct list *segtypes)
{
	struct list *sgtl, *tmp;
	struct segment_type *segtype;
	void *lib;

	list_iterate_safe(sgtl, tmp, segtypes) {
		segtype = list_item(sgtl, struct segment_type);
		list_del(&segtype->list);
		lib = segtype->library;
		segtype->ops->destroy(segtype);
#ifdef HAVE_LIBDL
		if (lib)
			dlclose(lib);
#endif
	}
}

int refresh_toolcontext(struct cmd_context *cmd)
{
	log_verbose("Reloading config files");

	/*
	 * Don't update the persistent filter cache as we will
	 * perform a full rescan.
	 */

	activation_release();
	lvmcache_destroy(cmd, 0);
	label_exit();
	_destroy_segtypes(&cmd->segtypes);
	_destroy_formats(&cmd->formats);
	if (cmd->filter) {
		cmd->filter->destroy(cmd->filter);
		cmd->filter = NULL;
	}
	dev_cache_exit();
	_destroy_tags(cmd);
	_destroy_tag_configs(cmd);

	cmd->config_valid = 0;

	cmd->hosttags = 0;

	if (!_init_lvm_conf(cmd))
		return 0;

	_init_logging(cmd);

	if (!_init_tags(cmd, cmd->cft))
		return 0;

	if (!_init_tag_configs(cmd))
		return 0;

	if (!_merge_config_files(cmd))
		return 0;

	if (!_process_config(cmd))
		return 0;

	if (!_init_dev_cache(cmd))
		return 0;

	if (!_init_filters(cmd, 0))
		return 0;

	if (!_init_formats(cmd))
		return 0;

	if (!init_lvmcache_orphans(cmd))
		return 0;

	if (!_init_segtypes(cmd))
		return 0;

	/*
	 * If we are a long-lived process, write out the updated persistent
	 * device cache for the benefit of short-lived processes.
	 */
	if (cmd->is_long_lived && cmd->dump_filter)
		persistent_filter_dump(cmd->filter);

	cmd->config_valid = 1;
	return 1;
}

void destroy_toolcontext(struct cmd_context *cmd)
{
	if (cmd->dump_filter)
		persistent_filter_dump(cmd->filter);

	archive_exit(cmd);
	backup_exit(cmd);
	lvmcache_destroy(cmd, 0);
	label_exit();
	_destroy_segtypes(&cmd->segtypes);
	_destroy_formats(&cmd->formats);
	cmd->filter->destroy(cmd->filter);
	dm_pool_destroy(cmd->mem);
	dev_cache_exit();
	_destroy_tags(cmd);
	_destroy_tag_configs(cmd);
	dm_pool_destroy(cmd->libmem);
	dm_free(cmd);

	release_log_memory();
	activation_exit();
	fin_log();
	fin_syslog();
}
