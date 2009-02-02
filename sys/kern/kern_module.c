/*	$NetBSD: kern_module.c,v 1.24.4.1 2009/02/02 22:15:15 snj Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 * Kernel module support.
 */

#include "opt_modular.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_module.c,v 1.24.4.1 2009/02/02 22:15:15 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/kobj.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>

#include <machine/stdarg.h>

#ifndef LKM	/* XXX */
struct vm_map *lkm_map;
#endif

struct modlist	module_list = TAILQ_HEAD_INITIALIZER(module_list);
struct modlist	module_bootlist = TAILQ_HEAD_INITIALIZER(module_bootlist);
static module_t	*module_active;
static char	module_base[64];
u_int		module_count;
kmutex_t	module_lock;

/* Ensure that the kernel's link set isn't empty. */
static modinfo_t module_dummy;
__link_set_add_rodata(modules, module_dummy);

static module_t	*module_lookup(const char *);
static int	module_do_load(const char *, bool, int, prop_dictionary_t,
		    module_t **, modclass_t class, bool);
static int	module_do_unload(const char *);
static void	module_error(const char *, ...);
static int	module_do_builtin(const char *, module_t **);
static int	module_fetch_info(module_t *);

/*
 * module_error:
 *
 *	Utility function: log an error.
 */
static void
module_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	printf("WARNING: module error: ");
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

/*
 * module_init:
 *
 *	Initialize the module subsystem.
 */
void
module_init(void)
{
	extern struct vm_map *lkm_map;

	if (lkm_map == NULL)
		lkm_map = kernel_map;
	mutex_init(&module_lock, MUTEX_DEFAULT, IPL_NONE);
#ifdef MODULAR	/* XXX */
	module_init_md();
#endif

#if __NetBSD_Version__ / 1000000 % 100 == 99	/* -current */
	snprintf(module_base, sizeof(module_base), "/stand/%s/%s/modules",
	    machine, osrelease);
#else						/* release */
	snprintf(module_base, sizeof(module_base), "/stand/%s/%d.%d/modules",
	    machine, __NetBSD_Version__ / 100000000,
	    __NetBSD_Version__ / 1000000 % 100);
#endif
}

/*
 * module_init_class:
 *
 *	Initialize all built-in and pre-loaded modules of the
 *	specified class.
 */
void
module_init_class(modclass_t class)
{
	__link_set_decl(modules, modinfo_t);
	modinfo_t *const *mip, *mi;
	module_t *mod;

	mutex_enter(&module_lock);
	/*
	 * Builtins first.  These can't depend on pre-loaded modules.
	 */
	__link_set_foreach(mip, modules) {
		mi = *mip;
		if (mi == &module_dummy) {
			continue;
		}
		if (class != MODULE_CLASS_ANY && class != mi->mi_class) {
			continue;
		}
		(void)module_do_builtin(mi->mi_name, NULL);
	}
	/*
	 * Now preloaded modules.  These will be pulled off the
	 * list as we call module_do_load();
	 */
	do {
		TAILQ_FOREACH(mod, &module_bootlist, mod_chain) {
			mi = mod->mod_info;
			if (class != MODULE_CLASS_ANY &&
			    class != mi->mi_class)
				continue;
			module_do_load(mi->mi_name, false, 0, NULL, NULL,
			    class, true);
			break;
		}
	} while (mod != NULL);
	mutex_exit(&module_lock);
}

/*
 * module_compatible:
 *
 *	Return true if the two supplied kernel versions are said to
 *	have the same binary interface for kernel code.  The entire
 *	version is signficant for the development tree (-current),
 *	major and minor versions are significant for official
 *	releases of the system.
 */
bool
module_compatible(int v1, int v2)
{

#if __NetBSD_Version__ / 1000000 % 100 == 99	/* -current */
	return v1 == v2;
#else						/* release */
	return abs(v1 - v2) < 10000;
#endif
}

/*
 * module_load:
 *
 *	Load a single module from the file system.
 */
int
module_load(const char *filename, int flags, prop_dictionary_t props,
	    modclass_t class)
{
	int error;

	/* Authorize. */
	error = kauth_authorize_system(kauth_cred_get(), KAUTH_SYSTEM_MODULE,
	    0, (void *)(uintptr_t)MODCTL_LOAD, NULL, NULL);
	if (error != 0) {
		return error;
	}

	mutex_enter(&module_lock);
	error = module_do_load(filename, false, flags, props, NULL, class,
	    false);
	mutex_exit(&module_lock);

	return error;
}

/*
 * module_autoload:
 *
 *	Load a single module from the file system, system initiated.
 */
