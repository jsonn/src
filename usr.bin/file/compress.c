/*	$NetBSD: compress.c,v 1.6 1998/09/20 15:27:15 christos Exp $	*/

/*
 * compress routines:
 *	zmagic() - returns 0 if not recognized, uncompresses and prints
 *		   information if recognized
 *	uncompress(method, old, n, newch) - uncompress old into new, 
 *					    using method, return sizeof new
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include "file.h"
#include <sys/cdefs.h>
#ifndef lint
#if 0
FILE_RCSID("@(#)Id: compress.c,v 1.12 1998/06/27 13:57:23 christos Exp ")
#else
__RCSID("$NetBSD: compress.c,v 1.6 1998/09/20 15:27:15 christos Exp $");
#endif
#endif


static struct {
   const char *magic;
   int   maglen;
   const char *const argv[3];
   int	 silent;
} compr[] = {
    { "\037\235", 2, { "uncompress", "-c", NULL }, 0 },	/* compressed */
    { "\037\213", 2, { "gzip", "-cdq", NULL }, 1 },	/* gzipped */
    { "\037\236", 2, { "gzip", "-cdq", NULL }, 1 },	/* frozen */
    { "\037\240", 2, { "gzip", "-cdq", NULL }, 1 },	/* SCO LZH */
    /* the standard pack utilities do not accept standard input */
    { "\037\036", 2, { "gzip", "-cdq", NULL }, 0 },	/* packed */
};

static int ncompr = sizeof(compr) / sizeof(compr[0]);


static int uncompress __P((int, const unsigned char *, unsigned char **, int));

int
zmagic(buf, nbytes)
unsigned char *buf;
int nbytes;
{
	unsigned char *newbuf;
	int newsize;
	int i;

	for (i = 0; i < ncompr; i++) {
		if (nbytes < compr[i].maglen)
			continue;
		if (memcmp(buf, compr[i].magic,  compr[i].maglen) == 0)
			break;
	}

	if (i == ncompr)
		return 0;

	if ((newsize = uncompress(i, buf, &newbuf, nbytes)) != 0) {
		tryit(newbuf, newsize, 1);
		free(newbuf);
		printf(" (");
		tryit(buf, nbytes, 0);
		printf(")");
	}
	return 1;
}


static int
uncompress(method, old, newch, n)
int method;
const unsigned char *old;
unsigned char **newch;
int n;
{
	int fdin[2], fdout[2];

	if (pipe(fdin) == -1 || pipe(fdout) == -1) {
		error("cannot create pipe (%s).\n", strerror(errno));	
		/*NOTREACHED*/
	}
	switch (fork()) {
	case 0:	/* child */
		(void) close(0);
		(void) dup(fdin[0]);
		(void) close(fdin[0]);
		(void) close(fdin[1]);

		(void) close(1);
		(void) dup(fdout[1]);
		(void) close(fdout[0]);
		(void) close(fdout[1]);
		if (compr[method].silent)
		    (void) close(2);

		execvp(compr[method].argv[0],
		       (char *const *)compr[method].argv);
		error("could not execute `%s' (%s).\n", 
		      compr[method].argv[0], strerror(errno));
		/*NOTREACHED*/
	case -1:
		error("could not fork (%s).\n", strerror(errno));
		/*NOTREACHED*/

	default: /* parent */
		(void) close(fdin[0]);
		(void) close(fdout[1]);
		if (write(fdin[1], old, n) != n) {
			error("write failed (%s).\n", strerror(errno));
			/*NOTREACHED*/
		}
		(void) close(fdin[1]);
		if ((*newch = (unsigned char *) malloc(n)) == NULL) {
			error("out of memory.\n");
			/*NOTREACHED*/
		}
		if ((n = read(fdout[0], *newch, n)) <= 0) {
			free(*newch);
			error("read failed (%s).\n", strerror(errno));
			/*NOTREACHED*/
		}
		(void) close(fdout[0]);
		(void) wait(NULL);
		return n;
	}
}
