/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This program is the ssh daemon.  It listens for connections from clients,
 * and performs authentication, executes use commands or shell, and forwards
 * information to/from the application to the user client over an encrypted
 * connection.  This can also handle forwarding of X11, TCP/IP, and
 * authentication agent connections.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 implementation:
 *
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: sshd.c,v 1.167 2001/02/12 23:26:20 markus Exp $");

#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/hmac.h>

#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "xmalloc.h"
#include "rsa.h"
#include "pty.h"
#include "packet.h"
#include "mpaux.h"
#include "log.h"
#include "servconf.h"
#include "uidswap.h"
#include "compat.h"
#include "buffer.h"
#include "cipher.h"
#include "kex.h"
#include "key.h"
#include "dh.h"
#include "myproposal.h"
#include "authfile.h"
#include "pathnames.h"
#include "atomicio.h"
#include "canohost.h"
#include "auth.h"
#include "misc.h"

#ifdef LIBWRAP
#include <tcpd.h>
#include <syslog.h>
int allow_severity = LOG_INFO;
int deny_severity = LOG_WARNING;
#endif /* LIBWRAP */

#ifndef O_NOCTTY
#define O_NOCTTY	0
#endif

extern char *__progname;

/* Server configuration options. */
ServerOptions options;

/* Name of the server configuration file. */
char *config_file_name = _PATH_SERVER_CONFIG_FILE;

/*
 * Flag indicating whether IPv4 or IPv6.  This can be set on the command line.
 * Default value is AF_UNSPEC means both IPv4 and IPv6.
 */
int IPv4or6 = AF_UNSPEC;

/*
 * Debug mode flag.  This can be set on the command line.  If debug
 * mode is enabled, extra debugging output will be sent to the system
 * log, the daemon will not go to background, and will exit after processing
 * the first connection.
 */
int debug_flag = 0;

/* Flag indicating that the daemon is being started from inetd. */
int inetd_flag = 0;

/* Flag indicating that sshd should not detach and become a daemon. */
int no_daemon_flag = 0;

/* debug goes to stderr unless inetd_flag is set */
int log_stderr = 0;

/* Saved arguments to main(). */
char **saved_argv;

/*
 * The sockets that the server is listening; this is used in the SIGHUP
 * signal handler.
 */
#define	MAX_LISTEN_SOCKS	16
int listen_socks[MAX_LISTEN_SOCKS];
int num_listen_socks = 0;

/*
 * the client's version string, passed by sshd2 in compat mode. if != NULL,
 * sshd will skip the version-number exchange
 */
char *client_version_string = NULL;
char *server_version_string = NULL;

/*
 * Any really sensitive data in the application is contained in this
 * structure. The idea is that this structure could be locked into memory so
 * that the pages do not get written into swap.  However, there are some
 * problems. The private key contains BIGNUMs, and we do not (in principle)
 * have access to the internals of them, and locking just the structure is
 * not very useful.  Currently, memory locking is not implemented.
 */
struct {
	Key	*server_key;		/* empheral server key */
	Key	*ssh1_host_key;		/* ssh1 host key */
	Key	**host_keys;		/* all private host keys */
	int	have_ssh1_key;
	int	have_ssh2_key;
} sensitive_data;

/*
 * Flag indicating whether the RSA server key needs to be regenerated.
 * Is set in the SIGALRM handler and cleared when the key is regenerated.
 */
int key_do_regen = 0;

/* This is set to true when SIGHUP is received. */
int received_sighup = 0;

/* session identifier, used by RSA-auth */
u_char session_id[16];

/* same for ssh2 */
u_char *session_id2 = NULL;
int session_id2_len = 0;

/* record remote hostname or ip */
u_int utmp_len = MAXHOSTNAMELEN;

/* Prototypes for various functions defined later in this file. */
void do_ssh1_kex(void);
void do_ssh2_kex(void);

void ssh_dh1_server(Kex *, Buffer *_kexinit, Buffer *);
void ssh_dhgex_server(Kex *, Buffer *_kexinit, Buffer *);

/*
 * Close all listening sockets
 */
static void
close_listen_socks(void)
{
	int i;
	for (i = 0; i < num_listen_socks; i++)
		close(listen_socks[i]);
	num_listen_socks = -1;
}

/*
 * Signal handler for SIGHUP.  Sshd execs itself when it receives SIGHUP;
 * the effect is to reread the configuration file (and to regenerate
 * the server key).
 */
static void
sighup_handler(int sig)
{
	received_sighup = 1;
	signal(SIGHUP, sighup_handler);
}

/*
 * Called from the main program after receiving SIGHUP.
 * Restarts the server.
 */
static void
sighup_restart(void)
{
	log("Received SIGHUP; restarting.");
	close_listen_socks();
	execv(saved_argv[0], saved_argv);
	log("RESTART FAILED: av[0]='%.100s', error: %.100s.", saved_argv[0], strerror(errno));
	exit(1);
}

/*
 * Generic signal handler for terminating signals in the master daemon.
 * These close the listen socket; not closing it seems to cause "Address
 * already in use" problems on some machines, which is inconvenient.
 */
static void
sigterm_handler(int sig)
{
	log("Received signal %d; terminating.", sig);
	close_listen_socks();
	unlink(options.pid_file);
	exit(255);
}

/*
 * SIGCHLD handler.  This is called whenever a child dies.  This will then
 * reap any zombies left by exited c.
 */
static void
main_sigchld_handler(int sig)
{
	int save_errno = errno;
	int status;

	while (waitpid(-1, &status, WNOHANG) > 0)
		;

	signal(SIGCHLD, main_sigchld_handler);
	errno = save_errno;
}

/*
 * Signal handler for the alarm after the login grace period has expired.
 */
static void
grace_alarm_handler(int sig)
{
	/* Close the connection. */
	packet_close();

	/* Log error and exit. */
	fatal("Timeout before authentication for %s.", get_remote_ipaddr());
}

/*
 * Signal handler for the key regeneration alarm.  Note that this
 * alarm only occurs in the daemon waiting for connections, and it does not
 * do anything with the private key or random state before forking.
 * Thus there should be no concurrency control/asynchronous execution
 * problems.
 */
static void
generate_empheral_server_key(void)
{
	log("Generating %s%d bit RSA key.", sensitive_data.server_key ? "new " : "",
	    options.server_key_bits);
	if (sensitive_data.server_key != NULL)
		key_free(sensitive_data.server_key);
	sensitive_data.server_key = key_generate(KEY_RSA1, options.server_key_bits);
	arc4random_stir();
	log("RSA key generation complete.");
}

static void
key_regeneration_alarm(int sig)
{
	int save_errno = errno;
	signal(SIGALRM, SIG_DFL);
	errno = save_errno;
	key_do_regen = 1;
}

