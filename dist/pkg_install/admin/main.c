/*	$NetBSD: main.c,v 1.1.1.7.4.1 2008/05/18 12:29:33 yamt Exp $	*/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <nbcompat.h>
#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#ifndef lint
__RCSID("$NetBSD: main.c,v 1.1.1.7.4.1 2008/05/18 12:29:33 yamt Exp $");
#endif

/*-
 * Copyright (c) 1999-2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Hubert Feyrer <hubert@feyrer.de>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_DIRENT_H
#include <dirent.h>
#endif
#if HAVE_ERR_H
#include <err.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_MD5_H
#include <md5.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif

#include "lib.h"

#define DEFAULT_SFX	".t[bg]z"	/* default suffix for ls{all,best} */

static const char Options[] = "K:SVbd:qs:";

int     filecnt;
int     pkgcnt;

static int	quiet;

static int checkpattern_fn(const char *, void *);
static void set_unset_variable(char **, Boolean);

/* print usage message and exit */
static void 
usage(void)
{
	(void) fprintf(stderr, "usage: %s [-bqSV] [-d lsdir] [-K pkg_dbdir] [-s sfx] command args ...\n"
	    "Where 'commands' and 'args' are:\n"
	    " rebuild                     - rebuild pkgdb from +CONTENTS files\n"
	    " rebuild-tree                - rebuild +REQUIRED_BY files from forward deps\n"
	    " check [pkg ...]             - check md5 checksum of installed files\n"
	    " add pkg ...                 - add pkg files to database\n"
	    " delete pkg ...              - delete file entries for pkg in database\n"
	    " set variable=value pkg ...  - set installation variable for package\n"
	    " unset variable pkg ...      - unset installation variable for package\n"
#ifdef PKGDB_DEBUG
	    " addkey key value            - add key and value\n"
	    " delkey key                  - delete reference to key\n"
#endif
	    " lsall /path/to/pkgpattern   - list all pkgs matching the pattern\n"
	    " lsbest /path/to/pkgpattern  - list pkgs matching the pattern best\n"
	    " dump                        - dump database\n"
	    " pmatch pattern pkg          - returns true if pkg matches pattern, otherwise false\n",
	    getprogname());
	exit(EXIT_FAILURE);
}

/*
 * Assumes CWD is in /var/db/pkg/<pkg>!
 */
