/*	$NetBSD: c_sh.c,v 1.2 1997/01/12 19:11:40 tls Exp $	*/

/*
 * built-in Bourne commands
 */

#include "sh.h"
#include "ksh_stat.h" 	/* umask() */
#include "ksh_time.h"
#include "ksh_times.h"

static	char *clocktos ARGS((clock_t t));

/* :, false and true */
int
c_label(wp)
	char **wp;
{
	return wp[0][0] == 'f' ? 1 : 0;
}

int
c_shift(wp)
	char **wp;
{
	register struct block *l = e->loc;
	register int n;
	long val;
	char *arg;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	arg = wp[builtin_opt.optind];

	if (arg) {
		evaluate(arg, &val, FALSE);
		n = val;
	} else
		n = 1;
	if (n < 0) {
		bi_errorf("%s: bad number", arg);
		return (1);
	}
	if (l->argc < n) {
		bi_errorf("nothing to shift");
		return (1);
	}
	l->argv[n] = l->argv[0];
	l->argv += n;
	l->argc -= n;
	return 0;
}

int
c_umask(wp)
	char **wp;
{
	register int i;
	register char *cp;
	int symbolic = 0;
	int old_umask;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "S")) != EOF)
		switch (optc) {
		  case 'S':
			symbolic = 1;
			break;
		  case '?':
			return 1;
		}
	cp = wp[builtin_opt.optind];
	if (cp == NULL) {
		old_umask = umask(0);
		umask(old_umask);
		if (symbolic) {
			char buf[18];
			int j;

			old_umask = ~old_umask;
			cp = buf;
			for (i = 0; i < 3; i++) {
				*cp++ = "ugo"[i];
				*cp++ = '=';
				for (j = 0; j < 3; j++)
					if (old_umask & (1 << (8 - (3*i + j))))
						*cp++ = "rwx"[j];
				*cp++ = ',';
			}
			cp[-1] = '\0';
			shprintf("%s\n", buf);
		} else
			shprintf("%#3.3o\n", old_umask);
	} else {
		int new_umask;

		if (digit(*cp)) {
			for (new_umask = 0; *cp >= '0' && *cp <= '7'; cp++)
				new_umask = new_umask * 8 + (*cp - '0');
			if (*cp) {
				bi_errorf("bad number");
				return 1;
			}
		} else {
			/* symbolic format */
			int positions, new_val;
			char op;

			old_umask = umask(0);
			umask(old_umask); /* in case of error */
			old_umask = ~old_umask;
			new_umask = old_umask;
			positions = 0;
			while (*cp) {
				while (*cp && strchr("augo", *cp))
					switch (*cp++) {
					case 'a': positions |= 0111; break;
					case 'u': positions |= 0100; break;
					case 'g': positions |= 0010; break;
					case 'o': positions |= 0001; break;
					}
				if (!positions)
					positions = 0111; /* default is a */
				if (!strchr("=+-", op = *cp))
					break;
				cp++;
				new_val = 0;
				while (*cp && strchr("rwxugoXs", *cp))
					switch (*cp++) {
					case 'r': new_val |= 04; break;
					case 'w': new_val |= 02; break;
					case 'x': new_val |= 01; break;
					case 'u': new_val |= old_umask >> 6;
						  break;
					case 'g': new_val |= old_umask >> 3;
						  break;
					case 'o': new_val |= old_umask >> 0;
						  break;
					case 'X': if (old_umask & 0111)
							new_val |= 01;
						  break;
					case 's': /* ignored */
						  break;
					}
				new_val = (new_val & 07) * positions;
				switch (op) {
				case '-':
					new_umask &= ~new_val;
					break;
				case '=':
					new_umask = new_val
					    | (new_umask & ~(positions * 07));
					break;
				case '+':
					new_umask |= new_val;
				}
				if (*cp == ',') {
					positions = 0;
					cp++;
				} else if (!strchr("=+-", *cp))
					break;
			}
			if (*cp) {
				bi_errorf("bad mask");
				return 1;
			}
			new_umask = ~new_umask;
		}
		umask(new_umask);
	}
	return 0;
}

