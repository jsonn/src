/*	$NetBSD: session.c,v 1.1.1.1.2.5 2003/09/17 23:46:42 christos Exp $	*/
/*
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 support by Markus Friedl.
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: session.c,v 1.110 2001/12/01 21:41:48 markus Exp $");

#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "xmalloc.h"
#include "sshpty.h"
#include "packet.h"
#include "buffer.h"
#include "mpaux.h"
#include "uidswap.h"
#include "compat.h"
#include "channels.h"
#include "bufaux.h"
#include "auth.h"
#include "auth-options.h"
#include "pathnames.h"
#include "log.h"
#include "servconf.h"
#include "sshlogin.h"
#include "serverloop.h"
#include "canohost.h"
#include "session.h"

#include <tzfile.h>

/* types */

#define TTYSZ 64
typedef struct Session Session;
struct Session {
	int	used;
	int	self;
	struct passwd *pw;
	Authctxt *authctxt;
	pid_t	pid;
	/* tty */
	char	*term;
	int	ptyfd, ttyfd, ptymaster;
	int	row, col, xpixel, ypixel;
	char	tty[TTYSZ];
	/* X11 */
	int	display_number;
	char	*display;
	int	screen;
	char	*auth_display;
	char	*auth_proto;
	char	*auth_data;
	int	single_connection;
	/* proto 2 */
	int	chanid;
	int	is_subsystem;
};

/* func */

Session *session_new(void);
void	session_set_fds(Session *, int, int, int);
static void	session_pty_cleanup(void *);
void	session_proctitle(Session *);
int	session_setup_x11fwd(Session *);
void	do_exec_pty(Session *, const char *);
void	do_exec_no_pty(Session *, const char *);
void	do_exec(Session *, const char *);
void	do_login(Session *, const char *);
void	do_child(Session *, const char *);
void	do_motd(void);
int	check_quietlogin(Session *, const char *);

static void do_authenticated1(Authctxt *);
static void do_authenticated2(Authctxt *);

static void session_close(Session *);
static int session_pty_req(Session *);

/* import */
extern ServerOptions options;
extern char *__progname;
extern int log_stderr;
extern int debug_flag;
extern u_int utmp_len;
extern int startup_pipe;
extern void destroy_sensitive_data(void);

/* original command from peer. */
const char *original_command = NULL;

/* data */
#define MAX_SESSIONS 10
Session	sessions[MAX_SESSIONS];

#ifdef HAVE_LOGIN_CAP
static login_cap_t *lc;
#endif

void
do_authenticated(Authctxt *authctxt)
{
	/*
	 * Cancel the alarm we set to limit the time taken for
	 * authentication.
	 */
	alarm(0);
	if (startup_pipe != -1) {
		close(startup_pipe);
		startup_pipe = -1;
	}
#ifdef HAVE_LOGIN_CAP
	if ((lc = login_getclass(authctxt->pw->pw_class)) == NULL) {
		error("unable to get login class");
		return;
	}
#ifdef BSD_AUTH
	if (auth_approval(NULL, lc, authctxt->pw->pw_name, "ssh") <= 0) {
		packet_disconnect("Approval failure for %s",
		    authctxt->pw->pw_name);
	}
#endif
#endif
	/* setup the channel layer */
	if (!no_port_forwarding_flag && options.allow_tcp_forwarding)
		channel_permit_all_opens();

	if (compat20)
		do_authenticated2(authctxt);
	else
		do_authenticated1(authctxt);

	/* remove agent socket */
	if (auth_get_socket_name())
		auth_sock_cleanup_proc(authctxt->pw);
#ifdef KRB4
	if (options.kerberos_ticket_cleanup)
		krb4_cleanup_proc(authctxt);
#endif
#ifdef KRB5
	if (options.kerberos_ticket_cleanup)
		krb5_cleanup_proc(authctxt);
#endif
}

/*
 * Prepares for an interactive session.  This is called after the user has
 * been successfully authenticated.  During this message exchange, pseudo
 * terminals are allocated, X11, TCP/IP, and authentication agent forwardings
 * are requested, etc.
 */