static void 
check1pkg(const char *pkgdir)
{
	FILE   *f;
	plist_t *p;
	package_t Plist;
	char   *PkgName, *dirp = NULL, *md5file;
	char    file[MaxPathSize];
	char    dir[MaxPathSize];

	f = fopen(CONTENTS_FNAME, "r");
	if (f == NULL)
		err(EXIT_FAILURE, "can't open %s/%s/%s", _pkgdb_getPKGDB_DIR(), pkgdir, CONTENTS_FNAME);

	Plist.head = Plist.tail = NULL;
	read_plist(&Plist, f);
	p = find_plist(&Plist, PLIST_NAME);
	if (p == NULL)
		errx(EXIT_FAILURE, "Package %s has no @name, aborting.",
		    pkgdir);
	PkgName = p->name;
	for (p = Plist.head; p; p = p->next) {
		switch (p->type) {
		case PLIST_FILE:
			if (dirp == NULL) {
				warnx("dirp not initialized, please send-pr!");
				abort();
			}
			
			(void) snprintf(file, sizeof(file), "%s/%s", dirp, p->name);

			if (isfile(file) || islinktodir(file)) {
				if (p->next && p->next->type == PLIST_COMMENT) {
					if (strncmp(p->next->name, CHECKSUM_HEADER, ChecksumHeaderLen) == 0) {
						if ((md5file = MD5File(file, NULL)) != NULL) {
							/* Mismatch? */
#ifdef PKGDB_DEBUG
							printf("%s: md5 should=<%s>, is=<%s>\n",
							    file, p->next->name + ChecksumHeaderLen, md5file);
#endif
							if (strcmp(md5file, p->next->name + ChecksumHeaderLen) != 0)
								printf("%s fails MD5 checksum\n", file);

							free(md5file);
						}
					} else if (strncmp(p->next->name, SYMLINK_HEADER, SymlinkHeaderLen) == 0) {
						char	buf[MaxPathSize + SymlinkHeaderLen];
						int	cc;

						(void) strlcpy(buf, SYMLINK_HEADER, sizeof(buf));
						if ((cc = readlink(file, &buf[SymlinkHeaderLen],
							  sizeof(buf) - SymlinkHeaderLen - 1)) < 0) {
							warnx("can't readlink `%s'", file);
						} else {
							buf[SymlinkHeaderLen + cc] = 0x0;
							if (strcmp(buf, p->next->name) != 0) {
								printf("symlink (%s) is not same as recorded value, %s: %s\n",
								    file, buf, p->next->name);
							}
						}
					}
				}
				
				filecnt++;
			} else if (isbrokenlink(file)) {
				warnx("%s: Symlink `%s' exists and is in %s but target does not exist!", PkgName, file, CONTENTS_FNAME);
			} else {
				warnx("%s: File `%s' is in %s but not on filesystem!", PkgName, file, CONTENTS_FNAME);
			}
			break;
		case PLIST_CWD:
			if (strcmp(p->name, ".") != 0)
				dirp = p->name;
			else {
				(void) snprintf(dir, sizeof(dir), "%s/%s", _pkgdb_getPKGDB_DIR(), pkgdir);
				dirp = dir;
			}
			break;
		case PLIST_IGNORE:
			p = p->next;
			break;
		case PLIST_SHOW_ALL:
		case PLIST_SRC:
		case PLIST_CMD:
		case PLIST_CHMOD:
		case PLIST_CHOWN:
		case PLIST_CHGRP:
		case PLIST_COMMENT:
		case PLIST_NAME:
		case PLIST_UNEXEC:
		case PLIST_DISPLAY:
		case PLIST_PKGDEP:
		case PLIST_MTREE:
		case PLIST_DIR_RM:
		case PLIST_IGNORE_INST:
		case PLIST_OPTION:
		case PLIST_PKGCFL:
		case PLIST_BLDDEP:
			break;
		}
	}
	free_plist(&Plist);
	fclose(f);
	pkgcnt++;
}

/*
 * add1pkg(<pkg>)
 *	adds the files listed in the +CONTENTS of <pkg> into the
 *	pkgdb.byfile.db database file in the current package dbdir.  It
 *	returns the number of files added to the database file.
 */
static int
add1pkg(const char *pkgdir)
{
	FILE	       *f;
	plist_t	       *p;
	package_t	Plist;
	char 	       *contents;
	const char	*PkgDBDir;
	char *PkgName, *dirp;
	char 		file[MaxPathSize];
	char		dir[MaxPathSize];
	int		cnt = 0;

	if (!pkgdb_open(ReadWrite))
		err(EXIT_FAILURE, "cannot open pkgdb");

	PkgDBDir = _pkgdb_getPKGDB_DIR();
	contents = pkgdb_pkg_file(pkgdir, CONTENTS_FNAME);
	if ((f = fopen(contents, "r")) == NULL)
		errx(EXIT_FAILURE, "%s: can't open `%s'", pkgdir, CONTENTS_FNAME);
	free(contents);

	Plist.head = Plist.tail = NULL;
	read_plist(&Plist, f);
	if ((p = find_plist(&Plist, PLIST_NAME)) == NULL) {
		errx(EXIT_FAILURE, "Package `%s' has no @name, aborting.", pkgdir);
	}

	PkgName = p->name;
	dirp = NULL;
	for (p = Plist.head; p; p = p->next) {
		switch(p->type) {
		case PLIST_FILE:
			if (dirp == NULL) {
				errx(EXIT_FAILURE, "@cwd not yet found, please send-pr!");
			}
			(void) snprintf(file, sizeof(file), "%s/%s", dirp, p->name);
			if (!(isfile(file) || islinktodir(file))) {
				if (isbrokenlink(file)) {
					warnx("%s: Symlink `%s' exists and is in %s but target does not exist!",
						PkgName, file, CONTENTS_FNAME);
				} else {
					warnx("%s: File `%s' is in %s but not on filesystem!",
						PkgName, file, CONTENTS_FNAME);
				}
			} else {
				pkgdb_store(file, PkgName);
				cnt++;
			}
			break;
		case PLIST_CWD:
			if (strcmp(p->name, ".") != 0) {
				dirp = p->name;
			} else {
				(void) snprintf(dir, sizeof(dir), "%s/%s", PkgDBDir, pkgdir);
				dirp = dir;
			}
			break;
		case PLIST_IGNORE:
			p = p->next;
			break;
		case PLIST_SHOW_ALL:
		case PLIST_SRC:
		case PLIST_CMD:
		case PLIST_CHMOD:
		case PLIST_CHOWN:
		case PLIST_CHGRP:
		case PLIST_COMMENT:
		case PLIST_NAME:
		case PLIST_UNEXEC:
		case PLIST_DISPLAY:
		case PLIST_PKGDEP:
		case PLIST_MTREE:
		case PLIST_DIR_RM:
		case PLIST_IGNORE_INST:
		case PLIST_OPTION:
		case PLIST_PKGCFL:
		case PLIST_BLDDEP:
			break;
		}
	}
	free_plist(&Plist);
	fclose(f);
	pkgdb_close();

	return cnt;
}