int
module_autoload(const char *filename, modclass_t class)
{
	int error;

	KASSERT(mutex_owned(&module_lock));

	/* Not yet for 5.0. */
	return EPERM;

	/* Authorize. */
	error = kauth_authorize_system(kauth_cred_get(), KAUTH_SYSTEM_MODULE,
	    0, (void *)(uintptr_t)MODCTL_LOAD, (void *)(uintptr_t)1, NULL);
	if (error != 0) {
		return error;
	}

	return module_do_load(filename, false, 0, NULL, NULL, class, true);
}

/*
 * module_unload:
 *
 *	Find and unload a module by name.
 */
int
module_unload(const char *name)
{
	int error;

	/* Authorize. */
	error = kauth_authorize_system(kauth_cred_get(), KAUTH_SYSTEM_MODULE,
	    0, (void *)(uintptr_t)MODCTL_UNLOAD, NULL, NULL);
	if (error != 0) {
		return error;
	}

	mutex_enter(&module_lock);
	error = module_do_unload(name);
	mutex_exit(&module_lock);

	return error;
}

/*
 * module_lookup:
 *
 *	Look up a module by name.
 */
module_t *
module_lookup(const char *name)
{
	module_t *mod;

	KASSERT(mutex_owned(&module_lock));

	TAILQ_FOREACH(mod, &module_list, mod_chain) {
		if (strcmp(mod->mod_info->mi_name, name) == 0) {
			break;
		}
	}

	return mod;
}

/*
 * module_hold:
 *
 *	Add a single reference to a module.  It's the caller's
 *	responsibility to ensure that the reference is dropped
 *	later.
 */
int
module_hold(const char *name)
{
	module_t *mod;

	mutex_enter(&module_lock);
	mod = module_lookup(name);
	if (mod == NULL) {
		mutex_exit(&module_lock);
		return ENOENT;
	}
	mod->mod_refcnt++;
	mutex_exit(&module_lock);

	return 0;
}

/*
 * module_rele:
 *
 *	Release a reference acquired with module_hold().
 */
void
module_rele(const char *name)
{
	module_t *mod;

	mutex_enter(&module_lock);
	mod = module_lookup(name);
	if (mod == NULL) {
		mutex_exit(&module_lock);
		panic("module_rele: gone");
	}
	mod->mod_refcnt--;
	mutex_exit(&module_lock);
}

/*
 * module_do_builtin:
 *
 *	Initialize a single module from the list of modules that are
 *	built into the kernel (linked into the kernel image).
 */
static int
module_do_builtin(const char *name, module_t **modp)
{
	__link_set_decl(modules, modinfo_t);
	modinfo_t *const *mip;
	const char *p, *s;
	char buf[MAXMODNAME];
	modinfo_t *mi;
	module_t *mod, *mod2;
	size_t len;
	int error, i;

	KASSERT(mutex_owned(&module_lock));

	/*
	 * Check to see if already loaded.
	 */
	if ((mod = module_lookup(name)) != NULL) {
		if (modp != NULL) {
			*modp = mod;
		}
		return 0;
	}

	/*
	 * Search the list to see if we have a module by this name.
	 */
	error = ENOENT;
	__link_set_foreach(mip, modules) {
		mi = *mip;
		if (mi == &module_dummy) {
			continue;
		}
		if (strcmp(mi->mi_name, name) == 0) {
			error = 0;
			break;
		}
	}
	if (error != 0) {
		return error;
	}

	/*
	 * Initialize pre-requisites.
	 */
	mod = kmem_zalloc(sizeof(*mod), KM_SLEEP);
	if (mod == NULL) {
		return ENOMEM;
	}
	if (modp != NULL) {
		*modp = mod;
	}
	if (mi->mi_required != NULL) {
		for (s = mi->mi_required; *s != '\0'; s = p) {
			if (*s == ',')
				s++;
			p = s;
			while (*p != '\0' && *p != ',')
				p++;
			len = min(p - s + 1, sizeof(buf));
			strlcpy(buf, s, len);
			if (buf[0] == '\0')
				break;
			if (mod->mod_nrequired == MAXMODDEPS - 1) {
				module_error("too many required modules");
				kmem_free(mod, sizeof(*mod));
				return EINVAL;
			}
			error = module_do_builtin(buf, &mod2);
			if (error != 0) {
				kmem_free(mod, sizeof(*mod));
				return error;
			}
			mod->mod_required[mod->mod_nrequired++] = mod2;
		}
	}

	/*
	 * Try to initialize the module.
	 */
	KASSERT(module_active == NULL);
	module_active = mod;
	error = (*mi->mi_modcmd)(MODULE_CMD_INIT, NULL);
	module_active = NULL;
	if (error != 0) {
		module_error("builtin module `%s' "
		    "failed to init", mi->mi_name);
		kmem_free(mod, sizeof(*mod));
		return error;
	}
	mod->mod_info = mi;
	mod->mod_source = MODULE_SOURCE_KERNEL;
	module_count++;
	TAILQ_INSERT_TAIL(&module_list, mod, mod_chain);

	/*
	 * If that worked, count dependencies.
	 */
	for (i = 0; i < mod->mod_nrequired; i++) {
		mod->mod_required[i]->mod_refcnt++;
	}

	return 0;
}

