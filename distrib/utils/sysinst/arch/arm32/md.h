/*	$NetBSD: md.h,v 1.1.2.2 1997/11/10 19:23:40 thorpej Exp $	*/

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
 *      This product includes software develooped for the NetBSD Project by
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
 *
 */

/* md.h -- Machine specific definitions for the arm32 */

/* Constants and defines */

/* Megs required for a full X installation. */
#define XNEEDMB 50

/* Disk names. */
EXTERN	char *disk_names[]
#ifdef MAIN
= {"wd", "sd", NULL}
#endif
;

/* Legal start character for a disk for checking input. */
#define ISDISKSTART(dn)	(dn == 'w' || dn == 's')

/* Definition of files to retreive from ftp. */
EXTERN char ftp_prefix[STRSIZE] INIT("/binary/Tarfiles");
EXTERN char dist_postfix[STRSIZE] INIT(".tar.gz");
EXTERN distinfo dist_list[]
#ifdef MAIN
= {
    {"kern%s%s",    1, NULL, "Kernel       : "},
    {"base%s%s",    1, NULL, "Base         : "},
    {"etc%s%s",	    1, NULL, "System (/etc): "},
    {"comp%s%s",    1, NULL, "Compiler     : "},
    {"games%s%s",   1, NULL, "Games        : "},
    {"man%s%s",     1, NULL, "Manuals      : "},
    {"misc%s%s",    1, NULL, "Miscellaneous: "},
    {"text%s%s",    1, NULL, "Text tools   : "},
/*
    {"xbase%s%s",   1, NULL, "X11 clients  : "},
    {"xfont%s%s",   1, NULL, "X11 fonts    : "},
    {"xfont%s%s",   1, NULL, "X11 servers  : "},
    {"xcontrib%s%s",1, NULL, "X11 contrib  : "},
    {"xcomp%s%s",   1, NULL, "X programming: "},
*/

    {NULL, 0, NULL }
}
#endif
;

EXTERN char *fdtype INIT("msdos");
