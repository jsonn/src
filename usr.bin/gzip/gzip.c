/*	$NetBSD: gzip.c,v 1.29.2.15 2004/06/18 09:56:35 tron Exp $	*/

/*
 * Copyright (c) 1997, 1998, 2003, 2004 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1997, 1998, 2003, 2004 Matthew R. Green\n\
     All rights reserved.\n");
__RCSID("$NetBSD: gzip.c,v 1.29.2.15 2004/06/18 09:56:35 tron Exp $");
#endif /* not lint */

/*
 * gzip.c -- GPL free gzip using zlib.
 *
 * TODO:
 *	- handle .taz/.tgz files?
 *	- use mmap where possible
 *	- handle some signals better (remove outfile?)
 *	- make bzip2/compress -v/-t/-l support work as well as possible
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <zlib.h>
#include <fts.h>
#include <libgen.h>
#include <stdarg.h>
#include <getopt.h>

/* what type of file are we dealing with */
enum filetype {
	FT_GZIP,
#ifndef NO_BZIP2_SUPPORT
	FT_BZIP2,
#endif
#ifndef NO_COMPRESS_SUPPORT
	FT_Z,
#endif
	FT_LAST,
	FT_UNKNOWN
};

#ifndef NO_BZIP2_SUPPORT
#include <bzlib.h>

#define BZ2_SUFFIX	".bz2"
#define BZIP2_MAGIC	"\102\132\150"
#endif

#ifndef NO_COMPRESS_SUPPORT
#define Z_SUFFIX	".Z"
#define Z_MAGIC		"\037\235"
#endif

#define GZ_SUFFIX	".gz"

#define BUFLEN		(64 * 1024)

#define GZIP_MAGIC0	0x1F
#define GZIP_MAGIC1	0x8B
#define GZIP_OMAGIC1	0x9E

#define GZIP_TIMESTAMP	(off_t)4
#define GZIP_ORIGNAME	(off_t)10

#define HEAD_CRC	0x02
#define EXTRA_FIELD	0x04
#define ORIG_NAME	0x08
#define COMMENT		0x10

#define OS_CODE		3	/* Unix */

static	const char	gzip_version[] = "NetBSD gzip 20040524";

static	int	cflag;			/* stdout mode */
static	int	dflag;			/* decompress mode */
static	int	lflag;			/* list mode */
static	int	numflag = 6;		/* gzip -1..-9 value */

#ifndef SMALL
static	int	fflag;			/* force mode */
static	int	nflag;			/* don't save name/timestamp */
static	int	Nflag;			/* don't restore name/timestamp */
static	int	qflag;			/* quiet mode */
static	int	rflag;			/* recursive mode */
static	int	tflag;			/* test */
static	char	*Sflag;
static	int	vflag;			/* verbose mode */
#else
#define		qflag	0
#endif

static	int	exit_value = 0;		/* exit value */

static	const char	*suffix;
#define suffix_len	(strlen(suffix) + 1)	/* len + nul */
static	char	*infile;		/* name of file coming in */

static	void	maybe_err(const char *fmt, ...);
static	void	maybe_errx(const char *fmt, ...);
static	void	maybe_warn(const char *fmt, ...);
static	void	maybe_warnx(const char *fmt, ...);
static	enum filetype file_gettype(u_char *);
static	int	check_outfile(const char *outfile, struct stat *sb);
static	off_t	gz_compress(FILE *, int, off_t *, const char *, time_t);
static	off_t	gz_uncompress(int, int, char *, size_t, off_t *, const char *);
static	off_t	file_compress(char *, char *, size_t);
static	off_t	file_uncompress(char *, char *, size_t);
static	void	handle_pathname(char *);
static	void	handle_file(char *, struct stat *);
static	void	handle_stdin(void);
static	void	handle_stdout(void);
static	void	print_ratio(off_t, off_t, FILE *);
static	void	print_list(int fd, off_t, const char *, time_t);
static	void	usage(void);
static	void	display_version(void);

#ifndef SMALL
static	void	prepend_gzip(char *, int *, char ***);
static	void	handle_dir(char *, struct stat *);
static	void	print_verbage(char *, char *, off_t, off_t);
static	void	print_test(const char *, int);
static	void	copymodes(const char *, struct stat *);
#endif

#ifndef NO_BZIP2_SUPPORT
static	off_t	unbzip2(int, int, char *, size_t, off_t *);
#endif

#ifndef NO_COMPRESS_SUPPORT
static	FILE 	*zopen(const char *, FILE *);
static	off_t	zuncompress(FILE *, FILE *, char *, size_t, off_t *);
#endif

int main(int, char *p[]);

#ifdef SMALL
#define getopt_long(a,b,c,d,e) getopt(a,b,c)
#else
static const struct option longopts[] = {
	{ "stdout",		no_argument,		0,	'c' },
	{ "to-stdout",		no_argument,		0,	'c' },
	{ "decompress",		no_argument,		0,	'd' },
	{ "uncompress",		no_argument,		0,	'd' },
	{ "force",		no_argument,		0,	'f' },
	{ "help",		no_argument,		0,	'h' },
	{ "list",		no_argument,		0,	'l' },
	{ "no-name",		no_argument,		0,	'n' },
	{ "name",		no_argument,		0,	'N' },
	{ "quiet",		no_argument,		0,	'q' },
	{ "recursive",		no_argument,		0,	'r' },
	{ "suffix",		required_argument,	0,	'S' },
	{ "test",		no_argument,		0,	't' },
	{ "verbose",		no_argument,		0,	'v' },
	{ "version",		no_argument,		0,	'V' },
	{ "fast",		no_argument,		0,	'1' },
	{ "best",		no_argument,		0,	'9' },
#if 0
	/*
	 * This is what else GNU gzip implements.  --ascii isn't useful
	 * on NetBSD, and I don't care to have a --license.
	 */
	{ "ascii",		no_argument,		0,	'a' },
	{ "license",		no_argument,		0,	'L' },
#endif
	{ NULL,			no_argument,		0,	0 },
};
#endif

