/*	$NetBSD: lvm-logging.h,v 1.1.1.1.2.2 2008/12/12 16:33:00 haad Exp $	*/

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

#ifndef _LVM_LOGGING_H
#define _LVM_LOGGING_H

void print_log(int level, const char *file, int line, const char *format, ...)
    __attribute__ ((format(printf, 4, 5)));

#define plog(l, x...) print_log(l, __FILE__, __LINE__ , ## x)

#include "log.h"

typedef void (*lvm2_log_fn_t) (int level, const char *file, int line,
			       const char *message);

void init_log_fn(lvm2_log_fn_t log_fn);

void init_indent(int indent);
void init_msg_prefix(const char *prefix);

void init_log_file(const char *log_file, int append);
void init_log_direct(const char *log_file, int append);
void init_log_while_suspended(int log_while_suspended);

void fin_log(void);
void release_log_memory(void);

void init_syslog(int facility);
void fin_syslog(void);

int error_message_produced(void);

/* Suppress messages to stdout/stderr (1) or everywhere (2) */
/* Returns previous setting */
int log_suppress(int suppress);

/* Suppress messages to syslog */
void syslog_suppress(int suppress);

#endif