static void
sshd_exchange_identification(int sock_in, int sock_out)
{
	int i, mismatch;
	int remote_major, remote_minor;
	int major, minor;
	char *s;
	char buf[256];			/* Must not be larger than remote_version. */
	char remote_version[256];	/* Must be at least as big as buf. */

	if ((options.protocol & SSH_PROTO_1) &&
	    (options.protocol & SSH_PROTO_2)) {
		major = PROTOCOL_MAJOR_1;
		minor = 99;
	} else if (options.protocol & SSH_PROTO_2) {
		major = PROTOCOL_MAJOR_2;
		minor = PROTOCOL_MINOR_2;
	} else {
		major = PROTOCOL_MAJOR_1;
		minor = PROTOCOL_MINOR_1;
	}
	snprintf(buf, sizeof buf, "SSH-%d.%d-%.100s\n", major, minor, SSH_VERSION);
	server_version_string = xstrdup(buf);

	if (client_version_string == NULL) {
		/* Send our protocol version identification. */
		if (atomic_write(sock_out, server_version_string, strlen(server_version_string))
		    != strlen(server_version_string)) {
			log("Could not write ident string to %s.", get_remote_ipaddr());
			fatal_cleanup();
		}

		/* Read other side's version identification. */
		memset(buf, 0, sizeof(buf)); 
		for (i = 0; i < sizeof(buf) - 1; i++) {
			if (atomic_read(sock_in, &buf[i], 1) != 1) {
				log("Did not receive ident string from %s.", get_remote_ipaddr());
				fatal_cleanup();
			}
			if (buf[i] == '\r') {
				buf[i] = '\n';
				buf[i + 1] = 0;
				/* Kludge for F-Secure Macintosh < 1.0.2 */
				if (i == 12 &&
				    strncmp(buf, "SSH-1.5-W1.0", 12) == 0)
					break;
				continue;
			}
			if (buf[i] == '\n') {
				/* buf[i] == '\n' */
				buf[i + 1] = 0;
				break;
			}
		}
		buf[sizeof(buf) - 1] = 0;
		client_version_string = xstrdup(buf);
	}

	/*
	 * Check that the versions match.  In future this might accept
	 * several versions and set appropriate flags to handle them.
	 */
	if (sscanf(client_version_string, "SSH-%d.%d-%[^\n]\n",
	    &remote_major, &remote_minor, remote_version) != 3) {
		s = "Protocol mismatch.\n";
		(void) atomic_write(sock_out, s, strlen(s));
		close(sock_in);
		close(sock_out);
		log("Bad protocol version identification '%.100s' from %s",
		    client_version_string, get_remote_ipaddr());
		fatal_cleanup();
	}
	debug("Client protocol version %d.%d; client software version %.100s",
	      remote_major, remote_minor, remote_version);

	compat_datafellows(remote_version);

	mismatch = 0;
	switch(remote_major) {
	case 1:
		if (remote_minor == 99) {
			if (options.protocol & SSH_PROTO_2)
				enable_compat20();
			else
				mismatch = 1;
			break;
		}
		if (!(options.protocol & SSH_PROTO_1)) {
			mismatch = 1;
			break;
		}
		if (remote_minor < 3) {
			packet_disconnect("Your ssh version is too old and "
			    "is no longer supported.  Please install a newer version.");
		} else if (remote_minor == 3) {
			/* note that this disables agent-forwarding */
			enable_compat13();
		}
		break;
	case 2:
		if (options.protocol & SSH_PROTO_2) {
			enable_compat20();
			break;
		}
		/* FALLTHROUGH */
	default:
		mismatch = 1;
		break;
	}
	chop(server_version_string);
	chop(client_version_string);
	debug("Local version string %.200s", server_version_string);

	if (mismatch) {
		s = "Protocol major versions differ.\n";
		(void) atomic_write(sock_out, s, strlen(s));
		close(sock_in);
		close(sock_out);
		log("Protocol major versions differ for %s: %.200s vs. %.200s",
		    get_remote_ipaddr(),
		    server_version_string, client_version_string);
		fatal_cleanup();
	}
	if (compat20)
		packet_set_ssh2_format();
}


/* Destroy the host and server keys.  They will no longer be needed. */
static void
destroy_sensitive_data(void)
{
	int i;

	if (sensitive_data.server_key) {
		key_free(sensitive_data.server_key);
		sensitive_data.server_key = NULL;
	}
	for(i = 0; i < options.num_host_key_files; i++) {
		if (sensitive_data.host_keys[i]) {
			key_free(sensitive_data.host_keys[i]);
			sensitive_data.host_keys[i] = NULL;
		}
	}
	sensitive_data.ssh1_host_key = NULL;
}
static Key *
load_private_key_autodetect(const char *filename)
{
	struct stat st;
	int type;
	Key *public, *private;

	if (stat(filename, &st) < 0) {
		perror(filename);
		return NULL;
	}
	/*
	 * try to load the public key. right now this only works for RSA1,
	 * since SSH2 keys are fully encrypted
	 */
	type = KEY_RSA1;
	public = key_new(type);
	if (!load_public_key(filename, public, NULL)) {
		/* ok, so we will assume this is 'some' key */
		type = KEY_UNSPEC;
	}
	key_free(public);

	/* Ok, try key with empty passphrase */
	private = key_new(type);
	if (load_private_key(filename, "", private, NULL)) {
		debug("load_private_key_autodetect: type %d %s",
		    private->type, key_type(private));
		return private;
	}
	key_free(private);
	return NULL;
}

static char *
list_hostkey_types(void)
{
	static char buf[1024];
	int i;
	buf[0] = '\0';
	for(i = 0; i < options.num_host_key_files; i++) {
		Key *key = sensitive_data.host_keys[i];
		if (key == NULL)
			continue;
		switch(key->type) {
		case KEY_RSA:
		case KEY_DSA:
			strlcat(buf, key_ssh_name(key), sizeof buf);
			strlcat(buf, ",", sizeof buf);
			break;
		}
	}
	i = strlen(buf);
	if (i > 0 && buf[i-1] == ',')
		buf[i-1] = '\0';
	debug("list_hostkey_types: %s", buf);
	return buf;
}

static Key *
get_hostkey_by_type(int type)
{
	int i;
	for(i = 0; i < options.num_host_key_files; i++) {
		Key *key = sensitive_data.host_keys[i];
		if (key != NULL && key->type == type)
			return key;
	}
	return NULL;
}

/*
 * returns 1 if connection should be dropped, 0 otherwise.
 * dropping starts at connection #max_startups_begin with a probability
 * of (max_startups_rate/100). the probability increases linearly until
 * all connections are dropped for startups > max_startups
 */
static int
drop_connection(int startups)
{
	double p, r;

	if (startups < options.max_startups_begin)
		return 0;
	if (startups >= options.max_startups)
		return 1;
	if (options.max_startups_rate == 100)
		return 1;

	p  = 100 - options.max_startups_rate;
	p *= startups - options.max_startups_begin;
	p /= (double) (options.max_startups - options.max_startups_begin);
	p += options.max_startups_rate;
	p /= 100.0;
	r = arc4random() / (double) UINT_MAX;

	debug("drop_connection: p %g, r %g", p, r);
	return (r < p) ? 1 : 0;
}