/*
 * module_do_load:
 *
 *	Helper routine: load a module from the file system, or one
 *	pushed by the boot loader.
 */
static int
module_do_load(const char *filename, bool isdep, int flags,
	       prop_dictionary_t props, module_t **modp, modclass_t class,
	       bool autoload)
{
	static TAILQ_HEAD(,module) pending = TAILQ_HEAD_INITIALIZER(pending);
	static int depth;
	const int maxdepth = 6;
	modinfo_t *mi;
	module_t *mod, *mod2;
	char buf[MAXMODNAME];
	const char *s, *p;
	int error;
	size_t len;
	u_int i;

	KASSERT(mutex_owned(&module_lock));

	error = 0;

	/*
	 * Avoid recursing too far.
	 */
	if (++depth > maxdepth) {
		module_error("too many required modules");
		depth--;
		return EMLINK;
	}

	/*
	 * Load the module and link.  Before going to the file system,
	 * scan the list of modules loaded by the boot loader.  Just
	 * before init is started the list of modules loaded at boot
	 * will be purged.  Before init is started we can assume that
	 * `filename' is a module name and not a path name.
	 */
	TAILQ_FOREACH(mod, &module_bootlist, mod_chain) {
		if (strcmp(mod->mod_info->mi_name, filename) == 0) {
			TAILQ_REMOVE(&module_bootlist, mod, mod_chain);
			break;
		}
	}
	if (mod != NULL) {
		TAILQ_INSERT_TAIL(&pending, mod, mod_chain);
	} else {
		mod = kmem_zalloc(sizeof(*mod), KM_SLEEP);
		if (mod == NULL) {
			depth--;
			return ENOMEM;
		}
		error = kobj_load_file(&mod->mod_kobj, filename, module_base,
		    autoload);
		if (error != 0) {
			kmem_free(mod, sizeof(*mod));
			depth--;
			module_error("unable to load kernel object");
			return error;
		}
		TAILQ_INSERT_TAIL(&pending, mod, mod_chain);
		mod->mod_source = MODULE_SOURCE_FILESYS;
		error = module_fetch_info(mod);
		if (error != 0) {
			goto fail;
		}
	}

	/*
	 * Check compatibility.
	 */
	mi = mod->mod_info;
	if (strlen(mi->mi_name) >= MAXMODNAME) {
		error = EINVAL;
		module_error("module name too long");
		goto fail;
	}
	if (!module_compatible(mi->mi_version, __NetBSD_Version__)) {
		module_error("module built for different version of system");
		if ((flags & MODCTL_LOAD_FORCE) != 0) {
			module_error("forced load, system may be unstable");
		} else {
			error = EPROGMISMATCH;
			goto fail;
		}
	}

	/*
	 * If a specific kind of module was requested, ensure that we have
	 * a match.
	 */
	if (class != MODULE_CLASS_ANY && class != mi->mi_class) {
		error = ENOENT;
		goto fail;
	}

	/*
	 * If loading a dependency, `filename' is a plain module name.
	 * The name must match.
	 */
	if (isdep && strcmp(mi->mi_name, filename) != 0) {
		error = ENOENT;
		goto fail;
	}

	/*
	 * Check to see if the module is already loaded.  If so, we may
	 * have been recursively called to handle a dependency, so be sure
	 * to set modp.
	 */
	if ((mod2 = module_lookup(mi->mi_name)) != NULL) {
		if (modp != NULL)
			*modp = mod2;
		error = EEXIST;
		goto fail;
	}

	/*
	 * Block circular dependencies.
	 */
	TAILQ_FOREACH(mod2, &pending, mod_chain) {
		if (mod == mod2) {
			continue;
		}
		if (strcmp(mod2->mod_info->mi_name, mi->mi_name) == 0) {
		    	error = EDEADLK;
			module_error("circular dependency detected");
		    	goto fail;
		}
	}

	/*
	 * Now try to load any requisite modules.
	 */
	if (mi->mi_required != NULL) {
		for (s = mi->mi_required; *s != '\0'; s = p) {
			if (*s == ',')
				s++;
			p = s;
			while (*p != '\0' && *p != ',')
				p++;
			len = p - s + 1;
			if (len >= MAXMODNAME) {
				error = EINVAL;
				module_error("required module name too long");
				goto fail;
			}
			strlcpy(buf, s, len);
			if (buf[0] == '\0')
				break;
			if (mod->mod_nrequired == MAXMODDEPS - 1) {
				error = EINVAL;
				module_error("too many required modules");
				goto fail;
			}
			if (strcmp(buf, mi->mi_name) == 0) {
				error = EDEADLK;
				module_error("self-dependency detected");
				goto fail;
			}
			error = module_do_load(buf, true, flags, NULL,
			    &mod->mod_required[mod->mod_nrequired++],
			    MODULE_CLASS_ANY, true);
			if (error != 0 && error != EEXIST)
				goto fail;
		}
	}

	/*
	 * We loaded all needed modules successfully: perform global
	 * relocations and initialize.
	 */
	error = kobj_affix(mod->mod_kobj, mi->mi_name);
	if (error != 0) {
		module_error("unable to affix module");
		goto fail2;
	}

	KASSERT(module_active == NULL);
	module_active = mod;
	error = (*mi->mi_modcmd)(MODULE_CMD_INIT, props);
	module_active = NULL;
	if (error != 0) {
		module_error("modctl function returned error %d", error);
		goto fail;
	}

	/*
	 * Good, the module loaded successfully.  Put it onto the
	 * list and add references to its requisite modules.
	 */
	module_count++;
	TAILQ_REMOVE(&pending, mod, mod_chain);
	TAILQ_INSERT_TAIL(&module_list, mod, mod_chain);
	for (i = 0; i < mod->mod_nrequired; i++) {
		KASSERT(mod->mod_required[i] != NULL);
		mod->mod_required[i]->mod_refcnt++;
	}
	if (modp != NULL) {
		*modp = mod;
	}
	depth--;
	return 0;

 fail:
	kobj_unload(mod->mod_kobj);
 fail2:
	TAILQ_REMOVE(&pending, mod, mod_chain);
	kmem_free(mod, sizeof(*mod));
	depth--;
	return error;
}

