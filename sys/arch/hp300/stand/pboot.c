/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)boot.c	8.1 (Berkeley) 6/10/93
 *	     $Id: pboot.c,v 1.1.2.1 1994/09/20 05:07:43 cgd Exp $
 */

#ifndef lint
static char rcsid[] = "$Id: pboot.c,v 1.1.2.1 1994/09/20 05:07:43 cgd Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/reboot.h>
#include <a.out.h>
#include "stand.h"
#include "samachdep.h"

/*
 * Boot program... bits in `howto' determine whether boot stops to
 * ask for system name.	 Boot device is derived from ROM provided
 * information.
 */

extern	unsigned opendev;
extern	char *lowram;
extern	int noconsole;

char *ssym, *esym;

char *name;
char *names[] = {
	"/netbsd", "/onetbsd", "/netbsd.old",
};
#define NUMNAMES	(sizeof(names)/sizeof(char *))

static int bdev, badapt, bctlr, bunit, bpart;

main()
{
	int currname = 0;
	int io;

	printf("\n>> NetBSD BOOT HP9000/%s CPU [%s]\n",
	       getmachineid(), "$Revision: 1.1.2.1 $");

	bdev   = B_TYPE(bootdev);
	badapt = B_ADAPTOR(bootdev);
	bctlr  = B_CONTROLLER(bootdev);
	bunit  = B_UNIT(bootdev);
	bpart  = B_PARTITION(bootdev);

	for (;;) {
		name = names[currname++];
		if (currname == NUMNAMES)
		    currname = 0;

		if (!noconsole) {
		    howto = 0;
		    getbootdev(&howto);
		}
		else
		    printf(": %s\n", name);

		io = open(name, 0);
		if (io >= 0) {
			copyunix(howto, opendev, io);
			close(io);
		}
		else
		    printf("boot: %s\n", strerror(errno));
	}
}

/*ARGSUSED*/
copyunix(howto, devtype, io)
	register int howto;	/* d7 contains boot flags */
	register u_int devtype;	/* d6 contains boot device */
	register int io;
{
	struct exec x;
	int i;
	register char *load;	/* a5 contains load addr for unix */
	register char *addr;
	int dev, adapt, ctlr, unit, part;

	dev   = B_TYPE(opendev);
	adapt = B_ADAPTOR(opendev);
	ctlr  = B_CONTROLLER(opendev);
	unit  = B_UNIT(opendev);
	part  = B_PARTITION(opendev);
	
	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) ||
	    N_BADMAG(x)) {
		printf("Bad format\n");
		return;
	}
	printf("Booting %s%d%c:%s @ 0x%x\n",
	    devsw[dev].dv_name, ctlr + (8 * adapt), 'a' + part, name, x.a_entry);

	/* Text */
	printf("%d", x.a_text);
	if (N_GETMAGIC(x) == ZMAGIC && lseek(io, 0x400, SEEK_SET) == -1)
		goto shread;
	load = addr = lowram;
	if (read(io, (char *)addr, x.a_text) != x.a_text)
		goto shread;
	addr += x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((int)addr & CLOFSET)
			*addr++ = 0;
	/* Data */
	printf("+%d", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;

	/* Bss */
	printf("+%d", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;

	/* Symbols */
	ssym = addr;
	bcopy(&x.a_syms, addr, sizeof(x.a_syms));
	addr += sizeof(x.a_syms);
	printf(" [%d+", x.a_syms);
	if (read(io, addr, x.a_syms) != x.a_syms)
		goto shread;
	addr += x.a_syms;

	if (read(io, &i, sizeof(int)) != sizeof(int))
		goto shread;

	bcopy(&i, addr, sizeof(int));
	if (i) {
		i -= sizeof(int);
		addr += sizeof(int);
		if (read(io, addr, i) != i)
		    goto shread;
		addr += i;
	}

	/* and that many bytes of (debug symbols?) */

	printf("%d] ", i);

#define	round_to_size(x) \
	(((int)(x) + sizeof(int) - 1) & ~(sizeof(int) - 1))
	esym = (char *)round_to_size(addr - lowram);
#undef round_to_size

	/* and note the end address of all this	*/
	printf("total=0x%x ", addr);

	x.a_entry += (int)lowram;
	printf(" start 0x%x\n", x.a_entry);

#ifdef DEBUG
	printf("ssym=0x%x esym=0x%x\n", ssym, esym);
	printf("\n\nReturn to boot...\n");
	getchar();
#endif

#ifdef __GNUC__
	asm("	movl %0,d7" : : "m" (howto));
	asm("	movl %0,d6" : : "m" (devtype));
	asm("	movl %0,a5" : : "a" (load));
	asm("	movl %0,a4" : : "a" (esym));
#endif
	(*((int (*)()) x.a_entry))();
	return;
shread:
	printf("Short read\n");
	return;
}

char line[100];

getbootdev(howto)
     int *howto;
{
	char c, *ptr = line;

	printf("Boot: [[[%s%d%c:]%s][-s][-a][-d]] :- ",
	    devsw[bdev].dv_name, bctlr + (8 * badapt), 'a' + bpart, name);

	if (tgets(line)) {
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
