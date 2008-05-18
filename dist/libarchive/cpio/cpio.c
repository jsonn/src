/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "cpio_platform.h"
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <archive.h>
#include <archive_entry.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "cpio.h"
#include "matching.h"

static int	copy_data(struct archive *, struct archive *);
static const char *cpio_rename(const char *name);
static int	entry_to_archive(struct cpio *, struct archive_entry *);
static int	file_to_archive(struct cpio *, const char *);
static void	long_help(void);
static void	mode_in(struct cpio *);
static void	mode_list(struct cpio *);
static void	mode_out(struct cpio *);
static void	mode_pass(struct cpio *, const char *);
static void	restore_time(struct cpio *, struct archive_entry *,
		    const char *, int fd);
static void	usage(void);
static void	version(void);

int
main(int argc, char *argv[])
{
	static char buff[16384];
	struct cpio _cpio; /* Allocated on stack. */
	struct cpio *cpio;
	int uid, gid;
	int opt;

	cpio = &_cpio;
	memset(cpio, 0, sizeof(*cpio));
	cpio->buff = buff;
	cpio->buff_size = sizeof(buff);

	/* Need cpio_progname before calling cpio_warnc. */
	if (*argv == NULL)
		cpio_progname = "bsdcpio";
	else {
		cpio_progname = strrchr(*argv, '/');
		if (cpio_progname != NULL)
			cpio_progname++;
		else
			cpio_progname = *argv;
	}

	cpio->uid_override = -1;
	cpio->gid_override = -1;
	cpio->argv = argv;
	cpio->argc = argc;
	cpio->mode = '\0';
	cpio->verbose = 0;
	cpio->compress = '\0';
	/* TODO: Implement old binary format in libarchive, use that here. */
	cpio->format = "odc"; /* Default format */
	cpio->extract_flags = ARCHIVE_EXTRACT_NO_AUTODIR;
	cpio->extract_flags |= ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
	cpio->extract_flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
	cpio->extract_flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
	/* TODO: If run by root, set owner as well. */
	cpio->bytes_per_block = 512;
	cpio->filename = NULL;

	while ((opt = cpio_getopt(cpio)) != -1) {
		switch (opt) {
		case 'A': /* NetBSD/OpenBSD */
			cpio->option_append = 1;
			break;
		case 'a': /* POSIX 1997 */
			cpio->option_atime_restore = 1;
			break;
		case 'B': /* POSIX 1997 */
			cpio->bytes_per_block = 5120;
			break;
		case 'C': /* NetBSD/OpenBSD */
			cpio->bytes_per_block = atoi(optarg);
			if (cpio->bytes_per_block <= 0)
				cpio_errc(1, 0, "Invalid blocksize %s", optarg);
			break;
		case 'c': /* POSIX 1997 */
			cpio->format = "odc";
			break;
		case 'd': /* POSIX 1997 */
			cpio->extract_flags &= ~ARCHIVE_EXTRACT_NO_AUTODIR;
			break;
		case 'E': /* NetBSD/OpenBSD */
			include_from_file(cpio, optarg);
			break;
		case 'F': /* NetBSD/OpenBSD/GNU cpio */
			cpio->filename = optarg;
			break;
		case 'f': /* POSIX 1997 */
			exclude(cpio, optarg);
			break;
		case 'H': /* GNU cpio (also --format) */
			cpio->format = optarg;
			break;
		case 'h':
			long_help();
			break;
		case 'I': /* NetBSD/OpenBSD */
			cpio->filename = optarg;
			break;
		case 'i': /* POSIX 1997 */
			cpio->mode = opt;
			break;
		case OPTION_INSECURE:
			cpio->extract_flags &= ~ARCHIVE_EXTRACT_SECURE_SYMLINKS;
			cpio->extract_flags &= ~ARCHIVE_EXTRACT_SECURE_NODOTDOT;
			break;
		case 'L': /* GNU cpio */
			cpio->option_follow_links = 1;
			break;
		case 'l': /* POSIX 1997 */
			cpio->option_link = 1;
			break;
		case 'm': /* POSIX 1997 */
			cpio->extract_flags |= ARCHIVE_EXTRACT_TIME;
			break;
		case 'O': /* GNU cpio */
			cpio->filename = optarg;
			break;
		case 'o': /* POSIX 1997 */
			cpio->mode = opt;
			break;
		case 'p': /* POSIX 1997 */
			cpio->mode = opt;
			break;
		case OPTION_QUIET: /* GNU cpio */
			cpio->quiet = 1;
			break;
		case 'R': /* GNU cpio, also --owner */
			if (owner_parse(optarg, &uid, &gid))
				usage();
			if (uid != -1)
				cpio->uid_override = uid;
			if (gid != -1)
				cpio->gid_override = gid;
			break;
		case 'r': /* POSIX 1997 */
			cpio->option_rename = 1;
			break;
		case 't': /* POSIX 1997 */
			cpio->option_list = 1;
			break;
		case 'u': /* POSIX 1997 */
			cpio->extract_flags
			    &= ~ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
			break;
		case 'v': /* POSIX 1997 */
			cpio->verbose++;
			break;
		case OPTION_VERSION: /* GNU convention */
			version();
			break;
#if 0
	        /*
		 * cpio_getopt() handles -W specially, so it's not
		 * available here.
		 */
		case 'W': /* Obscure, but useful GNU convention. */
			break;
#endif
		case 'y': /* tar convention */
			cpio->compress = opt;
			break;
		case 'Z': /* tar convention */
			cpio->compress = opt;
			break;
		case 'z': /* tar convention */
			cpio->compress = opt;
			break;
		default:
			usage();
		}
	}

	/* TODO: Sanity-check args, error out on nonsensical combinations. */

	cpio->argc -= optind;
	cpio->argv += optind;

	switch (cpio->mode) {
	case 'o':
		mode_out(cpio);
		break;
	case 'i':
		while (*cpio->argv != NULL) {
			include(cpio, *cpio->argv);
			--cpio->argc;
			++cpio->argv;
		}
		if (cpio->option_list)
			mode_list(cpio);
		else
			mode_in(cpio);
		break;
	case 'p':
		if (*cpio->argv == NULL || **cpio->argv == '\0')
			cpio_errc(1, 0,
			    "-p mode requires a target directory");
		mode_pass(cpio, *cpio->argv);
		break;
	default:
		cpio_errc(1, 0,
		    "Must specify at least one of -i, -o, or -p");
	}

	return (0);
}

