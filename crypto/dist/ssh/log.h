/*	$NetBSD: log.h,v 1.1.1.1.2.3 2001/12/10 23:53:12 he Exp $	*/
/*	$OpenBSD: log.h,v 1.4 2001/06/26 17:27:24 markus Exp $	*/

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#ifndef SSH_LOG_H
#define SSH_LOG_H

/* Supported syslog facilities and levels. */
typedef enum {
	SYSLOG_FACILITY_DAEMON,
	SYSLOG_FACILITY_USER,
	SYSLOG_FACILITY_AUTH,
	SYSLOG_FACILITY_LOCAL0,
	SYSLOG_FACILITY_LOCAL1,
	SYSLOG_FACILITY_LOCAL2,
	SYSLOG_FACILITY_LOCAL3,
	SYSLOG_FACILITY_LOCAL4,
	SYSLOG_FACILITY_LOCAL5,
	SYSLOG_FACILITY_LOCAL6,
	SYSLOG_FACILITY_LOCAL7
}       SyslogFacility;

typedef enum {
	SYSLOG_LEVEL_QUIET,
	SYSLOG_LEVEL_FATAL,
	SYSLOG_LEVEL_ERROR,
	SYSLOG_LEVEL_INFO,
	SYSLOG_LEVEL_VERBOSE,
	SYSLOG_LEVEL_DEBUG1,
	SYSLOG_LEVEL_DEBUG2,
	SYSLOG_LEVEL_DEBUG3
}       LogLevel;

void     log_init(char *, LogLevel, SyslogFacility, int);

SyslogFacility	log_facility_number(char *);
LogLevel log_level_number(char *);

void     fatal(const char *, ...) __attribute__((format(printf, 1, 2)));
void     error(const char *, ...) __attribute__((format(printf, 1, 2)));
void     log(const char *, ...) __attribute__((format(printf, 1, 2)));
void     verbose(const char *, ...) __attribute__((format(printf, 1, 2)));
void     debug(const char *, ...) __attribute__((format(printf, 1, 2)));
void     debug2(const char *, ...) __attribute__((format(printf, 1, 2)));
void     debug3(const char *, ...) __attribute__((format(printf, 1, 2)));

void     fatal_cleanup(void);
void     fatal_add_cleanup(void (*) (void *), void *);
void     fatal_remove_cleanup(void (*) (void *), void *);

#endif
