/* $NetBSD: audit-packages.c,v 1.1.1.1.4.2 2008/10/19 22:40:49 haad Exp $ */

/*
 * Copyright (c) 2007 Adrian Portelli <adrianp@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. Neither the name of author(s) nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <nbcompat.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#if defined(HAVE_ERR_H) || !defined(PKGSRC)
#include <err.h>
#else
#include <nbcompat/err.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

/* depending where we are being built pull in crypto support */
#ifdef PKGSRC
#include <nbcompat/sha2.h>
#else
#include <sha2.h>
#endif

#include "lib.h"

#ifndef	PATH_MAX
#  ifdef MAXPATHLEN
#    define PATH_MAX	MAXPATHLEN
#  else
#    define PATH_MAX	1024
#  endif
#endif

/* NetBSD has a special layout as it is included in the base OS */
#ifdef NETBSD
#  define PREFIX	"/usr"
#  define PKGPREFIX	"/usr/pkg"
#  define SYSCONFDIR	"/etc"
#else
#  define PKGPREFIX	PREFIX
#endif

/* macros */
#define STRIP(c) 	((c) == '\n' || (c) == '\'' || (c) == '\"' || \
			 (c) == '\t' || (c) == ' ' || (c) == '=')

/* default for src/ install */
const char DVL_BIN[] = PREFIX"/sbin/download-vulnerability-list";
const char EOL_URL[] = "ftp://ftp.NetBSD.org/pub/NetBSD/packages/vulns";

const int MSGSIZE = 1024;			/* max message size */
const int MAXLINELEN = 4092;			/* max line length */
const int MAXPKGNAMELEN = 1024;			/* max pkg name */
const int FORMAT[] = {1, 1, 0};			/* file format ver */
const int MAXVERBOSE = 3;			/* max verbosity */

/* globals (from config file) */
char *verify_bin = PKGPREFIX"/bin/gpg";		/* verify bin location */
char *pvfile = NULL;				/* p-v file location */
char *pvdir = NULL;				/* p-v dir location */
char *ignore = NULL;				/* ignore urls */

/* globals */
char *conf_file = SYSCONFDIR"/audit-packages.conf"; /* config file location */
char *program_name;				/* the program name */

/* program defaults */
int verbose = 0;				/* be quiet */
Boolean eol = FALSE;				/* don't check eol */
Boolean quiet = FALSE;				/* display full data */

int main(int, char **);
static void *safe_calloc(size_t, size_t);
static char *checkforpkg(const char *);
static void usage(void);
static int dvl(void);
static void old_pvfile(void);
static void pv_format(FILE *);
static char *gen_hash(char *);
static char *get_hash(char *);
static int check_hash(char *);
static int check_sig(char *);
static int pv_message(char *[], char *);
static int ap_ignore(char *[]);
static void show_info(char *);
static void set_pvfile(const char *);
static char *clean_conf(char *);
static int get_confvalues(void);
static char *safe_strdup(const char *);
static int checkforvuln(FILE *, char *, Boolean, char *, Boolean);
static char *trim_r(char *);

/*
 * TODO:
 *
 * built in gz/bzip2 support
 * merge download-vulnerability-list(1)
 *
 */

/*
 * get the options for what we are doing, and do the actual processing of
 * the pkg-vulnerabilities file
 */