int
main(int argc, char **argv)
{
	const char *progname = getprogname();
#ifndef SMALL
	char *gzip;
#endif
	int ch;

	/* XXX set up signals */

	suffix = GZ_SUFFIX;;

#ifndef SMALL
	if ((gzip = getenv("GZIP")) != NULL)
		prepend_gzip(gzip, &argc, &argv);
#endif

	/*
	 * XXX
	 * handle being called `gunzip', `zcat' and `gzcat'
	 */
	if (strcmp(progname, "gunzip") == 0)
		dflag = 1;
	else if (strcmp(progname, "zcat") == 0 ||
		 strcmp(progname, "gzcat") == 0)
		dflag = cflag = 1;

#ifdef SMALL
#define OPT_LIST "cdhHltV123456789"
#else
#define OPT_LIST "cdfhHlnNqrS:tvV123456789"
#endif

	while ((ch = getopt_long(argc, argv, OPT_LIST, longopts, NULL)) != -1)
		switch (ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'l':
			lflag = 1;
			dflag = 1;
			break;
		case 'V':
			display_version();
			/* NOTREACHED */
		case '1': case '2': case '3':
		case '4': case '5': case '6':
		case '7': case '8': case '9':
			numflag = ch - '0';
			break;
#ifndef SMALL
		case 'f':
			fflag = 1;
			break;
		case 'n':
			nflag = 1;
			Nflag = 0;
			break;
		case 'N':
			nflag = 0;
			Nflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'S':
			Sflag = optarg;
			break;
		case 't':
			cflag = 1;
			tflag = 1;
			dflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
#endif
		default:
			usage();
			/* NOTREACHED */
		}
	argv += optind;
	argc -= optind;

	if (argc == 0) {
		if (dflag)	/* stdin mode */
			handle_stdin();
		else		/* stdout mode */
			handle_stdout();
	} else {
		do {
			handle_pathname(argv[0]);
		} while (*++argv);
	}
#ifndef SMALL
	if (qflag == 0 && lflag && argc > 1)
		print_list(-1, 0, "(totals)", 0);
#endif
	exit(exit_value);
}

/* maybe print a warning */
void
maybe_warn(const char *fmt, ...)
{
	va_list ap;

	if (qflag == 0) {
		va_start(ap, fmt);
		vwarn(fmt, ap);
		va_end(ap);
	}
	if (exit_value == 0)
		exit_value = 1;
}

/* ... without an errno. */
void
maybe_warnx(const char *fmt, ...)
{
	va_list ap;

	if (qflag == 0) {
		va_start(ap, fmt);
		vwarnx(fmt, ap);
		va_end(ap);
	}
	if (exit_value == 0)
		exit_value = 1;
}

/* maybe print an error */
void
maybe_err(const char *fmt, ...)
{
	va_list ap;

	if (qflag == 0) {
		va_start(ap, fmt);
		vwarn(fmt, ap);
		va_end(ap);
	}
	exit(2);
}

/* ... without an errno. */
void
maybe_errx(const char *fmt, ...)
{
	va_list ap;

	if (qflag == 0) {
		va_start(ap, fmt);
		vwarnx(fmt, ap);
		va_end(ap);
	}
	exit(2);
}

#ifndef SMALL
/* split up $GZIP and prepend it to the argument list */
static void
prepend_gzip(char *gzip, int *argc, char ***argv)
{
	char *s, **nargv, **ac;
	int nenvarg = 0, i;

	/* scan how many arguments there are */
	for (s = gzip; *s; s++) {
		if (*s == ' ' || *s == '\t')
			continue;
		nenvarg++;
		for (; *s; s++)
			if (*s == ' ' || *s == '\t')
				break;
		if (*s == 0x0)
			break;
	}
	/* punt early */
	if (nenvarg == 0)
		return;

	*argc += nenvarg;
	ac = *argv;

	nargv = (char **)malloc((*argc + 1) * sizeof(char *));
	if (nargv == NULL)
		maybe_err("malloc");

	/* stash this away */
	*argv = nargv;

	/* copy the program name first */
	i = 0;
	nargv[i++] = *(ac++);

	/* take a copy of $GZIP and add it to the array */
	s = strdup(gzip);
	if (s == NULL)
		maybe_err("strdup");
	for (; *s; s++) {
		if (*s == ' ' || *s == '\t')
			continue;
		nargv[i++] = s;
		for (; *s; s++)
			if (*s == ' ' || *s == '\t') {
				*s = 0;
				break;
			}
	}

	/* copy the original arguments and a NULL */
	while (*ac)
		nargv[i++] = *(ac++);
	nargv[i] = NULL;
}
#endif

