/*	$NetBSD: load.c,v 1.15.2.1 2004/05/28 08:31:22 tron Exp $	 */

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Copyright 2002 Charles M. Hannum <root@ihack.net>
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
 *      This product includes software developed by John Polstra.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <dirent.h>

#include "debug.h"
#include "rtld.h"

static bool _rtld_load_by_name(const char *, Obj_Entry *, Needed_Entry **,
    int);

#ifdef RTLD_LOADER
Objlist _rtld_list_main =	/* Objects loaded at program startup */
  SIMPLEQ_HEAD_INITIALIZER(_rtld_list_main);
Objlist _rtld_list_global =	/* Objects dlopened with RTLD_GLOBAL */
  SIMPLEQ_HEAD_INITIALIZER(_rtld_list_global);

void
_rtld_objlist_add(Objlist *list, Obj_Entry *obj)
{
	Objlist_Entry *elm;

	elm = NEW(Objlist_Entry);
	elm->obj = obj;
	SIMPLEQ_INSERT_TAIL(list, elm, link);
}

Objlist_Entry *
_rtld_objlist_find(Objlist *list, const Obj_Entry *obj)
{
	Objlist_Entry *elm;

	for (elm = SIMPLEQ_FIRST(list); elm; elm = SIMPLEQ_NEXT(elm, link)) {
		if (elm->obj == obj)
			return elm;
	}
	return NULL;
}
#endif

/*
 * Load a shared object into memory, if it is not already loaded.  The
 * argument must be a string allocated on the heap.  This function assumes
 * responsibility for freeing it when necessary.
 *
 * Returns a pointer to the Obj_Entry for the object.  Returns NULL
 * on failure.
 */
Obj_Entry *
_rtld_load_object(char *filepath, int mode)
{
	Obj_Entry *obj;
	int fd = -1;
	struct stat sb;
	size_t pathlen = strlen(filepath);

	for (obj = _rtld_objlist->next; obj != NULL; obj = obj->next)
		if (pathlen == obj->pathlen && !strcmp(obj->path, filepath)) 
			break;

	/*
	 * If we didn't find a match by pathname, open the file and check
	 * again by device and inode.  This avoids false mismatches caused
	 * by multiple links or ".." in pathnames.
	 *
	 * To avoid a race, we open the file and use fstat() rather than
	 * using stat().
	 */
	if (obj == NULL) {
		if ((fd = open(filepath, O_RDONLY)) == -1) {
			_rtld_error("Cannot open \"%s\"", filepath);
			return NULL;
		}
		if (fstat(fd, &sb) == -1) {
			_rtld_error("Cannot fstat \"%s\"", filepath);
			close(fd);
			return NULL;
		}
		for (obj = _rtld_objlist->next; obj != NULL; obj = obj->next) {
			if (obj->ino == sb.st_ino && obj->dev == sb.st_dev) {
				close(fd);
				break;
			}
		}
	}

	if (obj == NULL) { /* First use of this object, so we must map it in */
		obj = _rtld_map_object(filepath, fd, &sb);
		(void)close(fd);
		if (obj == NULL) {
			free(filepath);
			return NULL;
		}
		_rtld_digest_dynamic(obj);

		*_rtld_objtail = obj;
		_rtld_objtail = &obj->next;
#ifdef RTLD_LOADER
		_rtld_linkmap_add(obj);	/* for GDB */
#endif
		dbg(("  %p .. %p: %s", obj->mapbase,
		    obj->mapbase + obj->mapsize - 1, obj->path));
		if (obj->textrel)
			dbg(("  WARNING: %s has impure text", obj->path));
	} else
		free(filepath);

	++obj->refcount;
#ifdef RTLD_LOADER
	if (mode & RTLD_MAIN && !obj->mainref) {
		obj->mainref = 1;
		rdbg(("adding %p (%s) to _rtld_list_main", obj, obj->path));
		_rtld_objlist_add(&_rtld_list_main, obj);
	}
	if (mode & RTLD_GLOBAL && !obj->globalref) {
		obj->globalref = 1;
		rdbg(("adding %p (%s) to _rtld_list_global", obj, obj->path));
		_rtld_objlist_add(&_rtld_list_global, obj);
	}
#endif
	return obj;
}