int
main(int argc, char **argv)
{
	char *bpkg = NULL;
	char *bpkg_ptr = NULL;
	char *pkg_type = NULL;
	char *check_hash_file = NULL;
	char *gen_hash_file = NULL;
	char *hash_generated = NULL;
	char *query_var = NULL;
	char *pkgname = NULL;
	char *bulk_file = NULL;

	int ch = 0;
	int retval = -1;

	Boolean download = FALSE;
	Boolean pkg_installed = FALSE;
	Boolean verify_sig = FALSE;
	Boolean check_one = FALSE;
	Boolean type = FALSE;
	Boolean cli_check_hash = FALSE;
	Boolean cli_gen_hash = FALSE;
	Boolean info = FALSE;
	Boolean bulk = FALSE;
	Boolean vuln_found = FALSE;

	FILE *pv, *bf;

	program_name = argv[0];

	setprogname(program_name);

	set_pvfile(_pkgdb_getPKGDB_DIR());

	opterr = 0;

	while ((ch = getopt(argc, argv, ":dveqK:n:h:g:c:p:st:F:Q:V")) != -1) {

		switch (ch) {

		case 'h':
			check_hash_file = optarg;
			cli_check_hash = TRUE;
			break;

		case 'g':
			gen_hash_file = optarg;
			cli_gen_hash = TRUE;
			break;

		case 'd':
			download = TRUE;
			break;

		case 'e':
			eol = TRUE;
			break;

		case 'K':
			_pkgdb_setPKGDB_DIR(optarg);
			break;

		case 'n':
			pkgname = optarg;
			check_one = TRUE;
			pkg_installed = FALSE;
			break;

		case 'c':
			conf_file = optarg;
			break;

		case 'p':
			pkgname = optarg;
			check_one = TRUE;
			pkg_installed = TRUE;
			break;

		case 'q':
			quiet = TRUE;
			break;

		case 'F':
			bulk_file = optarg;
			bulk = TRUE;
			break;

		case 's':
			verify_sig = TRUE;
			break;

		case 't':
			pkg_type = optarg;
			type = TRUE;
			break;

		case 'v':
			if (verbose <= MAXVERBOSE)
				++verbose;
			break;

		case 'Q':
			query_var = optarg;
			info = TRUE;
			break;

		case 'V':
			show_version();
			/* not reached */

		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argc += optind;

	/*
	 * generate the hash for a specified file (-g <file>)
	 *
	 * this is purely for download-vulnerability-list, users
	 * should not be directly calling audit-packages with -g <file>.
	 */
	if (cli_gen_hash == TRUE) {

		hash_generated = gen_hash(gen_hash_file);
		fprintf(stdout, "%s\n", hash_generated);

		exit(EXIT_SUCCESS);
	}

	/*
	 * check the hash and/or sig for a specified file
	 *
	 * if -h <file> is given then just the hash is checked
	 * but if -s -h <file> are given then both the hash and the
	 * sig are checked.  this is purely for
	 * download-vulnerability-list, users should not be directly
	 * calling audit-packages with -h <file> or -s -h <file>.
	 */
	if (cli_check_hash == TRUE) {

		retval = check_hash(check_hash_file);

		if (retval != 0) {
			exit(EXIT_FAILURE);
		} else {
			if (verify_sig == TRUE) {
				retval = check_sig(check_hash_file);

				if (retval != 0) {
					exit(EXIT_FAILURE);
				} else {
					exit(EXIT_SUCCESS);
				}
			} else {
				exit(EXIT_SUCCESS);
			}
		}
	}

	/* get the config file values */
	retval = get_confvalues();

	/* if we found some IGNORE_URLS lines */
	if (verbose >= 2) {
		fprintf(stderr, "debug2: Using PKGDB_DIR: %s\n", _pkgdb_getPKGDB_DIR());
		fprintf(stderr, "debug2: Using pkg-vulnerabilities file: %s\n", pvfile);
		fprintf(stderr, "debug2: Using verify tool: %s\n", verify_bin);

		if (ignore != NULL) {
			fprintf(stderr, "debug2: Using ignore directives: %s\n", ignore);
		} else {
			fprintf(stderr, "debug2: No ignore directives found.\n");
		}
	}

	/* now that we have read in the config file we can show the info */
	if (info == TRUE) {
		show_info(query_var);
		exit(EXIT_SUCCESS);
	}

	/* we need to download the file first and check it went ok */
	if (download == TRUE) {
		retval = dvl();

		if (retval != 0)
			exit(EXIT_FAILURE);
	}

	/* open pvfile */
	if ((pv = fopen(pvfile, "r")) == NULL) {
		errx(EXIT_FAILURE, "Unable to open: %s", pvfile);
	}

	/* check for an old vulnerabilities file if we're being verbose */
	if ((verbose >= 1) && (download == FALSE))
		old_pvfile();

	/* check the #FORMAT from the pkg-vulnerabilities file */
	pv_format(pv);

	rewind(pv);

	/* check the hashes */
	retval = check_hash(pvfile);

	if (retval != 0) {
		errx(EXIT_FAILURE, "Hash mismatch.");
	} else {
		if (verbose >= 2)
			fprintf(stderr, "debug2: Hash match.\n");
	}

	/* do signature checking - if required */
	if (verify_sig == TRUE) {
		retval = check_sig(pvfile);

		if (retval != 0) {
			errx(EXIT_FAILURE, "Signature verification failure.");
		}
	}

	/*
	 * this is for -p:
	 *  check a specific installed package for vulnerabilities
	 *  if we find that it's not installed, just exit silently
	 */
	if ((pkg_installed == TRUE) && (check_one == TRUE)) {
		if ((checkforpkg(pkgname)) != NULL) {
			if (verbose >= 3)
				fprintf(stderr, "debug3: Package found to be installed (-p): %s\n", pkgname);
		} else {
			if (verbose >= 3)
				fprintf(stderr, "debug3: Package not found to be installed (-p): %s\n", pkgname);
			exit(EXIT_SUCCESS);
		}
	}

	/*
	 * this is for -n:
	 *  check a specific package for vulnerabilities
	 *  here we don't care if it's installed or not
	 */
	if ((pkg_installed == FALSE) && (check_one == TRUE)) {
		if (verbose >= 3)
			fprintf(stderr, "debug3: Looking for package (-n): %s\n", pkgname);
	}

	rewind(pv);

	/* check a package for vulnerabilities */
	if (bulk == FALSE) {
		retval = checkforvuln(pv, pkgname, type, pkg_type, check_one);

		if (retval != 0)
			vuln_found = TRUE;
	} else {
		check_one = TRUE;

		if ((bf = fopen(bulk_file, "r")) == NULL) {
			errx(EXIT_FAILURE, "Unable to open: %s", bulk_file);
		}

		bpkg = safe_calloc(MAXLINELEN, sizeof(char));

		while ((bpkg_ptr = fgets(bpkg, MAXLINELEN, bf)) != NULL) {

			/* what we're not interested in */
			if ((bpkg[0] == '#') || (bpkg[0] == '\n'))
				continue;

			bpkg = trim_r(bpkg);

			retval = checkforvuln(pv, bpkg, type, pkg_type, check_one);
			if (retval != 0)
				vuln_found = TRUE;
		}

		free(bpkg);

		/* bail if ferror is set */
		if (ferror(bf) != 0)
			errx(EXIT_FAILURE, "Unable to read: %s", bulk_file);

		fclose(bf);
	}

	fclose(pv);

	if ((verbose >= 1) && (vuln_found == FALSE))
		fprintf(stderr, "No vulnerable packages found.\n");

	if (vuln_found == FALSE) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}

/* end main() */

/*
 * check a given pattern/package for a hit in the pkg-vulnerabilities file
 */
static int
checkforvuln(FILE *vuln_file, char *package, Boolean type, char *pkg_type, Boolean check_one)
{
	Boolean vuln_found = FALSE;

	char *line = NULL;
	char *line_tmp = NULL;
	char *pv_token = NULL;
	char *pv_entry[] = {NULL, NULL, NULL};

	int line_count = 0;
	int i = 0;
	int retval = -1;
	int vuln_count = 0;

	line = safe_calloc(MAXLINELEN, sizeof(char));

	while (fgets(line, MAXLINELEN, vuln_file) != NULL) {

		++line_count;

		/* what we're not interested in */
		if ((line[0] == '#') ||
		    (line[0] == '\n') ||
		    (strncmp(line, "-----BEGIN", 10) == 0) ||
		    (strncmp(line, "Hash:", 5) == 0))
			continue;

		/* effective EOF */
		if (strncmp(line, "Version:", 8) == 0)
			break;

		i = 0;

		line_tmp = trim_r(line);

		do {
			pv_token = strsep(&line_tmp, " \t");

			/*
			 * pv_entry[0] = pattern
			 * pv_entry[1] = type
			 * pv_entry[2] = URL
			 */

			/* loop processing all tokens into pv_entry[] */
			if ((pv_token != NULL) &&
			    (pv_token[0] != '\0') &&
			    (pv_token[0] != ' ')) {
				/* three tokens make a valid entry */
				pv_entry[i] = pv_token;
				++i;
			}
		} while ((pv_token != NULL) && (i <= 2));

		/* look for invalid (short) entries */
		if (i < 3) {
			errx(EXIT_FAILURE, "Invalid pkg-vulnerabilities entry rejected on line %i.", line_count);
		}

		/* if doing type checking, ignore what we don't want to see */
		if (type == TRUE) {
			if (strcmp(pv_entry[1], pkg_type) != 0)
				continue;
		}

		/* deal with URLs that we're ignorning */
		if (ignore != NULL) {
			retval = ap_ignore(pv_entry);

			/* if we got an ignore hit then stop here */
			if (retval == 1)
				continue;
		}

		if (check_one == TRUE) {

			/*
			 * if we're checking for just one package (i.e.
			 * check_one) regardless if it's installed or not
			 * (i.e. -n and -p) then use pkg_match
			 * to see if we have a hit using pattern
			 * matching.
			 */

			if ((pkg_match(pv_entry[0], package)) == 1)
				vuln_found = TRUE;
		} else {

			/*
			 * if we're not checking for a specific package
			 * then run checkforpkg to see if the
			 * pattern in pv_entry[0] is installed.
			 */

			package = checkforpkg(pv_entry[0]);
			if (package != NULL)
				vuln_found = TRUE;
		}

		/* display the messages for all the vulnerable packages seen */
		if (vuln_found == TRUE) {

			/* EOL or vulnerable message and increment the count */
			retval = pv_message(pv_entry, package);
			vuln_count = vuln_count + retval;

			/* reset the found flag */
			vuln_found = FALSE;
		}
	}

	/* bail if ferror is set */
	if (ferror(vuln_file) != 0)
		errx(EXIT_FAILURE, "Unable to read specified pkg-vulnerabilities file: %s", pvfile);

	rewind(vuln_file);
	free(line);

	return vuln_count;
}

/*
 * wrap calloc in some common error checking
 */
static void *
safe_calloc(size_t number, size_t size)
{
	void *ptr;

	ptr = calloc(number, size);

	if (ptr == NULL) {
		errx(EXIT_FAILURE, "Unable to allocate memory at line: %d.", __LINE__);
	}

	return ptr;
}

/*
 * strip any trailing characters we don't want from a string
 */
static char *
trim_r(char *trimmer)
{
	int i = 0;

	for (i = (strlen(trimmer) - 1); i > 0; --i) {
		if (STRIP(trimmer[i])) {
			trimmer[i] = '\0';
		} else {
			i = 0;
		}
	}

	return trimmer;
}

/*
 * clean a valid line from the configuration file
 */
static char *
clean_conf(char *conf_line)
{
	size_t len = 0;
	char *token = NULL;
	char *cp;

	if (((cp = strchr(conf_line, '\n')) == NULL) ||
	    ((cp = strchr(conf_line, '=')) == NULL)) {
		/* no newline or no '=' */
		errx(EXIT_FAILURE, "Malformed entry in audit-packages.conf file.");
	}

	/* split the line up and get what we need */
	token = strchr(conf_line, '=');

	/* remove any leading characters we don't want */
	while (STRIP(*token)) {
		token++;
	}

	/* remove any trailing characters we don't want */
	token = trim_r(token);

	len = strlen(token);

	/* look for entries with no associated value */
	if (len == 0) {
		token = NULL;
	}

	return token;
}

/*
 * read in our values from a configuration file
 */
static int
get_confvalues(void)
{
	FILE *conf;
	char *line_ptr = NULL;
	char *line = NULL;
	char *retval = NULL;
	Boolean f_ignore = FALSE;
	Boolean f_verify_bin = FALSE;
	Boolean f_set_pvfile = FALSE;

	if ((conf = fopen(conf_file, "r")) == NULL) {
		if (verbose >= 2)
			fprintf(stderr, "debug2: No configuration file found.\n");
		return 0;
	} else {
		if (verbose >= 1)
			fprintf(stderr, "Using configuration file: %s\n", conf_file);
	}

	line = safe_calloc(MAXLINELEN, sizeof(char));

	while ((line_ptr = fgets(line, MAXLINELEN, conf)) != NULL) {

		/* what we're not interested in */
		if ((line[0] == '#') || (line[0] == '\n'))
			continue;

		if ((strncmp(line, "IGNORE_URLS", 11) == 0) &&
		    (f_ignore == FALSE)) {
			retval = clean_conf(line);
			if (retval != NULL) {
				ignore = safe_strdup(retval);
				f_ignore = TRUE;
			}
		}
		else if ((strncmp(line, "GPG", 3) == 0) &&
		    (f_verify_bin == FALSE)) {
			retval = clean_conf(line);
			if (retval != NULL) {
				verify_bin = safe_strdup(retval);
				f_verify_bin = TRUE;
			}
		}
		else if ((strncmp(line, "PKGVULNDIR", 9) == 0) &&
		    (f_set_pvfile == FALSE)) {
			retval = clean_conf(line);
			if (retval != NULL) {
				set_pvfile(retval);
				f_set_pvfile = TRUE;
			}
		}

		retval = NULL;
	}

	/* bail if eof has not been set or ferror is set */
	if ((feof(conf) == 0) || (ferror(conf) != 0))
		errx(EXIT_FAILURE, "Unable to read specified configuration file: %s", conf_file);

	free(line);
	fclose(conf);

	return 0;
}

/*
 * check to see if a package exists
 */
static char *
checkforpkg(const char *package)
{
	char *retval = NULL;

	retval = find_best_matching_installed_pkg(package);

	if (retval == NULL && !ispkgpattern(package)) {
		char *pattern;

		if (asprintf(&pattern, "%s-[0-9]*", package) == -1)
			errx(EXIT_FAILURE, "asprintf failed");

		retval = find_best_matching_installed_pkg(pattern);
		free(pattern);
	}

	return retval;
}

/*
 * usage message for this program
 */
static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-deqsVv] [-c config_file] [-F file] [-g file] [-h file] [-K pkg_dbdir] [-n package] [-p package] [-Q varname ] [-t type]\n", program_name);
	fprintf(stderr, "\t-d : Run the download-vulnerability-list script before anything else.\n");
	fprintf(stderr, "\t-e : Check for end-of-life (eol) packages.\n");
	fprintf(stderr, "\t-q : Be quiet and just dump the detected vulnerable package names.\n");
	fprintf(stderr, "\t-s : Verify the signature of the pkg-vulnerabilities file.\n");
	fprintf(stderr, "\t-V : Display version and exit.\n");
	fprintf(stderr, "\t-v : Be more verbose. Specify multiple -v flags to increase verbosity.\n");
	fprintf(stderr, "\t-c : Specify a custom configuration file to use.\n");
	fprintf(stderr, "\t-F : Check all packages listed in a file for vulnerabilities.\n");
	fprintf(stderr, "\t-g : Compute the hash of a file.\n");
	fprintf(stderr, "\t-h : Check the hash of a file against the internally stored value.\n");
	fprintf(stderr, "\t-K : Use pkg_dbdir as PKG_DBDIR.\n");
	fprintf(stderr, "\t-n : Check a specific package for vulnerabilities.\n");
	fprintf(stderr, "\t-p : Check a specific installed package for vulnerabilities.\n");
	fprintf(stderr, "\t-Q : Display the current value of varname and exit.\n");
	fprintf(stderr, "\t-t : Only check for a specific type of vulnerability.\n");
	exit(EXIT_SUCCESS);
}

