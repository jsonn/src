/*
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
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

RCSID("$OpenBSD: sftp.c,v 1.7 2001/02/08 00:04:52 markus Exp $");

/* XXX: commandline mode */
/* XXX: copy between two remote hosts (commandline) */
/* XXX: short-form remote directory listings (like 'ls -C') */

#include "buffer.h"
#include "xmalloc.h"
#include "log.h"
#include "pathnames.h"

#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"
#include "sftp-int.h"

int use_ssh1 = 0;
char *ssh_program = _PATH_SSH_PROGRAM;
char *sftp_server = NULL;

static void
connect_to_server(char **args, int *in, int *out, pid_t *sshpid)
{
	int c_in, c_out;
#ifdef USE_PIPES
	int pin[2], pout[2];
	if ((pipe(pin) == -1) || (pipe(pout) == -1))
		fatal("pipe: %s", strerror(errno));
	*in = pin[0];
	*out = pout[1];
	c_in = pout[0];
	c_out = pin[1];
#else /* USE_PIPES */
	int inout[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, inout) == -1)
		fatal("socketpair: %s", strerror(errno));
	*in = *out = inout[0];
	c_in = c_out = inout[1];
#endif /* USE_PIPES */

	if ((*sshpid = fork()) == -1)
		fatal("fork: %s", strerror(errno));
	else if (*sshpid == 0) {
		if ((dup2(c_in, STDIN_FILENO) == -1) ||
		    (dup2(c_out, STDOUT_FILENO) == -1)) {
			fprintf(stderr, "dup2: %s\n", strerror(errno));
			exit(1);
		}
		close(*in);
		close(*out);
		close(c_in);
		close(c_out);
		execv(ssh_program, args);
		fprintf(stderr, "exec: %s: %s\n", ssh_program, strerror(errno));
		exit(1);
	}

	close(c_in);
	close(c_out);
}

static char **
make_ssh_args(char *add_arg)
{
	static char **args = NULL;
	static int nargs = 0;
	char debug_buf[4096];
	int i, use_subsystem = 1;

	/* no subsystem if protocol 1 or the server-spec contains a '/' */
	if (use_ssh1 ||
	    (sftp_server != NULL && strchr(sftp_server, '/') != NULL))
		use_subsystem = 0;

	/* Init args array */
	if (args == NULL) {
		nargs = use_subsystem ? 6 : 5;
		i = 0;
		args = xmalloc(sizeof(*args) * nargs);
		args[i++] = "ssh";
		args[i++] = use_ssh1 ? "-oProtocol=1" : "-oProtocol=2";
		if (use_subsystem)
			args[i++] = "-s";
		args[i++] = "-oForwardAgent=no";
		args[i++] = "-oForwardX11=no";
		args[i++] = NULL;
	}

	/* If asked to add args, then do so and return */
	if (add_arg) {
		i = nargs++ - 1;
		args = xrealloc(args, sizeof(*args) * nargs);
		args[i++] = add_arg;
		args[i++] = NULL;
		return(NULL);
	}

	/* Otherwise finish up and return the arg array */
	if (sftp_server != NULL)
		make_ssh_args(sftp_server);
	else
		make_ssh_args("sftp");

	/* XXX: overflow - doesn't grow debug_buf */
	debug_buf[0] = '\0';
	for(i = 0; args[i]; i++) {
		if (i)
			strlcat(debug_buf, " ", sizeof(debug_buf));

		strlcat(debug_buf, args[i], sizeof(debug_buf));
	}
	debug("SSH args \"%s\"", debug_buf);

	return(args);
}

static void
usage(void)
{
	fprintf(stderr, "usage: sftp [-1vC] [-osshopt=value] [user@]host\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int in, out, ch, debug_level, compress_flag;
	pid_t sshpid;
	char *host, *userhost;
	LogLevel ll;
	extern int optind;
	extern char *optarg;

	debug_level = compress_flag = 0;

	while ((ch = getopt(argc, argv, "1hvCo:s:S:")) != -1) {
		switch (ch) {
		case 'C':
			compress_flag = 1;
			break;
		case 'v':
			debug_level = MIN(3, debug_level + 1);
			break;
		case 'o':
			make_ssh_args("-o");
			make_ssh_args(optarg);
			break;
		case '1':
			use_ssh1 = 1;
			if (sftp_server == NULL)
				sftp_server = _PATH_SFTP_SERVER;
			break;
		case 's':
			sftp_server = optarg;
			break;
		case 'S':
			ssh_program = optarg;
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (optind == argc || argc > (optind + 1))
		usage();

	userhost = argv[optind];

	if ((host = strchr(userhost, '@')) == NULL)
		host = userhost;
	else {
		*host = '\0';
		if (!userhost[0]) {
			fprintf(stderr, "Missing username\n");
			usage();
		}
		make_ssh_args("-l");
		make_ssh_args(userhost);
		host++;
	}

	if (!*host) {
		fprintf(stderr, "Missing hostname\n");
		usage();
	}

	/* Set up logging and debug '-d' arguments to ssh */
	ll = SYSLOG_LEVEL_INFO;
	switch (debug_level) {
	case 1:
		ll = SYSLOG_LEVEL_DEBUG1;
		make_ssh_args("-v");
		break;
	case 2:
		ll = SYSLOG_LEVEL_DEBUG2;
		make_ssh_args("-v");
		make_ssh_args("-v");
		break;
	case 3:
		ll = SYSLOG_LEVEL_DEBUG3;
		make_ssh_args("-v");
		make_ssh_args("-v");
		make_ssh_args("-v");
		break;
	}

	if (compress_flag)
		make_ssh_args("-C");

	log_init(argv[0], ll, SYSLOG_FACILITY_USER, 1);

	make_ssh_args(host);

	fprintf(stderr, "Connecting to %s...\n", host);

	connect_to_server(make_ssh_args(NULL), &in, &out, &sshpid);

	do_init(in, out);

	interactive_loop(in, out);

	close(in);
	close(out);

	if (kill(sshpid, SIGHUP) == -1)
		fatal("Couldn't terminate ssh process: %s", strerror(errno));

	if (waitpid(sshpid, NULL, 0) == -1)
		fatal("Couldn't wait for ssh process: %s", strerror(errno));

	exit(0);
}