static void
delete1pkg(const char *pkgdir)
{
	if (!pkgdb_open(ReadWrite))
		err(EXIT_FAILURE, "cannot open pkgdb");
	(void) pkgdb_remove_pkg(pkgdir);
	pkgdb_close();
}

static void 
rebuild(void)
{
	DIR	       *dp;
	struct dirent  *de;
	const char     *PkgDBDir;
	char		cachename[MaxPathSize];

	pkgcnt = 0;
	filecnt = 0;

	(void) _pkgdb_getPKGDB_FILE(cachename, sizeof(cachename));
	if (unlink(cachename) != 0 && errno != ENOENT)
		err(EXIT_FAILURE, "unlink %s", cachename);

	setbuf(stdout, NULL);
	PkgDBDir = _pkgdb_getPKGDB_DIR();
	chdir(PkgDBDir);
#ifdef PKGDB_DEBUG
	printf("PkgDBDir='%s'\n", PkgDBDir);
#endif
	dp = opendir(".");
	if (dp == NULL)
		err(EXIT_FAILURE, "opendir failed");
	while ((de = readdir(dp))) {
		if (!(isdir(de->d_name) || islinktodir(de->d_name)))
			continue;

		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;

#ifdef PKGDB_DEBUG
		printf("%s\n", de->d_name);
#else
		if (!quiet) {
			printf(".");
		}
#endif

		filecnt += add1pkg(de->d_name);
		pkgcnt++;
	}
	chdir("..");
	closedir(dp);

	printf("\n");
	printf("Stored %d file%s from %d package%s in %s.\n",
	    filecnt, filecnt == 1 ? "" : "s",
	    pkgcnt, pkgcnt == 1 ? "" : "s",
	    cachename);
}

static void 
checkall(void)
{
	DIR    *dp;
	struct dirent *de;

	pkgcnt = 0;
	filecnt = 0;

	setbuf(stdout, NULL);
	chdir(_pkgdb_getPKGDB_DIR());

	dp = opendir(".");
	if (dp == NULL)
		err(EXIT_FAILURE, "opendir failed");
	while ((de = readdir(dp))) {
		if (!(isdir(de->d_name) || islinktodir(de->d_name)))
			continue;

		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;

		chdir(de->d_name);

		check1pkg(de->d_name);
		if (!quiet) {
			printf(".");
		}

		chdir("..");
	}
	closedir(dp);
	pkgdb_close();


	printf("\n");
	printf("Checked %d file%s from %d package%s.\n",
	    filecnt, (filecnt == 1) ? "" : "s",
	    pkgcnt, (pkgcnt == 1) ? "" : "s");
}

static int
checkpattern_fn(const char *pkg, void *vp)
{
	int *got_match, rc;

	rc = chdir(pkg);
	if (rc == -1)
		err(EXIT_FAILURE, "Cannot chdir to %s/%s", _pkgdb_getPKGDB_DIR(), pkg);

	check1pkg(pkg);
	if (!quiet) {
		printf(".");
	}

	chdir("..");

	got_match = vp;
	*got_match = 1;

	return 0;
}

static int
lspattern(const char *pkg, void *vp)
{
	const char *dir = vp;
	printf("%s/%s\n", dir, pkg);
	return 0;
}