/* compress input to output then close both files */
static off_t
gz_compress(FILE *in, int out, off_t *gsizep, const char *origname, time_t mtime)
{
	z_stream z;
	char inbuf[BUFLEN], outbuf[BUFLEN];
	off_t in_tot = 0, out_tot = 0;
	ssize_t in_size;
	char *str;
	int i, error;
	uLong crc;

	i = asprintf(&str, "%c%c%c%c%c%c%c%c%c%c%s", 
		     GZIP_MAGIC0, GZIP_MAGIC1,
		     Z_DEFLATED, origname ? ORIG_NAME : 0,
		     (int)mtime & 0xff,
		     (int)(mtime >> 8) & 0xff,
		     (int)(mtime >> 16) & 0xff,
		     (int)(mtime >> 24) & 0xff,
		     0, OS_CODE, origname ? origname : "");
	if (i == -1)     
		maybe_err("asprintf");
	if (origname)
		i++;
	if (write(out, str, i) != i) {
		maybe_warn("write");
		in_tot = -1;
		goto out;
	}
	free(str);

	memset(&z, 0, sizeof z);
	z.next_out = outbuf;
	z.avail_out = sizeof outbuf;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = 0;

	error = deflateInit2(&z, numflag, Z_DEFLATED,
			     -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
	if (error != Z_OK) {
		maybe_warnx("deflateInit2 failed");
		in_tot = -1;
		goto out;
	}

	crc = crc32(0L, Z_NULL, 0);
	for (;;) {
		if (z.avail_out == 0) {
			if (write(out, outbuf, sizeof outbuf) != sizeof outbuf) {
				maybe_warn("write");
				in_tot = -1;
				goto out;
			}

			out_tot += sizeof outbuf;
			z.next_out = outbuf;
			z.avail_out = sizeof outbuf;
		}

		if (z.avail_in == 0) {
			in_size = fread(inbuf, 1, sizeof inbuf, in);
			if (ferror(in)) {
				maybe_warn("fread");
				in_tot = -1;
				goto out;
			}
			if (in_size == 0)
				break;

			crc = crc32(crc, (const Bytef *)inbuf, (unsigned)in_size);
			in_tot += in_size;
			z.next_in = inbuf;
			z.avail_in = in_size;
		}

		error = deflate(&z, Z_NO_FLUSH);
		if (error != Z_OK && error != Z_STREAM_END) {
			maybe_warnx("deflate failed");
			in_tot = -1;
			goto out;
		}
	}

	/* clean up */
	for (;;) {
		size_t len;

		error = deflate(&z, Z_FINISH);
		if (error != Z_OK && error != Z_STREAM_END) {
			maybe_warnx("deflate failed");
			in_tot = -1;
			goto out;
		}

		len = sizeof outbuf - z.avail_out;

		if (write(out, outbuf, len) != len) {
			maybe_warn("write");
			out_tot = -1;
			goto out;
		}
		out_tot += len;
		z.next_out = outbuf;
		z.avail_out = sizeof outbuf;

		if (error == Z_STREAM_END)
			break;
	}

	if (deflateEnd(&z) != Z_OK) {
		maybe_warnx("deflateEnd failed");
		in_tot = -1;
		goto out;
	}

	i = asprintf(&str, "%c%c%c%c%c%c%c%c", 
		 (int)crc & 0xff,
		 (int)(crc >> 8) & 0xff,
		 (int)(crc >> 16) & 0xff,
		 (int)(crc >> 24) & 0xff,
		 (int)in_tot & 0xff,
		 (int)(in_tot >> 8) & 0xff,
		 (int)(in_tot >> 16) & 0xff,
		 (int)(in_tot >> 24) & 0xff);
	if (i != 8)
		maybe_err("asprintf");
	if (write(out, str, i) != i) {
		maybe_warn("write");
		in_tot = -1;
	}
	free(str);

out:
	if (gsizep)
		*gsizep = out_tot;
	return in_tot;
}

/*
 * uncompress input to output then close the input.  return the
 * uncompressed size written, and put the compressed sized read
 * into `*gsizep'.
 */
static off_t
gz_uncompress(int in, int out, char *pre, size_t prelen, off_t *gsizep,
	      const char *filename)
{
	z_stream z;
	char outbuf[BUFLEN], inbuf[BUFLEN];
	off_t out_tot, in_tot;
	enum {
		GZSTATE_MAGIC0,
		GZSTATE_MAGIC1,
		GZSTATE_METHOD,
		GZSTATE_FLAGS,
		GZSTATE_SKIPPING,
		GZSTATE_EXTRA,
		GZSTATE_EXTRA2,
		GZSTATE_EXTRA3,
		GZSTATE_ORIGNAME,
		GZSTATE_COMMENT,
		GZSTATE_HEAD_CRC1,
		GZSTATE_HEAD_CRC2,
		GZSTATE_INIT,
		GZSTATE_READ 
	} state = GZSTATE_MAGIC0;
	int flags = 0, skip_count = 0;
	int error, done_reading = 0;

#define ADVANCE()       { z.next_in++; z.avail_in--; }

	memset(&z, 0, sizeof z);
	z.avail_in = prelen;
	z.next_in = pre;
	z.avail_out = sizeof outbuf;
	z.next_out = outbuf;
	z.zalloc = NULL;
	z.zfree = NULL;
	z.opaque = 0;

	in_tot = prelen;
	out_tot = 0;

	for (;;) {
		if (z.avail_in == 0 && done_reading == 0) {
			size_t in_size = read(in, inbuf, BUFLEN);

			if (in_size == -1) {
#ifndef SMALL
				if (tflag && vflag)
					print_test(filename, 0);
#endif
				maybe_warn("failed to read stdin\n");
				out_tot = -1;
				goto stop;
			} else if (in_size == 0)
				done_reading = 1;

			z.avail_in = in_size;
			z.next_in = inbuf;

			in_tot += in_size;
		}
		switch (state) {
		case GZSTATE_MAGIC0:
			if (*z.next_in != GZIP_MAGIC0) {
				maybe_warn("input not gziped\n");
				out_tot = -1;
				goto stop;
			}
			ADVANCE();
			state++;
			break;

		case GZSTATE_MAGIC1:
			if (*z.next_in != GZIP_MAGIC1 &&
			    *z.next_in != GZIP_OMAGIC1) {
				maybe_warn("input not gziped\n");
				out_tot = -1;
				goto stop;
			}
			ADVANCE();
			state++;
			break;

		case GZSTATE_METHOD:
			if (*z.next_in != Z_DEFLATED) {
				maybe_warn("unknown compression method\n");
				out_tot = -1;
				goto stop;
			}
			ADVANCE();
			state++;
			break;

		case GZSTATE_FLAGS:
			flags = *z.next_in;
			ADVANCE();
			skip_count = 6;
			state++;
			break;

		case GZSTATE_SKIPPING:
			if (skip_count > 0) {
				skip_count--;
				ADVANCE();
			} else
				state++;
			break;

		case GZSTATE_EXTRA:
			if ((flags & EXTRA_FIELD) == 0) {
				state = GZSTATE_ORIGNAME;
				break;
			}
			skip_count = *z.next_in;
			ADVANCE();
			state++;
			break;

		case GZSTATE_EXTRA2:
			skip_count |= ((*z.next_in) << 8);
			ADVANCE();
			state++;
			break;

		case GZSTATE_EXTRA3:
			if (skip_count > 0) {
				skip_count--;
				ADVANCE();
			} else
				state++;
			break;

		case GZSTATE_ORIGNAME:
			if ((flags & ORIG_NAME) == 0) {
				state++;
				break;
			}
			if (*z.next_in == 0)
				state++;
			ADVANCE();
			break;

		case GZSTATE_COMMENT:
			if ((flags & COMMENT) == 0) {
				state++;
				break;
			}
			if (*z.next_in == 0)
				state++;
			ADVANCE();
			break;

		case GZSTATE_HEAD_CRC1:
			if (flags & HEAD_CRC)
				skip_count = 2;
			else
				skip_count = 0;
			state++;
			break;

		case GZSTATE_HEAD_CRC2:
			if (skip_count > 0) {
				skip_count--;
				ADVANCE();
			} else
				state++;
			break;

		case GZSTATE_INIT:
			if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
				maybe_warn("failed to inflateInit\n");
				out_tot = -1;
				goto stop;
			}
			state++;
			break;

		case GZSTATE_READ:
			error = inflate(&z, Z_FINISH);
			if (error == Z_STREAM_END || error == Z_BUF_ERROR) {
				size_t wr = BUFLEN - z.avail_out;

				/*
				 * If there's nothing to output, we must
				 * have an error...
				 */
				if (wr == 0) {
					out_tot = -1;
					goto stop;
				}
				if (
#ifndef SMALL
				    /* don't write anything with -t */
				    tflag == 0 &&
#endif
				    write(out, outbuf, wr) != wr) {
					maybe_warn("error writing "
						     "to output\n");
					out_tot = -1;
					goto stop;
				}

				out_tot += wr;

				if (error == Z_STREAM_END)
					goto stop;

				z.next_out = outbuf;
				z.avail_out = BUFLEN;

				break;
			}
			if (error < 0) {
				maybe_warnx("decompression error\n");
				out_tot = -1;
				goto stop;
			}
			break;
		}
		continue;
stop:
		break;
	}
	if (state > GZSTATE_INIT)
		inflateEnd(&z);

#ifndef SMALL
	if (tflag && vflag)
		print_test(filename, out_tot != -1);
#endif

	if (gsizep)
		*gsizep = in_tot;
	return (out_tot);
}

