/*	$NetBSD: module.h,v 1.1.2.2 2008/01/19 12:15:42 bouyer Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#ifndef _SYS_MODULE_H_
#define _SYS_MODULE_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/uio.h>

#define	MAXMODNAME	32
#define	MAXMODDEPS	10

/* Module classes, provided only for system boot and cosmetic purposes. */
typedef enum modclass {
	MODULE_CLASS_ANY,
	MODULE_CLASS_MISC,
	MODULE_CLASS_VFS,
	MODULE_CLASS_DRIVER,
	MODULE_CLASS_EXEC
} modclass_t;

/* Module sources: where did it come from? */
typedef enum modsrc {
	MODULE_SOURCE_KERNEL,
	MODULE_SOURCE_BOOT,
	MODULE_SOURCE_FILESYS
} modsrc_t;

/* Commands passed to module control routine. */
typedef enum modcmd {
	MODULE_CMD_INIT,
	MODULE_CMD_FINI,
	MODULE_CMD_STAT
} modcmd_t;

/* Module header structure. */
typedef struct modinfo {
	u_int		mi_release;
	modclass_t	mi_class;
	int		(*mi_modcmd)(modcmd_t, void *);
	const char	*mi_name;
	const char	*mi_required;
} const modinfo_t;

/* Per module information, maintained by kern_module.c */ 
typedef struct module {
	u_int			mod_refcnt;
	const modinfo_t		*mod_info;
	struct kobj		*mod_kobj;
	TAILQ_ENTRY(module)	mod_chain;
	struct module		*mod_required[MAXMODDEPS];
	u_int			mod_nrequired;
	modsrc_t		mod_source;
} module_t;

#ifdef _KERNEL

#include <sys/mutex.h>

/*
 * Per-module linkage.  Loadable modules have a `link_set_modules' section
 * containing only one entry, pointing to the module's modinfo_t record.
 * For the kernel, `link_set_modules' can contain multiple entries and
 * records all modules built into the kernel at link time.
 */
#define	MODULE(class, name, required)				\
static int name##_modcmd(modcmd_t, void *);			\
static const modinfo_t name##_modinfo = {			\
	.mi_release = __NetBSD_Version__,			\
	.mi_class = (class),					\
	.mi_modcmd = name##_modcmd,				\
	.mi_name = #name,					\
	.mi_required = (required)				\
}; 								\
__link_set_add_rodata(modules, name##_modinfo);

TAILQ_HEAD(modlist, module);

extern struct vm_map	*module_map;
extern kmutex_t		module_lock;
extern u_int		module_count;
extern struct modlist	module_list;

void	module_init(void);
void	module_init_class(modclass_t);
int	module_prime(void *, size_t);
void	module_jettison(void);

int	module_load(const char *, bool);
int	module_unload(const char *);
int	module_hold(const char *);
void	module_rele(const char *);

#else	/* _KERNEL */

#include <stdint.h>

#endif	/* _KERNEL */

typedef enum modctl {
	MODCTL_LOAD,		/* char *filename */
	MODCTL_FORCELOAD,	/* char *filename */
	MODCTL_UNLOAD,		/* char *name */
	MODCTL_STAT		/* struct iovec *buffer */
} modctl_t;

/*
 * This structure intentionally has the same layout for 32 and 64
 * bit builds.
 */
typedef struct modstat {
	char		ms_name[MAXMODNAME];
	char		ms_required[MAXMODNAME * MAXMODDEPS];
	uint64_t	ms_addr;
	modsrc_t	ms_source;
	modclass_t	ms_class;
	u_int		ms_size;
	u_int		ms_refcnt;
	u_int		ms_reserved[4];
} modstat_t;

int	modctl(int, void *);

#endif	/* !_SYS_MODULE_H_ */
