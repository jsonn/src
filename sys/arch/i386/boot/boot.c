/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	$Id: boot.c,v 1.17.2.2 1994/02/04 00:11:05 mycroft Exp $
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/param.h>
#include "boot.h"
#include <a.out.h>
#include <sys/reboot.h>

struct exec head;
int argv[9];
char *name;
char *names[] = {
	"/netbsd", "/onetbsd", "/netbsd.old",
};
#define NUMNAMES	(sizeof(names)/sizeof(char *))

extern char *version;

extern int end;
boot(drive)
int drive;
{
	int loadflags, currname = 0;
	char *t;
		
	printf("\n>> NetBSD BOOT @ 0x%x: %d/%d k [%s]\n",
		ouraddr,
		argv[7] = memsize(0),
		argv[8] = memsize(1),
		version);
	printf("use hd(1,a)/netbsd to boot sd0 when wd0 is also installed\n");
	gateA20();
    loadstart:
	/***************************************************************\
	* As a default set it to the first partition of the first	*
	* floppy or hard drive						*
	\***************************************************************/
	part = 0;
	unit = drive&0x7f;
	maj = (drive&0x80 ? 0 : 2);		/* a good first bet */

	name = names[currname++];

	loadflags = 0;
	if (currname == NUMNAMES)
		currname = 0;
	getbootdev(&loadflags);
	if (openrd()) {
		printf("Can't find %s\n", name);
		goto loadstart;
	}
	loadprog(loadflags);
	goto loadstart;
}

loadprog(howto)
	int		howto;
{
	long int startaddr;
	long int addr;	/* physical address.. not directly useable */
	int i;
	static int (*x_entry)() = 0;

	read(&head, sizeof(head));
	if (N_BADMAG(head)) {
		printf("invalid format\n");
		return;
	}

	poff = N_TXTOFF(head);

	startaddr = (int)head.a_entry;
	addr = (startaddr & 0x00f00000); /* some MEG boundary */
	printf("Booting %s(%d,%c)%s @ 0x%x\n",
	    devs[maj], unit, 'a'+part, name, addr);

	/*
	 * The +40960 is for memory used by locore.s for the kernel page
	 * table and proc0 stack.  XXX
	 */
	if ((addr + N_BSSADDR(head) + head.a_bss + 40960) >
	    ((memsize(1) + 1024) * 1024)) {
		printf("kernel too large\n");
		return;
	}

	/********************************************************/
	/* LOAD THE TEXT SEGMENT				*/
	/********************************************************/
	printf("%d", head.a_text);
	xread(addr, head.a_text);
	addr += head.a_text;

	/********************************************************/
	/* Load the Initialised data after the text		*/
	/********************************************************/
	if (N_GETMAGIC(head) == NMAGIC) {
		i = CLBYTES - (addr & CLOFSET);
		if (i < CLBYTES) {
			pbzero(addr, i);
			addr += i;
		}
	}

	printf("+%d", head.a_data);
	xread(addr, head.a_data);
	addr += head.a_data;

	/********************************************************/
	/* Skip over the uninitialised data			*/
	/* (but clear it)					*/
	/********************************************************/
	printf("+%d", head.a_bss);
	pbzero(addr, head.a_bss);

	argv[3] = (addr += head.a_bss);

	/********************************************************/
	/*copy in the symbol header				*/
	/********************************************************/
	pcpy(&head.a_syms, addr, sizeof(head.a_syms));
	addr += sizeof(head.a_syms);
	
	/********************************************************/
	/* READ in the symbol table				*/
	/********************************************************/
	printf("+[%d", head.a_syms);
	xread(addr, head.a_syms);
	addr += head.a_syms;
	
	/********************************************************/
	/* Followed by the next integer (another header)	*/
	/* more debug symbols?					*/
	/********************************************************/
	read(&i, sizeof(int));
	pcpy(&i, addr, sizeof(int));
	if (i) {
		i -= sizeof(int);
		addr += sizeof(int);
		printf("+%d", i);
		xread(addr, i);
		addr += i;
	}

	/********************************************************/
	/* and that many bytes of (debug symbols?)		*/
	/********************************************************/
	printf("]");
	argv[4] = ((addr+sizeof(int)-1))&~(sizeof(int)-1);

	/********************************************************/
	/* and note the end address of all this			*/
	/********************************************************/
	printf("=0x%x\n", addr);

	/*
	 *  We now pass the various bootstrap parameters to the loaded
	 *  image via the argument list
	 *
         *  arg0 = 8 (magic)
	 *  arg1 = boot flags
	 *  arg2 = boot device
	 *  arg3 = start of symbol table (0 if not loaded)
	 *  arg4 = end of symbol table (0 if not loaded)
	 *  arg5 = transfer address from image
	 *  arg6 = transfer address for next image pointer
         *  arg7 = conventional memory size (640)
         *  arg8 = extended memory size (8196)
	 */
	switch(maj) {
	case 2:
		/* if booting from floppy, always prompt for root device */
		howto |= RB_ASKNAME;
		break;
	}

	startaddr &= 0xffffff;
	argv[1] = howto;
	argv[2] = (MAKEBOOTDEV(maj, 0, 0, unit, part));
	argv[5] = startaddr;
	argv[6] = (int) &x_entry;
	argv[0] = 8;

	/****************************************************************/
	/* copy that first page and overwrite any BIOS variables	*/
	/****************************************************************/
	printf("entry point at 0x%x\n", (int)startaddr);
	startprog((int)startaddr, argv);
}

char namebuf[100];
getbootdev(howto)
	int *howto;
{
	char c, *ptr = namebuf;
	printf("Boot: [[[%s(%d,%c)]%s][-s][-a][-d]] :- ",
	    devs[maj], unit, 'a'+part, name);
	if (gets(namebuf)) {
		while (c = *ptr) {
			while (c == ' ')
				c = *++ptr;
			if (!c)
				return;
			if (c == '-')
				while ((c = *++ptr) && c != ' ')
					switch (c) {
					case 'a':
						*howto |= RB_ASKNAME;
						continue;
					case 's':
						*howto |= RB_SINGLE;
						continue;
					case 'd':
						*howto |= RB_KDB;
						continue;
					case 'b':
						*howto |= RB_HALT;
						continue;
					}
			else {
				name = ptr;
				while ((c = *++ptr) && c != ' ');
				if (c)
					*ptr++ = 0;
			}
		}
	} else
		printf("\n");
}