int
c_dot(wp)
	char **wp;
{
	char *file, *cp;
	char **argv;
	int argc;
	int i;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;

	if ((cp = wp[builtin_opt.optind]) == NULL)
		return 0;
	file = search(cp, path, R_OK, (int *) 0);
	if (file == NULL) {
		bi_errorf("%s: not found", cp);
		return 1;
	}

	/* Set positional parameters? */
	if (wp[builtin_opt.optind + 1]) {
		argv = wp + builtin_opt.optind;
		argv[0] = e->loc->argv[0]; /* preserve $0 */
		for (argc = 0; argv[argc + 1]; argc++)
			;
	} else {
		argc = 0;
		argv = (char **) 0;
	}
	i = include(file, argc, argv, 0);
	if (i < 0) { /* should not happen */
		bi_errorf("%s: %s", cp, strerror(errno));
		return 1;
	}
	return i;
}

int
c_wait(wp)
	char **wp;
{
	int UNINITIALIZED(rv);
	int sig;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	wp += builtin_opt.optind;
	if (*wp == (char *) 0) {
		while (waitfor((char *) 0, &sig) >= 0)
			;
		rv = sig;
	} else {
		for (; *wp; wp++)
			rv = waitfor(*wp, &sig);
		if (rv < 0)
			rv = sig ? sig : 127; /* magic exit code: bad job-id */
	}
	return rv;
}

int
c_read(wp)
	char **wp;
{
	register int c = 0;
	int expand = 1, history = 0;
	int expanding;
	int ecode = 0;
	register char *cp;
	int fd = 0;
	struct shf *shf;
	int optc;
	const char *emsg;
	XString cs, xs;
	struct tbl *vp;
	char UNINITIALIZED(*xp);

	while ((optc = ksh_getopt(wp, &builtin_opt, "prsu,")) != EOF)
		switch (optc) {
#ifdef KSH
		  case 'p':
			if ((fd = coproc_getfd(R_OK, &emsg)) < 0) {
				bi_errorf("-p: %s", emsg);
				return 1;
			}
			break;
#endif /* KSH */
		  case 'r':
			expand = 0;
			break;
		  case 's':
			history = 1;
			break;
		  case 'u':
			if (!*(cp = builtin_opt.optarg))
				fd = 0;
			else if ((fd = check_fd(cp, R_OK, &emsg)) < 0) {
				bi_errorf("-u: %s: %s", cp, emsg);
				return 1;
			}
			break;
		  case '?':
			return 1;
		}
	wp += builtin_opt.optind;

	if (*wp == NULL)
		*--wp = "REPLY";

	/* Since we can't necessarily seek backwards on non-regular files,
	 * don't buffer them so we can't read too much.
	 */
	shf = shf_reopen(fd, SHF_RD | SHF_INTERRUPT | can_seek(fd), shl_spare);

	if ((cp = strchr(*wp, '?')) != NULL) {
		*cp = 0;
		if (isatty(fd)) {
			/* at&t ksh says it prints prompt on fd if it's open
			 * for writing and is a tty, but it doesn't do it
			 * (it also doesn't check the interactive flag,
			 * as is indicated in the Kornshell book).
			 */
			shellf("%s", cp+1);
		}
	}

#ifdef KSH
	/* If we are reading from the co-process for the first time,
	 * make sure the other side of the pipe is closed first.  This allows
	 * the detection of eof.
	 *
	 * This is not compatiable with at&t ksh... the fd is kept so another
	 * coproc can be started with same ouput, however, this means eof
	 * can't be detected...  This is why it is closed here.
	 * If this call is removed, remove the eof check below, too.
	* coproc_readw_close(fd);
	 */
#endif /* KSH */

	if (history)
		Xinit(xs, xp, 128, ATEMP);
	expanding = 0;
	Xinit(cs, cp, 128, ATEMP);
	for (; *wp != NULL; wp++) {
		for (cp = Xstring(cs, cp); ; ) {
			if (c == '\n' || c == EOF)
				break;
			while (1) {
				c = shf_getc(shf);
				if (c == '\0'
#ifdef OS2
				    || c == '\r'
#endif /* OS2 */
				    )
					continue;
				if (c == EOF && shf_error(shf)
				    && shf_errno(shf) == EINTR)
				{
					/* Was the offending signal one that
					 * would normally kill a process?
					 * If so, pretend the read was killed.
					 */
					ecode = fatal_trap_check();

					/* non fatal (eg, CHLD), carry on */
					if (!ecode) {
						shf_clearerr(shf);
						continue;
					}
				}
				break;
			}
			if (history) {
				Xcheck(xs, xp);
				Xput(xs, xp, c);
			}
			Xcheck(cs, cp);
			if (expanding) {
				expanding = 0;
				if (c == '\n') {
					c = 0;
					if (Flag(FTALKING) && isatty(fd)) {
						/* set prompt in case this is
						 * called from .profile or $ENV
						 */
						set_prompt(PS2, (Source *) 0);
						pprompt(prompt, 0);
					}
				} else if (c != EOF)
					Xput(cs, cp, c);
				continue;
			}
			if (expand && c == '\\') {
				expanding = 1;
				continue;
			}
			if (c == '\n' || c == EOF)
				break;
			if (ctype(c, C_IFS)) {
				if (Xlength(cs, cp) == 0 && ctype(c, C_IFSWS))
					continue;
				if (wp[1])
					break;
			}
			Xput(cs, cp, c);
		}
		/* strip trailing IFS white space from last variable */
		if (!wp[1])
			while (Xlength(cs, cp) && ctype(cp[-1], C_IFS)
			       && ctype(cp[-1], C_IFSWS))
				cp--;
		Xput(cs, cp, '\0');
		vp = global(*wp);
		if (vp->flag & RDONLY) {
			shf_flush(shf);
			bi_errorf("%s is read only", *wp);
			return 1;
		}
		if (Flag(FEXPORT))
			typeset(*wp, EXPORT, 0, 0, 0);
		setstr(vp, Xstring(cs, cp));
	}

	shf_flush(shf);
	if (history) {
		Xput(xs, xp, '\0');
		source->line++;
		histsave(source->line, Xstring(xs, xp), 1);
		Xfree(xs, xp);
	}
#ifdef KSH
	/* if this is the co-process fd, close the file descriptor
	 * (can get eof if and only if all processes are have died, ie,
	 * coproc.njobs is 0 and the pipe is closed).
	 */
	if (c == EOF && !ecode)
		coproc_read_close(fd);
#endif /* KSH */

	return ecode ? ecode : c == EOF;
}

