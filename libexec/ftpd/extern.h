/*	$NetBSD: extern.h,v 1.5.2.2 1997/11/11 14:27:48 mrg Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.2 (Berkeley) 4/4/94
 */

void	blkfree __P((char **));
char   *conffilename __P((const char *));
char  **copyblk __P((char **));
void	cwd __P((char *));
void	delete __P((char *));
char   *do_conversion __P((const char *));
void	dologout __P((int));
void	fatal __P((char *));
int	ftpd_pclose __P((FILE *));
FILE   *ftpd_popen __P((char *, char *, int));
char   *getline __P((char *, int, FILE *));
void	logwtmp __P((char *, char *, char *));
void	lreply __P((int, const char *, ...));
void	makedir __P((char *));
void	nack __P((char *));
void	parse_conf __P((char *));
void	pass __P((char *));
void	passive __P((void));
void	perror_reply __P((int, char *));
void	pwd __P((void));
void	removedir __P((char *));
void	renamecmd __P((char *, char *));
char   *renamefrom __P((char *));
void	reply __P((int, const char *, ...));
void	retrieve __P((char *, char *));
void	send_file_list __P((char *));
void	show_chdir_messages __P((int));
void	statcmd __P((void));
void	statfilecmd __P((char *));
void	store __P((char *, char *, int));
void	upper __P((char *));
void	user __P((char *));
void	yyerror __P((char *));

#define CLASS_CHROOT	"chroot"
#define CLASS_GUEST	"guest"
#define CLASS_REAL	"real"

struct ftpconv {
	struct ftpconv	*next;
	char		*suffix;	/* Suffix of requested name */
	char		*types;		/* Valid file types */
	char		*disable;	/* File to disable conversions */
	char		*command;	/* Command to do the conversion */
};

struct ftpclass {
	int		 checkportcmd;	/* Check PORT commands are valid */
	char		*classname;	/* Current class */
	struct ftpconv 	*conversions;	/* List of conversions */
	char		*display;	/* Files to display upon chdir */
	unsigned int	 maxtimeout;	/* Maximum permitted timeout */
	int		 modify;	/* Allow dele, mkd, rmd, umask, chmod */
	char		*notify;	/* Files to notify about upon chdir */
	unsigned int	 timeout;	/* Default timeout */
	mode_t		 umask;		/* Umask to use */
};