int *startup_pipes = NULL;	/* options.max_startup sized array of fd ints */
int startup_pipe;		/* in child */

/*
 * Main program for the daemon.
 */
int
main(int ac, char **av)
{
	extern char *optarg;
	extern int optind;
	int opt, sock_in = 0, sock_out = 0, newsock, j, i, fdsetsz, on = 1;
	pid_t pid;
	socklen_t fromlen;
	fd_set *fdset;
	struct sockaddr_storage from;
	const char *remote_ip;
	int remote_port;
	FILE *f;
	struct linger linger;
	struct addrinfo *ai;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	int listen_sock, maxfd;
	int startup_p[2];
	int startups = 0;
	int ret, key_used = 0;

	if (arc4random_check() < 0)
		fatal("rnd(4) is mandatory.");

	/* Save argv. */
	saved_argv = av;

	/* Initialize configuration options to their default values. */
	initialize_server_options(&options);

	/* Parse command-line arguments. */
	while ((opt = getopt(ac, av, "f:p:b:k:h:g:V:u:dDiqQ46")) != -1) {
		switch (opt) {
		case '4':
			IPv4or6 = AF_INET;
			break;
		case '6':
			IPv4or6 = AF_INET6;
			break;
		case 'f':
			config_file_name = optarg;
			break;
		case 'd':
			if (0 == debug_flag) {
				debug_flag = 1;
				options.log_level = SYSLOG_LEVEL_DEBUG1;
			} else if (options.log_level < SYSLOG_LEVEL_DEBUG3) {
				options.log_level++;
			} else {
				fprintf(stderr, "Too high debugging level.\n");
				exit(1);
			}
			break;
		case 'D':
			no_daemon_flag = 1;
			break;
		case 'i':
			inetd_flag = 1;
			break;
		case 'Q':
			/* ignored */
			break;
		case 'q':
			options.log_level = SYSLOG_LEVEL_QUIET;
			break;
		case 'b':
			options.server_key_bits = atoi(optarg);
			break;
		case 'p':
			options.ports_from_cmdline = 1;
			if (options.num_ports >= MAX_PORTS) {
				fprintf(stderr, "too many ports.\n");
				exit(1);
			}
			options.ports[options.num_ports++] = atoi(optarg);
			break;
		case 'g':
			options.login_grace_time = atoi(optarg);
			break;
		case 'k':
			options.key_regeneration_time = atoi(optarg);
			break;
		case 'h':
			if (options.num_host_key_files >= MAX_HOSTKEYS) {
				fprintf(stderr, "too many host keys.\n");
				exit(1);
			}
			options.host_key_files[options.num_host_key_files++] = optarg;
			break;
		case 'V':
			client_version_string = optarg;
			/* only makes sense with inetd_flag, i.e. no listen() */
			inetd_flag = 1;
			break;
		case 'u':
			utmp_len = atoi(optarg);
			break;
		case '?':
		default:
			fprintf(stderr, "sshd version %s\n", SSH_VERSION);
			fprintf(stderr, "Usage: %s [options]\n", __progname);
			fprintf(stderr, "Options:\n");
			fprintf(stderr, "  -f file    Configuration file (default %s)\n", _PATH_SERVER_CONFIG_FILE);
			fprintf(stderr, "  -d         Debugging mode (multiple -d means more debugging)\n");
			fprintf(stderr, "  -i         Started from inetd\n");
			fprintf(stderr, "  -D         Do not fork into daemon mode\n");
			fprintf(stderr, "  -q         Quiet (no logging)\n");
			fprintf(stderr, "  -p port    Listen on the specified port (default: 22)\n");
			fprintf(stderr, "  -k seconds Regenerate server key every this many seconds (default: 3600)\n");
			fprintf(stderr, "  -g seconds Grace period for authentication (default: 600)\n");
			fprintf(stderr, "  -b bits    Size of server RSA key (default: 768 bits)\n");
			fprintf(stderr, "  -h file    File from which to read host key (default: %s)\n",
			    _PATH_HOST_KEY_FILE);
			fprintf(stderr, "  -u len     Maximum hostname length for utmp recording\n");
			fprintf(stderr, "  -4         Use IPv4 only\n");
			fprintf(stderr, "  -6         Use IPv6 only\n");
			exit(1);
		}
	}

	/*
	 * Force logging to stderr until we have loaded the private host
	 * key (unless started from inetd)
	 */
	log_init(__progname,
	    options.log_level == -1 ? SYSLOG_LEVEL_INFO : options.log_level,
	    options.log_facility == -1 ? SYSLOG_FACILITY_AUTH : options.log_facility,
	    !inetd_flag);

	/* Read server configuration options from the configuration file. */
	read_server_config(&options, config_file_name);

	/* Fill in default values for those options not explicitly set. */
	fill_default_server_options(&options);

	/* Check that there are no remaining arguments. */
	if (optind < ac) {
		fprintf(stderr, "Extra argument %s.\n", av[optind]);
		exit(1);
	}

	debug("sshd version %.100s", SSH_VERSION);

	/* load private host keys */
	sensitive_data.host_keys = xmalloc(options.num_host_key_files*sizeof(Key*));
	for(i = 0; i < options.num_host_key_files; i++)
		sensitive_data.host_keys[i] = NULL;
	sensitive_data.server_key = NULL;
	sensitive_data.ssh1_host_key = NULL;
	sensitive_data.have_ssh1_key = 0;
	sensitive_data.have_ssh2_key = 0;

	for(i = 0; i < options.num_host_key_files; i++) {
		Key *key = load_private_key_autodetect(options.host_key_files[i]);
		if (key == NULL) {
			error("Could not load host key: %.200s: %.100s",
			    options.host_key_files[i], strerror(errno));
			continue;
		}
		switch(key->type){
		case KEY_RSA1:
			sensitive_data.ssh1_host_key = key;
			sensitive_data.have_ssh1_key = 1;
			break;
		case KEY_RSA:
		case KEY_DSA:
			sensitive_data.have_ssh2_key = 1;
			break;
		}
		sensitive_data.host_keys[i] = key;
	}
	if ((options.protocol & SSH_PROTO_1) && !sensitive_data.have_ssh1_key) {
		log("Disabling protocol version 1. Could not load host key");
		options.protocol &= ~SSH_PROTO_1;
	}
	if ((options.protocol & SSH_PROTO_2) && !sensitive_data.have_ssh2_key) {
		log("Disabling protocol version 2. Could not load host key");
		options.protocol &= ~SSH_PROTO_2;
	}
	if (!(options.protocol & (SSH_PROTO_1|SSH_PROTO_2))) {
		log("sshd: no hostkeys available -- exiting.\n");
		exit(1);
	}

	/* Check certain values for sanity. */
	if (options.protocol & SSH_PROTO_1) {
		if (options.server_key_bits < 512 ||
		    options.server_key_bits > 32768) {
			fprintf(stderr, "Bad server key size.\n");
			exit(1);
		}
		/*
		 * Check that server and host key lengths differ sufficiently. This
		 * is necessary to make double encryption work with rsaref. Oh, I
		 * hate software patents. I dont know if this can go? Niels
		 */
		if (options.server_key_bits >
		    BN_num_bits(sensitive_data.ssh1_host_key->rsa->n) - SSH_KEY_BITS_RESERVED &&
		    options.server_key_bits <
		    BN_num_bits(sensitive_data.ssh1_host_key->rsa->n) + SSH_KEY_BITS_RESERVED) {
			options.server_key_bits =
			    BN_num_bits(sensitive_data.ssh1_host_key->rsa->n) + SSH_KEY_BITS_RESERVED;
			debug("Forcing server key to %d bits to make it differ from host key.",
			    options.server_key_bits);
		}
	}

	/* Initialize the log (it is reinitialized below in case we forked). */
	if (debug_flag && !inetd_flag)
		log_stderr = 1;
	log_init(__progname, options.log_level, options.log_facility, log_stderr);

	/*
	 * If not in debugging mode, and not started from inetd, disconnect
	 * from the controlling terminal, and fork.  The original process
	 * exits.
	 */
	if (!(debug_flag || inetd_flag || no_daemon_flag)) {
#ifdef TIOCNOTTY
		int fd;
#endif /* TIOCNOTTY */
		if (daemon(0, 0) < 0)
			fatal("daemon() failed: %.200s", strerror(errno));

		/* Disconnect from the controlling tty. */
#ifdef TIOCNOTTY
		fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
		if (fd >= 0) {
			(void) ioctl(fd, TIOCNOTTY, NULL);
			close(fd);
		}
#endif /* TIOCNOTTY */
	}
	/* Reinitialize the log (because of the fork above). */
	log_init(__progname, options.log_level, options.log_facility, log_stderr);

	/* Initialize the random number generator. */
	arc4random_stir();

	/* Chdir to the root directory so that the current disk can be
	   unmounted if desired. */
	chdir("/");

	/* Start listening for a socket, unless started from inetd. */
	if (inetd_flag) {
		int s1, s2;
		s1 = dup(0);	/* Make sure descriptors 0, 1, and 2 are in use. */
		s2 = dup(s1);
		sock_in = dup(0);
		sock_out = dup(1);
		startup_pipe = -1;
		/*
		 * We intentionally do not close the descriptors 0, 1, and 2
		 * as our code for setting the descriptors won\'t work if
		 * ttyfd happens to be one of those.
		 */
		debug("inetd sockets after dupping: %d, %d", sock_in, sock_out);
		if (options.protocol & SSH_PROTO_1)
			generate_empheral_server_key();
	} else {
		for (ai = options.listen_addrs; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;
			if (num_listen_socks >= MAX_LISTEN_SOCKS)
				fatal("Too many listen sockets. "
				    "Enlarge MAX_LISTEN_SOCKS");
			if (getnameinfo(ai->ai_addr, ai->ai_addrlen,
			    ntop, sizeof(ntop), strport, sizeof(strport),
			    NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
				error("getnameinfo failed");
				continue;
			}
			/* Create socket for listening. */
			listen_sock = socket(ai->ai_family, SOCK_STREAM, 0);
			if (listen_sock < 0) {
				/* kernel may not support ipv6 */
				verbose("socket: %.100s", strerror(errno));
				continue;
			}
			if (fcntl(listen_sock, F_SETFL, O_NONBLOCK) < 0) {
				error("listen_sock O_NONBLOCK: %s", strerror(errno));
				close(listen_sock);
				continue;
			}
			/*
			 * Set socket options.  We try to make the port
			 * reusable and have it close as fast as possible
			 * without waiting in unnecessary wait states on
			 * close.
			 */
			setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
			    (void *) &on, sizeof(on));
			linger.l_onoff = 1;
			linger.l_linger = 5;
			setsockopt(listen_sock, SOL_SOCKET, SO_LINGER,
			    (void *) &linger, sizeof(linger));

			debug("Bind to port %s on %s.", strport, ntop);

			/* Bind the socket to the desired port. */
			if (bind(listen_sock, ai->ai_addr, ai->ai_addrlen) < 0) {
				error("Bind to port %s on %s failed: %.200s.",
				    strport, ntop, strerror(errno));
				close(listen_sock);
				continue;
			}
			listen_socks[num_listen_socks] = listen_sock;
			num_listen_socks++;

			/* Start listening on the port. */
			log("Server listening on %s port %s.", ntop, strport);
			if (listen(listen_sock, 5) < 0)
				fatal("listen: %.100s", strerror(errno));

		}
		freeaddrinfo(options.listen_addrs);

		if (!num_listen_socks)
			fatal("Cannot bind any address.");

		if (!debug_flag) {
			/*
			 * Record our pid in /var/run/sshd.pid to make it
			 * easier to kill the correct sshd.  We don't want to
			 * do this before the bind above because the bind will
			 * fail if there already is a daemon, and this will
			 * overwrite any old pid in the file.
			 */
			f = fopen(options.pid_file, "w");
			if (f) {
				fprintf(f, "%u\n", (u_int) getpid());
				fclose(f);
			}
		}
		if (options.protocol & SSH_PROTO_1)
			generate_empheral_server_key();

		/* Arrange to restart on SIGHUP.  The handler needs listen_sock. */
		signal(SIGHUP, sighup_handler);

		signal(SIGTERM, sigterm_handler);
		signal(SIGQUIT, sigterm_handler);

		/* Arrange SIGCHLD to be caught. */
		signal(SIGCHLD, main_sigchld_handler);

		/* setup fd set for listen */
		fdset = NULL;
		maxfd = 0;
		for (i = 0; i < num_listen_socks; i++)
			if (listen_socks[i] > maxfd)
				maxfd = listen_socks[i];
		/* pipes connected to unauthenticated childs */
		startup_pipes = xmalloc(options.max_startups * sizeof(int));
		for (i = 0; i < options.max_startups; i++)
			startup_pipes[i] = -1;

		/*
		 * Stay listening for connections until the system crashes or
		 * the daemon is killed with a signal.
		 */
		for (;;) {
			if (received_sighup)
				sighup_restart();
			if (fdset != NULL)
				xfree(fdset);
			fdsetsz = howmany(maxfd+1, NFDBITS) * sizeof(fd_mask);
			fdset = (fd_set *)xmalloc(fdsetsz);
			memset(fdset, 0, fdsetsz);

			for (i = 0; i < num_listen_socks; i++)
				FD_SET(listen_socks[i], fdset);
			for (i = 0; i < options.max_startups; i++)
				if (startup_pipes[i] != -1)
					FD_SET(startup_pipes[i], fdset);

			/* Wait in select until there is a connection. */
			ret = select(maxfd+1, fdset, NULL, NULL, NULL);
			if (ret < 0 && errno != EINTR)
				error("select: %.100s", strerror(errno));
			if (key_used && key_do_regen) {
				generate_empheral_server_key();
				key_used = 0;
				key_do_regen = 0;
			}
			if (ret < 0)
				continue;

			for (i = 0; i < options.max_startups; i++)
				if (startup_pipes[i] != -1 &&
				    FD_ISSET(startup_pipes[i], fdset)) {
					/*
					 * the read end of the pipe is ready
					 * if the child has closed the pipe
					 * after successful authentication
					 * or if the child has died
					 */
					close(startup_pipes[i]);
					startup_pipes[i] = -1;
					startups--;
				}
			for (i = 0; i < num_listen_socks; i++) {
				if (!FD_ISSET(listen_socks[i], fdset))
					continue;
				fromlen = sizeof(from);
				newsock = accept(listen_socks[i], (struct sockaddr *)&from,
				    &fromlen);
				if (newsock < 0) {
					if (errno != EINTR && errno != EWOULDBLOCK)
						error("accept: %.100s", strerror(errno));
					continue;
				}
				if (fcntl(newsock, F_SETFL, 0) < 0) {
					error("newsock del O_NONBLOCK: %s", strerror(errno));
					continue;
				}
				if (drop_connection(startups) == 1) {
					debug("drop connection #%d", startups);
					close(newsock);
					continue;
				}
				if (pipe(startup_p) == -1) {
					close(newsock);
					continue;
				}

				for (j = 0; j < options.max_startups; j++)
					if (startup_pipes[j] == -1) {
						startup_pipes[j] = startup_p[0];
						if (maxfd < startup_p[0])
							maxfd = startup_p[0];
						startups++;
						break;
					}

				/*
				 * Got connection.  Fork a child to handle it, unless
				 * we are in debugging mode.
				 */
				if (debug_flag) {
					/*
					 * In debugging mode.  Close the listening
					 * socket, and start processing the
					 * connection without forking.
					 */
					debug("Server will not fork when running in debugging mode.");
					close_listen_socks();
					sock_in = newsock;
					sock_out = newsock;
					startup_pipe = -1;
					pid = getpid();
					break;
				} else {
					/*
					 * Normal production daemon.  Fork, and have
					 * the child process the connection. The
					 * parent continues listening.
					 */
					if ((pid = fork()) == 0) {
						/*
						 * Child.  Close the listening and max_startup
						 * sockets.  Start using the accepted socket.
						 * Reinitialize logging (since our pid has
						 * changed).  We break out of the loop to handle
						 * the connection.
						 */
						startup_pipe = startup_p[1];
						for (j = 0; j < options.max_startups; j++)
							if (startup_pipes[j] != -1)
								close(startup_pipes[j]);
						close_listen_socks();
						sock_in = newsock;
						sock_out = newsock;
						log_init(__progname, options.log_level, options.log_facility, log_stderr);
						break;
					}
				}

				/* Parent.  Stay in the loop. */
				if (pid < 0)
					error("fork: %.100s", strerror(errno));
				else
					debug("Forked child %d.", pid);

				close(startup_p[1]);

				/* Mark that the key has been used (it was "given" to the child). */
				if ((options.protocol & SSH_PROTO_1) &&
				    key_used == 0) {
					/* Schedule server key regeneration alarm. */
					signal(SIGALRM, key_regeneration_alarm);
					alarm(options.key_regeneration_time);
					key_used = 1;
				}

				arc4random_stir();

				/* Close the new socket (the child is now taking care of it). */
				close(newsock);
			}
			/* child process check (or debug mode) */
			if (num_listen_socks < 0)
				break;
		}
	}

	/* This is the child processing a new connection. */

	/*
	 * Disable the key regeneration alarm.  We will not regenerate the
	 * key since we are no longer in a position to give it to anyone. We
	 * will not restart on SIGHUP since it no longer makes sense.
	 */
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);

	/*
	 * Set socket options for the connection.  We want the socket to
	 * close as fast as possible without waiting for anything.  If the
	 * connection is not a socket, these will do nothing.
	 */
	/* setsockopt(sock_in, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on)); */
	linger.l_onoff = 1;
	linger.l_linger = 5;
	setsockopt(sock_in, SOL_SOCKET, SO_LINGER, (void *) &linger, sizeof(linger));

	/* Set keepalives if requested. */
	if (options.keepalives &&
	    setsockopt(sock_in, SOL_SOCKET, SO_KEEPALIVE, (void *)&on,
	    sizeof(on)) < 0)
		error("setsockopt SO_KEEPALIVE: %.100s", strerror(errno));

	/*
	 * Register our connection.  This turns encryption off because we do
	 * not have a key.
	 */
	packet_set_connection(sock_in, sock_out);

	remote_port = get_remote_port();
	remote_ip = get_remote_ipaddr();

	/* Check whether logins are denied from this host. */
