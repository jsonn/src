/*	$NetBSD: skeyinit.c,v 1.12.10.1 2000/07/17 19:55:54 mjl Exp $	*/

/* S/KEY v1.1b (skeyinit.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *
 * Modifications:
 *          Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * S/KEY initialization and seed update
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include <skey.h>

#ifndef SKEY_NAMELEN
#define SKEY_NAMELEN    4
#endif

int main __P((int, char **));

int main(int argc, char **argv)
{
	int     rval, nn, i, l;
	int n = 0, defaultsetup = 1, zerokey = 0, hexmode = 0;
	time_t  now;
	char	hostname[MAXHOSTNAMELEN + 1];
	char    seed[SKEY_MAX_PW_LEN+2], key[SKEY_BINKEY_SIZE], defaultseed[SKEY_MAX_SEED_LEN+1];
	char    passwd[SKEY_MAX_PW_LEN+2], passwd2[SKEY_MAX_PW_LEN+2], tbuf[27], buf[80];
	char    lastc, me[UT_NAMESIZE+1], *p, *pw, *ht = NULL;
	const	char *salt;
	struct	skey skey;
	struct	passwd *pp;
	struct	tm *tm;
	int c;

	if (geteuid() != 0)
		errx(1, "must be setuid root.");

	if (gethostname(hostname, sizeof(hostname)) < 0)
		err(1, "gethostname");
	(void)strncpy(defaultseed, hostname, sizeof(defaultseed)- 1);
	defaultseed[SKEY_NAMELEN] = '\0';
	(void)time(&now);
	(void)sprintf(tbuf, "%05ld", (long) (now % 100000));
	(void)strncat(defaultseed, tbuf, sizeof(defaultseed) - 5);

	if ((pp = getpwuid(getuid())) == NULL)
		err(1, "no user with uid %ld", (u_long)getuid());
	(void)strncpy(me, pp->pw_name, sizeof(me) - 1);

	if ((pp = getpwnam(me)) == NULL)
		err(1, "Who are you?");
	salt = pp->pw_passwd;

	while((c = getopt(argc, argv, "n:t:sxz")) != -1) {
		switch(c) {
			case 'n':
				n = atoi(optarg);
				if(n < 1 || n > SKEY_MAX_SEQ)
					errx(1, "count must be between 1 and %d", SKEY_MAX_SEQ);
				break;
			case 't':
				if(skey_set_algorithm(optarg) == NULL)
					errx(1, "Unknown hash algorithm %s", optarg);
				ht = optarg;
				break;
			case 's':
				defaultsetup = 0;
				break;
			case 'x':
				hexmode = 1;
				break;
			case 'z':
				zerokey = 1;
				break;
			default:
				err(1, "Usage: %s [-n count] [-t md4|md5|sha1] [-s] [-x] [-z] [user]\n", argv[0]);
		}
	}

	if(argc > optind) {
		pp = getpwnam(argv[optind]);
		if (pp == NULL)
			errx(1, "User %s unknown", argv[optind]);
		}

	if (strcmp(pp->pw_name, me) != 0) {
		if (getuid() != 0) {
			/* Only root can change other's passwds */
			errx(1, "Permission denied.");
		}
	}

	if (getuid() != 0) {
		pw = getpass("Password:");
		p = crypt(pw, salt);

		if (strcmp(p, pp->pw_passwd)) {
			errx(1, "Password incorrect.");
		}
	}

	rval = skeylookup(&skey, pp->pw_name);
	switch (rval) {
	case -1:
		err(1, "cannot open database");
	case 0:
		/* comment out user if asked to */
		if (zerokey)
			exit(skeyzero(&skey, pp->pw_name));

		printf("[Updating %s]\n", pp->pw_name);
		printf("Old key: [%s] %s\n", skey_get_algorithm(), skey.seed);

		/*
		 * lets be nice if they have a skey.seed that
		 * ends in 0-8 just add one
		 */
		l = strlen(skey.seed);
		if (l > 0) {
			lastc = skey.seed[l - 1];
			if (isdigit((unsigned char)lastc) && lastc != '9') {
				(void)strncpy(defaultseed, skey.seed,
				    sizeof(defaultseed) - 1);
				defaultseed[l - 1] = lastc + 1;
			}
			if (isdigit((unsigned char)lastc) && lastc == '9' &&
			    l < 16) {
				(void)strncpy(defaultseed, skey.seed,
				    sizeof(defaultseed) - 1);
				defaultseed[l - 1] = '0';
				defaultseed[l] = '0';
				defaultseed[l + 1] = '\0';
			}
		}
		break;
	case 1:
		if (zerokey)
			errx(1, "You have no entry to zero.");
		printf("[Adding %s]\n", pp->pw_name);
		break;
	}
	
	if(n==0)
		n = 99;

	/* Set hash type if asked to */
	if (ht) {
		/* Need to zero out old key when changing algorithm */
		if (strcmp(ht, skey_get_algorithm()) && skey_set_algorithm(ht))
			zerokey = 1;
	}

	if (!defaultsetup) {
		printf("You need the 6 english words generated from the \"skey\" command.\n");
		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);
			printf("Enter sequence count from 1 to %d: ", SKEY_MAX_SEQ);
			fgets(buf, sizeof(buf), stdin);
			n = atoi(buf);
			if (n > 0 && n < SKEY_MAX_SEQ)
				break;	/* Valid range */
			printf("\nError: Count must be between 0 and %d\n", SKEY_MAX_SEQ);
		}
	
		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			printf("Enter new seed [default %s]: ", defaultseed);
			fflush(stdout);
			fgets(seed, sizeof(seed), stdin);
			rip(seed);
			for (p = seed; *p; p++) {
				if (isalpha(*p)) {
					if (isupper(*p))
						*p = tolower(*p);
				} else if (!isdigit(*p)) {
					(void)puts("Error: seed may only contain alphanumeric characters");
					break;
				}
			}
			if (*p == '\0')
				break;  /* Valid seed */
		}
		if (strlen(seed) > SKEY_MAX_SEED_LEN) {
			printf("Notice: Seed truncated to %d characters.\n", SKEY_MAX_SEED_LEN);
			seed[SKEY_MAX_SEED_LEN] = '\0';
		}
		if (seed[0] == '\0')
			(void)strcpy(seed, defaultseed);

		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			printf("otp-%s %d %s\ns/key access password: ",
				skey_get_algorithm(), n, seed);
			fgets(buf, sizeof(buf), stdin);
			rip(buf);
			backspace(buf);

			if (buf[0] == '?') {
				puts("Enter 6 English words from secure S/Key calculation.");
				continue;
			} else if (buf[0] == '\0') {
				exit(1);
			}
			if (etob(key, buf) == 1 || atob8(key, buf) == 0)
				break;	/* Valid format */
			(void)puts("Invalid format - try again with 6 English words.");
		}
	} else {
	/* Get user's secret password */
	puts("Reminder - Only use this method if you are directly connected\n"
	      "           or have an encrypted channel. If you are using telnet\n"
	      "           or rlogin, exit with no password and use skeyinit -s.\n");

	for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			printf("Enter secret password: ");
			readpass(passwd, sizeof(passwd));
			if (passwd[0] == '\0')
				exit(1);

			if (strlen(passwd) < SKEY_MIN_PW_LEN) {
				(void)fprintf(stderr,
				    "Your password must be at least %d characters long.\n", SKEY_MIN_PW_LEN);
				continue;
			} else if (strcmp(passwd, pp->pw_name) == 0) {
				(void)fputs("Your password may not be the same as your user name.\n", stderr);
				continue;
			}
#if 0			 
			else if (strspn(passwd, "abcdefghijklmnopqrstuvwxyz") == strlen(passwd)) {
				(void)fputs("Your password must contain more than just lower case letters.\n"
					    "Whitespace, numbers, and puctuation are suggested.\n", stderr);
				continue;
			}
#endif
			printf("Again secret password: ");
			readpass(passwd2, sizeof(passwd));
			if (passwd2[0] == '\0')
				exit(1);

			if (strcmp(passwd, passwd2) == 0)
				break;

			puts("Passwords do not match.");
		}

		/* Crunch seed and password into starting key */
		(void)strcpy(seed, defaultseed);
		if (keycrunch(key, seed, passwd) != 0)
			err(2, "key crunch failed");
		nn = n;
		while (nn-- != 0)
			f(key);
	}
	(void)time(&now);
	tm = localtime(&now);
	(void)strftime(tbuf, sizeof(tbuf), " %b %d,%Y %T", tm);

	if ((skey.val = (char *)malloc(16 + 1)) == NULL)
		err(1, "Can't allocate memory");

	/* Zero out old key if necesary (entry would change size) */
	if (zerokey) {
		(void)skeyzero(&skey, pp->pw_name);
		/* Re-open keys file and seek to the end */
		if (skeylookup(&skey, pp->pw_name) == -1)
			err(1, "cannot open database");
	}

	btoa8(skey.val, key);

	/*
	 * Obtain an exclusive lock on the key file so we don't
	 * clobber someone authenticating themselves at the same time.
	 */
	for (i = 0; i < 300; i++) {
		if ((rval = flock(fileno(skey.keyfile), LOCK_EX|LOCK_NB)) == 0
		    || errno != EWOULDBLOCK)
			break;
		usleep(100000);			/* Sleep for 0.1 seconds */
	}
	if (rval == -1)	{			/* Can't get exclusive lock */
		errno = EAGAIN;
		err(1, "cannot open database");
	}

	/* Don't save algorithm type for md4 (keep record length same) */
	if (strcmp(skey_get_algorithm(), "md4") == 0)
		(void)fprintf(skey.keyfile, "%s %04d %-16s %s %-21s\n",
		    pp->pw_name, n, seed, skey.val, tbuf);
	else
		(void)fprintf(skey.keyfile, "%s %s %04d %-16s %s %-21s\n",
		    pp->pw_name, skey_get_algorithm(), n, seed, skey.val, tbuf);

	(void)fclose(skey.keyfile);

	(void)printf("\nID %s skey is otp-%s %d %s\n", pp->pw_name,
		     skey_get_algorithm(), n, seed);
	(void)printf("Next login password: %s\n\n",
		     hexmode ? put8(buf, key) : btoe(buf, key));

	return(0);
}
