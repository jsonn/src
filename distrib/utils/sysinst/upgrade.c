/*	$NetBSD: upgrade.c,v 1.4.2.1 1997/10/27 19:36:28 thorpej Exp $	*/

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

/* upgrade.c -- upgrade an installation. */

#include <stdio.h>
#include <curses.h>
#include "defs.h"
#include "msg_defs.h"
#include "menu_defs.h"

/* Do the system upgrade. */

void do_upgrade(void)
{
	doingwhat = msg_string (MSG_upgrade);

	msg_display (MSG_upgradeusure);
	process_menu (MENU_noyes);
	if (!yesno)
		return;

	get_ramsize ();

	if (find_disks () < 0)
		return;

	if (!fsck_disks())
		return;

	/* Move /mnt/etc /mnt/etc.old so old stuff isn't overwritten. */
	run_prog ("/bin/mv /mnt/etc /mnt/etc.old");

	/* Do any md updating of the file systems ... e.g. bootblocks,
	   copy file systems ... */
	if (!md_update ())
		return;

	/* Get the distribution files */
	process_menu (MENU_distmedium);
	if (nodist)
		return;

	if (got_dist) {
		/* Extract the distribution */
		extract_dist ();

		/* Configure the system */
		run_makedev ();

		/* Network configuration. */
		/* process_menu (MENU_confignet); */
		
		/* Clean up ... */
		if (clean_dist_dir)
			run_prog ("/bin/rm -rf %s", dist_dir);

		/* Mounted dist dir? */
		if (mnt2_mounted)
			run_prog ("/sbin/umoount /mnt2");
		
		/* Install complete ... reboot */
		msg_display (MSG_upgrcomplete);
		process_menu (MENU_ok);
	} else {
		msg_display (MSG_abortupgr);
		process_menu (MENU_ok);
	}
}