#ifdef LIBWRAP
	/* XXX LIBWRAP noes not know about IPv6 */
	{
		struct request_info req;

		request_init(&req, RQ_DAEMON, __progname, RQ_FILE, sock_in, NULL);
		fromhost(&req);

		if (!hosts_access(&req)) {
			close(sock_in);
			close(sock_out);
			refuse(&req);
		}
/*XXX IPv6 verbose("Connection from %.500s port %d", eval_client(&req), remote_port); */
	}
#endif /* LIBWRAP */
	/* Log the connection. */
	verbose("Connection from %.500s port %d", remote_ip, remote_port);

	/*
	 * We don\'t want to listen forever unless the other side
	 * successfully authenticates itself.  So we set up an alarm which is
	 * cleared after successful authentication.  A limit of zero
	 * indicates no limit. Note that we don\'t set the alarm in debugging
	 * mode; it is just annoying to have the server exit just when you
	 * are about to discover the bug.
	 */
	signal(SIGALRM, grace_alarm_handler);
	if (!debug_flag)
		alarm(options.login_grace_time);

	sshd_exchange_identification(sock_in, sock_out);
	/*
	 * Check that the connection comes from a privileged port.
	 * Rhosts-Authentication only makes sense from priviledged
	 * programs.  Of course, if the intruder has root access on his local
	 * machine, he can connect from any port.  So do not use these
	 * authentication methods from machines that you do not trust.
	 */
	if (remote_port >= IPPORT_RESERVED ||
	    remote_port < IPPORT_RESERVED / 2) {
		debug("Rhosts Authentication disabled, "
		    "originating port not trusted.");
		options.rhosts_authentication = 0;
	}
