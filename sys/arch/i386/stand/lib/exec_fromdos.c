/*	$NetBSD: exec_fromdos.c,v 1.1.4.1 1997/08/23 07:09:37 thorpej Exp $	 */

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996
 * 	Perry E. Metzger.  All rights reserved.
 * Copyright (c) 1997
 * 	Martin Husemann.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

/*
 * starts NetBSD a.out kernel
 * needs lowlevel startup from startprog.S
 * This is a special version of exec.c to support use of XMS.
 */

#include <sys/param.h>
#include <sys/exec_aout.h>
#include <sys/reboot.h>
#ifdef COMPAT_OLDBOOT
#include <sys/disklabel.h>
#endif

#include <lib/libsa/stand.h>
#include "libi386.h"

#ifdef COMPAT_OLDBOOT
static int
dev2major(devname, major)
	char           *devname;
	int            *major;
{
	static char    *devices[] = {"wd", "", "fd", "", "sd"};
#define NUMDEVICES (sizeof(devices)/sizeof(char *))
	int             i;

	for (i = 0; i < NUMDEVICES; i++)
		if (!strcmp(devname, devices[i])) {
			*major = i;
			return (0);
		}
	return (-1);
}
#endif

int 
exec_netbsd(file, loadaddr, boothowto, bootdev, consdev)
	const char     *file;
	physaddr_t      loadaddr;
	int             boothowto;
	char           *bootdev, *consdev;	/* passed to kernel */
{
	register int    io;
	struct exec     x;
	int             cc, magic;
	physaddr_t      entry;
	register physaddr_t cp;
	u_long          boot_argv[6];
	u_long		extmem;

#ifdef XMS
	physaddr_t tmpbuf = 0;
	physaddr_t origadr = 0;
#endif

#ifdef COMPAT_OLDBOOT
	char           *fsname, *devname;
	int             unit, part;
	const char     *filename;
	int             bootdevnr;
#else
	u_long          xbootinfo[2];
#endif

#ifdef	DEBUG
	printf("exec: file=%s loadaddr=0x%lx\n", file, loadaddr);
#endif
	io = open(file, 0);
	if (io < 0)
		return (-1);

	/*
	 * Read in the exec header, and validate it.
	 */
	if (read(io, (char *) &x, sizeof(x)) != sizeof(x))
		goto shread;
	magic = N_GETMAGIC(x);

	if ((magic != ZMAGIC) || (N_GETMID(x) != MID_MACHINE)) {
#ifdef DEBUG
		printf("invalid NetBSD kernel (%o/%ld)\n", magic, N_GETMID(x));
#endif
		errno = EFTYPE;
		goto closeout;
	}
	entry = x.a_entry & 0xffffff;

	if (!loadaddr)
		loadaddr = (entry & 0x100000);

	cp = loadaddr;

	extmem = getextmem();

#ifdef XMS
	if ((getextmem1() == 0) && (extmem = checkxms())) {
	        u_long kernsize;

		/* Approximate a size for the temporary buffer needed.
		 * Can't calculate the real size right now because we don't
		 * yet know the size of the stringtable. Overrunning the 
		 * temporary buffer doesn't break anything but DOS - which
		 * isn't needed at that time any more. Usualy it even doesn't
		 * harm DOS, because our buffer was the latest allocated and
		 * is followed by free memory. */
		kernsize = x.a_text + x.a_data + x.a_bss + sizeof(x.a_syms);
		if (x.a_syms > 0)
			kernsize += x.a_syms + sizeof(cc);
		/* add a little for the string table, convert to kB rounding
		 * up... */
		kernsize += (x.a_text + x.a_data) / 6;
		kernsize = (kernsize + 1023) / 1024;
		tmpbuf = xmsalloc(kernsize);
		if (!tmpbuf)
		    return(ENOMEM);
		origadr = cp;
		cp = tmpbuf;
	}
#endif

	/*
	 * Leave a copy of the exec header before the text.
	 * The kernel may use this to verify that the
	 * symbols were loaded by this boot program.
	 */
	vpbcopy(&x, cp, sizeof(x));
	cp += sizeof(x);
	/*
	 * Read in the text segment.
	 */

	printf("%ld", x.a_text);

	if (pread(io, cp, x.a_text - sizeof(x)) != x.a_text - sizeof(x))
		goto shread;
	cp += x.a_text - sizeof(x);

	/*
	 * Read in the data segment.
	 */

	printf("+%ld", x.a_data);

	if (pread(io, cp, x.a_data) != x.a_data)
		goto shread;
	cp += x.a_data;

	/*
	 * Zero out the BSS section.
	 * (Kernel doesn't care, but do it anyway.)
	 */


	printf("+%ld", x.a_bss);

	pbzero(cp, x.a_bss);
	cp += x.a_bss;

	/*
	 * Read in the symbol table and strings.
	 * (Always set the symtab size word.)
	 */
	vpbcopy(&x.a_syms, cp, sizeof(x.a_syms));
	cp += sizeof(x.a_syms);

	if (x.a_syms > 0) {

		/* Symbol table and string table length word. */

		printf("+[%ld", x.a_syms);

		if (pread(io, cp, x.a_syms) != x.a_syms)
			goto shread;
		cp += x.a_syms;

		read(io, &cc, sizeof(cc));

		vpbcopy(&cc, cp, sizeof(cc));
		cp += sizeof(cc);

		/* String table.  Length word includes itself. */

		printf("+%d]", cc);

		cc -= sizeof(int);
		if (cc <= 0)
			goto shread;
		if (pread(io, cp, cc) != cc)
			goto shread;
		cp += cc;
	}
#ifdef XMS
	if (tmpbuf) {
		/* cp points into tmpbuf, convert end address to physical address */
		boot_argv[3] = (((u_int) (cp - tmpbuf + loadaddr) + sizeof(int) - 1)) & (-sizeof(int));
		printf("=0x%lx\n", cp - tmpbuf);
	} else {
#endif
		boot_argv[3] = (((u_int) cp + sizeof(int) - 1)) & (-sizeof(int));
  		printf("=0x%lx\n", cp - loadaddr);
#ifdef XMS
	}
#endif

	boot_argv[0] = boothowto;

#ifdef COMPAT_OLDBOOT
	/* prepare boot device information for kernel */
	if (parsebootfile(file, &fsname, &devname, &unit, &part, &filename)
	    || strcmp(fsname, "ufs"))
		bootdevnr = 0;	/* XXX error out if parse error??? */
	else {
		int             major;

		if (strcmp(devname, "hd") == 0) {
			/* generic BIOS disk, have to guess type */
			struct open_file *f = &files[io];	/* XXX */

			if (biosdisk_gettype(f) == DTYPE_SCSI)
				devname = "sd";
			else
				devname = "wd";

			/*
			 * The old boot block performed the following
			 * conversion:
			 *
			 *	hdN -> Xd0
			 *
			 * where X is the type specified by the label.
			 * We mimmick that here, for lack of any better
			 * way of doing things.
			 */
			unit = 0;
		}

		if (dev2major(devname, &major))
			bootdevnr = 0;	/* XXX error out??? */
		else
			bootdevnr = MAKEBOOTDEV(major, 0, 0, unit, part);
	}

	boot_argv[1] = bootdevnr;
	boot_argv[2] = 0;	/* cyl offset, unused */
#else				/* XXX to be specified */
	xbootinfo[0] = vtophys(bootdev);
	xbootinfo[1] = vtophys(consdev);
	boot_argv[1] = 0;
	boot_argv[2] = vtophys(xbootinfo);	/* XXX cyl offset */
#endif
	/*
	 * boot_argv[3] = end (set above)
	 */
	boot_argv[4] = extmem;
	boot_argv[5] = getbasemem();

	close(io);

#ifdef DEBUG
	printf("Start @ 0x%lx ...\n", entry);
#endif

#ifdef XMS
	if (tmpbuf) {
		/*
		 * We know have done our last DOS IO, so we may
		 * trash the OS. Copy the data from the temporary
		 * buffer to its real adress.
		 */
		ppbcopy(tmpbuf, origadr, cp - tmpbuf);
	}
#endif

	startprog(entry, 6, boot_argv, 0x90000);
	panic("exec returned");

shread:
	printf("exec: short read\n");
	errno = EIO;
closeout:
	close(io);
	return (-1);
}