static int
lsbasepattern(const char *pkg, void *vp)
{
	puts(pkg);
	return 0;
}

static int
remove_required_by(const char *pkgname, void *cookie)
{
	char *path;

	path = pkgdb_pkg_file(pkgname, REQUIRED_BY_FNAME);

	if (unlink(path) == -1 && errno != ENOENT)
		err(EXIT_FAILURE, "Cannot remove %s", path);

	free(path);

	return 0;
}

static void
add_required_by(const char *pattern, const char *required_by)
{
	char *best_installed, *path;
	int fd;
	size_t len;

	best_installed = find_best_matching_installed_pkg(pattern);
	if (best_installed == NULL) {
		warnx("Dependency %s of %s unresolved", pattern, required_by);
		return;
	}

	path = pkgdb_pkg_file(best_installed, REQUIRED_BY_FNAME);
	free(best_installed);

	if ((fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644)) == -1)
		errx(EXIT_FAILURE, "Cannot write to %s", path);
	free(path);
	
	len = strlen(required_by);
	if (write(fd, required_by, len) != len ||
	    write(fd, "\n", 1) != 1 ||
	    close(fd) == -1)
		errx(EXIT_FAILURE, "Cannot write to %s", path);
}


static int
add_depends_of(const char *pkgname, void *cookie)
{
	FILE *fp;
	plist_t *p;
	package_t plist;
	char *path;

	path = pkgdb_pkg_file(pkgname, CONTENTS_FNAME);
	if ((fp = fopen(path, "r")) == NULL)
		errx(EXIT_FAILURE, "Cannot read %s of package %s",
		    CONTENTS_FNAME, pkgname);
	free(path);
	read_plist(&plist, fp);
	fclose(fp);

	for (p = plist.head; p; p = p->next) {
		if (p->type == PLIST_PKGDEP)
			add_required_by(p->name, pkgname);
	}

	free_plist(&plist);	

	return 0;
}

static void
rebuild_tree(void)
{
	if (iterate_pkg_db(remove_required_by, NULL) == -1)
		errx(EXIT_FAILURE, "cannot iterate pkgdb");
	if (iterate_pkg_db(add_depends_of, NULL) == -1)
		errx(EXIT_FAILURE, "cannot iterate pkgdb");
}

