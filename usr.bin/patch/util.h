/*	$NetBSD: util.h,v 1.4.10.1 2000/10/18 01:32:49 tv Exp $	*/

/* and for those machine that can't handle a variable argument list */

#ifdef CANVARARG

#define say1 say
#define say2 say
#define say3 say
#define say4 say
#define ask1 ask
#define ask2 ask
#define ask3 ask
#define ask4 ask
#define fatal1 fatal
#define fatal2 fatal
#define fatal3 fatal
#define fatal4 fatal
#define pfatal1 pfatal
#define pfatal2 pfatal
#define pfatal3 pfatal
#define pfatal4 pfatal

#else /* hope they allow multi-line macro actual arguments */

#ifdef lint

#define say1(a) say(a, 0, 0, 0)
#define say2(a,b) say(a, (b)==(b), 0, 0)
#define say3(a,b,c) say(a, (b)==(b), (c)==(c), 0)
#define say4(a,b,c,d) say(a, (b)==(b), (c)==(c), (d)==(d))
#define ask1(a) ask(a, 0, 0, 0)
#define ask2(a,b) ask(a, (b)==(b), 0, 0)
#define ask3(a,b,c) ask(a, (b)==(b), (c)==(c), 0)
#define ask4(a,b,c,d) ask(a, (b)==(b), (c)==(c), (d)==(d))
#define fatal1(a) fatal(a, 0, 0, 0)
#define fatal2(a,b) fatal(a, (b)==(b), 0, 0)
#define fatal3(a,b,c) fatal(a, (b)==(b), (c)==(c), 0)
#define fatal4(a,b,c,d) fatal(a, (b)==(b), (c)==(c), (d)==(d))
#define pfatal1(a) pfatal(a, 0, 0, 0)
#define pfatal2(a,b) pfatal(a, (b)==(b), 0, 0)
#define pfatal3(a,b,c) pfatal(a, (b)==(b), (c)==(c), 0)
#define pfatal4(a,b,c,d) pfatal(a, (b)==(b), (c)==(c), (d)==(d))

#else /* lint */
    /* if this doesn't work, try defining CANVARARG above */
#define say1(a) say(a, Nullch, Nullch, Nullch)
#define say2(a,b) say(a, b, Nullch, Nullch)
#define say3(a,b,c) say(a, b, c, Nullch)
#define say4 say
#define ask1(a) ask(a, Nullch, Nullch, Nullch)
#define ask2(a,b) ask(a, b, Nullch, Nullch)
#define ask3(a,b,c) ask(a, b, c, Nullch)
#define ask4 ask
#define fatal1(a) fatal(a, Nullch, Nullch, Nullch)
#define fatal2(a,b) fatal(a, b, Nullch, Nullch)
#define fatal3(a,b,c) fatal(a, b, c, Nullch)
#define fatal4 fatal
#define pfatal1(a) pfatal(a, Nullch, Nullch, Nullch)
#define pfatal2(a,b) pfatal(a, b, Nullch, Nullch)
#define pfatal3(a,b,c) pfatal(a, b, c, Nullch)
#define pfatal4 pfatal

#endif /* lint */

/* if neither of the above work, join all multi-line macro calls. */
#endif

EXT char serrbuf[BUFSIZ];		/* buffer for stderr */

int move_file __P((char *, char *));
void copy_file __P((char *, char *));
char *savestr __P((Reg1 char *));
void say __P((const char *, ...))
     __attribute__((__format__(__printf__, 1, 2)));
void fatal __P((const char *, ...))
     __attribute__((__format__(__printf__, 1, 2)));
void pfatal __P((const char *, ...))
     __attribute__((__format__(__printf__, 1, 2)));
void ask __P((const char *, ...))
     __attribute__((__format__(__printf__, 1, 2)));
void set_signals __P((int));
void ignore_signals __P((void));
void makedirs __P((Reg1 char *, bool));
char *fetchname __P((char *, int, int));