/*
 * we need to download the file first
 */
static int
dvl(void)
{
	int retval = -1;

	/* execute download-vulnerability-list */
	retval = fexec(DVL_BIN, NULL);

	if (retval != 0) {
		errx(EXIT_FAILURE, "Failure running: %s", DVL_BIN);
	}

	return retval;
}

/*
 * check for an old vulnerabilities file if we're being verbose
 */
static void
old_pvfile(void)
{
	float t_diff;
	int long t_current;
	time_t t_pvfile;
	struct stat pvstat;
	struct timeval now_time = {0, 0};

	/* we already know it exists */
	stat(pvfile, &pvstat);

	if ((gettimeofday(&now_time, NULL)) != 0) {
		warnx("Unable to get current time.  You pkg-vulnerabilities file may be out of date.");
	} else {
		/* difference between the file and now */
		t_current = now_time.tv_sec;
		t_pvfile = pvstat.st_ctime;
		t_diff = (((((float) t_current - (float) t_pvfile) / 60) / 60) / 24);

		if (t_diff >= 7)
			fprintf(stderr, "%s more than a week old, continuing...\n", pvfile);

		if (verbose >= 2)
			fprintf(stderr, "debug2: pkg-vulnerabilities file %.2f day(s) old.\n", t_diff);
	}
}