void
usage(void)
{
	const char	*p;

	p = cpio_progname;

	fprintf(stderr, "Brief Usage:\n");
	fprintf(stderr, "  List:    %s -it < archive\n", p);
	fprintf(stderr, "  Extract: %s -i < archive\n", p);
	fprintf(stderr, "  Create:  %s -o < filenames > archive\n", p);
#ifdef HAVE_GETOPT_LONG
	fprintf(stderr, "  Help:    %s --help\n", p);
#else
	fprintf(stderr, "  Help:    %s -h\n", p);
#endif
	exit(1);
}

static const char *long_help_msg =
	"First option must be a mode specifier:\n"
	"  -i Input  -o Output  -p Pass\n"
	"Common Options:\n"
	"  -v    Verbose\n"
	"Create: %p -o [options]  < [list of files] > [archive]\n"
	"  -z, -y  Compress archive with gzip/bzip2\n"
	"  --format {odc|newc|ustar}  Select archive format\n"
	"List: %p -it < [archive]\n"
	"Extract: %p -i [options] < [archive]\n";


/*
 * Note that the word 'bsdcpio' will always appear in the first line
 * of output.
 *
 * In particular, /bin/sh scripts that need to test for the presence
 * of bsdcpio can use the following template:
 *
 * if (cpio --help 2>&1 | grep bsdcpio >/dev/null 2>&1 ) then \
 *          echo bsdcpio; else echo not bsdcpio; fi
 */