int 
main(int argc, char *argv[])
{
	Boolean		 use_default_sfx = TRUE;
	Boolean 	 show_basename_only = FALSE;
	char		 lsdir[MaxPathSize];
	char		 sfx[MaxPathSize];
	char		*lsdirp = NULL;
	int		 ch;

	setprogname(argv[0]);

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, Options)) != -1)
		switch (ch) {
		case 'K':
			_pkgdb_setPKGDB_DIR(optarg);
			break;

		case 'S':
			sfx[0] = 0x0;
			use_default_sfx = FALSE;
			break;

		case 'V':
			show_version();
			/* NOTREACHED */

		case 'b':
			show_basename_only = TRUE;
			break;

		case 'd':
			(void) strlcpy(lsdir, optarg, sizeof(lsdir));
			lsdirp = lsdir;
			break;

		case 'q':
			quiet = 1;
			break;

		case 's':
			(void) strlcpy(sfx, optarg, sizeof(sfx));
			use_default_sfx = FALSE;
			break;

		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;

	if (argc <= 0) {
		usage();
	}

	if (use_default_sfx)
		(void) snprintf(sfx, sizeof(sfx), "%s", DEFAULT_SFX);

	if (strcasecmp(argv[0], "pmatch") == 0) {

		char *pattern, *pkg;
		
		argv++;		/* "pmatch" */

		if (argv[0] == NULL || argv[1] == NULL) {
			usage();
		}

		pattern = argv[0];
		pkg = argv[1];

		if (pkg_match(pattern, pkg)){
			return 0;
		} else {
			return 1;
		}
	  
	} else if (strcasecmp(argv[0], "rebuild") == 0) {

		rebuild();
		printf("Done.\n");

	  
	} else if (strcasecmp(argv[0], "rebuild-tree") == 0) {

		rebuild_tree();
		printf("Done.\n");

	} else if (strcasecmp(argv[0], "check") == 0) {

		argv++;		/* "check" */

		if (*argv != NULL) {
			/* args specified */
			int     rc;

			filecnt = 0;

			setbuf(stdout, NULL);

			rc = chdir(_pkgdb_getPKGDB_DIR());
			if (rc == -1)
				err(EXIT_FAILURE, "Cannot chdir to %s", _pkgdb_getPKGDB_DIR());

			while (*argv != NULL) {
				int got_match;

				got_match = 0;
				if (match_installed_pkgs(*argv, checkpattern_fn, &got_match) == -1)
					errx(EXIT_FAILURE, "Cannot process pkdbdb");
				if (got_match == 0) {
					char *pattern;

					if (ispkgpattern(*argv))
						errx(EXIT_FAILURE, "No matching pkg for %s.", *argv);

					if (asprintf(&pattern, "%s-[0-9]*", *argv) == -1)
						errx(EXIT_FAILURE, "asprintf failed");

					if (match_installed_pkgs(pattern, checkpattern_fn, &got_match) == -1)
						errx(EXIT_FAILURE, "Cannot process pkdbdb");

					if (got_match == 0)
						errx(EXIT_FAILURE, "cannot find package %s", *argv);
					free(pattern);
				}

				argv++;
			}

			printf("\n");
			printf("Checked %d file%s from %d package%s.\n",
			    filecnt, (filecnt == 1) ? "" : "s",
			    pkgcnt, (pkgcnt == 1) ? "" : "s");
		} else {
			checkall();
		}
		if (!quiet) {
			printf("Done.\n");
		}

	} else if (strcasecmp(argv[0], "lsall") == 0) {
		int saved_wd;

		argv++;		/* "lsall" */

		/* preserve cwd */
		saved_wd=open(".", O_RDONLY);
		if (saved_wd == -1)
			err(EXIT_FAILURE, "Cannot save working dir");

		while (*argv != NULL) {
			/* args specified */
			int     rc;
			const char *basep, *dir;
			char cwd[MaxPathSize];

			dir = lsdirp ? lsdirp : dirname_of(*argv);
			basep = basename_of(*argv);

			fchdir(saved_wd);
			rc = chdir(dir);
			if (rc == -1)
				err(EXIT_FAILURE, "Cannot chdir to %s", dir);

			if (getcwd(cwd, sizeof(cwd)) == NULL)
				err(EXIT_FAILURE, "getcwd");

			if (show_basename_only)
				rc = match_local_files(cwd, use_default_sfx, 1, basep, lsbasepattern, NULL);
			else
				rc = match_local_files(cwd, use_default_sfx, 1, basep, lspattern, cwd);
			if (rc == -1)
				errx(EXIT_FAILURE, "Error from match_local_files(\"%s\", \"%s\", ...)",
				     cwd, basep);

			argv++;
		}

		close(saved_wd);

	} else if (strcasecmp(argv[0], "lsbest") == 0) {
		int saved_wd;

		argv++;		/* "lsbest" */

		/* preserve cwd */
		saved_wd=open(".", O_RDONLY);
		if (saved_wd == -1)
			err(EXIT_FAILURE, "Cannot save working dir");

		while (*argv != NULL) {
			/* args specified */
			const char *basep, *dir;
			char cwd[MaxPathSize];
			char *p;

			dir = lsdirp ? lsdirp : dirname_of(*argv);
			basep = basename_of(*argv);

			fchdir(saved_wd);
			if (chdir(dir) == -1)
				err(EXIT_FAILURE, "Cannot chdir to %s", dir);

			if (getcwd(cwd, sizeof(cwd)) == NULL)
				err(EXIT_FAILURE, "getcwd");

			p = find_best_matching_file(cwd, basep, use_default_sfx, 1);

			if (p) {
				if (show_basename_only)
					printf("%s\n", p);
				else
					printf("%s/%s\n", cwd, p);
				free(p);
			}
			
			argv++;
		}

		close(saved_wd);

	} else if (strcasecmp(argv[0], "list") == 0 ||
	    strcasecmp(argv[0], "dump") == 0) {

		pkgdb_dump();

	} else if (strcasecmp(argv[0], "add") == 0) {
		argv++;		/* "add" */
		while (*argv != NULL) {
			add1pkg(*argv);
			argv++;
		}
	} else if (strcasecmp(argv[0], "delete") == 0) {
		argv++;		/* "delete" */
		while (*argv != NULL) {
			delete1pkg(*argv);
			argv++;
		}
	} else if (strcasecmp(argv[0], "set") == 0) {
		argv++;		/* "set" */
		set_unset_variable(argv, FALSE);
	} else if (strcasecmp(argv[0], "unset") == 0) {
		argv++;		/* "unset" */
		set_unset_variable(argv, TRUE);
	}
#ifdef PKGDB_DEBUG
	else if (strcasecmp(argv[0], "delkey") == 0) {
		int     rc;

		if (!pkgdb_open(ReadWrite))
			err(EXIT_FAILURE, "cannot open pkgdb");

		rc = pkgdb_remove(argv[2]);
		if (rc) {
			if (errno)
				perror("pkgdb_remove");
			else
				printf("Key not present in pkgdb.\n");
		}
		
		pkgdb_close();

	} else if (strcasecmp(argv[0], "addkey") == 0) {

		int     rc;

		if (!pkgdb_open(ReadWrite)) {
			err(EXIT_FAILURE, "cannot open pkgdb");
		}

		rc = pkgdb_store(argv[2], argv[3]);
		switch (rc) {
		case -1:
			perror("pkgdb_store");
			break;
		case 1:
			printf("Key already present.\n");
			break;
		default:
			/* 0: everything ok */
			break;
		}

		pkgdb_close();

	}
#endif
	else {
		usage();
	}

	return 0;
}