int
c_eval(wp)
	char **wp;
{
	register struct source *s;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	s = pushs(SWORDS, ATEMP);
	s->u.strv = wp + builtin_opt.optind;
	return shell(s, FALSE);
}

int
c_trap(wp)
	char **wp;
{
	int i;
	char *s;
	register Trap *p;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	wp += builtin_opt.optind;

	if (*wp == NULL) {
		int anydfl = 0;

		for (p = sigtraps, i = SIGNALS+1; --i >= 0; p++) {
			if (p->trap == NULL)
				anydfl = 1;
			else {
				shprintf("trap -- ");
				print_value_quoted(p->trap);
				shprintf(" %s\n", p->name);
			}
		}
#if 0 /* this is ugly and not clear POSIX needs it */
		/* POSIX may need this so output of trap can be saved and
		 * used to restore trap conditions
		 */
		if (anydfl) {
			shprintf("trap -- -");
			for (p = sigtraps, i = SIGNALS+1; --i >= 0; p++)
				if (p->trap == NULL && p->name)
					shprintf(" %s", p->name);
			shprintf(newline);
		}
#endif
		return 0;
	}

	s = (gettrap(*wp) == NULL) ? *wp++ : NULL; /* get command */
	if (s != NULL && s[0] == '-' && s[1] == '\0')
		s = NULL;

	/* set/clear traps */
	while (*wp != NULL) {
		p = gettrap(*wp++);
		if (p == NULL) {
			bi_errorf("bad signal %s", wp[-1]);
			return 1;
		}
		settrap(p, s);
	}
	return 0;
}