static void
long_help(void)
{
	const char	*prog;
	const char	*p;

	prog = cpio_progname;

	fflush(stderr);

	p = (strcmp(prog,"bsdcpio") != 0) ? "(bsdcpio)" : "";
	printf("%s%s: manipulate archive files\n", prog, p);

	for (p = long_help_msg; *p != '\0'; p++) {
		if (*p == '%') {
			if (p[1] == 'p') {
				fputs(prog, stdout);
				p++;
			} else
				putchar('%');
		} else
			putchar(*p);
	}
	version();
}

static void
version(void)
{
	fprintf(stdout,"bsdcpio %s -- %s\n",
	    BSDCPIO_VERSION_STRING,
	    archive_version());
	exit(0);
}

static void
mode_out(struct cpio *cpio)
{
	struct archive_entry *entry, *spare;
	unsigned long blocks;
	int r;

	if (cpio->option_append)
		cpio_errc(1, 0, "Append mode not yet supported.");
	cpio->archive = archive_write_new();
	if (cpio->archive == NULL)
		cpio_errc(1, 0, "Failed to allocate archive object");
	switch (cpio->compress) {
	case 'j': case 'y':
		archive_write_set_compression_bzip2(cpio->archive);
		break;
	case 'z':
		archive_write_set_compression_gzip(cpio->archive);
		break;
	case 'Z':
		archive_write_set_compression_compress(cpio->archive);
		break;
	default:
		archive_write_set_compression_none(cpio->archive);
		break;
	}
	r = archive_write_set_format_by_name(cpio->archive, cpio->format);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(cpio->archive));
	archive_write_set_bytes_per_block(cpio->archive, cpio->bytes_per_block);
	cpio->linkresolver = archive_entry_linkresolver_new();
	archive_entry_linkresolver_set_strategy(cpio->linkresolver,
	    archive_format(cpio->archive));

	r = archive_write_open_file(cpio->archive, cpio->filename);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(cpio->archive));
	process_lines(cpio, "-", file_to_archive);

	/*
	 * The hardlink detection may have queued up a couple of entries
	 * that can now be flushed.
	 */
	entry = NULL;
	archive_entry_linkify(cpio->linkresolver, &entry, &spare);
	while (entry != NULL) {
		entry_to_archive(cpio, entry);
		archive_entry_free(entry);
		entry = NULL;
		archive_entry_linkify(cpio->linkresolver, &entry, &spare);
	}

	r = archive_write_close(cpio->archive);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(cpio->archive));

	if (!cpio->quiet) {
		blocks = (archive_position_uncompressed(cpio->archive) + 511)
			      / 512;
		fprintf(stderr, "%lu %s\n", blocks,
		    blocks == 1 ? "block" : "blocks");
	}
	archive_write_finish(cpio->archive);
}

/*
 * This is used by both out mode (to copy objects from disk into
 * an archive) and pass mode (to copy objects from disk to
 * an archive_write_disk "archive").
 */