#ifdef KRB4
	if (!packet_connection_is_ipv4() &&
	    options.kerberos_authentication) {
		debug("Kerberos Authentication disabled, only available for IPv4.");
		options.kerberos_authentication = 0;
	}
#endif /* KRB4 */
#ifdef AFS
	/* If machine has AFS, set process authentication group. */
	if (k_hasafs()) {
		k_setpag();
		k_unlog();
	}
#endif /* AFS */

	packet_set_nonblocking();

	/* perform the key exchange */
	/* authenticate user and start session */
	if (compat20) {
		do_ssh2_kex();
		do_authentication2();
	} else {
		do_ssh1_kex();
		do_authentication();
	}

#ifdef KRB4
	/* Cleanup user's ticket cache file. */
	if (options.kerberos_ticket_cleanup)
		(void) dest_tkt();
#endif /* KRB4 */

	/* The connection has been terminated. */
	verbose("Closing connection to %.100s", remote_ip);
	packet_close();
	exit(0);
}

/*
 * SSH1 key exchange
 */
void
do_ssh1_kex(void)
{
	int i, len;
	int plen, slen;
	int rsafail = 0;
	BIGNUM *session_key_int;
	u_char session_key[SSH_SESSION_KEY_LENGTH];
	u_char cookie[8];
	u_int cipher_type, auth_mask, protocol_flags;
	u_int32_t rand = 0;

	/*
	 * Generate check bytes that the client must send back in the user
	 * packet in order for it to be accepted; this is used to defy ip
	 * spoofing attacks.  Note that this only works against somebody
	 * doing IP spoofing from a remote machine; any machine on the local
	 * network can still see outgoing packets and catch the random
	 * cookie.  This only affects rhosts authentication, and this is one
	 * of the reasons why it is inherently insecure.
	 */
	for (i = 0; i < 8; i++) {
		if (i % 4 == 0)
			rand = arc4random();
		cookie[i] = rand & 0xff;
		rand >>= 8;
	}

	/*
	 * Send our public key.  We include in the packet 64 bits of random
	 * data that must be matched in the reply in order to prevent IP
	 * spoofing.
	 */
	packet_start(SSH_SMSG_PUBLIC_KEY);
	for (i = 0; i < 8; i++)
		packet_put_char(cookie[i]);

	/* Store our public server RSA key. */
	packet_put_int(BN_num_bits(sensitive_data.server_key->rsa->n));
	packet_put_bignum(sensitive_data.server_key->rsa->e);
	packet_put_bignum(sensitive_data.server_key->rsa->n);

	/* Store our public host RSA key. */
	packet_put_int(BN_num_bits(sensitive_data.ssh1_host_key->rsa->n));
	packet_put_bignum(sensitive_data.ssh1_host_key->rsa->e);
	packet_put_bignum(sensitive_data.ssh1_host_key->rsa->n);

	/* Put protocol flags. */
	packet_put_int(SSH_PROTOFLAG_HOST_IN_FWD_OPEN);

	/* Declare which ciphers we support. */
	packet_put_int(cipher_mask_ssh1(0));

	/* Declare supported authentication types. */
	auth_mask = 0;
	if (options.rhosts_authentication)
		auth_mask |= 1 << SSH_AUTH_RHOSTS;
	if (options.rhosts_rsa_authentication)
		auth_mask |= 1 << SSH_AUTH_RHOSTS_RSA;
	if (options.rsa_authentication)
		auth_mask |= 1 << SSH_AUTH_RSA;
#ifdef KRB4
	if (options.kerberos_authentication)
		auth_mask |= 1 << SSH_AUTH_KERBEROS;
#endif
#ifdef AFS
	if (options.kerberos_tgt_passing)
		auth_mask |= 1 << SSH_PASS_KERBEROS_TGT;
	if (options.afs_token_passing)
		auth_mask |= 1 << SSH_PASS_AFS_TOKEN;
#endif
	if (options.challenge_reponse_authentication == 1)
		auth_mask |= 1 << SSH_AUTH_TIS;
	if (options.password_authentication)
		auth_mask |= 1 << SSH_AUTH_PASSWORD;
	packet_put_int(auth_mask);

	/* Send the packet and wait for it to be sent. */
	packet_send();
	packet_write_wait();

	debug("Sent %d bit server key and %d bit host key.",
	    BN_num_bits(sensitive_data.server_key->rsa->n),
	    BN_num_bits(sensitive_data.ssh1_host_key->rsa->n));

	/* Read clients reply (cipher type and session key). */
	packet_read_expect(&plen, SSH_CMSG_SESSION_KEY);

	/* Get cipher type and check whether we accept this. */
	cipher_type = packet_get_char();

	if (!(cipher_mask_ssh1(0) & (1 << cipher_type)))
		packet_disconnect("Warning: client selects unsupported cipher.");

	/* Get check bytes from the packet.  These must match those we
	   sent earlier with the public key packet. */
	for (i = 0; i < 8; i++)
		if (cookie[i] != packet_get_char())
			packet_disconnect("IP Spoofing check bytes do not match.");

	debug("Encryption type: %.200s", cipher_name(cipher_type));

	/* Get the encrypted integer. */
	session_key_int = BN_new();
	packet_get_bignum(session_key_int, &slen);

	protocol_flags = packet_get_int();
	packet_set_protocol_flags(protocol_flags);

	packet_integrity_check(plen, 1 + 8 + slen + 4, SSH_CMSG_SESSION_KEY);

	/*
	 * Decrypt it using our private server key and private host key (key
	 * with larger modulus first).
	 */
	if (BN_cmp(sensitive_data.server_key->rsa->n, sensitive_data.ssh1_host_key->rsa->n) > 0) {
		/* Server key has bigger modulus. */
		if (BN_num_bits(sensitive_data.server_key->rsa->n) <
		    BN_num_bits(sensitive_data.ssh1_host_key->rsa->n) + SSH_KEY_BITS_RESERVED) {
			fatal("do_connection: %s: server_key %d < host_key %d + SSH_KEY_BITS_RESERVED %d",
			    get_remote_ipaddr(),
			    BN_num_bits(sensitive_data.server_key->rsa->n),
			    BN_num_bits(sensitive_data.ssh1_host_key->rsa->n),
			    SSH_KEY_BITS_RESERVED);
		}
		if (rsa_private_decrypt(session_key_int, session_key_int,
		    sensitive_data.server_key->rsa) <= 0)
			rsafail++;
		if (rsa_private_decrypt(session_key_int, session_key_int,
		    sensitive_data.ssh1_host_key->rsa) <= 0)
			rsafail++;
	} else {
		/* Host key has bigger modulus (or they are equal). */
		if (BN_num_bits(sensitive_data.ssh1_host_key->rsa->n) <
		    BN_num_bits(sensitive_data.server_key->rsa->n) + SSH_KEY_BITS_RESERVED) {
			fatal("do_connection: %s: host_key %d < server_key %d + SSH_KEY_BITS_RESERVED %d",
			    get_remote_ipaddr(),
			    BN_num_bits(sensitive_data.ssh1_host_key->rsa->n),
			    BN_num_bits(sensitive_data.server_key->rsa->n),
			    SSH_KEY_BITS_RESERVED);
		}
		if (rsa_private_decrypt(session_key_int, session_key_int,
		    sensitive_data.ssh1_host_key->rsa) < 0)
			rsafail++;
		if (rsa_private_decrypt(session_key_int, session_key_int,
		    sensitive_data.server_key->rsa) < 0)
			rsafail++;
	}

	compute_session_id(session_id, cookie,
	    sensitive_data.ssh1_host_key->rsa->n,
	    sensitive_data.server_key->rsa->n);

	/* Destroy the private and public keys.  They will no longer be needed. */
	destroy_sensitive_data();

	/*
	 * Extract session key from the decrypted integer.  The key is in the
	 * least significant 256 bits of the integer; the first byte of the
	 * key is in the highest bits.
	 */
	if (!rsafail) {
		BN_mask_bits(session_key_int, sizeof(session_key) * 8);
		len = BN_num_bytes(session_key_int);
		if (len < 0 || len > sizeof(session_key)) {
			error("do_connection: bad session key len from %s: "
			    "session_key_int %d > sizeof(session_key) %lu",
			    get_remote_ipaddr(), len, (u_long)sizeof(session_key));
			rsafail++;
		} else {
			memset(session_key, 0, sizeof(session_key));
			BN_bn2bin(session_key_int,
			    session_key + sizeof(session_key) - len);
		}
	}
	if (rsafail) {
		log("do_connection: generating a fake encryption key");
		for (i = 0; i < SSH_SESSION_KEY_LENGTH; i++) {
			if (i % 4 == 0)
				rand = arc4random();
			session_key[i] = rand & 0xff;
			rand >>= 8;
		}
	}
	/* Destroy the decrypted integer.  It is no longer needed. */
	BN_clear_free(session_key_int);

	/* Xor the first 16 bytes of the session key with the session id. */
	for (i = 0; i < 16; i++)
		session_key[i] ^= session_id[i];

	/* Set the session key.  From this on all communications will be encrypted. */
	packet_set_encryption_key(session_key, SSH_SESSION_KEY_LENGTH, cipher_type);

	/* Destroy our copy of the session key.  It is no longer needed. */
	memset(session_key, 0, sizeof(session_key));

	debug("Received session key; encryption turned on.");

	/* Send an acknowledgement packet.  Note that this packet is sent encrypted. */
	packet_start(SSH_SMSG_SUCCESS);
	packet_send();
	packet_write_wait();
}

