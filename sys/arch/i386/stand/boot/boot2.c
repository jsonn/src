/*	$NetBSD: boot2.c,v 1.1.2.3 2004/09/21 13:17:10 skrll Exp $	*/

/*
 * Copyright (c) 2003
 *	David Laight.  All rights reserved
 * Copyright (c) 1996, 1997, 1999
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Perry E. Metzger.  All rights reserved.
 * Copyright (c) 1997
 *	Jason R. Thorpe.  All rights reserved
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
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
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

/* Based on stand/biosboot/main.c */

#include <sys/reboot.h>
#include <sys/bootblock.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/ufs.h>
#include <lib/libkern/libkern.h>

#include <libi386.h>
#include "devopen.h"

#ifdef SUPPORT_PS2
#include <biosmca.h>
#endif

int errno;
extern int boot_biosdev;
extern int boot_biossector;	/* may be wrong... */

extern struct x86_boot_params boot_params;

extern	const char bootprog_name[], bootprog_rev[], bootprog_date[],
	bootprog_maker[];

static const char * const names[][2] = {
    { "netbsd", "netbsd.gz" },
    { "netbsd.old", "netbsd.old.gz" },
    { "onetbsd", "onetbsd.gz" },
#ifdef notyet
    { "netbsd.el", "netbsd.el.gz" },
#endif /*notyet*/
};

#define NUMNAMES (sizeof(names)/sizeof(names[0]))
#define DEFFILENAME names[0][0]

#define MAXDEVNAME 16

static char *default_devname;
static int default_unit, default_partition;
static const char *default_filename;

char *sprint_bootsel(const char *);
void bootit(const char *, int, int);
void print_banner(void);
void boot2(uint32_t, uint32_t);

void	command_help(char *);
void	command_ls(char *);
void	command_quit(char *);
void	command_boot(char *);
void	command_dev(char *);
void	command_consdev(char *);

const struct bootblk_command commands[] = {
	{ "help",	command_help },
	{ "?",		command_help },
	{ "ls",		command_ls },
	{ "quit",	command_quit },
	{ "boot",	command_boot },
	{ "dev",	command_dev },
	{ "consdev",	command_consdev },
	{ NULL,		NULL },
};

int
parsebootfile(const char *fname, char **fsname, char **devname,
	u_int *unit, u_int *partition, const char **file)
{
	const char *col;

	*fsname = "ufs";
	*devname = default_devname;
	*unit = default_unit;
	*partition = default_partition;
	*file = default_filename;

	if (fname == NULL)
		return(0);

	if((col = strchr(fname, ':'))) {	/* device given */
		static char savedevname[MAXDEVNAME+1];
		int devlen;
		unsigned int u = 0, p = 0;
		int i = 0;

		devlen = col - fname;
		if (devlen > MAXDEVNAME)
			return(EINVAL);

#define isvalidname(c) ((c) >= 'a' && (c) <= 'z')
		if (!isvalidname(fname[i]))
			return(EINVAL);
		do {
			savedevname[i] = fname[i];
			i++;
		} while (isvalidname(fname[i]));
		savedevname[i] = '\0';

#define isnum(c) ((c) >= '0' && (c) <= '9')
		if (i < devlen) {
			if (!isnum(fname[i]))
				return(EUNIT);
			do {
				u *= 10;
				u += fname[i++] - '0';
			} while (isnum(fname[i]));
		}

#define isvalidpart(c) ((c) >= 'a' && (c) <= 'z')
		if (i < devlen) {
			if (!isvalidpart(fname[i]))
				return(EPART);
			p = fname[i++] - 'a';
		}

		if (i != devlen)
			return(ENXIO);

		*devname = savedevname;
		*unit = u;
		*partition = p;
		fname = col + 1;
	}

	if (*fname)
		*file = fname;

	return(0);
}

char *
sprint_bootsel(const char *filename)
{
	char *fsname, *devname;
	int unit, partition;
	const char *file;
	static char buf[80];

	if (parsebootfile(filename, &fsname, &devname, &unit,
			  &partition, &file) == 0) {
		sprintf(buf, "%s%d%c:%s", devname, unit, 'a' + partition, file);
		return(buf);
	}
	return("(invalid)");
}

void
bootit(const char *filename, int howto, int tell)
{

	if (tell) {
		printf("booting %s", sprint_bootsel(filename));
		if (howto)
			printf(" (howto 0x%x)", howto);
		printf("\n");
	}

	if (exec_netbsd(filename, 0, howto) < 0)
		printf("boot: %s: %s\n", sprint_bootsel(filename),
		       strerror(errno));
	else
		printf("boot returned\n");
}

