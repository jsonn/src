/*	$NetBSD: aicasm.h,v 1.1.6.2 2000/11/20 11:41:34 bouyer Exp $	*/

/*
 * Assembler for the sequencer program downloaded to Aic7xxx SCSI host adapters
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/aic7xxx/aicasm.h,v 1.6 1999/12/06 18:23:30 gibbs Exp $
 */

#include <sys/queue.h>

#ifdef __NetBSD__

#define STAILQ_EMPTY		SIMPLEQ_EMPTY
#define STAILQ_FIRST		SIMPLEQ_FIRST
#define STAILQ_NEXT		SIMPLEQ_NEXT
#define STAILQ_ENTRY		SIMPLEQ_ENTRY
#define STAILQ_INIT		SIMPLEQ_INIT
#define STAILQ_HEAD		SIMPLEQ_HEAD
#define STAILQ_INSERT_AFTER	SIMPLEQ_INSERT_AFTER
#define STAILQ_INSERT_HEAD	SIMPLEQ_INSERT_HEAD
#define STAILQ_INSERT_TAIL	SIMPLEQ_INSERT_TAIL

#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef struct path_entry {
	char	*directory;
	int	quoted_includes_only;
	SLIST_ENTRY(path_entry) links;
} *path_entry_t;

typedef enum {  
	QUOTED_INCLUDE,
	BRACKETED_INCLUDE,
	SOURCE_FILE
} include_type;

SLIST_HEAD(path_list, path_entry);

extern struct path_list search_path;
extern struct scope_list scope_stack;
extern struct symlist patch_functions;
extern int includes_search_curdir;		/* False if we've seen -I- */
extern char *appname;
extern int yylineno;
extern char *yyfilename;

void stop(const char *errstring, int err_code);
void include_file(char *file_name, include_type type);
struct instruction *seq_alloc(void);
struct scope *scope_alloc(void);
void process_scope(struct scope *);