static void
do_authenticated1(Authctxt *authctxt)
{
	Session *s;
	char *command;
	int success, type, plen, screen_flag;
	int compression_level = 0, enable_compression_after_reply = 0;
	u_int proto_len, data_len, dlen;

	s = session_new();
	s->authctxt = authctxt;
	s->pw = authctxt->pw;

	/*
	 * We stay in this loop until the client requests to execute a shell
	 * or a command.
	 */
	for (;;) {
		success = 0;

		/* Get a packet from the client. */
		type = packet_read(&plen);

		/* Process the packet. */
		switch (type) {
		case SSH_CMSG_REQUEST_COMPRESSION:
			packet_integrity_check(plen, 4, type);
			compression_level = packet_get_int();
			if (compression_level < 1 || compression_level > 9) {
				packet_send_debug("Received illegal compression level %d.",
				     compression_level);
				break;
			}
			/* Enable compression after we have responded with SUCCESS. */
			enable_compression_after_reply = 1;
			success = 1;
			break;

		case SSH_CMSG_REQUEST_PTY:
			success = session_pty_req(s);
			break;

		case SSH_CMSG_X11_REQUEST_FORWARDING:
			s->auth_proto = packet_get_string(&proto_len);
			s->auth_data = packet_get_string(&data_len);

			screen_flag = packet_get_protocol_flags() &
			    SSH_PROTOFLAG_SCREEN_NUMBER;
			debug2("SSH_PROTOFLAG_SCREEN_NUMBER: %d", screen_flag);

			if (packet_remaining() == 4) {
				if (!screen_flag)
					debug2("Buggy client: "
					    "X11 screen flag missing");
				s->screen = packet_get_int();
			} else {
				s->screen = 0;
			}
			packet_done();
			success = session_setup_x11fwd(s);
			if (!success) {
				xfree(s->auth_proto);
				xfree(s->auth_data);
				s->auth_proto = NULL;
				s->auth_data = NULL;
			}
			break;

		case SSH_CMSG_AGENT_REQUEST_FORWARDING:
			if (no_agent_forwarding_flag || compat13) {
				debug("Authentication agent forwarding not permitted for this authentication.");
				break;
			}
			debug("Received authentication agent forwarding request.");
			success = auth_input_request_forwarding(s->pw);
			break;

		case SSH_CMSG_PORT_FORWARD_REQUEST:
			if (no_port_forwarding_flag) {
				debug("Port forwarding not permitted for this authentication.");
				break;
			}
			if (!options.allow_tcp_forwarding) {
				debug("Port forwarding not permitted.");
				break;
			}
			debug("Received TCP/IP port forwarding request.");
			channel_input_port_forward_request(s->pw->pw_uid == 0, options.gateway_ports);
			success = 1;
			break;

		case SSH_CMSG_MAX_PACKET_SIZE:
			if (packet_set_maxsize(packet_get_int()) > 0)
				success = 1;
			break;
			
#if defined(AFS) || defined(KRB5)
		case SSH_CMSG_HAVE_KERBEROS_TGT:
			if (!options.kerberos_tgt_passing) {
				verbose("Kerberos TGT passing disabled.");
			} else {
				char *kdata = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				
				/* XXX - 0x41, see creds_to_radix version */
				if (kdata[0] != 0x41) {
#ifdef KRB5
					krb5_data tgt;
					tgt.data = kdata;
					tgt.length = dlen;
					
					if (auth_krb5_tgt(s->authctxt, &tgt))
						success = 1;
					else
						verbose("Kerberos v5 TGT refused for %.100s", s->authctxt->user);
#endif /* KRB5 */
				} else {
#ifdef AFS
					if (auth_krb4_tgt(s->authctxt, kdata))
						success = 1;
					else
						verbose("Kerberos v4 TGT refused for %.100s", s->authctxt->user);
#endif /* AFS */
				}
				xfree(kdata);
			}
			break;
#endif /* AFS || KRB5 */
			
#ifdef AFS
		case SSH_CMSG_HAVE_AFS_TOKEN:
			if (!options.afs_token_passing || !k_hasafs()) {
				verbose("AFS token passing disabled.");
			} else {
				/* Accept AFS token. */
				char *token = packet_get_string(&dlen);
				packet_integrity_check(plen, 4 + dlen, type);
				
				if (auth_afs_token(s->authctxt, token))
					success = 1;
				else
					verbose("AFS token refused for %.100s",
					    s->authctxt->user);
				xfree(token);
			}
			break;
#endif /* AFS */

		case SSH_CMSG_EXEC_SHELL:
		case SSH_CMSG_EXEC_CMD:
			if (type == SSH_CMSG_EXEC_CMD) {
				command = packet_get_string(&dlen);
				debug("Exec command '%.500s'", command);
				do_exec(s, command);
				xfree(command);
			} else {
				do_exec(s, NULL);
			}
			packet_done();
			session_close(s);
			return;

		default:
			/*
			 * Any unknown messages in this phase are ignored,
			 * and a failure message is returned.
			 */
			log("Unknown packet type received after authentication: %d", type);
		}
		packet_start(success ? SSH_SMSG_SUCCESS : SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();

		/* Enable compression now that we have replied if appropriate. */
		if (enable_compression_after_reply) {
			enable_compression_after_reply = 0;
			packet_start_compression(compression_level);
		}
	}
}

/*
 * This is called to fork and execute a command when we have no tty.  This
 * will call do_child from the child, and server_loop from the parent after
 * setting up file descriptors and such.
 */
void
do_exec_no_pty(Session *s, const char *command)
{
	int pid;

#ifdef USE_PIPES
	int pin[2], pout[2], perr[2];
	/* Allocate pipes for communicating with the program. */
	if (pipe(pin) < 0 || pipe(pout) < 0 || pipe(perr) < 0)
		packet_disconnect("Could not create pipes: %.100s",
				  strerror(errno));
#else /* USE_PIPES */
	int inout[2], err[2];
	/* Uses socket pairs to communicate with the program. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, inout) < 0 ||
	    socketpair(AF_UNIX, SOCK_STREAM, 0, err) < 0)
		packet_disconnect("Could not create socket pairs: %.100s",
				  strerror(errno));
#endif /* USE_PIPES */
	if (s == NULL)
		fatal("do_exec_no_pty: no session");

	session_proctitle(s);

	/* Fork the child. */
	if ((pid = fork()) == 0) {
		/* Child.  Reinitialize the log since the pid has changed. */
		log_init(__progname, options.log_level, options.log_facility, log_stderr);

		/*
		 * Create a new session and process group since the 4.4BSD
		 * setlogin() affects the entire process group.
		 */
		if (setsid() < 0)
			error("setsid failed: %.100s", strerror(errno));

#ifdef USE_PIPES
		/*
		 * Redirect stdin.  We close the parent side of the socket
		 * pair, and make the child side the standard input.
		 */
		close(pin[1]);
		if (dup2(pin[0], 0) < 0)
			perror("dup2 stdin");
		close(pin[0]);

		/* Redirect stdout. */
		close(pout[0]);
		if (dup2(pout[1], 1) < 0)
			perror("dup2 stdout");
		close(pout[1]);

		/* Redirect stderr. */
		close(perr[0]);
		if (dup2(perr[1], 2) < 0)
			perror("dup2 stderr");
		close(perr[1]);
#else /* USE_PIPES */
		/*
		 * Redirect stdin, stdout, and stderr.  Stdin and stdout will
		 * use the same socket, as some programs (particularly rdist)
		 * seem to depend on it.
		 */
		close(inout[1]);
		close(err[1]);
		if (dup2(inout[0], 0) < 0)	/* stdin */
			perror("dup2 stdin");
		if (dup2(inout[0], 1) < 0)	/* stdout.  Note: same socket as stdin. */
			perror("dup2 stdout");
		if (dup2(err[0], 2) < 0)	/* stderr */
			perror("dup2 stderr");
#endif /* USE_PIPES */

		/* Do processing for the child (exec command etc). */
		do_child(s, command);
		/* NOTREACHED */
	}
	if (pid < 0)
		packet_disconnect("fork failed: %.100s", strerror(errno));
	s->pid = pid;
	/* Set interactive/non-interactive mode. */
	packet_set_interactive(s->display != NULL);
#ifdef USE_PIPES
	/* We are the parent.  Close the child sides of the pipes. */
	close(pin[0]);
	close(pout[1]);
	close(perr[1]);

	if (compat20) {
		session_set_fds(s, pin[1], pout[0], s->is_subsystem ? -1 : perr[0]);
	} else {
		/* Enter the interactive session. */
		server_loop(pid, pin[1], pout[0], perr[0]);
		/* server_loop has closed pin[1], pout[0], and perr[0]. */
	}
#else /* USE_PIPES */
	/* We are the parent.  Close the child sides of the socket pairs. */
	close(inout[0]);
	close(err[0]);

	/*
	 * Enter the interactive session.  Note: server_loop must be able to
	 * handle the case that fdin and fdout are the same.
	 */
	if (compat20) {
		session_set_fds(s, inout[1], inout[1], s->is_subsystem ? -1 : err[1]);
	} else {
		server_loop(pid, inout[1], inout[1], err[1]);
		/* server_loop has closed inout[1] and err[1]. */
	}
#endif /* USE_PIPES */
}

/*
 * This is called to fork and execute a command when we have a tty.  This
 * will call do_child from the child, and server_loop from the parent after
 * setting up file descriptors, controlling tty, updating wtmp, utmp,
 * lastlog, and other such operations.
 */
void
do_exec_pty(Session *s, const char *command)
{
	int fdout, ptyfd, ttyfd, ptymaster;
	pid_t pid;

	if (s == NULL)
		fatal("do_exec_pty: no session");
	ptyfd = s->ptyfd;
	ttyfd = s->ttyfd;

	/* Fork the child. */
	if ((pid = fork()) == 0) {

		/* Child.  Reinitialize the log because the pid has changed. */
		log_init(__progname, options.log_level, options.log_facility, log_stderr);
		/* Close the master side of the pseudo tty. */
		close(ptyfd);

		/* Make the pseudo tty our controlling tty. */
		pty_make_controlling_tty(&ttyfd, s->tty);

		/* Redirect stdin/stdout/stderr from the pseudo tty. */
		if (dup2(ttyfd, 0) < 0)
			error("dup2 stdin: %s", strerror(errno));
		if (dup2(ttyfd, 1) < 0)
			error("dup2 stdout: %s", strerror(errno));
		if (dup2(ttyfd, 2) < 0)
			error("dup2 stderr: %s", strerror(errno));

		/* Close the extra descriptor for the pseudo tty. */
		close(ttyfd);

		/* record login, etc. similar to login(1) */
		if (!(options.use_login && command == NULL))
			do_login(s, command);

		/* Do common processing for the child, such as execing the command. */
		do_child(s, command);
		/* NOTREACHED */
	}
	if (pid < 0)
		packet_disconnect("fork failed: %.100s", strerror(errno));
	s->pid = pid;

	/* Parent.  Close the slave side of the pseudo tty. */
	close(ttyfd);

	/*
	 * Create another descriptor of the pty master side for use as the
	 * standard input.  We could use the original descriptor, but this
	 * simplifies code in server_loop.  The descriptor is bidirectional.
	 */
	fdout = dup(ptyfd);
	if (fdout < 0)
		packet_disconnect("dup #1 failed: %.100s", strerror(errno));

	/* we keep a reference to the pty master */
	ptymaster = dup(ptyfd);
	if (ptymaster < 0)
		packet_disconnect("dup #2 failed: %.100s", strerror(errno));
	s->ptymaster = ptymaster;

	/* Enter interactive session. */
	packet_set_interactive(1);
	if (compat20) {
		session_set_fds(s, ptyfd, fdout, -1);
	} else {
		server_loop(pid, ptyfd, fdout, -1);
		/* server_loop _has_ closed ptyfd and fdout. */
	}
}

/*
 * This is called to fork and execute a command.  If another command is
 * to be forced, execute that instead.
 */
void
do_exec(Session *s, const char *command)
{
	if (forced_command) {
		original_command = command;
		command = forced_command;
		debug("Forced command '%.900s'", command);
	}

	if (s->ttyfd != -1)
		do_exec_pty(s, command);
	else
		do_exec_no_pty(s, command);

	original_command = NULL;
}


/* administrative, login(1)-like work */
void
do_login(Session *s, const char *command)
{
	char *time_string;
	char hostname[MAXHOSTNAMELEN];
	socklen_t fromlen;
	struct sockaddr_storage from;
	time_t last_login_time;
	struct passwd * pw = s->pw;
	pid_t pid = getpid();

	/*
	 * Get IP address of client. If the connection is not a socket, let
	 * the address be 0.0.0.0.
	 */
	memset(&from, 0, sizeof(from));
	if (packet_connection_is_on_socket()) {
		fromlen = sizeof(from);
		if (getpeername(packet_get_connection_in(),
		     (struct sockaddr *) & from, &fromlen) < 0) {
			debug("getpeername: %.100s", strerror(errno));
			fatal_cleanup();
		}
	}

	/* Get the time and hostname when the user last logged in. */
	if (options.print_lastlog) {
		hostname[0] = '\0';
		last_login_time = get_last_login_time(pw->pw_uid, pw->pw_name,
		    hostname, sizeof(hostname));
	}

	/* Record that there was a login on that tty from the remote host. */
	record_login(pid, s->tty, pw->pw_name, pw->pw_uid,
	    get_remote_name_or_ip(utmp_len, options.reverse_mapping_check),
	    (struct sockaddr *)&from);

	if (check_quietlogin(s, command))
		return;

	if (pw->pw_expire || pw->pw_change) {
		struct timeval tv;
		const time_t pw_warntime = _PASSWORD_WARNDAYS * SECSPERDAY;

		(void)gettimeofday(&tv, (struct timezone *)NULL);
		if (pw->pw_expire && pw->pw_expire - tv.tv_sec < pw_warntime)
			printf("Warning: your account expires on %s",
			    ctime(&pw->pw_expire));
		if (pw->pw_change && pw->pw_change - tv.tv_sec < pw_warntime)
			printf("Warning: your password expires on %s",
			    ctime(&pw->pw_change));
	}

	if (options.print_lastlog && last_login_time != 0) {
		time_string = ctime(&last_login_time);
		if (strchr(time_string, '\n'))
			*strchr(time_string, '\n') = 0;
		if (strcmp(hostname, "") == 0)
			printf("Last login: %s\r\n", time_string);
		else
			printf("Last login: %s from %s\r\n", time_string, hostname);
	}

	do_motd();
}

/*
 * Display the message of the day.
 */
void
do_motd(void)
{
	FILE *f;
	char buf[256];

	if (options.print_motd) {
#ifdef HAVE_LOGIN_CAP
		f = fopen(login_getcapstr(lc, "welcome", "/etc/motd",
		    "/etc/motd"), "r");
#else
		f = fopen("/etc/motd", "r");
#endif
		if (f) {
			while (fgets(buf, sizeof(buf), f))
				fputs(buf, stdout);
			fclose(f);
		}
	}
}


/*
 * Check for quiet login, either .hushlogin or command given.
 */
int
check_quietlogin(Session *s, const char *command)
{
	char buf[256];
	struct passwd *pw = s->pw;
	struct stat st;

	/* Return 1 if .hushlogin exists or a command given. */
	if (command != NULL)
		return 1;
	snprintf(buf, sizeof(buf), "%.200s/.hushlogin", pw->pw_dir);
#ifdef HAVE_LOGIN_CAP
	if (login_getcapbool(lc, "hushlogin", 0) || stat(buf, &st) >= 0)
		return 1;
#else
	if (stat(buf, &st) >= 0)
		return 1;
#endif
	return 0;
}

/*
 * Sets the value of the given variable in the environment.  If the variable
 * already exists, its value is overriden.
 */
static void
child_set_env(char ***envp, u_int *envsizep, const char *name,
	      const char *value)
{
	char **env;
	u_int envsize;
	u_int i, namelen;

	/*
	 * Find the slot where the value should be stored.  If the variable
	 * already exists, we reuse the slot; otherwise we append a new slot
	 * at the end of the array, expanding if necessary.
	 */
	env = *envp;
	namelen = strlen(name);
	for (i = 0; env[i]; i++)
		if (strncmp(env[i], name, namelen) == 0 && env[i][namelen] == '=')
			break;
	if (env[i]) {
		/* Reuse the slot. */
		xfree(env[i]);
	} else {
		/* New variable.  Expand if necessary. */
		envsize = *envsizep;
		if (i >= envsize - 1) {
			envsize += 50;
			env = (*envp) = xrealloc(env, envsize * sizeof(char *));
			*envsizep = envsize;
		}
		/* Need to set the NULL pointer at end of array beyond the new slot. */
		env[i + 1] = NULL;
	}

	/* Allocate space and format the variable in the appropriate slot. */
	env[i] = xmalloc(strlen(name) + 1 + strlen(value) + 1);
	snprintf(env[i], strlen(name) + 1 + strlen(value) + 1, "%s=%s", name, value);
}

/*
 * Reads environment variables from the given file and adds/overrides them
 * into the environment.  If the file does not exist, this does nothing.
 * Otherwise, it must consist of empty lines, comments (line starts with '#')
 * and assignments of the form name=value.  No other forms are allowed.
 */
static void
read_environment_file(char ***env, u_int *envsize,
		      const char *filename)
{
	FILE *f;
	char buf[4096];
	char *cp, *value;

	f = fopen(filename, "r");
	if (!f)
		return;

	while (fgets(buf, sizeof(buf), f)) {
		for (cp = buf; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '#' || *cp == '\n')
			continue;
		if (strchr(cp, '\n'))
			*strchr(cp, '\n') = '\0';
		value = strchr(cp, '=');
		if (value == NULL) {
			fprintf(stderr, "Bad line in %.100s: %.200s\n", filename, buf);
			continue;
		}
		/*
		 * Replace the equals sign by nul, and advance value to
		 * the value string.
		 */
		*value = '\0';
		value++;
		child_set_env(env, envsize, cp, value);
	}
	fclose(f);
}

/*
 * Performs common processing for the child, such as setting up the
 * environment, closing extra file descriptors, setting the user and group
 * ids, and executing the command or shell.
 */
void
do_child(Session *s, const char *command)
{
	const char *shell, *hostname = NULL, *cp = NULL;
	struct passwd *pw = s->pw;
	char buf[256];
	char cmd[1024];
	FILE *f = NULL;
	u_int envsize, i;
	char **env;
	extern char **environ;
	struct stat st;
	char *argv[10];
	int do_xauth;

	do_xauth =
	    s->display != NULL && s->auth_proto != NULL && s->auth_data != NULL;

	/* remove hostkey from the child's memory */
	destroy_sensitive_data();

	/* login(1) is only called if we execute the login shell */
	if (options.use_login && command != NULL)
		options.use_login = 0;

	if (!options.use_login) {
#ifdef HAVE_LOGIN_CAP
		if (!login_getcapbool(lc, "ignorenologin", 0) && pw->pw_uid)
			f = fopen(login_getcapstr(lc, "nologin", _PATH_NOLOGIN,
			    _PATH_NOLOGIN), "r");
#else
		if (pw->pw_uid)
			f = fopen(_PATH_NOLOGIN, "r");
#endif
		if (f) {
			/* /etc/nologin exists.  Print its contents and exit. */
			while (fgets(buf, sizeof(buf), f))
				fputs(buf, stderr);
			fclose(f);
			exit(254);
		}
	}
	/* Set login name, uid, gid, and groups. */
	/* Login(1) does this as well, and it needs uid 0 for the "-h"
	   switch, so we let login(1) to this for us. */
	if (!options.use_login) {
		if (getuid() == 0 || geteuid() == 0) {
#ifdef HAVE_LOGIN_CAP
			if (setusercontext(lc, pw, pw->pw_uid,
			    (LOGIN_SETALL & ~LOGIN_SETPATH)) < 0) {
				perror("unable to set user context");
				exit(1);
			}
#else
			if (setlogin(pw->pw_name) < 0)
				error("setlogin failed: %s", strerror(errno));
			if (setgid(pw->pw_gid) < 0) {
				perror("setgid");
				exit(1);
			}
			/* Initialize the group list. */
			if (initgroups(pw->pw_name, pw->pw_gid) < 0) {
				perror("initgroups");
				exit(1);
			}
			endgrent();

			/* Permanently switch to the desired uid. */
			permanently_set_uid(pw);
#endif
		}
		if (getuid() != pw->pw_uid || geteuid() != pw->pw_uid)
			fatal("Failed to set uids to %u.", (u_int) pw->pw_uid);
	}
	/*
	 * Get the shell from the password data.  An empty shell field is
	 * legal, and means /bin/sh.
	 */
	shell = (pw->pw_shell[0] == '\0') ? _PATH_BSHELL : pw->pw_shell;
#ifdef HAVE_LOGIN_CAP
	shell = login_getcapstr(lc, "shell", (char *)shell, (char *)shell);
#endif

	/* Initialize the environment. */
	envsize = 100;
	env = xmalloc(envsize * sizeof(char *));
	env[0] = NULL;

	if (!options.use_login) {
		/* Set basic environment. */
		child_set_env(&env, &envsize, "USER", pw->pw_name);
		child_set_env(&env, &envsize, "LOGNAME", pw->pw_name);
		child_set_env(&env, &envsize, "HOME", pw->pw_dir);
#ifdef HAVE_LOGIN_CAP
		(void) setusercontext(lc, pw, pw->pw_uid, LOGIN_SETPATH);
		child_set_env(&env, &envsize, "PATH", getenv("PATH"));
#else
		child_set_env(&env, &envsize, "PATH", _PATH_STDPATH);
#endif

		snprintf(buf, sizeof buf, "%.200s/%.50s",
			 _PATH_MAILDIR, pw->pw_name);
		child_set_env(&env, &envsize, "MAIL", buf);

		/* Normal systems set SHELL by default. */
		child_set_env(&env, &envsize, "SHELL", shell);
	}
	if (getenv("TZ"))
		child_set_env(&env, &envsize, "TZ", getenv("TZ"));

	/* Set custom environment options from RSA authentication. */
	if (!options.use_login) {
		while (custom_environment) {
			struct envstring *ce = custom_environment;
			char *s = ce->s;
			int i;
			for (i = 0; s[i] != '=' && s[i]; i++)
				;
			if (s[i] == '=') {
				s[i] = 0;
				child_set_env(&env, &envsize, s, s + i + 1);
			}
			custom_environment = ce->next;
			xfree(ce->s);
			xfree(ce);
		}
	}

	snprintf(buf, sizeof buf, "%.50s %d %d",
		 get_remote_ipaddr(), get_remote_port(), get_local_port());
	child_set_env(&env, &envsize, "SSH_CLIENT", buf);

	if (s->ttyfd != -1)
		child_set_env(&env, &envsize, "SSH_TTY", s->tty);
	if (s->term)
		child_set_env(&env, &envsize, "TERM", s->term);
	if (s->display)
		child_set_env(&env, &envsize, "DISPLAY", s->display);
	if (original_command)
		child_set_env(&env, &envsize, "SSH_ORIGINAL_COMMAND",
		    original_command);
#ifdef KRB4
	if (s->authctxt->krb4_ticket_file)
		child_set_env(&env, &envsize, "KRBTKFILE",
			      s->authctxt->krb4_ticket_file);
#endif
#ifdef KRB5
	if (s->authctxt->krb5_ticket_file)
		child_set_env(&env, &envsize, "KRB5CCNAME",
			      s->authctxt->krb5_ticket_file);
#endif
	if (auth_get_socket_name() != NULL)
		child_set_env(&env, &envsize, SSH_AUTHSOCKET_ENV_NAME,
			      auth_get_socket_name());

	/* read $HOME/.ssh/environment. */
	if (!options.use_login) {
		snprintf(buf, sizeof buf, "%.200s/.ssh/environment",
		    pw->pw_dir);
		read_environment_file(&env, &envsize, buf);
	}
	if (debug_flag) {
		/* dump the environment */
		fprintf(stderr, "Environment:\n");
		for (i = 0; env[i]; i++)
			fprintf(stderr, "  %.200s\n", env[i]);
	}
	/* we have to stash the hostname before we close our socket. */
	if (options.use_login)
		hostname = get_remote_name_or_ip(utmp_len,
		    options.reverse_mapping_check);
	/*
	 * Close the connection descriptors; note that this is the child, and
	 * the server will still have the socket open, and it is important
	 * that we do not shutdown it.  Note that the descriptors cannot be
	 * closed before building the environment, as we call
	 * get_remote_ipaddr there.
	 */
	if (packet_get_connection_in() == packet_get_connection_out())
		close(packet_get_connection_in());
	else {
		close(packet_get_connection_in());
		close(packet_get_connection_out());
	}
	/*
	 * Close all descriptors related to channels.  They will still remain
	 * open in the parent.
	 */
	/* XXX better use close-on-exec? -markus */
	channel_close_all();

	/*
	 * Close any extra file descriptors.  Note that there may still be
	 * descriptors left by system functions.  They will be closed later.
	 */
	endpwent();

	/*
	 * Close any extra open file descriptors so that we don\'t have them
	 * hanging around in clients.  Note that we want to do this after
	 * initgroups, because at least on Solaris 2.3 it leaves file
	 * descriptors open.
	 */
	for (i = 3; i < 64; i++)
		close(i);

	/*
	 * Must take new environment into use so that .ssh/rc, /etc/sshrc and
	 * xauth are run in the proper environment.
	 */
	environ = env;

#ifdef AFS
	/* Try to get AFS tokens for the local cell. */
	if (k_hasafs()) {
		char cell[64];
		
		if (k_afs_cell_of_file(pw->pw_dir, cell, sizeof(cell)) == 0)
			krb_afslog(cell, 0);
		
		krb_afslog(0, 0);
	}
#endif /* AFS */

	/* Change current directory to the user\'s home directory. */
	if (chdir(pw->pw_dir) < 0) {
		fprintf(stderr, "Could not chdir to home directory %s: %s\n",
		    pw->pw_dir, strerror(errno));
#ifdef HAVE_LOGIN_CAP
		if (login_getcapbool(lc, "requirehome", 0))
			exit(1);
#endif
	}

	/*
	 * Run $HOME/.ssh/rc, /etc/sshrc, or xauth (whichever is found first
	 * in this order).
	 */
	if (!options.use_login) {
		/* ignore _PATH_SSH_USER_RC for subsystems */
		if (!s->is_subsystem && (stat(_PATH_SSH_USER_RC, &st) >= 0)) {
			snprintf(cmd, sizeof cmd, "%s -c '%s %s'",
			    shell, _PATH_BSHELL, _PATH_SSH_USER_RC);
			if (debug_flag)
				fprintf(stderr, "Running %s\n", cmd);
			f = popen(cmd, "w");
			if (f) {
				if (do_xauth)
					fprintf(f, "%s %s\n", s->auth_proto,
					    s->auth_data);
				pclose(f);
			} else
				fprintf(stderr, "Could not run %s\n",
				    _PATH_SSH_USER_RC);
		} else if (stat(_PATH_SSH_SYSTEM_RC, &st) >= 0) {
			if (debug_flag)
				fprintf(stderr, "Running %s %s\n", _PATH_BSHELL,
				    _PATH_SSH_SYSTEM_RC);
			f = popen(_PATH_BSHELL " " _PATH_SSH_SYSTEM_RC, "w");
			if (f) {
				if (do_xauth)
					fprintf(f, "%s %s\n", s->auth_proto,
					    s->auth_data);
				pclose(f);
			} else
				fprintf(stderr, "Could not run %s\n",
				    _PATH_SSH_SYSTEM_RC);
		} else if (do_xauth && options.xauth_location != NULL) {
			/* Add authority data to .Xauthority if appropriate. */

			if (debug_flag) {
				fprintf(stderr,
				    "Running %.100s add "
				    "%.100s %.100s %.100s\n",
				    options.xauth_location, s->auth_display,
				    s->auth_proto, s->auth_data);
			}
			snprintf(cmd, sizeof cmd, "%s -q -",
			    options.xauth_location);
			f = popen(cmd, "w");
			if (f) {
				fprintf(f, "add %s %s %s\n", s->auth_display,
				    s->auth_proto, s->auth_data);
				pclose(f);
			} else {
				fprintf(stderr, "Could not run %s\n",
				    cmd);
			}
		}
		/* Get the last component of the shell name. */
		cp = strrchr(shell, '/');
		if (cp)
			cp++;
		else
			cp = shell;
	}

	/* restore SIGPIPE for child */
	signal(SIGPIPE,  SIG_DFL);

	/*
	 * If we have no command, execute the shell.  In this case, the shell
	 * name to be passed in argv[0] is preceded by '-' to indicate that
	 * this is a login shell.
	 */
	if (!command) {
		if (!options.use_login) {
			char buf[256];

			/* Start the shell.  Set initial character to '-'. */
			buf[0] = '-';
			strncpy(buf + 1, cp, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = 0;

			/* Execute the shell. */
			argv[0] = buf;
			argv[1] = NULL;
			execve(shell, argv, env);

			/* Executing the shell failed. */
			perror(shell);
			exit(1);

		} else {
			/* Launch login(1). */

			execl("/usr/bin/login", "login", "-h", hostname,
			     "-p", "-f", "--", pw->pw_name, (char *)NULL);

			/* Login couldn't be executed, die. */

			perror("login");
			exit(1);
		}
	}
	/*
	 * Execute the command using the user's shell.  This uses the -c
	 * option to execute the command.
	 */
	argv[0] = (char *) cp;
	argv[1] = "-c";
	argv[2] = (char *) command;
	argv[3] = NULL;
	execve(shell, argv, env);
	perror(shell);
	exit(1);
}

Session *
session_new(void)
{
	int i;
	static int did_init = 0;
	if (!did_init) {
		debug("session_new: init");
		for(i = 0; i < MAX_SESSIONS; i++) {
			sessions[i].used = 0;
		}
		did_init = 1;
	}
	for(i = 0; i < MAX_SESSIONS; i++) {
		Session *s = &sessions[i];
		if (! s->used) {
			memset(s, 0, sizeof(*s));
			s->chanid = -1;
			s->ptyfd = -1;
			s->ttyfd = -1;
			s->used = 1;
			s->self = i;
			debug("session_new: session %d", i);
			return s;
		}
	}
	return NULL;
}

static void
session_dump(void)
{
	int i;
	for(i = 0; i < MAX_SESSIONS; i++) {
		Session *s = &sessions[i];
		debug("dump: used %d session %d %p channel %d pid %d",
		    s->used,
		    s->self,
		    s,
		    s->chanid,
		    s->pid);
	}
}

int
session_open(Authctxt *authctxt, int chanid)
{
	Session *s = session_new();
	debug("session_open: channel %d", chanid);
	if (s == NULL) {
		error("no more sessions");
		return 0;
	}
	s->authctxt = authctxt;
	s->pw = authctxt->pw;
	if (s->pw == NULL)
		fatal("no user for session %d", s->self);
	debug("session_open: session %d: link with channel %d", s->self, chanid);
	s->chanid = chanid;
	return 1;
}

static Session *
session_by_channel(int id)
{
	int i;
	for(i = 0; i < MAX_SESSIONS; i++) {
		Session *s = &sessions[i];
		if (s->used && s->chanid == id) {
			debug("session_by_channel: session %d channel %d", i, id);
			return s;
		}
	}
	debug("session_by_channel: unknown channel %d", id);
	session_dump();
	return NULL;
}

static Session *
session_by_pid(pid_t pid)
{
	int i;
	debug("session_by_pid: pid %d", pid);
	for(i = 0; i < MAX_SESSIONS; i++) {
		Session *s = &sessions[i];
		if (s->used && s->pid == pid)
			return s;
	}
	error("session_by_pid: unknown pid %d", pid);
	session_dump();
	return NULL;
}

static int
session_window_change_req(Session *s)
{
	s->col = packet_get_int();
	s->row = packet_get_int();
	s->xpixel = packet_get_int();
	s->ypixel = packet_get_int();
	packet_done();
	pty_change_window_size(s->ptyfd, s->row, s->col, s->xpixel, s->ypixel);
	return 1;
}

static int
session_pty_req(Session *s)
{
	u_int len;
	int n_bytes;

	if (no_pty_flag) {
		debug("Allocating a pty not permitted for this authentication.");
		return 0;
	}
	if (s->ttyfd != -1) {
		packet_disconnect("Protocol error: you already have a pty.");
		return 0;
	}

	s->term = packet_get_string(&len);

	if (compat20) {
		s->col = packet_get_int();
		s->row = packet_get_int();
	} else {
		s->row = packet_get_int();
		s->col = packet_get_int();
	}
	s->xpixel = packet_get_int();
	s->ypixel = packet_get_int();

	if (strcmp(s->term, "") == 0) {
		xfree(s->term);
		s->term = NULL;
	}

	/* Allocate a pty and open it. */
	debug("Allocating pty.");
	if (!pty_allocate(&s->ptyfd, &s->ttyfd, s->tty, sizeof(s->tty))) {
		if (s->term)
			xfree(s->term);
		s->term = NULL;
		s->ptyfd = -1;
		s->ttyfd = -1;
		error("session_pty_req: session %d alloc failed", s->self);
		return 0;
	}
	debug("session_pty_req: session %d alloc %s", s->self, s->tty);

	/* for SSH1 the tty modes length is not given */
	if (!compat20)
		n_bytes = packet_remaining();
	tty_parse_modes(s->ttyfd, &n_bytes);

	/*
	 * Add a cleanup function to clear the utmp entry and record logout
	 * time in case we call fatal() (e.g., the connection gets closed).
	 */
	fatal_add_cleanup(session_pty_cleanup, (void *)s);
	pty_setowner(s->pw, s->tty);

	/* Set window size from the packet. */
	pty_change_window_size(s->ptyfd, s->row, s->col, s->xpixel, s->ypixel);

	packet_done();
	session_proctitle(s);
	return 1;
}

static int
session_subsystem_req(Session *s)
{
	struct stat st;
	u_int len;
	int success = 0;
	char *cmd, *subsys = packet_get_string(&len);
	int i;

	packet_done();
	log("subsystem request for %s", subsys);

	for (i = 0; i < options.num_subsystems; i++) {
		if (strcmp(subsys, options.subsystem_name[i]) == 0) {
			cmd = options.subsystem_command[i];
			if (stat(cmd, &st) < 0) {
				error("subsystem: cannot stat %s: %s", cmd,
				    strerror(errno));
				break;
			}
			debug("subsystem: exec() %s", cmd);
			s->is_subsystem = 1;
			do_exec(s, cmd);
			success = 1;
		}
	}

	if (!success)
		log("subsystem request for %s failed, subsystem not found",
		    subsys);

	xfree(subsys);
	return success;
}

static int
session_x11_req(Session *s)
{
	int success;

	s->single_connection = packet_get_char();
	s->auth_proto = packet_get_string(NULL);
	s->auth_data = packet_get_string(NULL);
	s->screen = packet_get_int();
	packet_done();

	success = session_setup_x11fwd(s);
	if (!success) {
		xfree(s->auth_proto);
		xfree(s->auth_data);
		s->auth_proto = NULL;
		s->auth_data = NULL;
	}
	return success;
}

static int
session_shell_req(Session *s)
{
	packet_done();
	do_exec(s, NULL);
	return 1;
}

static int
session_exec_req(Session *s)
{
	u_int len;
	char *command = packet_get_string(&len);
	packet_done();
	do_exec(s, command);
	xfree(command);
	return 1;
}

static int
session_auth_agent_req(Session *s)
{
	static int called = 0;
	packet_done();
	if (no_agent_forwarding_flag) {
		debug("session_auth_agent_req: no_agent_forwarding_flag");
		return 0;
	}
	if (called) {
		return 0;
	} else {
		called = 1;
		return auth_input_request_forwarding(s->pw);
	}
}

void
session_input_channel_req(int id, void *arg)
{
	u_int len;
	int reply;
	int success = 0;
	char *rtype;
	Session *s;
	Channel *c;

	rtype = packet_get_string(&len);
	reply = packet_get_char();

	s = session_by_channel(id);
	if (s == NULL)
		fatal("session_input_channel_req: channel %d: no session", id);
	c = channel_lookup(id);
	if (c == NULL)
		fatal("session_input_channel_req: channel %d: bad channel", id);

	debug("session_input_channel_req: session %d channel %d request %s reply %d",
	    s->self, id, rtype, reply);

	/*
	 * a session is in LARVAL state until a shell, a command
	 * or a subsystem is executed
	 */
	if (c->type == SSH_CHANNEL_LARVAL) {
		if (strcmp(rtype, "shell") == 0) {
			success = session_shell_req(s);
		} else if (strcmp(rtype, "exec") == 0) {
			success = session_exec_req(s);
		} else if (strcmp(rtype, "pty-req") == 0) {
			success =  session_pty_req(s);
		} else if (strcmp(rtype, "x11-req") == 0) {
			success = session_x11_req(s);
		} else if (strcmp(rtype, "auth-agent-req@openssh.com") == 0) {
			success = session_auth_agent_req(s);
		} else if (strcmp(rtype, "subsystem") == 0) {
			success = session_subsystem_req(s);
		}
	}
	if (strcmp(rtype, "window-change") == 0) {
		success = session_window_change_req(s);
	}

	if (reply) {
		packet_start(success ?
		    SSH2_MSG_CHANNEL_SUCCESS : SSH2_MSG_CHANNEL_FAILURE);
		packet_put_int(c->remote_id);
		packet_send();
	}
	xfree(rtype);
}

void
session_set_fds(Session *s, int fdin, int fdout, int fderr)
{
	if (!compat20)
		fatal("session_set_fds: called for proto != 2.0");
	/*
	 * now that have a child and a pipe to the child,
	 * we can activate our channel and register the fd's
	 */
	if (s->chanid == -1)
		fatal("no channel for session %d", s->self);
	channel_set_fds(s->chanid,
	    fdout, fdin, fderr,
	    fderr == -1 ? CHAN_EXTENDED_IGNORE : CHAN_EXTENDED_READ,
	    1);
}

/*
 * Function to perform pty cleanup. Also called if we get aborted abnormally
 * (e.g., due to a dropped connection).
 */
static void
session_pty_cleanup(void *session)
{
	Session *s = session;

	if (s == NULL) {
		error("session_pty_cleanup: no session");
		return;
	}
	if (s->ttyfd == -1)
		return;

	debug("session_pty_cleanup: session %d release %s", s->self, s->tty);

	/* Record that the user has logged out. */
	if (s->pid != 0)
		record_logout(s->pid, s->tty);

	/* Release the pseudo-tty. */
	pty_release(s->tty);

	/*
	 * Close the server side of the socket pairs.  We must do this after
	 * the pty cleanup, so that another process doesn't get this pty
	 * while we're still cleaning up.
	 */
	if (close(s->ptymaster) < 0)
		error("close(s->ptymaster): %s", strerror(errno));

	/* unlink pty from session */
	s->ttyfd = -1;
}

static void
session_exit_message(Session *s, int status)
{
	Channel *c;
	if (s == NULL)
		fatal("session_close: no session");
	c = channel_lookup(s->chanid);
	if (c == NULL)
		fatal("session_exit_message: session %d: no channel %d",
		    s->self, s->chanid);
	debug("session_exit_message: session %d channel %d pid %d",
	    s->self, s->chanid, s->pid);

	if (WIFEXITED(status)) {
		channel_request_start(s->chanid,
		    "exit-status", 0);
		packet_put_int(WEXITSTATUS(status));
		packet_send();
	} else if (WIFSIGNALED(status)) {
		channel_request_start(s->chanid,
		    "exit-signal", 0);
		packet_put_int(WTERMSIG(status));
		packet_put_char(WCOREDUMP(status));
		packet_put_cstring("");
		packet_put_cstring("");
		packet_send();
	} else {
		/* Some weird exit cause.  Just exit. */
		packet_disconnect("wait returned status %04x.", status);
	}

	/* disconnect channel */
	debug("session_exit_message: release channel %d", s->chanid);
	channel_cancel_cleanup(s->chanid);
	/*
	 * emulate a write failure with 'chan_write_failed', nobody will be
	 * interested in data we write.
	 * Note that we must not call 'chan_read_failed', since there could
	 * be some more data waiting in the pipe.
	 */
	if (c->ostate != CHAN_OUTPUT_CLOSED)
		chan_write_failed(c);
	s->chanid = -1;
}

static void
session_close(Session *s)
{
	debug("session_close: session %d pid %d", s->self, s->pid);
	if (s->ttyfd != -1) {
		fatal_remove_cleanup(session_pty_cleanup, (void *)s);
		session_pty_cleanup(s);
	}
	if (s->term)
		xfree(s->term);
	if (s->display)
		xfree(s->display);
	if (s->auth_display)
		xfree(s->auth_display);
	if (s->auth_data)
		xfree(s->auth_data);
	if (s->auth_proto)
		xfree(s->auth_proto);
	s->used = 0;
	session_proctitle(s);
}

void
session_close_by_pid(pid_t pid, int status)
{
	Session *s = session_by_pid(pid);
	if (s == NULL) {
		debug("session_close_by_pid: no session for pid %d", pid);
		return;
	}
	if (s->chanid != -1)
		session_exit_message(s, status);
	session_close(s);
}

/*
 * this is called when a channel dies before
 * the session 'child' itself dies
 */
void
session_close_by_channel(int id, void *arg)
{
	Session *s = session_by_channel(id);
	if (s == NULL) {
		debug("session_close_by_channel: no session for id %d", id);
		return;
	}
	debug("session_close_by_channel: channel %d child %d", id, s->pid);
	if (s->pid != 0) {
		debug("session_close_by_channel: channel %d: has child", id);
		/*
		 * delay detach of session, but release pty, since
		 * the fd's to the child are already closed
		 */
		if (s->ttyfd != -1) {
			fatal_remove_cleanup(session_pty_cleanup, (void *)s);
			session_pty_cleanup(s);
		}
		return;
	}
	/* detach by removing callback */
	channel_cancel_cleanup(s->chanid);
	s->chanid = -1;
	session_close(s);
}

void
session_destroy_all(void)
{
	int i;
	for(i = 0; i < MAX_SESSIONS; i++) {
		Session *s = &sessions[i];
		if (s->used) 
			session_close(s);
	}
}

static char *
session_tty_list(void)
{
	static char buf[1024];
	int i;
	buf[0] = '\0';
	for(i = 0; i < MAX_SESSIONS; i++) {
		Session *s = &sessions[i];
		if (s->used && s->ttyfd != -1) {
			if (buf[0] != '\0')
				strlcat(buf, ",", sizeof buf);
			strlcat(buf, strrchr(s->tty, '/') + 1, sizeof buf);
		}
	}
	if (buf[0] == '\0')
		strlcpy(buf, "notty", sizeof buf);
	return buf;
}

void
session_proctitle(Session *s)
{
	if (s->pw == NULL)
		error("no user for session %d", s->self);
	else
		setproctitle("%s@%s", s->pw->pw_name, session_tty_list());
}

int
session_setup_x11fwd(Session *s)
{
	struct stat st;
	char display[512], auth_display[512];
	char hostname[MAXHOSTNAMELEN];

	if (no_x11_forwarding_flag) {
		packet_send_debug("X11 forwarding disabled in user configuration file.");
		return 0;
	}
	if (!options.x11_forwarding) {
		debug("X11 forwarding disabled in server configuration file.");
		return 0;
	}
	if (!options.xauth_location ||
	    (stat(options.xauth_location, &st) == -1)) {
		packet_send_debug("No xauth program; cannot forward with spoofing.");
		return 0;
	}
	if (options.use_login) {
		packet_send_debug("X11 forwarding disabled; "
		    "not compatible with UseLogin=yes.");
		return 0;
	}
	if (s->display != NULL) {
		debug("X11 display already set.");
		return 0;
	}
	s->display_number = x11_create_display_inet(options.x11_display_offset,
	    options.gateway_ports);
	if (s->display_number == -1) {
		debug("x11_create_display_inet failed.");
		return 0;
	}

	/* Set up a suitable value for the DISPLAY variable. */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		fatal("gethostname: %.100s", strerror(errno));
	/*
	 * auth_display must be used as the displayname when the
	 * authorization entry is added with xauth(1).  This will be
	 * different than the DISPLAY string for localhost displays.
	 */
	if (!options.gateway_ports) {
		snprintf(display, sizeof display, "localhost:%d.%d",
		    s->display_number, s->screen);
		snprintf(auth_display, sizeof auth_display, "%.400s/unix:%d.%d",
		    hostname, s->display_number, s->screen);
		s->display = xstrdup(display);
		s->auth_display = xstrdup(auth_display);
	} else {
		snprintf(display, sizeof display, "%.400s:%d.%d", hostname,
		    s->display_number, s->screen);
		s->display = xstrdup(display);
		s->auth_display = xstrdup(display);
	}

	return 1;
}

static void
do_authenticated2(Authctxt *authctxt)
{
	server_loop2(authctxt);
}