#ifndef SMALL
/*
 * set the owner, mode, flags & utimes for a file
 */
static void
copymodes(const char *file, struct stat *sbp)
{
	struct timeval times[2];

	/*
	 * If we have no info on the input, give this file some
	 * default values and return..
	 */
	if (sbp == NULL) {
		mode_t mask = umask(022);

		(void)chmod(file, DEFFILEMODE & ~mask);
		(void)umask(mask);
		return; 
	}

	/* if the chown fails, remove set-id bits as-per compress(1) */
	if (chown(file, sbp->st_uid, sbp->st_gid) < 0) {
		if (errno != EPERM)
			maybe_warn("couldn't chown: %s", file);
		sbp->st_mode &= ~(S_ISUID|S_ISGID);
	}

	/* we only allow set-id and the 9 normal permission bits */
	sbp->st_mode &= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
	if (chmod(file, sbp->st_mode) < 0)
		maybe_warn("couldn't chmod: %s", file);

	/* only try flags if they exist already */
        if (sbp->st_flags != 0 && chflags(file, sbp->st_flags) < 0)
		maybe_warn("couldn't chflags: %s", file);

	TIMESPEC_TO_TIMEVAL(&times[0], &sbp->st_atimespec);
	TIMESPEC_TO_TIMEVAL(&times[1], &sbp->st_mtimespec);
	if (utimes(file, times) < 0)
		maybe_warn("couldn't utimes: %s", file);
}
#endif

/* what sort of file is this? */
static enum filetype
file_gettype(u_char *buf)
{

	if (buf[0] == GZIP_MAGIC0 &&
	    (buf[1] == GZIP_MAGIC1 || buf[1] == GZIP_OMAGIC1))
		return FT_GZIP;
	else
#ifndef NO_BZIP2_SUPPORT
	if (memcmp(buf, BZIP2_MAGIC, 3) == 0 &&
	    buf[3] >= '0' && buf[3] <= '9')
		return FT_BZIP2;
	else
#endif
#ifndef NO_COMPRESS_SUPPORT
	if (memcmp(buf, Z_MAGIC, 2) == 0)
		return FT_Z;
	else
#endif
		return FT_UNKNOWN;
}

