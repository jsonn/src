/*	$NetBSD: sshconnect.c,v 1.16.2.1 2002/06/10 18:05:04 tv Exp $	*/
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Code to connect to a remote host, and to perform the client side of the
 * login (authentication) dialog.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: sshconnect.c,v 1.119 2002/01/21 15:13:51 markus Exp $");

#include <openssl/bn.h>

#include "ssh.h"
#include "xmalloc.h"
#include "rsa.h"
#include "buffer.h"
#include "packet.h"
#include "uidswap.h"
#include "compat.h"
#include "key.h"
#include "sshconnect.h"
#include "hostfile.h"
#include "log.h"
#include "readconf.h"
#include "atomicio.h"
#include "misc.h"
#include "readpass.h"

char *client_version_string = NULL;
char *server_version_string = NULL;

extern Options options;
extern char *__progname;

static const char *
sockaddr_ntop(struct sockaddr *sa)
{
	static char addrbuf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, addrbuf, sizeof(addrbuf), NULL, 0,
	    NI_NUMERICHOST) != 0)
		abort();	/* XXX abort is bad -- do something else */
	return addrbuf;
}

/*
 * Connect to the given ssh server using a proxy command.
 */
static int
ssh_proxy_connect(const char *host, u_short port, struct passwd *pw,
		  const char *proxy_command)
{
	Buffer command;
	const char *cp;
	char *command_string;
	int pin[2], pout[2];
	pid_t pid;
	char strport[NI_MAXSERV];

	/* Convert the port number into a string. */
	snprintf(strport, sizeof strport, "%hu", port);

	/* Build the final command string in the buffer by making the
	   appropriate substitutions to the given proxy command. */
	buffer_init(&command);
	for (cp = proxy_command; *cp; cp++) {
		if (cp[0] == '%' && cp[1] == '%') {
			buffer_append(&command, "%", 1);
			cp++;
			continue;
		}
		if (cp[0] == '%' && cp[1] == 'h') {
			buffer_append(&command, host, strlen(host));
			cp++;
			continue;
		}
		if (cp[0] == '%' && cp[1] == 'p') {
			buffer_append(&command, strport, strlen(strport));
			cp++;
			continue;
		}
		buffer_append(&command, cp, 1);
	}
	buffer_append(&command, "\0", 1);

	/* Get the final command string. */
	command_string = buffer_ptr(&command);

	/* Create pipes for communicating with the proxy. */
	if (pipe(pin) < 0 || pipe(pout) < 0)
		fatal("Could not create pipes to communicate with the proxy: %.100s",
		    strerror(errno));

	debug("Executing proxy command: %.500s", command_string);

	/* Fork and execute the proxy command. */
	if ((pid = fork()) == 0) {
		char *argv[10];

		/* Child.  Permanently give up superuser privileges. */
		permanently_set_uid(pw);

		/* Redirect stdin and stdout. */
		close(pin[1]);
		if (pin[0] != 0) {
			if (dup2(pin[0], 0) < 0)
				perror("dup2 stdin");
			close(pin[0]);
		}
		close(pout[0]);
		if (dup2(pout[1], 1) < 0)
			perror("dup2 stdout");
		/* Cannot be 1 because pin allocated two descriptors. */
		close(pout[1]);

		/* Stderr is left as it is so that error messages get
		   printed on the user's terminal. */
		argv[0] = _PATH_BSHELL;
		argv[1] = "-c";
		argv[2] = command_string;
		argv[3] = NULL;

		/* Execute the proxy command.  Note that we gave up any
		   extra privileges above. */
		execv(argv[0], argv);
		perror(argv[0]);
		exit(1);
	}
	/* Parent. */
	if (pid < 0)
		fatal("fork failed: %.100s", strerror(errno));

	/* Close child side of the descriptors. */
	close(pin[0]);
	close(pout[1]);

	/* Free the command name. */
	buffer_free(&command);

	/* Set the connection file descriptors. */
	packet_set_connection(pout[0], pin[1]);

	/* Indicate OK return */
	return 0;
}

/*
 * Creates a (possibly privileged) socket for use as the ssh connection.
 */