static int
file_to_archive(struct cpio *cpio, const char *srcpath)
{
	struct stat st;
	const char *destpath;
	struct archive_entry *entry, *spare;
	size_t len;
	const char *p;
	int lnklen;
	int r;

	/*
	 * Create an archive_entry describing the source file.
	 */
	entry = archive_entry_new();
	if (entry == NULL)
		cpio_errc(1, 0, "Couldn't allocate entry");
	archive_entry_copy_sourcepath(entry, srcpath);

	/* Get stat information. */
	if (cpio->option_follow_links)
		r = stat(srcpath, &st);
	else
		r = lstat(srcpath, &st);
	if (r != 0) {
		cpio_warnc(errno, "Couldn't stat \"%s\"", srcpath);
		archive_entry_free(entry);
		return (0);
	}

	if (cpio->uid_override >= 0)
		st.st_uid = cpio->uid_override;
	if (cpio->gid_override >= 0)
		st.st_gid = cpio->uid_override;
	archive_entry_copy_stat(entry, &st);

	/* If its a symlink, pull the target. */
	if (S_ISLNK(st.st_mode)) {
		lnklen = readlink(srcpath, cpio->buff, cpio->buff_size);
		if (lnklen < 0) {
			cpio_warnc(errno,
			    "%s: Couldn't read symbolic link", srcpath);
			archive_entry_free(entry);
			return (0);
		}
		cpio->buff[lnklen] = 0;
		archive_entry_set_symlink(entry, cpio->buff);
	}

	/*
	 * Generate a destination path for this entry.
	 * "destination path" is the name to which it will be copied in
	 * pass mode or the name that will go into the archive in
	 * output mode.
	 */
	destpath = srcpath;
	if (cpio->destdir) {
		len = strlen(cpio->destdir) + strlen(srcpath) + 8;
		if (len >= cpio->pass_destpath_alloc) {
			while (len >= cpio->pass_destpath_alloc) {
				cpio->pass_destpath_alloc += 512;
				cpio->pass_destpath_alloc *= 2;
			}
			free(cpio->pass_destpath);
			cpio->pass_destpath = malloc(cpio->pass_destpath_alloc);
			if (cpio->pass_destpath == NULL)
				cpio_errc(1, ENOMEM,
				    "Can't allocate path buffer");
		}
		strcpy(cpio->pass_destpath, cpio->destdir);
		p = srcpath;
		while (p[0] == '/')
			++p;
		strcat(cpio->pass_destpath, p);
		destpath = cpio->pass_destpath;
	}
	if (cpio->option_rename)
		destpath = cpio_rename(destpath);
	if (destpath == NULL)
		return (0);
	archive_entry_copy_pathname(entry, destpath);

	/*
	 * If we're trying to preserve hardlinks, match them here.
	 */
	spare = NULL;
	if (cpio->linkresolver != NULL
	    && !S_ISDIR(st.st_mode)) {
		archive_entry_linkify(cpio->linkresolver, &entry, &spare);
	}

	if (entry != NULL) {
		r = entry_to_archive(cpio, entry);
		archive_entry_free(entry);
	}
	if (spare != NULL) {
		if (r == 0)
			r = entry_to_archive(cpio, spare);
		archive_entry_free(spare);
	}
	return (r);
}

static int
entry_to_archive(struct cpio *cpio, struct archive_entry *entry)
{
	const char *destpath = archive_entry_pathname(entry);
	const char *srcpath = archive_entry_sourcepath(entry);
	int fd = -1;
	ssize_t bytes_read;
	int r;

	/* Print out the destination name to the user. */
	if (cpio->verbose)
		fprintf(stderr,"%s", destpath);

	/*
	 * Obviously, this only gets invoked in pass mode.
	 */
	if (cpio->option_link) {
		/* Note: link(2) doesn't create parent directories. */
		archive_entry_set_hardlink(entry, srcpath);
		r = archive_write_header(cpio->archive, entry);
		if (r != ARCHIVE_OK)
			cpio_warnc(archive_errno(cpio->archive),
			    archive_error_string(cpio->archive));
		if (r == ARCHIVE_FATAL)
			exit(1);
		return (0);
	}

	/*
	 * Make sure we can open the file (if necessary) before
	 * trying to write the header.
	 */
	if (archive_entry_filetype(entry) == AE_IFREG) {
		if (archive_entry_size(entry) > 0) {
			fd = open(srcpath, O_RDONLY);
			if (fd < 0) {
				cpio_warnc(errno,
				    "%s: could not open file", srcpath);
				goto cleanup;
			}
		}
	} else {
		archive_entry_set_size(entry, 0);
	}

	r = archive_write_header(cpio->archive, entry);

	if (r != ARCHIVE_OK)
		cpio_warnc(archive_errno(cpio->archive),
		    "%s: %s",
		    destpath,
		    archive_error_string(cpio->archive));

	if (r == ARCHIVE_FATAL)
		exit(1);

	if (r >= ARCHIVE_WARN && fd >= 0) {
		bytes_read = read(fd, cpio->buff, cpio->buff_size);
		while (bytes_read > 0) {
			r = archive_write_data(cpio->archive,
			    cpio->buff, bytes_read);
			if (r < 0)
				cpio_errc(1, archive_errno(cpio->archive),
				    archive_error_string(cpio->archive));
			if (r < bytes_read) {
				cpio_warnc(0,
				    "Truncated write; file may have grown while being archived.");
			}
			bytes_read = read(fd, cpio->buff, cpio->buff_size);
		}
	}

	restore_time(cpio, entry, srcpath, fd);

cleanup:
	if (cpio->verbose)
		fprintf(stderr,"\n");
	if (fd >= 0)
		close(fd);
	return (0);
}

