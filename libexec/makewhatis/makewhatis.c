/*	$NetBSD: makewhatis.c,v 1.7.4.2 2000/07/13 16:42:34 tron Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthias Scheler.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1999 The NetBSD Foundation, Inc.\n\
	All rights reserved.\n");
#endif /* not lint */

#ifndef lint
__RCSID("$NetBSD: makewhatis.c,v 1.7.4.2 2000/07/13 16:42:34 tron Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <locale.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

typedef struct manpagestruct manpage;
struct manpagestruct {
	manpage *mp_left,*mp_right;
	ino_t	 mp_inode;
	char     mp_name[1];
};

typedef struct whatisstruct whatis;
struct whatisstruct {
	whatis	*wi_left,*wi_right;
	char	*wi_data;
};

int              main (int, char **);
char		*findwhitespace(char *);
char		*GetS(gzFile, char *, int);
int		 manpagesection (char *);
int		 addmanpage (manpage **, ino_t, char *);
int		 addwhatis (whatis **, char *);
char		*replacestring (char *, char *, char *);
void		 catpreprocess (char *);
char		*parsecatpage (gzFile *);
int		 manpreprocess (char *);
char		*nroff (gzFile *);
char		*parsemanpage (gzFile *, int);
char		*getwhatisdata (char *);
void		 processmanpages (manpage **,whatis **);
int		 dumpwhatis (FILE *, whatis *);

char *default_manpath[] = {
	"/usr/share/man",
	NULL
};

char sectionext[] = "0123456789ln";
char whatisdb[]   = "whatis.db";

extern char *__progname;

int
main(int argc,char **argv)
{
	char	**manpath;
	FTS	*fts;
	FTSENT	*fe;
	manpage	*source;
	whatis	*dest;
	FILE	*out;

	(void)setlocale(LC_ALL, "");

	manpath = (argc < 2) ? default_manpath : &argv[1];

	if ((fts = fts_open(manpath, FTS_LOGICAL, NULL)) == NULL) {
		perror(__progname);
		return EXIT_FAILURE;
	}

	source = NULL;
	while ((fe = fts_read(fts)) != NULL) {
		switch (fe->fts_info) {
		case FTS_F:
			if (manpagesection(fe->fts_path) >= 0)
				if (!addmanpage(&source,
					fe->fts_statp->st_ino,
					fe->fts_path))
					err(EXIT_FAILURE, NULL);
		case FTS_D:
		case FTS_DC:
		case FTS_DEFAULT:
		case FTS_DP:
		case FTS_SLNONE:
			break;
		default:
			errx(EXIT_FAILURE, "%s: %s", fe->fts_path,
			    strerror(fe->fts_errno));

		}
	}

	(void)fts_close(fts);

	dest = NULL;
	processmanpages(&source, &dest);

	if (chdir(manpath[0]) < 0)
		errx(EXIT_FAILURE, "%s: %s", manpath[0], strerror(errno));

	if ((out = fopen(whatisdb, "w")) == NULL)
		errx(EXIT_FAILURE, "%s: %s", whatisdb, strerror(errno));

	if (!(dumpwhatis(out, dest) ||
	    (fclose(out) < 0)) ||
	    (chmod(whatisdb, S_IRUSR|S_IRGRP|S_IROTH) < 0))
		errx(EXIT_FAILURE, "%s: %s", whatisdb, strerror(errno));

	return EXIT_SUCCESS;
}

char
*findwhitespace(char *str)

{
	while (!isspace(*str))
		if (*str++ == '\0') {
			str = NULL;
			break;
		}

	return str;
}

char
*GetS(gzFile in, char *buffer, int length)

{
	char	*ptr;

	if (((ptr = gzgets(in, buffer, length)) != NULL) && (*ptr == '\0'))
		ptr = NULL;

	return ptr;
}

int
manpagesection(char *name)
{
	char	*ptr;

	if ((ptr = strrchr(name, '/')) != NULL)
		ptr++;
	else
		ptr = name;

	while ((ptr = strchr(ptr, '.')) != NULL) {
		int section;

		ptr++;
		section=0;
		while (sectionext[section] != '\0')
			if (sectionext[section] == *ptr)
				return section;
			else
				section++;
	}

	return -1;
}

int
addmanpage(manpage **tree,ino_t inode,char *name)
{
	manpage	*mp;

	while ((mp = *tree) != NULL) {
		if (mp->mp_inode == inode)
			return 1;
		tree = &((inode < mp->mp_inode) ? mp->mp_left : mp->mp_right);
	}

	if ((mp = malloc(sizeof(manpage) + strlen(name))) == NULL)
		return 0;

	mp->mp_left = NULL;
	mp->mp_right = NULL;
	mp->mp_inode = inode;
	(void) strcpy(mp->mp_name, name);
	*tree = mp;

	return 1;
}

int
addwhatis(whatis **tree, char *data)
{
	whatis *wi;
	int result;

	while (isspace(*data))
		data++;

	if (*data == '/') {
		char *ptr;

		ptr = ++data;
		while ((*ptr != '\0') && !isspace(*ptr))
			if (*ptr++ == '/')
				data = ptr;
	}

	while ((wi = *tree) != NULL) {
		result=strcmp(data, wi->wi_data);
		if (result == 0) return 1;
		tree = &((result < 0) ? wi->wi_left : wi->wi_right);
	}

	if ((wi = malloc(sizeof(whatis) + strlen(data))) == NULL)
		return 0;

	wi->wi_left = NULL;
	wi->wi_right = NULL;
	wi->wi_data = data;
	*tree = wi;

	return 1;
}

void
catpreprocess(char *from)
{
	char	*to;

	to = from;
	while (isspace(*from)) from++;

	while (*from != '\0')
		if (isspace(*from)) {
			while (isspace(*++from));
			if (*from != '\0')
				*to++ = ' ';
		}
		else if (*(from + 1) == '\10')
			from += 2;
		else
			*to++ = *from++;

	*to = '\0';
}

char *
replacestring(char *string, char *old, char *new)

{
	char	*ptr, *result;
	int	 slength, olength, nlength, pos;

	if (new == NULL)
		return strdup(string);

	ptr = strstr(string, old);
	if (ptr == NULL)
		return strdup(string);

	slength = strlen(string);
	olength = strlen(old);
	nlength = strlen(new);
	if ((result = malloc(slength - olength + nlength + 1)) == NULL)
		return NULL;

	pos = ptr - string;
	(void) memcpy(result, string, pos);
	(void) memcpy(&result[pos], new, nlength);
	(void) strcpy(&result[pos + nlength], &string[pos + olength]);

	return result;
}

char *
parsecatpage(gzFile *in)
{
	char 	 buffer[8192];
	char	*section, *ptr, *last;
	int	 size;

	do {
		if (GetS(in, buffer, sizeof(buffer)) == NULL)
			return NULL;
	}
	while (buffer[0] == '\n');

	section = NULL;
	if ((ptr = strchr(buffer, '(')) != NULL) {
		if ((last = strchr(ptr + 1, ')')) !=NULL) {
			int 	length;

			length = last - ptr + 1;
			if ((section = malloc(length + 5)) == NULL)
				return NULL;

			*section = ' ';
			(void) memcpy(section + 1, ptr, length);
			(void) strcpy(section + 1 + length, " - ");
		}
	}

	for (;;) {
		if (GetS(in, buffer, sizeof(buffer)) == NULL) {
			free(section);
			return NULL;
		}
		if (strncmp(buffer, "N\10NA\10AM\10ME\10E", 12) == 0)
			break;
	}

	ptr = last = buffer;
	size = sizeof(buffer) - 1;
	while ((size > 0) && (GetS(in, ptr, size) != NULL)) {
		int	 length;

		catpreprocess(ptr);

		length = strlen(ptr);
		if (length == 0) {
			*last = '\0';

			ptr = replacestring(buffer, " - ", section);
			free(section);
			return ptr;
		}
		if ((length > 1) && (ptr[length - 1] == '-') &&
		    isalpha(ptr[length - 2]))
			last = &ptr[--length];
		else {
			last = &ptr[length++];
			*last = ' ';
		}

		ptr += length;
		size -= length;
	}

	free(section);

	return NULL;
}

int
manpreprocess(char *line)
{
	char	*from, *to;

	to = from = line;
	while (isspace(*from)) from++;
	if (strncmp(from, ".\\\"", 3) == 0)
		return 1;

	while (*from != '\0')
		if (isspace(*from)) {
			while (isspace(*++from));
			if ((*from != '\0') && (*from != ','))
				*to++ = ' ';
		}
		else if (*from == '\\')
			switch (*++from) {
			case '\0':
			case '-':
				break;
			case 's':
				if ((*from=='+') || (*from=='-'))
					from++;
				while (isdigit(*from))
					from++;
				break;
			default:
				from++;
			}
		else
			if (*from == '"')
				from++;
			else
				*to++ = *from++;

	*to = '\0';

	if (strncasecmp(line, ".Xr", 3) == 0) {
		char	*sect;

		from = line + 3;
		if (isspace(*from))
			from++;

		if ((sect = findwhitespace(from)) != NULL) {
			int	 length;

			*sect++ = '\0';
			length = strlen(from);
			(void) memmove(line, from, length);
			line[length++] = '(';
			to = &line[length];
			length = strlen(sect);
			(void) memmove(to, sect, length);
			(void) strcpy(&to[length], ")");
		}
	}

	return 0;
}

char *
nroff(gzFile *in)
{
	char tempname[MAXPATHLEN], buffer[65536], *data;
	int tempfd, bytes, pipefd[2], status;
	static int devnull = -1;
	pid_t child;

	if (gzrewind(in) < 0) {
		perror(__progname);
		return NULL;
	}

	if ((devnull < 0) &&
	    ((devnull = open(_PATH_DEVNULL, O_WRONLY, 0)) < 0)) {
		perror(__progname);
		return NULL;
	}

	(void)strcpy(tempname, _PATH_TMP "makewhatis.XXXXXX");
	if ((tempfd = mkstemp(tempname)) < 0) {
		perror(__progname);
		return NULL;
	}

	while ((bytes = gzread(in, buffer, sizeof(buffer))) > 0)
		if (write(tempfd, buffer, bytes) != bytes) {
			bytes = -1;
			break;
		}

	if ((bytes < 0) ||
            (lseek(tempfd, 0, SEEK_SET) < 0) ||
            (pipe(pipefd) < 0)) {
		perror(__progname);
		(void)close(tempfd);
		(void)unlink(tempname);
		return NULL;
	}

	switch (child = vfork()) {
	case -1:
		perror(__progname);
		(void)close(pipefd[1]);
		(void)close(pipefd[0]);
		(void)close(tempfd);
		(void)unlink(tempname);
		return NULL;
		/* NOTREACHED */
	case 0:
		(void)close(pipefd[0]);
		if (tempfd != STDIN_FILENO) {
			(void)dup2(tempfd, STDIN_FILENO);
			(void)close(tempfd);
		}
		if (pipefd[1] != STDOUT_FILENO) {
			(void)dup2(pipefd[1], STDOUT_FILENO);
			(void)close(pipefd[1]);
		}
		if (devnull != STDERR_FILENO) {
			(void)dup2(devnull, STDERR_FILENO);
			(void)close(devnull);
		}
		(void)execlp("nroff", "nroff", "-S", "-man", NULL);
		_exit(EXIT_FAILURE);
	default:
		(void)close(pipefd[1]);
		(void)close(tempfd);
		/* NOTREACHED */
	}

	if ((in = gzdopen(pipefd[0], "r")) == NULL) {
		if (errno == 0)
			errno = ENOMEM;
		perror(__progname);
		(void)close(pipefd[0]);
		(void)kill(child, SIGTERM);
		while (waitpid(child, NULL, 0) != child);
		(void)unlink(tempname);
		return NULL;
	}

	data = parsecatpage(in);
	while (gzread(in, buffer, sizeof(buffer)) > 0);
	(void)gzclose(in);

	while (waitpid(child, &status, 0) != child);
	if ((data != NULL) &&
	    !(WIFEXITED(status) && (WEXITSTATUS(status) == 0))) {
		free(data);
		data = NULL;
	}

	(void)unlink(tempname);

	return data;
}

char *
parsemanpage(gzFile *in, int defaultsection)
{
	char	*section, buffer[8192], *ptr;

	section = NULL;
	do {
		if (GetS(in, buffer, sizeof(buffer) - 1) == NULL) {
			free(section);
			return NULL;
		}
		if (manpreprocess(buffer))
			continue;
		if (strncasecmp(buffer, ".Dt", 3) == 0) {
			char	*end;

			ptr = &buffer[3];
			if (isspace(*ptr))
				ptr++;
			if ((ptr = findwhitespace(ptr)) == NULL)
				continue;

			if ((end = findwhitespace(++ptr)) != NULL)
				*end = '\0';

			free(section);
			if ((section = malloc(strlen(ptr) + 7)) != NULL) {
				section[0] = ' ';
				section[1] = '(';
				(void) strcpy(&section[2], ptr);
				(void) strcat(&section[2], ") - ");
			}
		}
		else if (strncasecmp(buffer, ".Ds", 3) == 0)
			return nroff(in);
	} while ((strncasecmp(buffer, ".Sh NAME", 8) != 0));

	do {
		if (GetS(in, buffer, sizeof(buffer) - 1) == NULL) {
			free(section);
			return NULL;
		}
	} while (manpreprocess(buffer));

	if (strncasecmp(buffer, ".Nm", 3) == 0) {
		int	length, offset;

		ptr = &buffer[3];
		while (isspace(*ptr))
			ptr++;

		length = strlen(ptr);
		if ((length > 1) && (ptr[length - 1] == ',') &&
		    isspace(ptr[length - 2])) {
			ptr[--length] = '\0';
			ptr[length - 1] = ',';
		}
		(void) memmove(buffer, ptr, length + 1);

		offset = length + 3;
		ptr = &buffer[offset];
		for (;;) {
			int	 more;

			if ((sizeof(buffer) == offset) ||
		            (GetS(in, ptr, sizeof(buffer) - offset)
			       == NULL)) {
				free(section);
				return NULL;
			}
			if (manpreprocess(ptr))
				continue;

			if (strncasecmp(ptr, ".Nm", 3) != 0) break;

			ptr += 3;
			if (isspace(*ptr))
				ptr++;

			buffer[length++] = ' ';
			more = strlen(ptr);
			if ((more > 1) && (ptr[more - 1] == ',') &&
			    isspace(ptr[more - 2])) {
				ptr[--more] = '\0';
				ptr[more - 1] = ',';
			}

			(void) memmove(&buffer[length], ptr, more + 1);
			length += more;
			offset = length + 3;

			ptr = &buffer[offset];
		}

		if (strncasecmp(ptr, ".Nd", 3) == 0) {
			(void) strcpy(&buffer[length], " -");

			while (strncasecmp(ptr, ".Sh", 3) != 0) {
				int	 more;

				if (*ptr == '.') {
					char	*space;

					if ((space = findwhitespace(ptr)) == NULL)
						ptr = "";
					else {
						space++;
						(void) memmove(ptr, space,
							   strlen(space) + 1);
					}
				}

				if (*ptr != '\0') {
					buffer[offset - 1] = ' ';
					more = strlen(ptr) + 1;
					offset += more;
				}
				ptr = &buffer[offset];
				if ((sizeof(buffer) == offset) ||
			            (GetS(in, ptr, sizeof(buffer) - offset)
					== NULL)) {
					free(section);
					return NULL;
				}
				if (manpreprocess(ptr))
					*ptr = '\0';
			}
		}
	}
	else {
		int	 offset;

		if (*buffer == '.') {
			char	*space;

			if ((space = findwhitespace(buffer)) == NULL) {
				free(section);
				return NULL;
			}
			space++;
			(void) memmove(buffer, space, strlen(space) + 1);
		}

		offset = strlen(buffer) + 1;
		for (;;) {
			int	 more;

			ptr = &buffer[offset];
			if ((sizeof(buffer) == offset) ||
		            (GetS(in, ptr, sizeof(buffer) - offset)
				== NULL)) {
				free(section);
				return NULL;
			}
			if (manpreprocess(ptr) || (*ptr == '\0'))
				continue;

			if ((strncasecmp(ptr, ".Sh", 3) == 0) ||
			    (strncasecmp(ptr, ".Ss", 3) == 0))
				break;

			if (*ptr == '.') {
				char	*space;

				if ((space = findwhitespace(ptr)) == NULL) {
					continue;
				}

				space++;
				(void) memmove(ptr, space, strlen(space) + 1);
			}

			buffer[offset - 1] = ' ';
			more = strlen(ptr);
			if ((more > 1) && (ptr[more - 1] == ',') &&
			    isspace(ptr[more - 2])) {
				ptr[more - 1] = '\0';
				ptr[more - 2] = ',';
			}
			else more++;
			offset += more;
		}
	}

	if (section == NULL) {
		char sectionbuffer[24];

		(void) sprintf(sectionbuffer, " (%c) - ",
			sectionext[defaultsection]);
		ptr = replacestring(buffer, " - ", sectionbuffer);
	}
	else {
		ptr = replacestring(buffer, " - ", section);
		free(section);
	}
	return ptr;
}

char *
getwhatisdata(char *name)
{
	gzFile	*in;
	char	*data;
	int	 section;

	if ((in = gzopen(name, "r")) == NULL) {
		errx(EXIT_FAILURE, "%s: %s",
		    name,
		    strerror((errno == 0) ? ENOMEM : errno));
		/* NOTREACHED */
	}

	section = manpagesection(name);
	data = (section == 0) ? parsecatpage(in) : parsemanpage(in, section);

	(void) gzclose(in);
	return data;
}

void
processmanpages(manpage **source, whatis **dest)
{
	manpage	*mp;

	mp = *source;
	*source = NULL;

	while (mp != NULL) {
		manpage *obsolete;
		char *data;

		if (mp->mp_left != NULL)
			processmanpages(&mp->mp_left,dest);

		if ((data = getwhatisdata(mp->mp_name)) != NULL) {
			if (!addwhatis(dest,data))
				err(EXIT_FAILURE, NULL);
		}

		obsolete = mp;
		mp = mp->mp_right;
		free(obsolete);
	}
}

int
dumpwhatis (FILE *out, whatis *tree)
{
	while (tree != NULL) {
		if (tree->wi_left)
			if (!dumpwhatis(out, tree->wi_left)) return 0;

		if ((fputs(tree->wi_data, out) == EOF) ||
		    (fputc('\n', out) == EOF))
			return 0;

		tree = tree->wi_right;
	}

	return 1;
}