struct set_installed_info_arg {
	char *variable;
	char *value;
	int got_match;
};

static int
set_installed_info_var(const char *name, void *cookie)
{
	struct set_installed_info_arg *arg = cookie;
	char *filename;
	int retval;

	filename = pkgdb_pkg_file(name, INSTALLED_INFO_FNAME);

	retval = var_set(filename, arg->variable, arg->value);

	free(filename);
	arg->got_match = 1;

	return retval;
}

static void
set_unset_variable(char **argv, Boolean unset)
{
	struct set_installed_info_arg arg;
	char *eq;
	char *variable;
	int ret = 0;

	if (argv[0] == NULL || argv[1] == NULL)
		usage();
	
	variable = NULL;

	if (unset) {
		arg.variable = argv[0];
		arg.value = NULL;
	} else {	
		eq = NULL;
		if ((eq=strchr(argv[0], '=')) == NULL)
			usage();
		
		variable = malloc(eq-argv[0]+1);
		strlcpy(variable, argv[0], eq-argv[0]+1);
		
		arg.variable = variable;
		arg.value = eq+1;
		
		if (strcmp(variable, AUTOMATIC_VARNAME) == 0 &&
		    strcasecmp(arg.value, "yes") != 0 &&
		    strcasecmp(arg.value, "no") != 0) {
			errx(EXIT_FAILURE,
			     "unknown value `%s' for " AUTOMATIC_VARNAME,
			     arg.value);
		}
	}
	if (strpbrk(arg.variable, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") != NULL) {
		free(variable);
		errx(EXIT_FAILURE,
		     "variable name must not contain uppercase letters");
	}

	argv++;
	while (*argv != NULL) {
		arg.got_match = 0;
		if (match_installed_pkgs(*argv, set_installed_info_var, &arg) == -1)
			errx(EXIT_FAILURE, "Cannot process pkdbdb");
		if (arg.got_match == 0) {
			char *pattern;

			if (ispkgpattern(*argv)) {
				warnx("no matching pkg for `%s'", *argv);
				ret++;
			} else {
				if (asprintf(&pattern, "%s-[0-9]*", *argv) == -1)
					errx(EXIT_FAILURE, "asprintf failed");

				if (match_installed_pkgs(pattern, set_installed_info_var, &arg) == -1)
					errx(EXIT_FAILURE, "Cannot process pkdbdb");

				if (arg.got_match == 0) {
					warnx("cannot find package %s", *argv);
					++ret;
				}
				free(pattern);
			}
		}

		argv++;
	}

	if (ret > 0)
		exit(EXIT_FAILURE);

	free(variable);

	return;
}

void
cleanup(int signo)
{
}