int
c_exitreturn(wp)
	char **wp;
{
	int how = LEXIT;
	char *arg;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	arg = wp[builtin_opt.optind];

	if (arg != NULL && !getn(arg, &exstat)) {
		exstat = 1;
		warningf(TRUE, "%s: bad number", arg);
	}
	if (wp[0][0] == 'r') { /* return */
		struct env *ep;

		/* need to tell if this is exit or return so trap exit will
		 * work right (POSIX)
		 */
		for (ep = e; ep; ep = ep->oenv)
			if (STOP_RETURN(ep->type)) {
				how = LRETURN;
				break;
			}
	}

	if (how == LEXIT && !really_exit && j_stopped_running()) {
		really_exit = 1;
		how = LSHELL;
	}

	quitenv();	/* get rid of any i/o redirections */
	unwind(how);
	/*NOTREACHED*/
	return 0;
}

int
c_brkcont(wp)
	char **wp;
{
	int n, quit;
	struct env *ep, *last_ep = (struct env *) 0;
	char *arg;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	arg = wp[builtin_opt.optind];

	if (!arg)
		n = 1;
	else if (!bi_getn(arg, &n))
		return 1;
	quit = n;
	if (quit <= 0) {
		/* at&t ksh does this for non-interactive shells only - weird */
		bi_errorf("%s: bad value", arg);
		return 1;
	}

	/* Stop at E_NONE, E_PARSE, E_FUNC, or E_INCL */
	for (ep = e; ep && !STOP_BRKCONT(ep->type); ep = ep->oenv)
		if (ep->type == E_LOOP) {
			if (--quit == 0)
				break;
			ep->flags |= EF_BRKCONT_PASS;
			last_ep = ep;
		}

	if (quit) {
		/* at&t ksh doesn't print a message - just does what it
		 * can.  We print a message 'cause it helps in debugging
		 * scripts, but don't generate an error (ie, keep going).
		 */
		if (n == quit) {
			warningf(TRUE, "%s: cannot %s", wp[0], wp[0]);
			return 0; 
		}
		/* POSIX says if n is too big, the last enclosing loop
		 * shall be used.  Doesn't say to print an error but we
		 * do anyway 'cause the user messed up.
		 */
		last_ep->flags &= ~EF_BRKCONT_PASS;
		warningf(TRUE, "%s: can only %s %d level(s)",
			wp[0], wp[0], n - quit);
	}

	unwind(*wp[0] == 'b' ? LBREAK : LCONTIN);
	/*NOTREACHED*/
}

int
c_set(wp)
	char **wp;
{
	int argi, setargs;
	struct block *l = e->loc;
	register char **owp = wp;

	if (wp[1] == NULL) {
		static const char *const args [] = { "set", "-", NULL };
		return c_typeset((char **) args);
	}

	argi = parse_args(wp, OF_SET, &setargs);
	if (argi < 0)
		return 1;
	/* set $# and $* */
	if (setargs) {
		owp = wp += argi - 1;
		wp[0] = l->argv[0]; /* save $0 */
		while (*++wp != NULL)
			*wp = str_save(*wp, &l->area);
		l->argc = wp - owp - 1;
		l->argv = (char **) alloc(sizeofN(char *, l->argc+2), &l->area);
		for (wp = l->argv; (*wp++ = *owp++) != NULL; )
			;
	}
	/* POSIX says set exit status is 0, but old scripts that use
	 * getopt(1), use the construct: set -- `getopt ab:c "$@"`
	 * which assumes the exit value set will be that of the ``
	 * (subst_exstat is cleared in execute() so that it will be 0
	 * if there are no command substitutions).
	 */
	return Flag(FPOSIX) ? 0 : subst_exstat;
}

int
c_unset(wp)
	char **wp;
{
	register char *id;
	int optc, unset_var = 1;
	int ret = 0;

	while ((optc = ksh_getopt(wp, &builtin_opt, "fv")) != EOF)
		switch (optc) {
		  case 'f':
			unset_var = 0;
			break;
		  case 'v':
			unset_var = 1;
			break;
		  case '?':
			return 1;
		}
	wp += builtin_opt.optind;
	for (; (id = *wp) != NULL; wp++)
		if (unset_var) {	/* unset variable */
			struct tbl *vp = global(id);

			if (!(vp->flag & ISSET))
			    ret = 1;
			if ((vp->flag&RDONLY)) {
				bi_errorf("%s is read only", vp->name);
				return 1;
			}
			unset(vp, strchr(id, '[') ? 1 : 0);
		} else {		/* unset function */
			if (define(id, (struct op *) NULL))
				ret = 1;
		}
	return ret;
}