static bool
_rtld_load_by_name(const char *name, Obj_Entry *obj, Needed_Entry **needed, int mode)
{
	Library_Xform *x = _rtld_xforms;
	Obj_Entry *o = NULL;
	size_t i, j;
	bool got = false;
	union {
		int i;
		char s[16];
	} val;

	dbg(("load by name %s %p", name, x));
	for (; x; x = x->next) {
		if (strcmp(x->name, name) != 0)
			continue;

		i = sizeof(val);

		if (sysctl(x->ctl, x->ctlmax, &val, &i, NULL, 0) == -1) {
			xwarnx(_PATH_LD_HINTS ": unknown sysctl for %s", name);
			break;
		}

		switch (x->ctltype[x->ctlmax - 1]) {
		case CTLTYPE_INT:
			xsnprintf(val.s, sizeof(val.s), "%d", val.i);
			break;
		case CTLTYPE_STRING:
			break;
		default:
			xwarnx("unsupported sysctl type %d",
			    x->ctltype[x->ctlmax - 1]);
			break;
		}

		dbg(("sysctl returns %s", val.s));

		for (i = 0; i < RTLD_MAX_ENTRY && x->entry[i].value != NULL;
		    i++) {
			dbg(("entry %ld", (unsigned long)i));
			if (strcmp(x->entry[i].value, val.s) == 0)
				break;
		}

		if (i == RTLD_MAX_ENTRY) {
			xwarnx("sysctl value %s not found for lib%s",
			    val.s, name);
			break;
		}
		/* XXX: This can mess up debuggers, cause we lie about
		 * what we loaded in the needed objects */
		for (j = 0; j < RTLD_MAX_LIBRARY &&
		    x->entry[i].library[j] != NULL; j++) {
			o = _rtld_load_library(x->entry[i].library[j], obj,
			    mode);
			if (o == NULL) {
				xwarnx("could not load %s for %s",
				    x->entry[i].library[j], name);
				continue;
			}
			got = true;
			if (j == 0)
				(*needed)->obj = o;
			else {
				/* make a new one and put it in the chain */
				Needed_Entry *ne = xmalloc(sizeof(*ne));
				ne->name = (*needed)->name;
				ne->obj = o;
				ne->next = (*needed)->next;
				(*needed)->next = ne;
				*needed = ne;
			}
				
		}
		
	}

	if (got)
		return true;

	return ((*needed)->obj = _rtld_load_library(name, obj, mode)) != NULL;
}


/*
 * Given a shared object, traverse its list of needed objects, and load
 * each of them.  Returns 0 on success.  Generates an error message and
 * returns -1 on failure.
 */
int
_rtld_load_needed_objects(Obj_Entry *first, int mode)
{
	Obj_Entry *obj;
	int status = 0;

	for (obj = first; obj != NULL; obj = obj->next) {
		Needed_Entry *needed;

		for (needed = obj->needed; needed != NULL;
		    needed = needed->next) {
			const char *name = obj->strtab + needed->name;
			if (!_rtld_load_by_name(name, obj, &needed, mode))
				status = -1;	/* FIXME - cleanup */
#ifdef RTLD_LOADER
			if (status == -1)
				return status;
#endif
		}
	}

	return status;
}

#ifdef RTLD_LOADER
int
_rtld_preload(const char *preload_path)
{
	const char *path;
	char *cp, *buf;
	int status = 0;

	if (preload_path != NULL && *preload_path != '\0') {
		cp = buf = xstrdup(preload_path);
		while ((path = strsep(&cp, " :")) != NULL && status == 0) {
			if (!_rtld_load_object(xstrdup(path), RTLD_MAIN))
				status = -1;
			else
				dbg((" preloaded \"%s\"", path));
		}
		free(buf);
	}

	return (status);
}
#endif
