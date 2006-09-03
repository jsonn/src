/* $NetBSD: fileassoc.h,v 1.2.4.3 2006/09/03 15:25:56 yamt Exp $ */

/*-
 * Copyright (c) 2006 Elad Efrat <elad@NetBSD.org>
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
 *      This product includes software developed by Elad Efrat.
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

#ifndef _SYS_FILEASSOC_H_
#define _SYS_FILEASSOC_H_

#include "opt_fileassoc.h"

#include <sys/cdefs.h>
#include <sys/param.h>

/* Max. number of hooks. */
#ifndef FILEASSOC_NHOOKS
#define	FILEASSOC_NHOOKS	4
#endif /* !FILEASSOC_NHOOKS */

#define	FILEASSOC_INVAL		-1

typedef int fileassoc_t;
typedef void (*fileassoc_cleanup_cb_t)(void *, int);
typedef void (*fileassoc_cb_t)(void *);

#define	FILEASSOC_CLEANUP_TABLE	0
#define	FILEASSOC_CLEANUP_FILE	1

void fileassoc_init(void);
fileassoc_t fileassoc_register(const char *, fileassoc_cleanup_cb_t);
int fileassoc_deregister(fileassoc_t);
void *fileassoc_tabledata_lookup(struct mount *, fileassoc_t);
void *fileassoc_lookup(struct vnode *, fileassoc_t);
void *fileassoc_lookup_hint(struct vnode *, fileassoc_t, uint64_t);
int fileassoc_table_add(struct mount *, size_t);
int fileassoc_table_delete(struct mount *);
int fileassoc_table_clear(struct mount *, fileassoc_t);
int fileassoc_tabledata_add(struct mount *, fileassoc_t, void *);
int fileassoc_tabledata_clear(struct mount *, fileassoc_t);
int fileassoc_file_delete(struct vnode *);
int fileassoc_add(struct vnode *, fileassoc_t, void *);
int fileassoc_add_hint(struct vnode *, fileassoc_t, void *, uint64_t);
int fileassoc_clear(struct vnode *, fileassoc_t);
int fileassoc_table_run(struct mount *, fileassoc_t, fileassoc_cb_t);

#endif /* !_SYS_FILEASSOC_H_ */