int
c_times(wp)
	char **wp;
{
	struct tms all;

	(void) ksh_times(&all);
	shprintf("Shell: %8s user ", clocktos(all.tms_utime));
	shprintf("%8s system\n", clocktos(all.tms_stime));
	shprintf("Kids:  %8s user ", clocktos(all.tms_cutime));
	shprintf("%8s system\n", clocktos(all.tms_cstime));

	return 0;
}

/*
 * time pipeline (really a statement, not a built-in command)
 */
int
timex(t, f)
	struct op *t;
	int f;
{
	int rv;
	struct tms t0, t1;
	clock_t t0t, t1t;
	extern clock_t j_usrtime, j_systime; /* computed by j_wait */

	j_usrtime = j_systime = 0;
	t0t = ksh_times(&t0);
	rv = execute(t->left, f);
	t1t = ksh_times(&t1);

	shf_fprintf(shl_out, "%8s real ", clocktos(t1t - t0t));
	shf_fprintf(shl_out, "%8s user ",
	       clocktos(t1.tms_utime - t0.tms_utime + j_usrtime));
	shf_fprintf(shl_out, "%8s system ",
	       clocktos(t1.tms_stime - t0.tms_stime + j_systime));
	shf_fprintf(shl_out, newline);

	return rv;
}

static char *
clocktos(t)
	clock_t t;
{
	static char temp[20];
	register int i;
	register char *cp = temp + sizeof(temp);

	if (CLK_TCK != 100)	/* convert to 1/100'ths */
	    t = (t < 1000000000/CLK_TCK) ?
		    (t * 100) / CLK_TCK : (t / CLK_TCK) * 100;

	*--cp = '\0';
	*--cp = 's';
	for (i = -2; i <= 0 || t > 0; i++) {
		if (i == 0)
			*--cp = '.';
		*--cp = '0' + (char)(t%10);
		t /= 10;
	}
	return cp;
}

/* exec with no args - args case is taken care of in comexec() */
int
c_exec(wp)
	char ** wp;
{
	int i;

	/* make sure redirects stay in place */
	if (e->savefd != NULL) {
		for (i = 0; i < NUFILE; i++) {
			if (e->savefd[i] > 0)
				close(e->savefd[i]);
			/* keep anything > 2 private */
			if (i > 2 && e->savefd[i])
				fd_clexec(i);
		}
		e->savefd = NULL; 
	}
	return 0;
}

/* dummy function, special case in comexec() */
int
c_builtin(wp)
	char ** wp;
{
	return 0;
}

extern	int c_test ARGS((char **wp));		/* in c_test.c */
extern	int c_ulimit ARGS((char **wp));		/* in c_ulimit.c */

/* A leading = means assignments before command are kept;
 * a leading * means a POSIX special builtin;
 * a leading + means a POSIX regular builtin
 * (* and + should not be combined).
 */
const struct builtin shbuiltins [] = {
	{"*=.", c_dot},
	{"*=:", c_label},
	{"[", c_test},
	{"*=break", c_brkcont},
	{"=builtin", c_builtin},
	{"*=continue", c_brkcont},
	{"*=eval", c_eval},
	{"*=exec", c_exec},
	{"*=exit", c_exitreturn},
	{"+false", c_label},
	{"*=return", c_exitreturn},
	{"*=set", c_set},
	{"*=shift", c_shift},
	{"=times", c_times},
	{"*=trap", c_trap},
	{"+=wait", c_wait},
	{"+read", c_read},
	{"test", c_test},
	{"+true", c_label},
	{"ulimit", c_ulimit},
	{"+umask", c_umask},
	{"*=unset", c_unset},
#ifdef OS2
	/* In OS2, the first line of a file can be "extproc name", which
	 * tells the command interpreter (cmd.exe) to use name to execute
	 * the file.  For this to be useful, ksh must ignore commands
	 * starting with extproc and this does the trick...
	 */
	{"extproc", c_label},
#endif /* OS2 */
	{NULL, NULL}
};