/*
 * SSH2 key exchange: diffie-hellman-group1-sha1
 */
void
do_ssh2_kex(void)
{
	Buffer *server_kexinit;
	Buffer *client_kexinit;
	int payload_len;
	int i;
	Kex *kex;
	char *cprop[PROPOSAL_MAX];

/* KEXINIT */

	if (options.ciphers != NULL) {
		myproposal[PROPOSAL_ENC_ALGS_CTOS] =
		myproposal[PROPOSAL_ENC_ALGS_STOC] = options.ciphers;
	}
	if (options.macs != NULL) {
		myproposal[PROPOSAL_MAC_ALGS_CTOS] =
		myproposal[PROPOSAL_MAC_ALGS_STOC] = options.macs;
	}
	myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = list_hostkey_types();

	server_kexinit = kex_init(myproposal);
	client_kexinit = xmalloc(sizeof(*client_kexinit));
	buffer_init(client_kexinit);

	/* algorithm negotiation */
	kex_exchange_kexinit(server_kexinit, client_kexinit, cprop);
	kex = kex_choose_conf(cprop, myproposal, 1);
	for (i = 0; i < PROPOSAL_MAX; i++)
		xfree(cprop[i]);

	switch (kex->kex_type) {
	case DH_GRP1_SHA1:
		ssh_dh1_server(kex, client_kexinit, server_kexinit);
		break;
	case DH_GEX_SHA1:
		ssh_dhgex_server(kex, client_kexinit, server_kexinit);
		break;
	default:
		fatal("Unsupported key exchange %d", kex->kex_type);
	}

	debug("send SSH2_MSG_NEWKEYS.");
	packet_start(SSH2_MSG_NEWKEYS);
	packet_send();
	packet_write_wait();
	debug("done: send SSH2_MSG_NEWKEYS.");

	debug("Wait SSH2_MSG_NEWKEYS.");
	packet_read_expect(&payload_len, SSH2_MSG_NEWKEYS);
	debug("GOT SSH2_MSG_NEWKEYS.");

#ifdef DEBUG_KEXDH
	/* send 1st encrypted/maced/compressed message */
	packet_start(SSH2_MSG_IGNORE);
	packet_put_cstring("markus");
	packet_send();
	packet_write_wait();
#endif

	debug("done: KEX2.");
}

