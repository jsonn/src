/*	$NetBSD: veriexecctl.c,v 1.5.6.1 2005/06/10 14:47:43 tron Exp $	*/

/*-
 * Copyright 2005 Elad Efrat <elad@bsd.org.il>
 * Copyright 2005 Brett Lymn <blymn@netbsd.org> 
 *
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *
 *
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/verified_exec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "veriexecctl.h"

#define	VERIEXEC_DEVICE	"/dev/veriexec"

#define	usage(code)	errx(code, "Usage: %s [-v] [load <signature_file>] " \
			     "[fingerprints]", \
			    getprogname())

extern struct veriexec_params params; /* in veriexecctl_parse.y */
extern char *filename; /* in veriexecctl_conf.l */
int fd, verbose = 0, no_mem = 0, phase;
unsigned line;

/*
 * Prototypes
 */
int
fingerprint_load(char *infile);

FILE *
openlock(const char *path)
{
	int fd;

	fd = open(path, O_RDONLY|O_EXLOCK, 0);
	if (fd < 0)
		return (NULL);

	return (fdopen(fd, "r"));
}

struct vexec_up *
dev_lookup(dev_t d)
{
	struct vexec_up *p;

	CIRCLEQ_FOREACH(p, &params_list, vu_list) {
		if (p->vu_param.dev == d)
			return (p);
	}

	return (NULL);
}

struct vexec_up *
dev_add(dev_t d)
{
	struct vexec_up *up;

	up = calloc(1, sizeof(*up));
	if (up == NULL)
		err(1, "No memory");

	up->vu_param.dev = d;
	up->vu_param.hash_size = 1;

	CIRCLEQ_INSERT_TAIL(&params_list, up, vu_list);

	return (up);
}

/* Load all devices, get rid of the list. */
int
phase1_preload(void)
{
	if (verbose)
		printf("Phase 1: Calculating hash table sizes:\n");

	while (!CIRCLEQ_EMPTY(&params_list)) {
		struct vexec_up *vup;

		vup = CIRCLEQ_FIRST(&params_list);

		if (ioctl(fd, VERIEXEC_TABLESIZE, &(vup->vu_param)) < 0) {
			(void) fprintf(stderr, "Error in phase 1: Can't "
			    "set hash table size for device %d: %s.\n",
			    vup->vu_param.dev, strerror(errno));

			return (-1);
		}

		if (verbose) {
			printf(" => Hash table sizing successful for device "
			    "%d. (%u entries)\n", vup->vu_param.dev,
			    vup->vu_param.hash_size);
		}

		CIRCLEQ_REMOVE(&params_list, vup, vu_list);
		free(vup);
	}

	return (0);
}

/*
 * Load the fingerprint. Assumes that the fingerprint pseudo-device is
 * opened and the file handle is in fd.
 */
void
phase2_load(void)
{
	if (ioctl(fd, VERIEXEC_LOAD, &params) < 0) {
		(void) fprintf(stderr, "%s: %s\n", params.file,
			       strerror(errno));
	}
	free(params.fingerprint);
}

/*
 * Fingerprint load handling.
 */
int
fingerprint_load(char *infile)
{
	CIRCLEQ_INIT(&params_list);

	if ((yyin = openlock(infile)) == NULL) {
		err(1, "Failed to open %s", infile);
	}

	/*
	 * Phase 1: Scan all config files, creating the list of devices
	 *	    we have fingerprinted files on, and the amount of
	 *	    files per device. Lock all files to maintain sync.
	 */
	phase = 1;

	if (verbose) {
		(void) fprintf(stderr, "Phase 1: Building hash table information:\n");
		(void) fprintf(stderr, "=> Parsing \"%s\"\n", infile);
	}

	yyparse();

	if (phase1_preload() < 0)
		exit(1);

	/*
	 * Phase 2: After we have a circular queue containing all the
	 * 	    devices we care about and the sizes for the hash
	 *	    tables, do a rescan, this time actually loading the
	 *	    file data.
	 */
	rewind(yyin);
	phase = 2;
	if (verbose) {
		(void) fprintf(stderr, "Phase 2: Loading per-file "
			       "fingerprints.\n");
		(void) fprintf(stderr, "=> Parsing \"%s\"\n", infile);
	}

	yyparse();

	(void) fclose(yyin);
	
	return(0);
}

int
main(int argc, char **argv)
{
	char *newp;
	int c;
	unsigned size;
	struct veriexec_fp_report report;

	if ((argc < 2) || (argc > 4)) {
		usage(1);
	}

	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
		case 'v':
			verbose = 1;
			break;

		default:
			usage(1);
		}
	}

	argc -= optind;
	argv += optind;

	fd = open(VERIEXEC_DEVICE, O_RDWR, 0);
	if (fd == -1) {
		err(1, "Failed to open pseudo-device");
	}

	  /*
	   * Handle the different commands we can do.
	   */
	if (strcasecmp(argv[0], "load") == 0) {
		line = 0;
		filename = argv[1];
		fingerprint_load(argv[1]);
	} else if (strcasecmp(argv[0], "fingerprints") == 0) {
		size = report.size = 100;
		if ((report.fingerprints = malloc(report.size)) == NULL) {
			fprintf(stderr, "fingerprints: malloc failed.\n");
			exit(1);
		}
		
		if (ioctl(fd, VERIEXEC_FINGERPRINTS, &report) == 0) {
			if (size != report.size) {
				if (verbose)
					fprintf(stderr, "fingerprints: "
						"buffer insufficient, "
						"reallocating to %d bytes.\n",
						report.size);
				
				/* fingerprint store was not large enough
				   make more room and try again. */
				if ((newp = realloc(report.fingerprints,
						    report.size)) == NULL) {
					fprintf(stderr, "fingerprints: "
						"realloc failed\n");
					exit(1);
				}
				if (ioctl(fd, VERIEXEC_FINGERPRINTS,
					  &report) < 0) {
					fprintf(stderr,
						"fingerprints ioctl: %s\n",
						strerror(errno));
					exit(1);
				}
			}
		} else {
			(void) fprintf(stderr,
				       "fingerprints ioctl: %s\n",
				       strerror(errno));
			exit(1);
		}

		printf("Supported fingerprints: %s\n", report.fingerprints);
	} else {
		usage(1);
	}

	(void) close(fd);
	return (0);
}