static void
restore_time(struct cpio *cpio, struct archive_entry *entry,
    const char *name, int fd)
{
#ifndef HAVE_UTIMES
	static int warned = 0;

	(void)cpio; /* UNUSED */
	(void)entry; /* UNUSED */
	(void)name; /* UNUSED */
	(void)fd; /* UNUSED */

	if (!warned)
		cpio_warnc(0, "Can't restore access times on this platform");
	warned = 1;
	return;
#else
	struct timeval times[2];

	if (!cpio->option_atime_restore)
		return;

        times[1].tv_sec = archive_entry_mtime(entry);
        times[1].tv_usec = archive_entry_mtime_nsec(entry) / 1000;

        times[0].tv_sec = archive_entry_atime(entry);
        times[0].tv_usec = archive_entry_atime_nsec(entry) / 1000;

#ifdef HAVE_FUTIMES
        if (fd >= 0 && futimes(fd, times) == 0)
		return;
#endif

#ifdef HAVE_LUTIMES
        if (lutimes(name, times) != 0)
#else
        if (!S_ISLNK(archive_entry_mode(entry)) && utimes(name, times) != 0)
#endif
                cpio_warnc(errno, "Can't update time for %s", name);
#endif
}


static void
mode_in(struct cpio *cpio)
{
	struct archive *a;
	struct archive_entry *entry;
	struct archive *ext;
	const char *destpath;
	unsigned long blocks;
	int r;

	ext = archive_write_disk_new();
	if (ext == NULL)
		cpio_errc(1, 0, "Couldn't allocate restore object");
	r = archive_write_disk_set_options(ext, cpio->extract_flags);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(ext));
	a = archive_read_new();
	if (a == NULL)
		cpio_errc(1, 0, "Couldn't allocate archive object");
	archive_read_support_compression_all(a);
	archive_read_support_format_all(a);

	if (archive_read_open_file(a, cpio->filename, cpio->bytes_per_block))
		cpio_errc(1, archive_errno(a),
		    archive_error_string(a));
	for (;;) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			cpio_errc(1, archive_errno(a),
			    archive_error_string(a));
		}
		if (excluded(cpio, archive_entry_pathname(entry)))
			continue;
		if (cpio->option_rename) {
			destpath = cpio_rename(archive_entry_pathname(entry));
			archive_entry_set_pathname(entry, destpath);
		} else
			destpath = archive_entry_pathname(entry);
		if (destpath == NULL)
			continue;
		if (cpio->verbose)
			fprintf(stdout, "%s\n", destpath);
		if (cpio->uid_override >= 0)
			archive_entry_set_uid(entry, cpio->uid_override);
		if (cpio->gid_override >= 0)
			archive_entry_set_gid(entry, cpio->gid_override);
		r = archive_write_header(ext, entry);
		if (r != ARCHIVE_OK) {
			fprintf(stderr, "%s: %s\n",
			    archive_entry_pathname(entry),
			    archive_error_string(ext));
		} else if (archive_entry_size(entry) > 0) {
			r = copy_data(a, ext);
		}
	}
	r = archive_read_close(a);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(a));
	r = archive_write_close(ext);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(ext));
	if (!cpio->quiet) {
		blocks = (archive_position_uncompressed(a) + 511)
			      / 512;
		fprintf(stderr, "%lu %s\n", blocks,
		    blocks == 1 ? "block" : "blocks");
	}
	archive_read_finish(a);
	archive_write_finish(ext);
	exit(0);
}