/*
 * SSH2 key exchange
 */

/* diffie-hellman-group1-sha1 */

void
ssh_dh1_server(Kex *kex, Buffer *client_kexinit, Buffer *server_kexinit)
{
#ifdef DEBUG_KEXDH
	int i;
#endif
	int payload_len, dlen;
	int slen;
	u_char *signature = NULL;
	u_char *server_host_key_blob = NULL;
	u_int sbloblen;
	u_int klen, kout;
	u_char *kbuf;
	u_char *hash;
	BIGNUM *shared_secret = 0;
	DH *dh;
	BIGNUM *dh_client_pub = 0;
	Key *hostkey;

	hostkey = get_hostkey_by_type(kex->hostkey_type);
	if (hostkey == NULL)
		fatal("Unsupported hostkey type %d", kex->hostkey_type);

/* KEXDH */
	/* generate DH key */
	dh = dh_new_group1();			/* XXX depends on 'kex' */
	dh_gen_key(dh);

	debug("Wait SSH2_MSG_KEXDH_INIT.");
	packet_read_expect(&payload_len, SSH2_MSG_KEXDH_INIT);

	/* key, cert */
	dh_client_pub = BN_new();
	if (dh_client_pub == NULL)
		fatal("dh_client_pub == NULL");
	packet_get_bignum2(dh_client_pub, &dlen);

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\ndh_client_pub= ");
	BN_print_fp(stderr, dh_client_pub);
	fprintf(stderr, "\n");
	debug("bits %d", BN_num_bits(dh_client_pub));
#endif

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\np= ");
	BN_print_fp(stderr, dh->p);
	fprintf(stderr, "\ng= ");
	bn_print(dh->g);
	fprintf(stderr, "\npub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
	DHparams_print_fp(stderr, dh);
#endif
	if (!dh_pub_is_valid(dh, dh_client_pub))
		packet_disconnect("bad client public DH value");

	klen = DH_size(dh);
	kbuf = xmalloc(klen);
	kout = DH_compute_key(kbuf, dh_client_pub, dh);

#ifdef DEBUG_KEXDH
	debug("shared secret: len %d/%d", klen, kout);
	fprintf(stderr, "shared secret == ");
	for (i = 0; i< kout; i++)
		fprintf(stderr, "%02x", (kbuf[i])&0xff);
	fprintf(stderr, "\n");
#endif
	shared_secret = BN_new();

	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	/* XXX precompute? */
	key_to_blob(hostkey, &server_host_key_blob, &sbloblen);

	/* calc H */			/* XXX depends on 'kex' */
	hash = kex_hash(
	    client_version_string,
	    server_version_string,
	    buffer_ptr(client_kexinit), buffer_len(client_kexinit),
	    buffer_ptr(server_kexinit), buffer_len(server_kexinit),
	    (char *)server_host_key_blob, sbloblen,
	    dh_client_pub,
	    dh->pub_key,
	    shared_secret
	);
	buffer_free(client_kexinit);
	buffer_free(server_kexinit);
	xfree(client_kexinit);
	xfree(server_kexinit);
	BN_free(dh_client_pub);
#ifdef DEBUG_KEXDH
	fprintf(stderr, "hash == ");
	for (i = 0; i< 20; i++)
		fprintf(stderr, "%02x", (hash[i])&0xff);
	fprintf(stderr, "\n");
#endif
	/* save session id := H */
	/* XXX hashlen depends on KEX */
	session_id2_len = 20;
	session_id2 = xmalloc(session_id2_len);
	memcpy(session_id2, hash, session_id2_len);

	/* sign H */
	/* XXX hashlen depends on KEX */
	key_sign(hostkey, &signature, &slen, hash, 20);

	destroy_sensitive_data();

	/* send server hostkey, DH pubkey 'f' and singed H */
	packet_start(SSH2_MSG_KEXDH_REPLY);
	packet_put_string((char *)server_host_key_blob, sbloblen);
	packet_put_bignum2(dh->pub_key);	/* f */
	packet_put_string((char *)signature, slen);
	packet_send();
	xfree(signature);
	xfree(server_host_key_blob);
	packet_write_wait();

	kex_derive_keys(kex, hash, shared_secret);
	BN_clear_free(shared_secret);
	packet_set_kex(kex);

	/* have keys, free DH */
	DH_free(dh);
}

/* diffie-hellman-group-exchange-sha1 */

void
ssh_dhgex_server(Kex *kex, Buffer *client_kexinit, Buffer *server_kexinit)
{
#ifdef DEBUG_KEXDH
	int i;
#endif
	int payload_len, dlen;
	int slen, nbits;
	u_char *signature = NULL;
	u_char *server_host_key_blob = NULL;
	u_int sbloblen;
	u_int klen, kout;
	u_char *kbuf;
	u_char *hash;
	BIGNUM *shared_secret = 0;
	DH *dh;
	BIGNUM *dh_client_pub = 0;
	Key *hostkey;

	hostkey = get_hostkey_by_type(kex->hostkey_type);
	if (hostkey == NULL)
		fatal("Unsupported hostkey type %d", kex->hostkey_type);

/* KEXDHGEX */
	debug("Wait SSH2_MSG_KEX_DH_GEX_REQUEST.");
	packet_read_expect(&payload_len, SSH2_MSG_KEX_DH_GEX_REQUEST);
	nbits = packet_get_int();
	dh = choose_dh(nbits);

	debug("Sending SSH2_MSG_KEX_DH_GEX_GROUP.");
	packet_start(SSH2_MSG_KEX_DH_GEX_GROUP);
	packet_put_bignum2(dh->p);
	packet_put_bignum2(dh->g);
	packet_send();
	packet_write_wait();

	/* Compute our exchange value in parallel with the client */

	dh_gen_key(dh);

	debug("Wait SSH2_MSG_KEX_DH_GEX_INIT.");
	packet_read_expect(&payload_len, SSH2_MSG_KEX_DH_GEX_INIT);

	/* key, cert */
	dh_client_pub = BN_new();
	if (dh_client_pub == NULL)
		fatal("dh_client_pub == NULL");
	packet_get_bignum2(dh_client_pub, &dlen);

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\ndh_client_pub= ");
	BN_print_fp(stderr, dh_client_pub);
	fprintf(stderr, "\n");
	debug("bits %d", BN_num_bits(dh_client_pub));
#endif

#ifdef DEBUG_KEXDH
	fprintf(stderr, "\np= ");
	BN_print_fp(stderr, dh->p);
	fprintf(stderr, "\ng= ");
	bn_print(dh->g);
	fprintf(stderr, "\npub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
	DHparams_print_fp(stderr, dh);
#endif
	if (!dh_pub_is_valid(dh, dh_client_pub))
		packet_disconnect("bad client public DH value");

	klen = DH_size(dh);
	kbuf = xmalloc(klen);
	kout = DH_compute_key(kbuf, dh_client_pub, dh);

#ifdef DEBUG_KEXDH
	debug("shared secret: len %d/%d", klen, kout);
	fprintf(stderr, "shared secret == ");
	for (i = 0; i< kout; i++)
		fprintf(stderr, "%02x", (kbuf[i])&0xff);
	fprintf(stderr, "\n");
#endif
	shared_secret = BN_new();

	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	/* XXX precompute? */
	key_to_blob(hostkey, &server_host_key_blob, &sbloblen);

	/* calc H */			/* XXX depends on 'kex' */
	hash = kex_hash_gex(
	    client_version_string,
	    server_version_string,
	    buffer_ptr(client_kexinit), buffer_len(client_kexinit),
	    buffer_ptr(server_kexinit), buffer_len(server_kexinit),
	    (char *)server_host_key_blob, sbloblen,
	    nbits, dh->p, dh->g,
	    dh_client_pub,
	    dh->pub_key,
	    shared_secret
	);
	buffer_free(client_kexinit);
	buffer_free(server_kexinit);
	xfree(client_kexinit);
	xfree(server_kexinit);
	BN_free(dh_client_pub);
#ifdef DEBUG_KEXDH
	fprintf(stderr, "hash == ");
	for (i = 0; i< 20; i++)
		fprintf(stderr, "%02x", (hash[i])&0xff);
	fprintf(stderr, "\n");
#endif
	/* save session id := H */
	/* XXX hashlen depends on KEX */
	session_id2_len = 20;
	session_id2 = xmalloc(session_id2_len);
	memcpy(session_id2, hash, session_id2_len);

	/* sign H */
	/* XXX hashlen depends on KEX */
	key_sign(hostkey, &signature, &slen, hash, 20);

	destroy_sensitive_data();

	/* send server hostkey, DH pubkey 'f' and singed H */
	packet_start(SSH2_MSG_KEX_DH_GEX_REPLY);
	packet_put_string((char *)server_host_key_blob, sbloblen);
	packet_put_bignum2(dh->pub_key);	/* f */
	packet_put_string((char *)signature, slen);
	packet_send();
	xfree(signature);
	xfree(server_host_key_blob);
	packet_write_wait();

	kex_derive_keys(kex, hash, shared_secret);
	BN_clear_free(shared_secret);
	packet_set_kex(kex);

	/* have keys, free DH */
	DH_free(dh);
}