/*
 * get the #FORMAT from the pkg-vulnerabilities file
 */
static void
pv_format(FILE * pv)
{
	char *line = NULL;
	char *line_ptr;

	int major = 0;
	int minor = 0;
	int teeny = 0;
	int format_found = 0;

	if (verbose >= 3)
		fprintf(stderr, "debug3: File format required: #FORMAT %i.%i.%i\n", FORMAT[0], FORMAT[1], FORMAT[2]);

	line = safe_calloc(MAXLINELEN, sizeof(char));

	while ((line_ptr = fgets(line, MAXLINELEN, pv)) != NULL) {

		/* this time round this is all we're interested in */
		if (strncmp(line, "#FORMAT", 6) == 0) {

			sscanf(line, "#FORMAT %i.%i.%i",
			    &major, &minor, &teeny);

			if (line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = '\0';

			format_found = 1;

			if (verbose >= 3)
				fprintf(stderr, "debug3: File format detected: %s\n", line);

			break;
		}
	}

	/* compare the #FORMAT with what we expect to see */
	if (format_found == 1) {
		if ((major < FORMAT[0]) ||
		    (minor < FORMAT[1]) ||
		    (teeny < FORMAT[2])) {
			errx(EXIT_FAILURE, "Your pkg-vulnerabilites file is out of date.\nPlease update audit-packages and run download-vulnerability-list again.");
		}
	} else {
		errx(EXIT_FAILURE, "No file format version found in: %s.\nPlease update audit-packages and run download-vulnerability-list again.", pvfile);
	}

	free(line);
}

/*
 * extract the stored hash in the pkg-vulnerabilities file
 */
static char *
get_hash(char *hash_input)
{
	char *line = NULL;
	char *hash = NULL;
	char *line_ptr = NULL;

	int hash_found = 0;

	FILE *hash_in;

	if ((hash_in = fopen(hash_input, "r")) == NULL) {
		errx(EXIT_FAILURE, "Unable to open: %s", hash_input);
	}

	line = safe_calloc(MAXLINELEN, sizeof(char));

	while ((line_ptr = fgets(line, MAXLINELEN, hash_in)) != NULL) {
		if (strncmp(line, "#CHECKSUM SHA512", 16) == 0) {

			hash = safe_calloc(SHA512_DIGEST_STRING_LENGTH, sizeof(char));
			sscanf(line, "#CHECKSUM SHA512 %129s", hash);
			hash_found = 1;

			break;
		}
	}

	if (hash_found == 0) {
		errx(EXIT_FAILURE, "No hash found in: %s\nPlease update audit-packages and run download-vulnerability-list again.", pvfile);
	}

	fclose(hash_in);
	free(line);

	return hash;
}

/*
 * check the internally stored hash against the computed hash (-h <file>)
 */
static int
check_hash(char *hash_input)
{
	int retval = -1;
	char *hash_stored = NULL;
	char *hash_generated = NULL;

	hash_generated = gen_hash(hash_input);

	/* if gen_hash() failed then return now */
	if (hash_generated == NULL)
		return retval;

	if (verbose >= 2)
		fprintf(stderr, "debug2: Hash generated: %s\n", hash_generated);

	hash_stored = get_hash(hash_input);

	/* if get_hash() failed then return now */
	if (hash_stored == NULL)
		return retval;

	if (verbose >= 2)
		fprintf(stderr, "debug2: Hash stored: %s\n", hash_stored);

	/* do the hash comparison */
	if (strncmp(hash_generated, hash_stored, SHA512_DIGEST_STRING_LENGTH) == 0) {
		retval = 0;
	} else {
		retval = -1;
	}

	return retval;
}

/*
 * do the hash calculation on specified input
 */
static char *
gen_hash(char *hash_input)
{
	char *hash_result = NULL;
	char *hash_calc = NULL;
	char *line = NULL;
	char *line_ptr;

	int j = 0;
	int i = 0;

	FILE *hash_in;
	SHA512_CTX hash_ctx;

	if ((hash_in = fopen(hash_input, "r")) == NULL) {
		errx(EXIT_FAILURE, "Unable to open: %s", hash_input);
	}

	SHA512_Init(&hash_ctx);

	line = safe_calloc(MAXLINELEN, sizeof(char));

	while ((line_ptr = fgets(line, MAXLINELEN, hash_in)) != NULL) {

		/* what we're not interested in */
		if ((strncmp(line, "# $NetBSD:", 10) == 0) ||
		    (line[0] == '\n') ||
		    (strncmp(line, "-----BEGIN", 10) == 0) ||
		    (strncmp(line, "Hash:", 5) == 0) ||
		    (strncmp(line, "#CHECKSUM", 9) == 0 ))
			continue;

		/* effective EOF */
		if (strncmp(line, "Version:", 8) == 0)
			break;

		SHA512_Update(&hash_ctx, (unsigned char *)line, strlen(line));
	}

	/* get the hash_result into a human readable string */
	hash_calc = safe_calloc(SHA512_DIGEST_STRING_LENGTH, sizeof(char));
	hash_result = safe_calloc(SHA512_DIGEST_LENGTH, sizeof(char));

	SHA512_Final((unsigned char *)hash_result, &hash_ctx);

	for (i = 0; i < SHA512_DIGEST_LENGTH; ++i) {
		sprintf(&hash_calc[j], "%02x", hash_result[i] & 0xFF);
		j = j + 2;
	}

	fclose(hash_in);
	free(hash_result);
	free(line);

	return hash_calc;
}

/*
 * do signature checking - if required
 */
static int
check_sig(char *sig_input)
{
	int retval = -1;

	if (verbose >= 3)
		fprintf(stderr, "debug3: Attempting to verify signature.\n");

	/* execute our verification tool */
	retval = fexec(verify_bin, "--verify", "--batch", "--no-options", sig_input, NULL);

	return retval;
}

/*
 * print the messages for eol and vulnerable packages
 */
static int
pv_message(char *pv_entry[], char *package)
{
	int retval = 0;

	/* deal with eol'ed packages */
	if (strcmp(pv_entry[1], "eol") == 0) {
		if (eol == TRUE) {
			if (quiet == FALSE) {
				fprintf(stdout, "Package %s has reached end-of-life (eol), see %s/eol-packages\n", pv_entry[0], EOL_URL);
			} else {
				fprintf(stdout, "%s\n", pv_entry[0]);
			}
		}
	} else {
		/* return that we found a vulnerable package */
		retval = 1;

		/* Just make sure we display _something_ useful here */
		if (package == NULL)
			package = pv_entry[0];

		if (quiet == FALSE) {
			fprintf(stdout, "Package %s has a %s vulnerability, see: %s\n", package, pv_entry[1], pv_entry[2]);
		} else {
			fprintf(stdout, "%s\n", package);
		}
	}

	return retval;
}

/*
 * deal with URLs that we're ignorning
 */
static int
ap_ignore(char *pv_entry[])
{
	char *ignore_tmp = NULL;
	char *ig_token = NULL;

	int retval = 0;
	int ignore_hit = 0;

	ignore_tmp = safe_strdup(ignore);

	while ((ig_token = strsep(&ignore_tmp, " ")) != NULL) {

		/* ignore empty tokens as well */
		if (ig_token[0] != '\0') {
			/* see we have an IGNORE_URLS hit */
			if (strcmp(pv_entry[2], ig_token) == 0) {
				ignore_hit = 1;
				break;
			}
		}
	}

	/* if we're seen an IGNORE_URLS then don't bother going on */
	if (ignore_hit == 1) {
		if (verbose >= 1) {
			fprintf(stderr, "Ignoring vulnerability for %s with pattern: %s\n", pv_entry[2], pv_entry[0]);
		}

		/* return that we got an ignore hit */
		retval = 1;
	}

	return retval;
}

/*
 * at the moment we really don't need to clean anything up
 */
void
cleanup(int signo)
{
}

/*
 * print what the current settings are
 */
static void
show_info(char *varname)
{
	if (strncmp(varname, "GPG", 3) == 0) {
		fprintf(stdout, "%s\n", verify_bin);
	}
	else if (strncmp(varname, "PKGVULNDIR", 9) == 0) {
		fprintf(stdout, "%s\n", pvdir);
	}
	else if (strncmp(varname, "IGNORE_URLS", 11) == 0) {
		fprintf(stdout, "%s\n", ignore);
	}
}

/*
 * set the location for the pkg-vulnerabilities file
 */
static void
set_pvfile(const char *vuln_dir)
{
	char *pvloc = NULL;
	size_t retval;
	const char pvname[] = "/pkg-vulnerabilities";

	pvloc = safe_calloc(MAXPATHLEN, sizeof(char));
	retval = strlcpy(pvloc, vuln_dir, MAXPATHLEN);
	retval = strlcat(pvloc, pvname, MAXPATHLEN);

	pvdir = safe_strdup(vuln_dir);
	pvfile = safe_strdup(pvloc);

	free(pvloc);
}

/*
 * duplicate a string and check the return value
 */
static char *
safe_strdup(const char *dupe)
{
	char *retval;

	if ((retval = strdup(dupe)) == NULL) {
		errx(EXIT_FAILURE, "Unable to allocate memory at line: %d.", __LINE__);
	}

	return retval;
}
