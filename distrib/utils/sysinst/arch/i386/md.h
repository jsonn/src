/*	$NetBSD: md.h,v 1.3.2.5 1997/12/17 14:49:50 mellon Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
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

/* md.h -- Machine specific definitions for the i386 */

/* constants and defines */

/* Megs required for a full X installation. */
#define XNEEDMB 50

/* fdisk partition information dlxxxx => NetBSD disklabel, bxxxx => bios */
EXTERN int bcyl, bhead, bsec, bsize, bcylsize;
EXTERN int bstuffset INIT(0);

enum info {ID,SIZE,START,FLAG,SET};
EXTERN int part[4][5] INIT({{0}});
EXTERN int activepart;
EXTERN int bsdpart;
EXTERN int usefull;

/*
 *  Default filesets to fetch and install during installation
 *  or upgrade. The standard sets are:
 *      base, etc, comp, games, man, misc, text,
 *      xbase, xfont, xserver, xcontrib, xcomp.
 *
 * i386 has the  MD set kern first, because generic kernels are  too
 * big to fit on install floppies. i386 does not yet include the x sets. 
 *
 * Third entry is the last extension name in the split sets for loading
 * from floppy.
 */
EXTERN distinfo dist_list[]
#ifdef MAIN
= {
    {"kern",	1, "ae", "Kernel       : "},
    {"base",	1, "bo", "Base         : "},
    {"etc",	1, "aa", "System (/etc): "},
    {"comp",	1, "bd", "Compiler     : "},
    {"games",	1, "am", "Games        : "},
    {"man",	1, "ak", "Manuals      : "},
    {"misc",	1, "aj", "Miscellaneous: "},
    {"text",	1, "ae", "Text tools   : "},
    {NULL, 0, NULL, NULL }
}
#endif
;

/*
 * Disk names accepted as valid targets for a from-scratch installation.
 *
 * On  i386, we allow "wd"  ST-506/IDE disks and  "sd" scsi disks.
 */
EXTERN	char *disk_names[]
#ifdef MAIN
= {"wd", "sd", NULL}
#endif
;


/*
 * Legal start character for a disk for checking input. 
 * this must return 1 for a character that matches the first
 * characters of each member of disk_names.
 *
 * On  i386, that means matching 'w' for st-506/ide and 's' for sd.
 */
#define ISDISKSTART(dn)	(dn == 'w' || dn == 's')

/*
 * Machine-specific command to write a new label to a disk.
 * For example, i386  uses "/sbin/disklabel -w -r", just like i386
 * miniroot scripts, though this may leave a bogus incore label.
 * Sun ports should probably use  DISKLABEL_CMD "/sbin/disklabel -w"
 * to get incore  to ondisk inode translation for the Sun proms.
 * If not defined, we assume the port does not support disklabels and
 * hand-edited disklabel will NOT be written by MI code.
 *
 * On i386, do what the 1.2 install scripts did. 
 */
#define DISKLABEL_CMD "disklabel -w -r"


/*
 * Default fileystem type for floppy disks.
 * On i386, that is  msdos.
 */
EXTERN	char *fdtype INIT("msdos");



/*
 *  prototypes for MD code.
 */


/* from fdisk.c */
void	set_fdisk_geom (void);
void	disp_cur_geom (void);
int	check_geom (void);
int 	partsoverlap (int, int);
void	get_fdisk_info (void);