static int
copy_data(struct archive *ar, struct archive *aw)
{
	int r;
	size_t size;
	const void *block;
	off_t offset;

	for (;;) {
		r = archive_read_data_block(ar, &block, &size, &offset);
		if (r == ARCHIVE_EOF)
			return (ARCHIVE_OK);
		if (r != ARCHIVE_OK) {
			cpio_warnc(archive_errno(ar),
			    "%s", archive_error_string(ar));
			return (r);
		}
		r = archive_write_data_block(aw, block, size, offset);
		if (r != ARCHIVE_OK) {
			cpio_warnc(archive_errno(aw),
			    archive_error_string(aw));
			return (r);
		}
	}
}

static void
mode_list(struct cpio *cpio)
{
	struct archive *a;
	struct archive_entry *entry;
	unsigned long blocks;
	int r;

	a = archive_read_new();
	if (a == NULL)
		cpio_errc(1, 0, "Couldn't allocate archive object");
	archive_read_support_compression_all(a);
	archive_read_support_format_all(a);

	if (archive_read_open_file(a, cpio->filename, cpio->bytes_per_block))
		cpio_errc(1, archive_errno(a),
		    archive_error_string(a));
	for (;;) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			cpio_errc(1, archive_errno(a),
			    archive_error_string(a));
		}
		if (excluded(cpio, archive_entry_pathname(entry)))
			continue;
		if (cpio->verbose) {
			/* TODO: uname/gname lookups */
			/* TODO: Clean this up. */
			fprintf(stdout,
			    "%s%3d %8s%8s " CPIO_FILESIZE_PRINTF " %s\n",
			    archive_entry_strmode(entry),
			    archive_entry_nlink(entry),
			    archive_entry_uname(entry),
			    archive_entry_gname(entry),
			    (CPIO_FILESIZE_TYPE)archive_entry_size(entry),
			    archive_entry_pathname(entry));
		} else
			fprintf(stdout, "%s\n", archive_entry_pathname(entry));
	}
	r = archive_read_close(a);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(a));
	if (!cpio->quiet) {
		blocks = (archive_position_uncompressed(a) + 511)
			      / 512;
		fprintf(stderr, "%lu %s\n", blocks,
		    blocks == 1 ? "block" : "blocks");
	}
	archive_read_finish(a);
	exit(0);
}

static void
mode_pass(struct cpio *cpio, const char *destdir)
{
	int r;

	/* Ensure target dir has a trailing '/' to simplify path surgery. */
	cpio->destdir = malloc(strlen(destdir) + 8);
	strcpy(cpio->destdir, destdir);
	if (destdir[strlen(destdir) - 1] != '/')
		strcat(cpio->destdir, "/");

	cpio->archive = archive_write_disk_new();
	if (cpio->archive == NULL)
		cpio_errc(1, 0, "Failed to allocate archive object");
	r = archive_write_disk_set_options(cpio->archive, cpio->extract_flags);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(cpio->archive));
	cpio->linkresolver = archive_entry_linkresolver_new();
	archive_write_disk_set_standard_lookup(cpio->archive);
	process_lines(cpio, "-", file_to_archive);

	archive_entry_linkresolver_free(cpio->linkresolver);
	r = archive_write_close(cpio->archive);
	if (r != ARCHIVE_OK)
		cpio_errc(1, 0, archive_error_string(cpio->archive));
	archive_write_finish(cpio->archive);
}

