/* Author: Wietse Venema <wietse@wzv.win.tue.nl> */

#include "bsd_locl.h"

RCSID("$Id: utmpx_login.c,v 1.1.1.1.4.2 2000/06/16 18:46:18 thorpej Exp $");

/* utmpx_login - update utmp and wtmp after login */

#ifndef HAVE_UTMPX_H
int utmpx_login(char *line, char *user, char *host) { return 0; }
#else

static void
utmpx_update(struct utmpx *ut, char *line, char *user, char *host)
{
    struct timeval tmp;
    char *clean_tty = clean_ttyname(line);

    strncpy(ut->ut_line, clean_tty, sizeof(ut->ut_line));
#ifdef HAVE_STRUCT_UTMPX_UT_ID
    strncpy(ut->ut_id, make_id(clean_tty), sizeof(ut->ut_id));
#endif
    strncpy(ut->ut_user, user, sizeof(ut->ut_user));
    strncpy(ut->ut_host, host, sizeof(ut->ut_host));
#ifdef HAVE_STRUCT_UTMPX_UT_SYSLEN
    ut->ut_syslen = strlen(host) + 1;
    if (ut->ut_syslen > sizeof(ut->ut_host))
        ut->ut_syslen = sizeof(ut->ut_host);
#endif
    ut->ut_type = USER_PROCESS;
    gettimeofday (&tmp, 0);
    ut->ut_tv.tv_sec = tmp.tv_sec;
    ut->ut_tv.tv_usec = tmp.tv_usec;
    pututxline(ut);
#ifdef WTMPX_FILE
    updwtmpx(WTMPX_FILE, ut);
#elif defined(WTMP_FILE)
    {
	struct utmp utmp;
	int fd;

	prepare_utmp (&utmp, line, user, host);
	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) >= 0) {
	    write(fd, &utmp, sizeof(struct utmp));
	    close(fd);
	}
    }
#endif
}

int
utmpx_login(char *line, char *user, char *host)
{
    struct utmpx *ut;
    pid_t   mypid = getpid();
    int     ret = (-1);

    /*
     * SYSV4 ttymon and login use tty port names with the "/dev/" prefix
     * stripped off. Rlogind and telnetd, on the other hand, make utmpx
     * entries with device names like /dev/pts/nnn. We therefore cannot use
     * getutxline(). Return nonzero if no utmp entry was found with our own
     * process ID for a login or user process.
     */

    while ((ut = getutxent())) {
        /* Try to find a reusable entry */
	if (ut->ut_pid == mypid
	    && (   ut->ut_type == INIT_PROCESS
		|| ut->ut_type == LOGIN_PROCESS
		|| ut->ut_type == USER_PROCESS)) {
	    utmpx_update(ut, line, user, host);
	    ret = 0;
	    break;
	}
    }
    if (ret == -1) {
        /* Grow utmpx file by one record. */
        struct utmpx newut;
	memset(&newut, 0, sizeof(newut));
	newut.ut_pid = mypid;
        utmpx_update(&newut, line, user, host);
	ret = 0;
    }
    endutxent();
    return (ret);
}
#endif /* HAVE_UTMPX_H */