void
print_banner(void)
{

	printf("\n");
	printf(">> %s, Revision %s\n", bootprog_name, bootprog_rev);
	printf(">> (%s, %s)\n", bootprog_maker, bootprog_date);
	printf(">> Memory: %d/%d k\n", getbasemem(), getextmem());
}


void
boot2(uint32_t boot_biosdev, uint32_t boot_biossector)
{
	int currname;
	char c;

	initio(boot_params.bp_consdev);

#ifdef SUPPORT_PS2
	biosmca();
#endif
	gateA20();

	if (boot_params.bp_flags & X86_BP_FLAGS_RESET_VIDEO)
		biosvideomode();

	print_banner();

	/* try to set default device to what BIOS tells us */
	bios2dev(boot_biosdev, &default_devname, &default_unit,
		boot_biossector, &default_partition);

	/* if the user types "boot" without filename */
	default_filename = DEFFILENAME;

	printf("Press return to boot now, any other key for boot menu\n");
	currname = 0;
	for (;;) {
		printf("booting %s - starting in ",
		       sprint_bootsel(names[currname][0]));

		c = awaitkey(boot_params.bp_timeout, 1);
		if ((c != '\r') && (c != '\n') && (c != '\0') &&
		    ((boot_params.bp_flags & X86_BP_FLAGS_PASSWORD) == 0
		    || check_password(boot_params.bp_password))) {
			printf("type \"?\" or \"help\" for help.\n");
			bootmenu(); /* does not return */
		}

		/*
		 * try pairs of names[] entries, foo and foo.gz
		 */
		/* don't print "booting..." again */
		bootit(names[currname][0], 0, 0);
		/* since it failed, try compressed bootfile. */
		bootit(names[currname][1], 0, 1);
		/* since it failed, try switching bootfile. */
		currname = (currname + 1) % NUMNAMES;
	}
}

/* ARGSUSED */
void
command_help(char *arg)
{

	printf("commands are:\n"
	    "boot [xdNx:][filename] [-acdqsv]\n"
	    "     (ex. \"hd0a:netbsd.old -s\"\n"
	    "ls [path]\n"
	    "dev xd[N[x]]:\n"
	    "consdev {pc|com[0123]|com[0123]kbd|auto}\n"
	    "help|?\n"
	    "quit\n");
}

void
command_ls(char *arg)
{
	const char *save = default_filename;

	default_filename = "/";
	ufs_ls(arg);
	default_filename = save;
}

/* ARGSUSED */
void
command_quit(char *arg)
{

	printf("Exiting...\n");
	delay(1000000);
	reboot();
	/* Note: we shouldn't get to this point! */
	panic("Could not reboot!");
	exit(0);
}

void
command_boot(char *arg)
{
	char *filename;
	int howto;

	if (parseboot(arg, &filename, &howto))
		bootit(filename, howto, 1);
}

void
command_dev(char *arg)
{
	static char savedevname[MAXDEVNAME + 1];
	char *fsname, *devname;
	const char *file; /* dummy */

	if (*arg == '\0') {
		printf("%s%d%c:\n", default_devname, default_unit,
		       'a' + default_partition);
		return;
	}

	if (!strchr(arg, ':') ||
	    parsebootfile(arg, &fsname, &devname, &default_unit,
			  &default_partition, &file)) {
		command_help(NULL);
		return;
	}

	/* put to own static storage */
	strncpy(savedevname, devname, MAXDEVNAME + 1);
	default_devname = savedevname;
}

static const struct cons_devs {
    const char	*name;
    u_int	tag;
} cons_devs[] = {
	{ "pc",		CONSDEV_PC },
	{ "com0",	CONSDEV_COM0 },
	{ "com1",	CONSDEV_COM1 },
	{ "com2",	CONSDEV_COM2 },
	{ "com3",	CONSDEV_COM3 },
	{ "com0kbd",	CONSDEV_COM0KBD },
	{ "com1kbd",	CONSDEV_COM1KBD },
	{ "com2kbd",	CONSDEV_COM2KBD },
	{ "com3kbd",	CONSDEV_COM3KBD },
	{ "auto",	CONSDEV_AUTO },
	{ 0, 0 } };

void
command_consdev(char *arg)
{
	const struct cons_devs *cdp;

	for (cdp = cons_devs; cdp->name; cdp++) {
		if (!strcmp(arg, cdp->name)) {
			initio(cdp->tag);
			print_banner();
			return;
		}
	}
	printf("invalid console device.\n");
}