#ifndef SMALL
/* check the outfile is OK. */
static int
check_outfile(const char *outfile, struct stat *sb)
{
	int ok = 1;

	if (fflag == 0 && lflag == 0 && stat(outfile, sb) == 0) {
		if (isatty(STDIN_FILENO)) {
			char ans[10] = { 'n', '\0' };	/* default */

			fprintf(stderr, "%s already exists -- do you wish to "
					"overwrite (y or n)? " , outfile);
			(void)fgets(ans, sizeof(ans) - 1, stdin);
			if (ans[0] != 'y' && ans[0] != 'Y') {
				fprintf(stderr, "\tnot overwritting\n");
				ok = 0;
			} else
				unlink(outfile);
		} else {
			maybe_warnx("%s already exists -- skipping", outfile);
			ok = 0;
		}
	}
	return ok;
}
#endif

/*
 * compress the given file: create a corresponding .gz file and remove the
 * original.
 */
static off_t
file_compress(char *file, char *outfile, size_t outsize)
{
	FILE *in;
	int out;
	struct stat isb, osb;
	off_t size;
#ifndef SMALL
	u_int32_t mtime = 0;
	char *savename;
#endif

	if (cflag == 0) {
		(void)strncpy(outfile, file, outsize - suffix_len);
		outfile[outsize - suffix_len] = '\0';
		(void)strlcat(outfile, suffix, outsize);

#ifndef SMALL
		if (check_outfile(outfile, &osb) == 0)
			goto lose;
	
		if (stat(file, &isb) == 0) {
			if (isb.st_nlink > 1 && fflag == 0) {
				maybe_warnx("%s has %d other link%s -- "
					    "skipping", file, isb.st_nlink - 1,
					    isb.st_nlink == 1 ? "" : "s");
				goto lose;
			}
			if (nflag == 0)
				mtime = (u_int32_t)isb.st_mtime;
		}
#endif
	}
	in = fopen(file, "r");
	if (in == NULL) {
		maybe_warn("can't fopen %s", file);
		goto lose;
	}

#ifndef SMALL
	if (nflag == 0)
		savename = basename(file);
	else
		savename = NULL;
#endif
	if (cflag == 0) {
		out = open(outfile, O_WRONLY|O_CREAT|O_EXCL, 0600);
		if (out == -1) {
			maybe_warn("could not create output: %s", outfile);
			goto lose;
		}
	} else
		out = STDOUT_FILENO;

#ifdef SMALL
	gz_compress(in, out, NULL, NULL, 0);
#else
	gz_compress(in, out, NULL, savename, mtime);
#endif

	(void)fclose(in);

	/*
	 * if we compressed to stdout, we don't know the size and
	 * we don't know the new file name, punt.  if we can't stat
	 * the file, whine, otherwise set the size from the stat
	 * buffer.  we only blow away the file if we can stat the
	 * output, just in case.
	 */
	if (cflag == 0) {
		if (close(out) == -1)
			maybe_warn("couldn't close ouput");

		if (stat(outfile, &osb) < 0) {
			maybe_warn("couldn't stat: %s", outfile);
			maybe_warnx("leaving original %s", file);
			size = 0;
		} else {
			unlink(file);
			size = osb.st_size;
		}
#ifndef SMALL
		copymodes(outfile, &isb);
#endif
	} else {
lose:
		size = -1;
	}

	return (size);
}

/* uncompress the given file and remove the original */
static off_t
file_uncompress(char *file, char *outfile, size_t outsize)
{
	struct stat isb, osb;
	char *s;
	off_t size;
	ssize_t len = strlen(file);
	unsigned char header1[4], name[PATH_MAX + 1];
	enum filetype method;
	int fd, zfd;
#ifndef SMALL
	time_t timestamp = 0;
#endif

	/* gather the old name info */

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		maybe_warn("can't open %s", file);
		goto lose;
	}
	if (read(fd, header1, sizeof header1) != sizeof header1) {
		/* we don't want to fail here. */
#ifndef SMALL
		if (fflag)
			goto lose_close_it;
#endif
		maybe_warn("can't read %s", file);
		goto lose;
	}

	method = file_gettype(header1);

#ifndef SMALL
	if (Sflag == NULL) {
# ifndef NO_BZIP2_SUPPORT
		if (method == FT_BZIP2)
			suffix = BZ2_SUFFIX;
		else
# endif
# ifndef NO_COMPRESS_SUPPORT
		if (method == FT_Z)
			suffix = Z_SUFFIX;
# endif
	}

	if (fflag == 0 && method == FT_UNKNOWN) {
		maybe_warnx("%s: not in gzip format", file);
		goto lose_close_it;
	}
#endif

	if (cflag == 0 || lflag) {
		s = &file[len - suffix_len + 1];
		if (strncmp(s, suffix, suffix_len) == 0) {
			(void)strncpy(outfile, file, len - suffix_len + 1);
			outfile[len - suffix_len + 1] = '\0';
		} else if (lflag == 0) {
			maybe_warnx("unknown suffix %s", s);
			goto lose_close_it;
		}
	}

#ifdef SMALL
	if (method == FT_GZIP && lflag)
#else
	if (method == FT_GZIP && (Nflag || lflag))
