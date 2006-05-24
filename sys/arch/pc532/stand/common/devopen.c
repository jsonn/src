/*	$NetBSD: devopen.c,v 1.4.8.1 2006/05/24 10:57:08 yamt Exp $	*/

/*-
 *  Copyright (c) 1993 John Brezak
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/reboot.h>

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <pc532/stand/common/samachdep.h>

static int	atoi(const char *);
static void	usage(void);
static int	devlookup(const char *, int);
static int	devparse(const char *, int *, int *, int *, int *, int *,
		    char **);

u_int opendev;

#define	ispart(c)	((c) >= 'a' && (c) <= 'h')

static int
atoi(const char *cp)
{
	int val = 0;

	while (isdigit(*cp))
		val = val * 10 + (*cp++ - '0');
	return(val);
}

static void
usage(void)
{

	printf("Usage: device(adaptor, controller, drive, partition)file\n");
	printf("       <device><unit><partitionletter>:file\n");
}

static int
devlookup(const char *d, int len)
{
	struct devsw *dp = devsw;
	int i;

	for (i = 0; i < ndevs; i++, dp++)
		if (dp->dv_name && strncmp(dp->dv_name, d, len) == 0)
			return(i);

	printf("No such device - Configured devices are:\n");
	for (dp = devsw, i = 0; i < ndevs; i++, dp++)
		if (dp->dv_name)
			printf(" %s", dp->dv_name);
	printf("\n");
	errno = ENODEV;
	return(-1);
}

/*
 * Parse a device spec in one of two forms.
 *
 * dev(adapt, ctlr, unit, part)file
 * [A-Za-z]*[0-9]*[A-Za-z]:file
 *    dev   unit  part
 */
static int
devparse(const char *fname, int *dev, int *adapt, int *ctlr, int *unit,
    int *part, char **file)
{
	int i;
	const char *s, *args[4];

	/* get device name and make lower case */
	for (s = fname; *s && *s != '/' && *s != ':' && *s != '('; s++)
		;

	if (*s == '(') {
		/* first form */
		/* lookup device and get index */
		if ((*dev = devlookup(fname, s - fname)) < 0)
			goto baddev;

		/* tokenize device ident */
		args[0] = ++s;
		for (i = 1; *s && *s != ')'; s++) {
			if (i > 3)
				goto baddev;
			if (*s == ',')
				args[i++] = ++s;
		}
		switch(i) {
		case 4:
			*adapt = atoi(args[0]);
			*ctlr  = atoi(args[1]);
			*unit  = atoi(args[2]);
			*part  = atoi(args[3]);
			break;
		case 3:
			*ctlr  = atoi(args[0]);
			*unit  = atoi(args[1]);
			*part  = atoi(args[2]);
			break;
		case 2:
			*unit  = atoi(args[0]);
			*part  = atoi(args[1]);
			break;
		case 1:
			*part  = atoi(args[0]);
			break;
		case 0:
			break;
		}
		*file = (char *)++s;
	} else if (*s == ':') {
		/* second form */
		/* isolate device */
		for (s = fname; *s != ':' && !isdigit(*s); s++)
			;

		/* lookup device and get index */
		if ((*dev = devlookup(fname, s - fname)) < 0)
			goto baddev;

		/* isolate unit */
		if ((*unit = atoi(s)) > ((1 << (sizeof(char) * 8)) - 1))
			goto bad;
		for (; isdigit(*s); s++)
			;

		/* translate partition */
		if (!ispart(*s))
			goto bad;

		*part = *s++ - 'a';
		if (*s != ':')
			goto bad;
		*file = (char *)++s;
	} else
		/* no device present */
		*file = (char *)fname;

	/* return the remaining unparsed part as the file to boot */
	return(0);

bad:
	usage();

baddev:
	return(-1);
}

int
devopen(struct open_file *f, const char *fname, char **file)
{
	int error, dev, adapt, ctlr, unit, part;
	struct devsw *dp = &devsw[0];

	dev   = B_TYPE(bootdev);
	adapt = B_ADAPTOR(bootdev);
	ctlr  = B_CONTROLLER(bootdev);
	unit  = B_UNIT(bootdev);
	part  = B_PARTITION(bootdev);

	if ((error = devparse(fname, &dev, &adapt, &ctlr, &unit, &part, file))
	    != 0)
		return(error);

	dp = &devsw[dev];

	if (!dp->dv_open)
		return(ENODEV);

	f->f_dev = dp;

	if ((error = (*dp->dv_open)(f, ctlr, unit, part)) == 0) {
		opendev = MAKEBOOTDEV(dev, adapt, ctlr, unit, part);
		return(0);
	}

	printf("%s(%d,%d,%d,%d): %s\n", devsw[dev].dv_name,
		adapt, ctlr, unit, part, strerror(error));

	return(error);
}