static int
ssh_create_socket(struct passwd *pw, int privileged, int family)
{
	int sock, gaierr;
	struct addrinfo hints, *res;

	/*
	 * If we are running as root and want to connect to a privileged
	 * port, bind our own socket to a privileged port.
	 */
	if (privileged) {
		int p = IPPORT_RESERVED - 1;
		sock = rresvport_af(&p, family);
		if (sock < 0)
			error("rresvport: af=%d %.100s", family, strerror(errno));
		else
			debug("Allocated local port %d.", p);
		return sock;
	}
	/*
	 * Just create an ordinary socket on arbitrary port.  We use
	 * the user's uid to create the socket.
	 */
	temporarily_use_uid(pw);
	sock = socket(family, SOCK_STREAM, 0);
	if (sock < 0)
		error("socket: %.100s", strerror(errno));
	restore_uid();

	/* Bind the socket to an alternative local IP address */
	if (options.bind_address == NULL)
		return sock;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	gaierr = getaddrinfo(options.bind_address, "0", &hints, &res);
	if (gaierr) {
		error("getaddrinfo: %s: %s", options.bind_address,
		    gai_strerror(gaierr));
		close(sock);
		return -1;
	}
	if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
		error("bind: %s: %s", options.bind_address, strerror(errno));
		close(sock);
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);
	return sock;
}

/*
 * Opens a TCP/IP connection to the remote server on the given host.
 * The address of the remote host will be returned in hostaddr.
 * If port is 0, the default port will be used.  If anonymous is zero,
 * a privileged port will be allocated to make the connection.
 * This requires super-user privileges if anonymous is false.
 * Connection_attempts specifies the maximum number of tries (one per
 * second).  If proxy_command is non-NULL, it specifies the command (with %h
 * and %p substituted for host and port, respectively) to use to contact
 * the daemon.
 * Return values:
 *    0 for OK
 *    ECONNREFUSED if we got a "Connection Refused" by the peer on any address
 *    ECONNABORTED if we failed without a "Connection refused"
 * Suitable error messages for the connection failure will already have been
 * printed.
 */
int
ssh_connect(const char *host, struct sockaddr_storage * hostaddr,
    u_short port, int family, int connection_attempts,
    int anonymous, struct passwd *pw, const char *proxy_command)
{
	int gaierr;
	int on = 1;
	int sock = -1, attempt;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	struct addrinfo hints, *ai, *aitop;
	struct linger linger;
	struct servent *sp;
	/*
	 * Did we get only other errors than "Connection refused" (which
	 * should block fallback to rsh and similar), or did we get at least
	 * one "Connection refused"?
	 */
	int full_failure = 1;

	debug("ssh_connect: getuid %u geteuid %u anon %d",
	    (u_int) getuid(), (u_int) geteuid(), anonymous);

	/* Get default port if port has not been set. */
	if (port == 0) {
		sp = getservbyname(SSH_SERVICE_NAME, "tcp");
		if (sp)
			port = ntohs(sp->s_port);
		else
			port = SSH_DEFAULT_PORT;
	}
	/* If a proxy command is given, connect using it. */
	if (proxy_command != NULL)
		return ssh_proxy_connect(host, port, pw, proxy_command);

	/* No proxy command. */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", port);
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0)
		fatal("%s: %.100s: %s", __progname, host,
		    gai_strerror(gaierr));

	/*
	 * Try to connect several times.  On some machines, the first time
	 * will sometimes fail.  In general socket code appears to behave
	 * quite magically on many machines.
		 */
	for (attempt = 0; ;) {
		if (attempt > 0)
			debug("Trying again...");

		/* Loop through addresses for this host, and try each one in
		   sequence until the connection succeeds. */
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;
			if (getnameinfo(ai->ai_addr, ai->ai_addrlen,
			    ntop, sizeof(ntop), strport, sizeof(strport),
			    NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
				error("ssh_connect: getnameinfo failed");
				continue;
			}
			debug("Connecting to %.200s [%.100s] port %s.",
				host, ntop, strport);

			/* Create a socket for connecting. */
			sock = ssh_create_socket(pw,
			    !anonymous && geteuid() == 0,
			    ai->ai_family);
			if (sock < 0)
				/* Any error is already output */
				continue;

			/* Connect to the host.  We use the user's uid in the
			 * hope that it will help with tcp_wrappers showing
			 * the remote uid as root.
			 */
			temporarily_use_uid(pw);
			if (connect(sock, ai->ai_addr, ai->ai_addrlen) >= 0) {
				/* Successful connection. */
				memcpy(hostaddr, ai->ai_addr, ai->ai_addrlen);
				restore_uid();
				break;
			} else {
				if (errno == ECONNREFUSED)
					full_failure = 0;
				log("ssh: connect to address %s port %s: %s",
				    sockaddr_ntop(ai->ai_addr), strport,
				    strerror(errno));
				restore_uid();
				/*
				 * Close the failed socket; there appear to
				 * be some problems when reusing a socket for
				 * which connect() has already returned an
				 * error.
				 */
				close(sock);
			}
		}
		if (ai)
			break;	/* Successful connection. */

		attempt++;
		if (attempt >= connection_attempts)
			break;
		/* Sleep a moment before retrying. */
		sleep(1);
	}

	freeaddrinfo(aitop);

	/* Return failure if we didn't get a successful connection. */
	if (attempt >= connection_attempts)
		return full_failure ? ECONNABORTED : ECONNREFUSED;

	debug("Connection established.");

	/*
	 * Set socket options.  We would like the socket to disappear as soon
	 * as it has been closed for whatever reason.
	 */
	/* setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on)); */
	linger.l_onoff = 1;
	linger.l_linger = 5;
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *)&linger, sizeof(linger));

	/* Set keepalives if requested. */
	if (options.keepalives &&
	    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on,
	    sizeof(on)) < 0)
		error("setsockopt SO_KEEPALIVE: %.100s", strerror(errno));

	/* Set the connection. */
	packet_set_connection(sock, sock);

	return 0;
}