#endif
	{
#ifndef SMALL
		unsigned char header2[4];	/* timestamp */

		if (lseek(fd, GZIP_TIMESTAMP, SEEK_SET) == -1) {
			maybe_warn("can't lseek %s", file);
			goto close_header_read;
		}
		if (read(fd, header2, sizeof header2) != sizeof header2) {
			if (fflag)
				goto lose_close_it;
			maybe_warn("can't read %s", file);
			goto lose;
               }
               timestamp = ((time_t)header2[3] << 24)
                         + ((time_t)header2[2] << 16)
                         + ((time_t)header2[1] << 8)
                         +  (time_t)header2[0];
#endif

		if (header1[3] & ORIG_NAME) {
			size_t rbytes;
			int i;

			if (lseek(fd, GZIP_ORIGNAME, SEEK_SET) == -1) {
				maybe_warn("can't lseek %s", file);
				goto close_header_read;
			}
			rbytes = read(fd, name, PATH_MAX + 1);
			if (rbytes < 0) {
				maybe_warn("can't read %s", file);
				goto lose_close_it;
			}
			for (i = 0; i < rbytes && name[i]; i++)
				;
			if (i < rbytes) {
				name[i] = 0;
				/* now maybe merge old dirname */
				if (strchr(outfile, '/') == NULL)
					(void) strlcpy(outfile, name, outsize);
				else {
					char	newbuf[PATH_MAX + 1];

					(void) snprintf(newbuf, sizeof(newbuf),
						"%s/%s", dirname(outfile),
						name);
					(void) strlcpy(outfile, newbuf,
						outsize);
				}
			}
		}
	}
close_header_read:
	close(fd);

	if (cflag == 0 || lflag) {
#ifndef SMALL
		if (check_outfile(outfile, &osb) == 0)
			goto lose;

#endif
		if (stat(file, &isb) == 0) {
#ifndef SMALL
			if (isb.st_nlink > 1 && lflag == 0 && fflag == 0) {
				maybe_warnx("%s has %d other links -- skipping",
				    file, isb.st_nlink - 1);
				goto lose;
			}
			if (nflag == 0 && timestamp)
				isb.st_mtime = timestamp;
#endif
		} else
			goto lose;
	}

	if (cflag == 0 && lflag == 0) {
		zfd = open(outfile, O_WRONLY|O_CREAT|O_EXCL, 0600);
		if (zfd == -1) {
			maybe_warn("can't open %s", outfile);
			goto lose;
		}
	} else
		zfd = STDOUT_FILENO;

#ifndef NO_BZIP2_SUPPORT
	if (method == FT_BZIP2) {
		int in;

		/* XXX */
		if (lflag) {
			maybe_warnx("no -l with bzip2 files");
			goto lose;
		}

		if ((in = open(file, O_RDONLY)) == -1) {
			maybe_warn("open for read: %s", file);
			goto lose;
		}

		size = unbzip2(in, zfd, NULL, 0, NULL);
		if (size == -1) {
			if (cflag == 0)
				unlink(outfile);
			maybe_warnx("%s: uncompress failed", file);
			goto lose;
		}
		if (close(in) != 0)
			maybe_warn("couldn't close input");
		if (cflag == 0 && close(zfd) != 0)
			maybe_warn("couldn't close output");
	} else
#endif

#ifndef NO_COMPRESS_SUPPORT
	if (method == FT_Z) {
		FILE *in, *out;

		/* XXX */
		if (lflag) {
			maybe_warnx("no -l with Lempel-Ziv files");
			goto lose;
		}

		if ((in = zopen(file, NULL)) == NULL) {
			maybe_warn("open for read: %s", file);
			goto lose;
		}

		out = fdopen(zfd, "w");
		if (out == NULL) {
			maybe_warn("open for write: %s", outfile);
			goto lose;
		}

		size = zuncompress(in, out, NULL, 0, NULL);
		if (ferror(in) || fclose(in) != 0) {
			unlink(outfile);
			(void)fclose(out);
			maybe_warn("failed infile fclose");
		}
		if (cflag == 0) {
			if (size == -1) {
				maybe_warnx("%s: uncompress failed", file);
				(void)fclose(out);
				unlink(outfile);
				goto lose;
			}
			if (fclose(out) != 0) {
				unlink(outfile);
				maybe_warn("failed outfile close");
				goto lose;
			}
		}
	} else
#endif
	{
		int in;

		if (lflag) {
			if ((zfd = open(file, O_RDONLY)) == -1) {
				maybe_warn("open: %s", file);
				goto lose;
			}
			print_list(zfd, isb.st_size, outfile, isb.st_mtime);
			return 0;	/* XXX */
		}

		in = open(file, O_RDONLY);
		if (in == -1) {
			maybe_warn("can't open %s", file);
			goto lose;
		}

		size = gz_uncompress(in, zfd, NULL, 0, NULL, file);
		(void)close(in);
		if (cflag == 0) {
			if (close(zfd))
				maybe_warn("failed close");
			if (size == -1) {
				maybe_warnx("%s: uncompress failed", file);
				unlink(outfile);
				goto lose;
			}
		}
	}

	/* if testing, or we uncompressed to stdout, this is all we need */
#ifndef SMALL
	if (tflag)
		return (size);
#endif
	if (cflag)
		return (size);

	/*
	 * if we create a file...
	 */
	if (cflag == 0) {
		/*
		 * if we can't stat the file, or we are uncompressing to
		 * stdin, don't remove the file.
		 */
		if (stat(outfile, &osb) < 0) {
			maybe_warn("couldn't stat (leaving original): %s",
				   outfile);
			goto lose;
		}
		if (osb.st_size != size) {
			maybe_warn("stat gave different size: %llu != %llu "
			    "(leaving original)",
			    (unsigned long long)size,
			    (unsigned long long)osb.st_size);
			goto lose;
		}
		if (cflag == 0)
			unlink(file);
		size = osb.st_size;
#ifndef SMALL
		copymodes(outfile, &isb);
#endif
	}
	return (size);

lose_close_it:
	close(fd);