/*
 * module_do_unload:
 *
 *	Helper routine: do the dirty work of unloading a module.
 */
static int
module_do_unload(const char *name)
{
	module_t *mod;
	int error;
	u_int i;

	KASSERT(mutex_owned(&module_lock));

	mod = module_lookup(name);
	if (mod == NULL) {
		return ENOENT;
	}
	if (mod->mod_refcnt != 0 || mod->mod_source == MODULE_SOURCE_KERNEL) {
		return EBUSY;
	}
	KASSERT(module_active == NULL);
	module_active = mod;
	error = (*mod->mod_info->mi_modcmd)(MODULE_CMD_FINI, NULL);
	module_active = NULL;
	if (error != 0) {
		return error;
	}
	module_count--;
	TAILQ_REMOVE(&module_list, mod, mod_chain);
	for (i = 0; i < mod->mod_nrequired; i++) {
		mod->mod_required[i]->mod_refcnt--;
	}
	if (mod->mod_kobj != NULL) {
		kobj_unload(mod->mod_kobj);
	}
	kmem_free(mod, sizeof(*mod));

	return 0;
}

/*
 * module_prime:
 *
 *	Push a module loaded by the bootloader onto our internal
 *	list.
 */
int
module_prime(void *base, size_t size)
{
	module_t *mod;
	int error;

	mod = kmem_zalloc(sizeof(*mod), KM_SLEEP);
	if (mod == NULL) {
		return ENOMEM;
	}
	mod->mod_source = MODULE_SOURCE_BOOT;

	error = kobj_load_mem(&mod->mod_kobj, base, size);
	if (error != 0) {
		kmem_free(mod, sizeof(*mod));
		module_error("unable to load object pushed by boot loader");
		return error;
	}
	error = module_fetch_info(mod);
	if (error != 0) {
		kobj_unload(mod->mod_kobj);
		kmem_free(mod, sizeof(*mod));
		module_error("unable to load object pushed by boot loader");
		return error;
	}

	TAILQ_INSERT_TAIL(&module_bootlist, mod, mod_chain);

	return 0;
}

/*
 * module_fetch_into:
 *
 *	Fetch modinfo record from a loaded module.
 */
static int
module_fetch_info(module_t *mod)
{
	int error;
	void *addr;
	size_t size;

	/*
	 * Find module info record and check compatibility.
	 */
	error = kobj_find_section(mod->mod_kobj, "link_set_modules",
	    &addr, &size);
	if (error != 0) {
		module_error("`link_set_modules' section not present");
		return error;
	}
	if (size != sizeof(modinfo_t **)) {
		module_error("`link_set_modules' section wrong size");
		return error;
	}
	mod->mod_info = *(modinfo_t **)addr;

	return 0;
}

/*
 * module_find_section:
 *
 *	Allows a module that is being initialized to look up a section
 *	within its ELF object.
 */
int
module_find_section(const char *name, void **addr, size_t *size)
{

	KASSERT(mutex_owned(&module_lock));
	KASSERT(module_active != NULL);

	return kobj_find_section(module_active->mod_kobj, name, addr, size);
}