/*
 * Waits for the server identification string, and sends our own
 * identification string.
 */
static void
ssh_exchange_identification(void)
{
	char buf[256], remote_version[256];	/* must be same size! */
	int remote_major, remote_minor, i, mismatch;
	int connection_in = packet_get_connection_in();
	int connection_out = packet_get_connection_out();
	int minor1 = PROTOCOL_MINOR_1;

	/* Read other side\'s version identification. */
	for (;;) {
		for (i = 0; i < sizeof(buf) - 1; i++) {
			int len = atomic_read(connection_in, &buf[i], 1);
			if (len < 0)
				fatal("ssh_exchange_identification: read: %.100s", strerror(errno));
			if (len != 1)
				fatal("ssh_exchange_identification: Connection closed by remote host");
			if (buf[i] == '\r') {
				buf[i] = '\n';
				buf[i + 1] = 0;
				continue;		/**XXX wait for \n */
			}
			if (buf[i] == '\n') {
				buf[i + 1] = 0;
				break;
			}
		}
		buf[sizeof(buf) - 1] = 0;
		if (strncmp(buf, "SSH-", 4) == 0)
			break;
		debug("ssh_exchange_identification: %s", buf);
	}
	server_version_string = xstrdup(buf);

	/*
	 * Check that the versions match.  In future this might accept
	 * several versions and set appropriate flags to handle them.
	 */
	if (sscanf(server_version_string, "SSH-%d.%d-%[^\n]\n",
	    &remote_major, &remote_minor, remote_version) != 3)
		fatal("Bad remote protocol version identification: '%.100s'", buf);
	debug("Remote protocol version %d.%d, remote software version %.100s",
	    remote_major, remote_minor, remote_version);

	compat_datafellows(remote_version);
	mismatch = 0;

	switch (remote_major) {
	case 1:
		if (remote_minor == 99 &&
		    (options.protocol & SSH_PROTO_2) &&
		    !(options.protocol & SSH_PROTO_1_PREFERRED)) {
			enable_compat20();
			break;
		}
		if (!(options.protocol & SSH_PROTO_1)) {
			mismatch = 1;
			break;
		}
		if (remote_minor < 3) {
			fatal("Remote machine has too old SSH software version.");
		} else if (remote_minor == 3 || remote_minor == 4) {
			/* We speak 1.3, too. */
			enable_compat13();
			minor1 = 3;
			if (options.forward_agent) {
				log("Agent forwarding disabled for protocol 1.3");
				options.forward_agent = 0;
			}
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
	if (mismatch)
		fatal("Protocol major versions differ: %d vs. %d",
		    (options.protocol & SSH_PROTO_2) ? PROTOCOL_MAJOR_2 : PROTOCOL_MAJOR_1,
		    remote_major);
	/* Send our own protocol version identification. */
	snprintf(buf, sizeof buf, "SSH-%d.%d-%.100s\n",
	    compat20 ? PROTOCOL_MAJOR_2 : PROTOCOL_MAJOR_1,
	    compat20 ? PROTOCOL_MINOR_2 : minor1,
	    SSH_VERSION);
	if (atomic_write(connection_out, buf, strlen(buf)) != strlen(buf))
		fatal("write: %.100s", strerror(errno));
	client_version_string = xstrdup(buf);
	chop(client_version_string);
	chop(server_version_string);
	debug("Local version string %.100s", client_version_string);
}

/* defaults to 'no' */
static int
confirm(const char *prompt)
{
	const char *msg, *again = "Please type 'yes' or 'no': ";
	char *p;
	int ret = -1;

	if (options.batch_mode)
		return 0;
	for (msg = prompt;;msg = again) {
		p = read_passphrase(msg, RP_ECHO);
		if (p == NULL ||
		    (p[0] == '\0') || (p[0] == '\n') ||
		    strncasecmp(p, "no", 2) == 0)
			ret = 0;
		if (strncasecmp(p, "yes", 3) == 0)
			ret = 1;
		if (p)
			xfree(p);
		if (ret != -1)
			return ret;
	}
}

/*
 * check whether the supplied host key is valid, return -1 if the key
 * is not valid. the user_hostfile will not be updated if 'readonly' is true.
 */

static int
check_host_key(char *host, struct sockaddr *hostaddr, Key *host_key,
    int readonly, const char *user_hostfile, const char *system_hostfile)
{
	Key *file_key;
	char *type = key_type(host_key);
	char *ip = NULL;
	char hostline[1000], *hostp, *fp;
	HostStatus host_status;
	HostStatus ip_status;
	int local = 0, host_ip_differ = 0;
	char ntop[NI_MAXHOST];
	char msg[1024];
	int len, host_line, ip_line;
	const char *host_file = NULL, *ip_file = NULL;

	/*
	 * Force accepting of the host key for loopback/localhost. The
	 * problem is that if the home directory is NFS-mounted to multiple
	 * machines, localhost will refer to a different machine in each of
	 * them, and the user will get bogus HOST_CHANGED warnings.  This
	 * essentially disables host authentication for localhost; however,
	 * this is probably not a real problem.
	 */
	/**  hostaddr == 0! */
	switch (hostaddr->sa_family) {
	case AF_INET:
		local = (ntohl(((struct sockaddr_in *)hostaddr)->
		   sin_addr.s_addr) >> 24) == IN_LOOPBACKNET;
		break;
	case AF_INET6:
		local = IN6_IS_ADDR_LOOPBACK(
		    &(((struct sockaddr_in6 *)hostaddr)->sin6_addr));
		break;
	default:
		local = 0;
		break;
	}
	if (options.no_host_authentication_for_localhost == 1 && local &&
	    options.host_key_alias == NULL) {
		debug("Forcing accepting of host key for "
		    "loopback/localhost.");
		return 0;
	}

	/*
	 * We don't have the remote ip-address for connections
	 * using a proxy command
	 */
	if (options.proxy_command == NULL) {
		if (getnameinfo(hostaddr, hostaddr->sa_len, ntop, sizeof(ntop),
		    NULL, 0, NI_NUMERICHOST) != 0)
			fatal("check_host_key: getnameinfo failed");
		ip = xstrdup(ntop);
	} else {
		ip = xstrdup("<no hostip for proxy command>");
	}
	/*
	 * Turn off check_host_ip if the connection is to localhost, via proxy
	 * command or if we don't have a hostname to compare with
	 */
	if (options.check_host_ip &&
	    (local || strcmp(host, ip) == 0 || options.proxy_command != NULL))
		options.check_host_ip = 0;

	/*
	 * Allow the user to record the key under a different name. This is
	 * useful for ssh tunneling over forwarded connections or if you run
	 * multiple sshd's on different ports on the same machine.
	 */
	if (options.host_key_alias != NULL) {
		host = options.host_key_alias;
		debug("using hostkeyalias: %s", host);
	}

	/*
	 * Store the host key from the known host file in here so that we can
	 * compare it with the key for the IP address.
	 */
	file_key = key_new(host_key->type);

	/*
	 * Check if the host key is present in the user\'s list of known
	 * hosts or in the systemwide list.
	 */
	host_file = user_hostfile;
	host_status = check_host_in_hostfile(host_file, host, host_key,
	    file_key, &host_line);
	if (host_status == HOST_NEW) {
		host_file = system_hostfile;
		host_status = check_host_in_hostfile(host_file, host, host_key,
		    file_key, &host_line);
	}
	/*
	 * Also perform check for the ip address, skip the check if we are
	 * localhost or the hostname was an ip address to begin with
	 */
	if (options.check_host_ip) {
		Key *ip_key = key_new(host_key->type);

		ip_file = user_hostfile;
		ip_status = check_host_in_hostfile(ip_file, ip, host_key,
		    ip_key, &ip_line);
		if (ip_status == HOST_NEW) {
			ip_file = system_hostfile;
			ip_status = check_host_in_hostfile(ip_file, ip,
			    host_key, ip_key, &ip_line);
		}
		if (host_status == HOST_CHANGED &&
		    (ip_status != HOST_CHANGED || !key_equal(ip_key, file_key)))
			host_ip_differ = 1;

		key_free(ip_key);
	} else
		ip_status = host_status;

	key_free(file_key);

	switch (host_status) {
	case HOST_OK:
		/* The host is known and the key matches. */
		debug("Host '%.200s' is known and matches the %s host key.",
		    host, type);
		debug("Found key in %s:%d", host_file, host_line);
		if (options.check_host_ip && ip_status == HOST_NEW) {
			if (readonly)
				log("%s host key for IP address "
				    "'%.128s' not in list of known hosts.",
				    type, ip);
			else if (!add_host_to_hostfile(user_hostfile, ip,
			    host_key))
				log("Failed to add the %s host key for IP "
				    "address '%.128s' to the list of known "
				    "hosts (%.30s).", type, ip, user_hostfile);
			else
				log("Warning: Permanently added the %s host "
				    "key for IP address '%.128s' to the list "
				    "of known hosts.", type, ip);
		}
		break;
	case HOST_NEW:
		if (readonly)
			goto fail;
		/* The host is new. */
		if (options.strict_host_key_checking == 1) {
			/*
			 * User has requested strict host key checking.  We
			 * will not add the host key automatically.  The only
			 * alternative left is to abort.
			 */
			error("No %s host key is known for %.200s and you "
			    "have requested strict checking.", type, host);
			goto fail;
		} else if (options.strict_host_key_checking == 2) {
			/* The default */
			fp = key_fingerprint(host_key, SSH_FP_MD5, SSH_FP_HEX);
			snprintf(msg, sizeof(msg),
			    "The authenticity of host '%.200s (%s)' can't be "
			    "established.\n"
			    "%s key fingerprint is %s.\n"
			    "Are you sure you want to continue connecting "
			    "(yes/no)? ", host, ip, type, fp);
			xfree(fp);
			if (!confirm(msg))
				goto fail;
		}
		if (options.check_host_ip && ip_status == HOST_NEW) {
			snprintf(hostline, sizeof(hostline), "%s,%s", host, ip);
			hostp = hostline;
		} else
			hostp = host;

		/*
		 * If not in strict mode, add the key automatically to the
		 * local known_hosts file.
		 */
		if (!add_host_to_hostfile(user_hostfile, hostp, host_key))
			log("Failed to add the host to the list of known "
			    "hosts (%.500s).", user_hostfile);
		else
			log("Warning: Permanently added '%.200s' (%s) to the "
			    "list of known hosts.", hostp, type);
		break;
	case HOST_CHANGED:
		if (options.check_host_ip && host_ip_differ) {
			char *msg;
			if (ip_status == HOST_NEW)
				msg = "is unknown";
			else if (ip_status == HOST_OK)
				msg = "is unchanged";
			else
				msg = "has a different value";
			error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			error("@       WARNING: POSSIBLE DNS SPOOFING DETECTED!          @");
			error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			error("The %s host key for %s has changed,", type, host);
			error("and the key for the according IP address %s", ip);
			error("%s. This could either mean that", msg);
			error("DNS SPOOFING is happening or the IP address for the host");
			error("and its host key have changed at the same time.");
			if (ip_status != HOST_NEW)
				error("Offending key for IP in %s:%d", ip_file, ip_line);
		}
		/* The host key has changed. */
		fp = key_fingerprint(host_key, SSH_FP_MD5, SSH_FP_HEX);
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @");
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!");
		error("Someone could be eavesdropping on you right now (man-in-the-middle attack)!");
		error("It is also possible that the %s host key has just been changed.", type);
		error("The fingerprint for the %s key sent by the remote host is\n%s.",
		    type, fp);
		error("Please contact your system administrator.");
		error("Add correct host key in %.100s to get rid of this message.",
		    user_hostfile);
		error("Offending key in %s:%d", host_file, host_line);
		xfree(fp);

		/*
		 * If strict host key checking is in use, the user will have
		 * to edit the key manually and we can only abort.
		 */
		if (options.strict_host_key_checking) {
			error("%s host key for %.200s has changed and you have "
			    "requested strict checking.", type, host);
			goto fail;
		}

		/*
		 * If strict host key checking has not been requested, allow
		 * the connection but without password authentication or
		 * agent forwarding.
		 */
		if (options.password_authentication) {
			error("Password authentication is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.password_authentication = 0;
		}
		if (options.forward_agent) {
			error("Agent forwarding is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.forward_agent = 0;
		}
		if (options.forward_x11) {
			error("X11 forwarding is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.forward_x11 = 0;
		}
		if (options.num_local_forwards > 0 ||
		    options.num_remote_forwards > 0) {
			error("Port forwarding is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.num_local_forwards =
			    options.num_remote_forwards = 0;
		}
		/*
		 * XXX Should permit the user to change to use the new id.
		 * This could be done by converting the host key to an
		 * identifying sentence, tell that the host identifies itself
		 * by that sentence, and ask the user if he/she whishes to
		 * accept the authentication.
		 */
		break;
	}

	if (options.check_host_ip && host_status != HOST_CHANGED &&
	    ip_status == HOST_CHANGED) {
		snprintf(msg, sizeof(msg),
		    "Warning: the %s host key for '%.200s' "
		    "differs from the key for the IP address '%.128s'"
		    "\nOffending key for IP in %s:%d",
		    type, host, ip, ip_file, ip_line);
		if (host_status == HOST_OK) {
			len = strlen(msg);
			snprintf(msg + len, sizeof(msg) - len,
			    "\nMatching host key in %s:%d",
			     host_file, host_line);
		}
		if (options.strict_host_key_checking == 1) {
			log(msg);
			error("Exiting, you have requested strict checking.");
			goto fail;
		} else if (options.strict_host_key_checking == 2) {
			strlcat(msg, "\nAre you sure you want "
			    "to continue connecting (yes/no)? ", sizeof(msg));
			if (!confirm(msg))
				goto fail;
		} else {
			log(msg);
		}
	}

	xfree(ip);
	return 0;

fail:
	xfree(ip);
	return -1;
}

int
verify_host_key(char *host, struct sockaddr *hostaddr, Key *host_key)
{
	struct stat st;

	/* return ok if the key can be found in an old keyfile */
	if (stat(options.system_hostfile2, &st) == 0 ||
	    stat(options.user_hostfile2, &st) == 0) {
		if (check_host_key(host, hostaddr, host_key, /*readonly*/ 1,
		    options.user_hostfile2, options.system_hostfile2) == 0)
			return 0;
	}
	return check_host_key(host, hostaddr, host_key, /*readonly*/ 0,
	    options.user_hostfile, options.system_hostfile);
}

/*
 * Starts a dialog with the server, and authenticates the current user on the
 * server.  This does not need any extra privileges.  The basic connection
 * to the server must already have been established before this is called.
 * If login fails, this function prints an error and never returns.
 * This function does not require super-user privileges.
 */
void
ssh_login(Key **keys, int nkeys, const char *orighost,
    struct sockaddr *hostaddr, struct passwd *pw)
{
	char *host, *cp;
	char *server_user, *local_user;

	local_user = xstrdup(pw->pw_name);
	server_user = options.user ? options.user : local_user;

	/* Convert the user-supplied hostname into all lowercase. */
	host = xstrdup(orighost);
	for (cp = host; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);

	/* Exchange protocol version identification strings with the server. */
	ssh_exchange_identification();

	/* Put the connection into non-blocking mode. */
	packet_set_nonblocking();

	/* key exchange */
	/* authenticate user */
	if (compat20) {
		ssh_kex2(host, hostaddr);
		ssh_userauth2(local_user, server_user, host, keys, nkeys);
	} else {
		ssh_kex(host, hostaddr);
		ssh_userauth1(local_user, server_user, host, keys, nkeys);
	}
}

void
ssh_put_password(char *password)
{
	int size;
	char *padded;

	if (datafellows & SSH_BUG_PASSWORDPAD) {
		packet_put_cstring(password);
		return;
	}
	size = roundup(strlen(password) + 1, 32);
	padded = xmalloc(size);
	memset(padded, 0, size);
	strlcpy(padded, password, size);
	packet_put_string(padded, size);
	memset(padded, 0, size);
	xfree(padded);
}