lose:
	return -1;
}

#ifndef SMALL
static off_t
cat_stdin(unsigned char * prepend, size_t count, off_t *gsizep)
{
	char buf[BUFLEN];
	size_t rv;
	off_t in_tot;

	in_tot = count;
	if (write(STDOUT_FILENO, prepend, count) != count) {
		maybe_warn("write to stdout");
		return -1;
	}
	for (;;) {
		rv = read(STDIN_FILENO, buf, sizeof buf);

		if (write(STDOUT_FILENO, buf, rv) != rv) {
			maybe_warn("write to stdout");
			return -1;
		}
		in_tot += rv;
	}

	if (gsizep)
		*gsizep = in_tot;
	return (in_tot);
}
#endif

static void
handle_stdin(void)
{
	unsigned char header1[4];
	off_t usize, gsize;
	enum filetype method;
#ifndef NO_COMPRESS_SUPPORT
	FILE *in;
#endif

#ifndef SMALL
	if (fflag == 0 && lflag == 0 && isatty(STDIN_FILENO)) {
		maybe_warnx("standard input is a terminal -- ignoring");
		return;
	}
#endif

	if (lflag) {
		struct stat isb;

		/* XXX could read the whole file, etc. */
		if (fstat(STDIN_FILENO, &isb) < 0) {
			maybe_warn("fstat");
			return;
		}
		print_list(STDIN_FILENO, isb.st_size, "stdout", isb.st_mtime);
		return;
	}

	if (read(STDIN_FILENO, header1, sizeof header1) != sizeof header1) {
		maybe_warn("can't read stdin");
		return;
	}

	method = file_gettype(header1);
	switch (method) {
	default:
#ifndef SMALL
		if (fflag == 0) {
			maybe_warnx("unknown compression format");
			return;
		}
		usize = cat_stdin(header1, sizeof header1, &gsize);
		break;
#endif
	case FT_GZIP:
		usize = gz_uncompress(STDIN_FILENO, STDOUT_FILENO, 
			      header1, sizeof header1, &gsize, "(stdin)");
		break;
#ifndef NO_BZIP2_SUPPORT
	case FT_BZIP2:
		usize = unbzip2(STDIN_FILENO, STDOUT_FILENO,
				header1, sizeof header1, &gsize);
		break;
#endif
#ifndef NO_COMPRESS_SUPPORT
	case FT_Z:
		if ((in = zopen(NULL, stdin)) == NULL) {
			maybe_warnx("zopen of stdin");
			return;
		}

		usize = zuncompress(in, stdout, header1, sizeof header1, &gsize);
		break;
#endif
	}

#ifndef SMALL
        if (vflag && !tflag && usize != -1 && gsize != -1)
		print_verbage(NULL, 0, usize, gsize);
#endif 

}

static void
handle_stdout(void)
{
	off_t gsize, usize;

#ifndef SMALL
	if (fflag == 0 && isatty(STDOUT_FILENO)) {
		maybe_warnx("standard output is a terminal -- ignoring");
		return;
	}
#endif
	usize = gz_compress(stdin, STDOUT_FILENO, &gsize, NULL, 0);

#ifndef SMALL
        if (vflag && !tflag && usize != -1 && gsize != -1)
		print_verbage(NULL, 0, usize, gsize);
#endif 
}

/* do what is asked for, for the path name */
static void
handle_pathname(char *path)
{
	char *opath = path, *s = NULL;
	ssize_t len;
	struct stat sb;

	/* check for stdout/stdin */
	if (path[0] == '-' && path[1] == '\0') {
		if (dflag)
			handle_stdin();
		else
			handle_stdout();
	}

retry:
	if (stat(path, &sb) < 0) {
		/* lets try <path>.gz if we're decompressing */
		if (dflag && s == NULL && errno == ENOENT) {
			len = strlen(path);
			s = malloc(len + suffix_len + 1);
			if (s == NULL)
				maybe_err("malloc");
			memmove(s, path, len);
			memmove(&s[len], suffix, suffix_len);
			s[len + suffix_len] = 0x0;
			path = s;
			goto retry;
		}
		maybe_warn("can't stat: %s", opath);
		goto out;
	}

	if (S_ISDIR(sb.st_mode)) {
#ifndef SMALL
		if (rflag)
			handle_dir(path, &sb);
		else
#endif
			maybe_warn("%s is a directory", path);
		goto out;
	}

	if (S_ISREG(sb.st_mode))
		handle_file(path, &sb);

out:
	if (s)
		free(s);
}

/* compress/decompress a file */
static void
handle_file(char *file, struct stat *sbp)
{
	off_t usize, gsize;
	char	outfile[PATH_MAX];

	infile = file;
	if (dflag) {
		usize = file_uncompress(file, outfile, sizeof(outfile));
		if (usize == -1)
			return;
		gsize = sbp->st_size;
	} else {
		gsize = file_compress(file, outfile, sizeof(outfile));
		if (gsize == -1)
			return;
		usize = sbp->st_size;
	}


#ifndef SMALL
	if (vflag && !tflag)
		print_verbage(file, (cflag) ? NULL : outfile, usize, gsize);
#endif
}

#ifndef SMALL
/* this is used with -r to recursively decend directories */
static void
handle_dir(char *dir, struct stat *sbp)
{
	char *path_argv[2];
	FTS *fts;
	FTSENT *entry;

	path_argv[0] = dir;
	path_argv[1] = 0;
	fts = fts_open(path_argv, FTS_PHYSICAL, NULL);
	if (fts == NULL) {
		warn("couldn't fts_open %s", dir);
		return;
	}

	while ((entry = fts_read(fts))) {
		switch(entry->fts_info) {
		case FTS_D:
		case FTS_DP:
			continue;

		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			maybe_warn("%s", entry->fts_path);
			continue;
		case FTS_F:
			handle_file(entry->fts_name, entry->fts_statp);
		}
	}
	(void)fts_close(fts);
}
#endif