/*
 * Prompt for a new name for this entry.  Returns a pointer to the
 * new name or NULL if the entry should not be copied.  This
 * implements the semantics defined in POSIX.1-1996, which specifies
 * that an input of '.' means the name should be unchanged.  GNU cpio
 * treats '.' as a literal new name.
 */
static const char *
cpio_rename(const char *name)
{
	static char buff[1024];
	FILE *t;
	char *p, *ret;

	t = fopen("/dev/tty", "r+");
	if (t == NULL)
		return (name);
	fprintf(t, "%s (Enter/./(new name))? ", name);
	fflush(t);

	p = fgets(buff, sizeof(buff), t);
	fclose(t);
	if (p == NULL)
		/* End-of-file is a blank line. */
		return (NULL);

	while (*p == ' ' || *p == '\t')
		++p;
	if (*p == '\n' || *p == '\0')
		/* Empty line. */
		return (NULL);
	if (*p == '.' && p[1] == '\n')
		/* Single period preserves original name. */
		return (name);
	ret = p;
	/* Trim the final newline. */
	while (*p != '\0' && *p != '\n')
		++p;
	/* Overwrite the final \n with a null character. */
	*p = '\0';
	return (ret);
}


/*
 * Read lines from file and do something with each one.  If option_null
 * is set, lines are terminated with zero bytes; otherwise, they're
 * terminated with newlines.
 *
 * This uses a self-sizing buffer to handle arbitrarily-long lines.
 * If the "process" function returns non-zero for any line, this
 * function will return non-zero after attempting to process all
 * remaining lines.
 */
int
process_lines(struct cpio *cpio, const char *pathname,
    int (*process)(struct cpio *, const char *))
{
	FILE *f;
	char *buff, *buff_end, *line_start, *line_end, *p;
	size_t buff_length, bytes_read, bytes_wanted;
	int separator;
	int ret;

	separator = cpio->option_null ? '\0' : '\n';
	ret = 0;

	if (strcmp(pathname, "-") == 0)
		f = stdin;
	else
		f = fopen(pathname, "r");
	if (f == NULL)
		cpio_errc(1, errno, "Couldn't open %s", pathname);
	buff_length = 8192;
	buff = malloc(buff_length);
	if (buff == NULL)
		cpio_errc(1, ENOMEM, "Can't read %s", pathname);
	line_start = line_end = buff_end = buff;
	for (;;) {
		/* Get some more data into the buffer. */
		bytes_wanted = buff + buff_length - buff_end;
		bytes_read = fread(buff_end, 1, bytes_wanted, f);
		buff_end += bytes_read;
		/* Process all complete lines in the buffer. */
		while (line_end < buff_end) {
			if (*line_end == separator) {
				*line_end = '\0';
				if ((*process)(cpio, line_start) != 0)
					ret = -1;
				line_start = line_end + 1;
				line_end = line_start;
			} else
				line_end++;
		}
		if (feof(f))
			break;
		if (ferror(f))
			cpio_errc(1, errno, "Can't read %s", pathname);
		if (line_start > buff) {
			/* Move a leftover fractional line to the beginning. */
			memmove(buff, line_start, buff_end - line_start);
			buff_end -= line_start - buff;
			line_end -= line_start - buff;
			line_start = buff;
		} else {
			/* Line is too big; enlarge the buffer. */
			p = realloc(buff, buff_length *= 2);
			if (p == NULL)
				cpio_errc(1, ENOMEM,
				    "Line too long in %s", pathname);
			buff_end = p + (buff_end - buff);
			line_end = p + (line_end - buff);
			line_start = buff = p;
		}
	}
	/* At end-of-file, handle the final line. */
	if (line_end > line_start) {
		*line_end = '\0';
		if ((*process)(cpio, line_start) != 0)
			ret = -1;
	}
	free(buff);
	if (f != stdin)
		fclose(f);
	return (ret);
}
