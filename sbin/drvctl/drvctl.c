/* $NetBSD: drvctl.c,v 1.6 2008/01/27 01:38:33 dyoung Exp $ */

/*
 * Copyright (c) 2004
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/drvctlio.h>

#define OPTS "QRSa:dlpr"

#define	OPEN_MODE(mode)							\
	(((mode) == 'd' || (mode) == 'r') ? O_RDWR			\
					  : O_RDONLY)

static void usage(void);

static void
usage(void)
{

	fprintf(stderr, "Usage: %s -r [-a attribute] busdevice [locator ...]\n"
	    "       %s -d device\n"
	    "       %s -l device\n"
	    "       %s -p device\n"
	    "       %s -Q device\n"
	    "       %s -R device\n"
	    "       %s -S device\n",
	    getprogname(), getprogname(), getprogname(), getprogname(),
	    getprogname(), getprogname(), getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	int c, mode;
	char *attr = 0;
	extern char *optarg;
	extern int optind;
	int fd, res;
	size_t children;
	struct devpmargs paa = {.devname = "", .flags = 0};
	struct devlistargs laa = {.l_devname = "", .l_childname = NULL,
				  .l_children = 0};
	struct devdetachargs daa;
	struct devrescanargs raa;
	int *locs, i;
	prop_dictionary_t command_dict, args_dict, results_dict,
			  data_dict;
	prop_string_t string;
	prop_number_t number;
	char *xml;

	mode = 0;
	while ((c = getopt(argc, argv, OPTS)) != -1) {
		switch (c) {
		case 'Q':
		case 'R':
		case 'S':
		case 'd':
		case 'l':
		case 'p':
		case 'r':
			mode = c;
			break;
		case 'a':
			attr = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1 || mode == 0)
		usage();

	fd = open(DRVCTLDEV, OPEN_MODE(mode), 0);
	if (fd < 0)
		err(2, "open %s", DRVCTLDEV);

	switch (mode) {
	case 'Q':
		paa.flags = DEVPM_F_SUBTREE;
		/*FALLTHROUGH*/
	case 'R':
		strlcpy(paa.devname, argv[0], sizeof(paa.devname));

		if (ioctl(fd, DRVRESUMEDEV, &paa) == -1)
			err(3, "DRVRESUMEDEV");
		break;
	case 'S':
		strlcpy(paa.devname, argv[0], sizeof(paa.devname));

		if (ioctl(fd, DRVSUSPENDDEV, &paa) == -1)
			err(3, "DRVSUSPENDDEV");
		break;
	case 'd':
		strlcpy(daa.devname, argv[0], sizeof(daa.devname));

		if (ioctl(fd, DRVDETACHDEV, &daa) == -1)
			err(3, "DRVDETACHDEV");
		break;
	case 'l':
		strlcpy(laa.l_devname, argv[0], sizeof(laa.l_devname));

		if (ioctl(fd, DRVLISTDEV, &laa) == -1)
			err(3, "DRVLISTDEV");

		children = laa.l_children;

		laa.l_childname = malloc(children * sizeof(laa.l_childname[0]));
		if (laa.l_childname == NULL)
			err(5, "DRVLISTDEV");
		if (ioctl(fd, DRVLISTDEV, &laa) == -1)
			err(3, "DRVLISTDEV");
		if (laa.l_children > children)
			err(6, "DRVLISTDEV: number of children grew");

		for (i = 0; i < laa.l_children; i++)
			printf("%s %s\n", laa.l_devname, laa.l_childname[i]);
		break;
	case 'r':
		memset(&raa, 0, sizeof(raa));
		strlcpy(raa.busname, argv[0], sizeof(raa.busname));
		if (attr)
			strlcpy(raa.ifattr, attr, sizeof(raa.ifattr));
		if (argc > 1) {
			locs = malloc((argc - 1) * sizeof(int));
			if (!locs)
				err(5, "malloc int[%d]", argc - 1);
			for (i = 0; i < argc - 1; i++)
				locs[i] = atoi(argv[i + 1]);
			raa.numlocators = argc - 1;
			raa.locators = locs;
		}

		if (ioctl(fd, DRVRESCANBUS, &raa) == -1)
			err(3, "DRVRESCANBUS");
		break;
	case 'p':

		command_dict = prop_dictionary_create();
		args_dict = prop_dictionary_create();

		string = prop_string_create_cstring_nocopy("get-properties");
		prop_dictionary_set(command_dict, "drvctl-command", string);
		prop_object_release(string);

		string = prop_string_create_cstring(argv[0]);
		prop_dictionary_set(args_dict, "device-name", string);
		prop_object_release(string);

		prop_dictionary_set(command_dict, "drvctl-arguments",
				    args_dict);
		prop_object_release(args_dict);

		res = prop_dictionary_sendrecv_ioctl(command_dict, fd,
						     DRVCTLCOMMAND,
						     &results_dict);
		prop_object_release(command_dict);
		if (res)
			errx(3, "DRVCTLCOMMAND: %s", strerror(res));

		number = prop_dictionary_get(results_dict, "drvctl-error");
		if (prop_number_integer_value(number) != 0) {
			errx(3, "get-properties: %s",
			    strerror((int)prop_number_integer_value(number)));
		}

		data_dict = prop_dictionary_get(results_dict,
						"drvctl-result-data");
		if (data_dict == NULL) {
			errx(3, "get-properties: failed to return result data");
		}

		xml = prop_dictionary_externalize(data_dict);
		prop_object_release(results_dict);

		printf("Properties for device `%s':\n%s",
		       argv[0], xml);
		free(xml);
		break;
	default:
		errx(4, "unknown command");
	}

	return (0);
}