/* print a ratio */
static void
print_ratio(off_t in, off_t out, FILE *where)
{
	int64_t percent10;	/* 10 * percent */
	off_t diff = in - out;
	char ch;

	if (in == 0)
		percent10 = 0;
	else if (diff > 0x400000) 	/* anything with 22 or more bits */
		percent10 = diff / (in / 1000);
	else
		percent10 = (1000 * diff) / in;

	if (percent10 < 0) {
		percent10 = -percent10;
		ch = '-';
	} else
		ch = ' ';

	/*
	 * ugh.  for negative percentages < 10, we need to avoid printing a
	 * a space between the "-" and the single number.
	 */
	if (ch == '-' && percent10 / 10LL < 10)
		fprintf(where, " -%1d.%1u%%", (unsigned)(percent10 / 10LL),
					      (unsigned)(percent10 % 10LL));
	else
		fprintf(where, "%c%2d.%1u%%", ch, (unsigned)(percent10 / 10LL),
					          (unsigned)(percent10 % 10LL));
}

#ifndef SMALL
/* print compression statistics, and the new name (if there is one!) */
static void
print_verbage(char *file, char *nfile, off_t usize, off_t gsize)
{
	if (file)
		fprintf(stderr, "%s:%s  ", file,
		    strlen(file) < 7 ? "\t\t" : "\t");
	print_ratio((off_t)usize, (off_t)gsize, stderr);
	if (nfile)
		fprintf(stderr, " -- replaced with %s", nfile);
	fprintf(stderr, "\n");
	fflush(stderr);
}

/* print test results */
static void
print_test(const char *file, int ok)
{

	if (exit_value == 0 && ok == 0)
		exit_value = 1;
	fprintf(stderr, "%s:%s  %s\n", file,
	    strlen(file) < 7 ? "\t\t" : "\t", ok ? "OK" : "NOT OK");
	fflush(stderr);
}
#endif

/* print a file's info ala --list */
/* eg:
  compressed uncompressed  ratio uncompressed_name
      354841      1679360  78.8% /usr/pkgsrc/distfiles/libglade-2.0.1.tar
*/
static void
print_list(int fd, off_t out, const char *outfile, time_t ts)
{
	static int first = 1;
#ifndef SMALL
	static off_t in_tot, out_tot;
	u_int32_t crc;
#endif
	off_t in;
	int rv;

	if (first) {
#ifndef SMALL
		if (vflag)
			printf("method  crc     date  time  ");
#endif
		if (qflag == 0)
			printf("  compressed uncompressed  "
			       "ratio uncompressed_name\n");
	}
	first = 0;

	/* print totals? */
#ifndef SMALL
	if (fd == -1) {
		in = in_tot;
		out = out_tot;
	} else
#endif
	{
		/* read the last 4 bytes - this is the uncompressed size */
		rv = lseek(fd, (off_t)(-8), SEEK_END);
		if (rv != -1) {
			unsigned char buf[8];
			u_int32_t usize;

			if (read(fd, (char *)buf, sizeof(buf)) != sizeof(buf))
				maybe_warn("read of uncompressed size");
			usize = buf[4] | buf[5] << 8 | buf[6] << 16 | buf[7] << 24;
			in = (off_t)usize;
#ifndef SMALL
			crc = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
#endif
		}
	}

#ifndef SMALL
	if (vflag && fd == -1)
		printf("                            ");
	else if (vflag) {
		char *date = ctime(&ts);

		/* skip the day, 1/100th second, and year */
		date += 4;
		date[12] = 0;
		printf("%5s %08x %11s ", "defla"/*XXX*/, crc, date);
	}
	in_tot += in;
	out_tot += out;
#endif
	printf("%12llu %12llu ", (unsigned long long)out, (unsigned long long)in);
	print_ratio(in, out, stdout);
	printf(" %s\n", outfile);
}

/* display the usage of NetBSD gzip */
static void
usage(void)
{

	fprintf(stderr, "%s\n", gzip_version);
	fprintf(stderr,
    "usage: %s [-" OPT_LIST "] [<file> [<file> ...]]\n"
#ifndef SMALL
    " -c --stdout          write to stdout, keep original files\n"
    "    --to-stdout\n"
    " -d --decompress      uncompress files\n"
    "    --uncompress\n"
    " -f --force           force overwriting & compress links\n"
    " -h --help            display this help\n"
    " -n --no-name         don't save original file name or time stamp\n"
    " -N --name            save or restore original file name and time stamp\n"
    " -q --quiet           output no warnings\n"
    " -r --recursive       recursively compress files in directories\n"
    " -S .suf              use suffix .suf instead of .gz\n"
    "    --suffix .suf\n"
    " -t --test            test compressed file\n"
    " -v --verbose         print extra statistics\n"
    " -V --version         display program version\n"
    " -1 --fast            fastest (worst) compression\n"
    " -2 .. -8             set compression level\n"
    " -9 --best            best (slowest) compression\n",
#else
    ,
#endif
	    getprogname());
	exit(0);
}

/* display the version of NetBSD gzip */
static void
display_version(void)
{

	fprintf(stderr, "%s\n", gzip_version);
	exit(0);
}

#ifndef NO_BZIP2_SUPPORT
#include "unbzip2.c"
#endif
#ifndef NO_COMPRESS_SUPPORT
#include "zuncompress.c"
#endif
