/*	$NetBSD: util.c,v 1.7.10.1 2000/10/18 01:32:49 tv Exp $	*/
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: util.c,v 1.7.10.1 2000/10/18 01:32:49 tv Exp $");
#endif /* not lint */

#include "EXTERN.h"
#include "common.h"
#include "INTERN.h"
#include "util.h"
#include "backupfile.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

/* Rename a file, copying it if necessary. */

int
move_file(from,to)
char *from, *to;
{
    char bakname[512];
    Reg1 char *s;
    Reg2 int i;
    Reg3 int fromfd;

    /* to stdout? */

    if (strEQ(to, "-")) {
#ifdef DEBUGGING
	if (debug & 4)
	    say2("Moving %s to stdout.\n", from);
#endif
	fromfd = open(from, 0);
	if (fromfd < 0)
	    pfatal2("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(1, buf, i) != 1)
		pfatal1("write failed");
	Close(fromfd);
	return 0;
    }

    if (origprae) {
	Strcpy(bakname, origprae);
	Strcat(bakname, to);
    } else {
#ifndef NODIR
	char *backupname = find_backup_file_name(to);
	if (backupname == (char *) 0)
	    fatal1("out of memory\n");
	Strcpy(bakname, backupname);
	free(backupname);
#else /* NODIR */
	Strcpy(bakname, to);
    	Strcat(bakname, simple_backup_suffix);
#endif /* NODIR */
    }

    if (stat(to, &filestat) == 0) {	/* output file exists */
	dev_t to_device = filestat.st_dev;
	ino_t to_inode  = filestat.st_ino;
	char *simplename = bakname;
	
	for (s=bakname; *s; s++) {
	    if (*s == '/')
		simplename = s+1;
	}
	/* Find a backup name that is not the same file.
	   Change the first lowercase char into uppercase;
	   if that isn't sufficient, chop off the first char and try again.  */
	while (stat(bakname, &filestat) == 0 &&
		to_device == filestat.st_dev && to_inode == filestat.st_ino) {
	    /* Skip initial non-lowercase chars.  */
	    for (s=simplename; *s && !islower((unsigned char)*s); s++) ;
	    if (*s)
		*s = toupper(*s);
	    else
		Strcpy(simplename, simplename+1);
	}
	while (unlink(bakname) >= 0) ;	/* while() is for benefit of Eunice */
#ifdef DEBUGGING
	if (debug & 4)
	    say3("Moving %s to %s.\n", to, bakname);
#endif
	if (link(to, bakname) < 0) {
	    /* Maybe `to' is a symlink into a different file system.
	       Copying replaces the symlink with a file; using rename
	       would be better.  */
	    Reg4 int tofd;
	    Reg5 int bakfd;

	    bakfd = creat(bakname, 0666);
	    if (bakfd < 0) {
		say4("Can't backup %s, output is in %s: %s\n", to, from,
		     strerror(errno));
		return -1;
	    }
	    tofd = open(to, 0);
	    if (tofd < 0)
		pfatal2("internal error, can't open %s", to);
	    while ((i=read(tofd, buf, sizeof buf)) > 0)
		if (write(bakfd, buf, i) != i)
		    pfatal1("write failed");
	    Close(tofd);
	    Close(bakfd);
	}
	while (unlink(to) >= 0) ;
    }
#ifdef DEBUGGING
    if (debug & 4)
	say3("Moving %s to %s.\n", from, to);
#endif
    if (link(from, to) < 0) {		/* different file system? */
	Reg4 int tofd;
	
	tofd = creat(to, 0666);
	if (tofd < 0) {
	    say4("Can't create %s, output is in %s: %s\n",
	      to, from, strerror(errno));
	    return -1;
	}
	fromfd = open(from, 0);
	if (fromfd < 0)
	    pfatal2("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(tofd, buf, i) != i)
		pfatal1("write failed");
	Close(fromfd);
	Close(tofd);
    }
    Unlink(from);
    return 0;
}

/* Copy a file. */

void
copy_file(from,to)
char *from, *to;
{
    Reg3 int tofd;
    Reg2 int fromfd;
    Reg1 int i;
    
    tofd = creat(to, 0666);
    if (tofd < 0)
	pfatal2("can't create %s", to);
    fromfd = open(from, 0);
    if (fromfd < 0)
	pfatal2("internal error, can't reopen %s", from);
    while ((i=read(fromfd, buf, sizeof buf)) > 0)
	if (write(tofd, buf, i) != i)
	    pfatal2("write to %s failed", to);
    Close(fromfd);
    Close(tofd);
}

/* Allocate a unique area for a string. */

char *
savestr(s)
Reg1 char *s;
{
    Reg3 char *rv;
    Reg2 char *t;

    if (!s)
	s = "Oops";
    t = s;
    while (*t++);
    rv = malloc((MEM) (t - s));
    if (rv == Nullch) {
	if (using_plan_a)
	    out_of_mem = TRUE;
	else
	    fatal1("out of memory\n");
    }
    else {
	t = rv;
	while ((*t++ = *s++) != '\0');
    }
    return rv;
}

#if defined(lint) && defined(CANVARARG)

/*VARARGS ARGSUSED*/
say(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
fatal(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
pfatal(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
ask(pat) char *pat; { ; }

#else

/* Vanilla terminal output (buffered). */

void
#ifdef __STDC__
say(const char *pat, ...)
#else
say(va_alist)
	va_dcl
#endif
{
    va_list ap;
#ifdef __STDC__
    va_start(ap, pat);
#else
    const char *pat;

    va_start(ap);
    pat = va_arg(ap, const char *);
#endif
	
    vfprintf(stderr, pat, ap);
    va_end(ap);
    Fflush(stderr);
}

/* Terminal output, pun intended. */

void				/* very void */
#ifdef __STDC__
fatal(const char *pat, ...)
#else
fatal(va_alist)
	va_dcl
#endif
{
    va_list ap;
#ifdef __STDC__
    va_start(ap, pat);
#else
    const char *pat;

    va_start(ap);
    pat = va_arg(ap, const char *);
#endif
	
    fprintf(stderr, "patch: **** ");
    vfprintf(stderr, pat, ap);
    va_end(ap);
    my_exit(1);
}

/* Say something from patch, something from the system, then silence . . . */

void				/* very void */
#ifdef __STDC__
pfatal(const char *pat, ...)
#else
pfatal(va_alist)
	va_dcl
#endif
{
    va_list ap;
    int errnum = errno;
#ifdef __STDC__
    va_start(ap, pat);
#else
    const char *pat;

    va_start(ap);
    pat = va_arg(ap, const char *);
#endif
	
    fprintf(stderr, "patch: **** ");
    vfprintf(stderr, pat, ap);
    fprintf(stderr, ": %s\n", strerror(errnum));
    va_end(ap);
    my_exit(1);
}
/* Get a response from the user, somehow or other. */

void
#ifdef __STDC__
ask(const char *pat, ...)
#else
ask(va_alist)
	va_dcl
#endif
{
    int ttyfd;
    int r;
    bool tty2 = isatty(2);
    va_list ap;
#ifdef __STDC__
    va_start(ap, pat);
#else
    const char *pat;

    va_start(ap);
    pat = va_arg(ap, const char *);
#endif

    (void) vsprintf(buf, pat, ap);
    va_end(ap);
    Fflush(stderr);
    write(2, buf, strlen(buf));
    if (tty2) {				/* might be redirected to a file */
	r = read(2, buf, sizeof buf);
    }
    else if (isatty(1)) {		/* this may be new file output */
	Fflush(stdout);
	write(1, buf, strlen(buf));
	r = read(1, buf, sizeof buf);
    }
    else if ((ttyfd = open("/dev/tty", 2)) >= 0 && isatty(ttyfd)) {
					/* might be deleted or unwriteable */
	write(ttyfd, buf, strlen(buf));
	r = read(ttyfd, buf, sizeof buf);
	Close(ttyfd);
    }
    else if (isatty(0)) {		/* this is probably patch input */
	Fflush(stdin);
	write(0, buf, strlen(buf));
	r = read(0, buf, sizeof buf);
    }
    else {				/* no terminal at all--default it */
	buf[0] = '\n';
	r = 1;
    }
    if (r <= 0)
	buf[0] = 0;
    else
	buf[r] = '\0';
    if (!tty2)
	say2("%s",buf);
}
#endif /* lint */

/* How to handle certain events when not in a critical region. */

void
set_signals(reset)
int reset;
{
#ifndef lint
#ifdef VOIDSIG
    static void (*hupval) __P((int)),(*intval) __P((int));
#else
    static int (*hupval) __P((int)),(*intval)__P((int));
#endif

    if (!reset) {
	hupval = signal(SIGHUP, SIG_IGN);
	if (hupval != SIG_IGN)
#ifdef VOIDSIG
	    hupval = my_exit;
#else
	    hupval = (int(*) __P((int)))my_exit;
#endif
	intval = signal(SIGINT, SIG_IGN);
	if (intval != SIG_IGN)
#ifdef VOIDSIG
	    intval = my_exit;
#else
	    intval = (int(*) __P((int)))my_exit;
#endif
    }
    Signal(SIGHUP, hupval);
    Signal(SIGINT, intval);
#endif
}

/* How to handle certain events when in a critical region. */

void
ignore_signals()
{
#ifndef lint
    Signal(SIGHUP, SIG_IGN);
    Signal(SIGINT, SIG_IGN);
#endif
}

/* Make sure we'll have the directories to create a file.
   If `striplast' is TRUE, ignore the last element of `filename'.  */

void
makedirs(filename,striplast)
Reg1 char *filename;
bool striplast;
{
    char tmpbuf[256];
    Reg2 char *s = tmpbuf;
    char *dirv[20];		/* Point to the NULs between elements.  */
    Reg3 int i;
    Reg4 int dirvp = 0;		/* Number of finished entries in dirv. */

    /* Copy `filename' into `tmpbuf' with a NUL instead of a slash
       between the directories.  */
    while (*filename) {
	if (*filename == '/') {
	    filename++;
	    dirv[dirvp++] = s;
	    *s++ = '\0';
	}
	else {
	    *s++ = *filename++;
	}
    }
    *s = '\0';
    dirv[dirvp] = s;
    if (striplast)
	dirvp--;
    if (dirvp < 0)
	return;

    strcpy(buf, "mkdir");
    s = buf;
    for (i=0; i<=dirvp; i++) {
	struct stat sbuf;

	if (stat(tmpbuf, &sbuf) && errno == ENOENT) {
	    while (*s) s++;
	    *s++ = ' ';
	    strcpy(s, tmpbuf);
	}
	*dirv[i] = '/';
    }
    if (s != buf)
	system(buf);
}

/* Make filenames more reasonable. */

char *
fetchname(at,strip_leading,assume_exists)
char *at;
int strip_leading;
int assume_exists;
{
    char *fullname;
    char *name;
    Reg1 char *t;
    char tmpbuf[200];
    int sleading = strip_leading;

    if (!at)
	return Nullch;
    while (isspace((unsigned char)*at))
	at++;
#ifdef DEBUGGING
    if (debug & 128)
	say4("fetchname %s %d %d\n",at,strip_leading,assume_exists);
#endif
    filename_is_dev_null = FALSE;
    if (strnEQ(at, "/dev/null", 9)) {	/* so files can be created by diffing */
        filename_is_dev_null = TRUE;
	return Nullch;			/*   against /dev/null. */
    }
    name = fullname = t = savestr(at);

    /* Strip off up to `sleading' leading slashes and null terminate.  */
    for (; *t && !isspace((unsigned char)*t); t++)
	if (*t == '/')
	    if (--sleading >= 0)
		name = t+1;
    *t = '\0';

    /* If no -p option was given (957 is the default value!),
       we were given a relative pathname,
       and the leading directories that we just stripped off all exist,
       put them back on.  */
    if (strip_leading == 957 && name != fullname && *fullname != '/') {
	name[-1] = '\0';
	if (stat(fullname, &filestat) == 0 && S_ISDIR (filestat.st_mode)) {
	    name[-1] = '/';
	    name=fullname;
	}
    }

    name = savestr(name);
    free(fullname);

    if (stat(name, &filestat) && !assume_exists) {
	char *filebase = basename(name);
	int pathlen = filebase - name;

	/* Put any leading path into `tmpbuf'.  */
	strncpy(tmpbuf, name, pathlen);

#define try(f, a1, a2) (Sprintf(tmpbuf + pathlen, f, a1, a2), stat(tmpbuf, &filestat) == 0)
#define try1(f, a1) (Sprintf(tmpbuf + pathlen, f, a1), stat(tmpbuf, &filestat) == 0)
	if (   try("RCS/%s%s", filebase, RCSSUFFIX)
	    || try1("RCS/%s"  , filebase)
	    || try(    "%s%s", filebase, RCSSUFFIX)
	    || try("SCCS/%s%s", SCCSPREFIX, filebase)
	    || try(     "%s%s", SCCSPREFIX, filebase))
	  return name;
	free(name);
	name = Nullch;
    }

    return name;
}
