/*	$NetBSD: readconf.c,v 1.15.2.1 2002/06/26 16:53:39 tv Exp $	*/
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for reading the configuration files.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: readconf.c,v 1.100 2002/06/19 00:27:55 deraadt Exp $");

#include "ssh.h"
#include "xmalloc.h"
#include "compat.h"
#include "cipher.h"
#include "pathnames.h"
#include "log.h"
#include "readconf.h"
#include "match.h"
#include "misc.h"
#include "kex.h"
#include "mac.h"

/* Format of the configuration file:

   # Configuration data is parsed as follows:
   #  1. command line options
   #  2. user-specific file
   #  3. system-wide file
   # Any configuration value is only changed the first time it is set.
   # Thus, host-specific definitions should be at the beginning of the
   # configuration file, and defaults at the end.

   # Host-specific declarations.  These may override anything above.  A single
   # host may match multiple declarations; these are processed in the order
   # that they are given in.

   Host *.ngs.fi ngs.fi
     User foo

   Host fake.com
     HostName another.host.name.real.org
     User blaah
     Port 34289
     ForwardX11 no
     ForwardAgent no

   Host books.com
     RemoteForward 9999 shadows.cs.hut.fi:9999
     Cipher 3des

   Host fascist.blob.com
     Port 23123
     User tylonen
     RhostsAuthentication no
     PasswordAuthentication no

   Host puukko.hut.fi
     User t35124p
     ProxyCommand ssh-proxy %h %p

   Host *.fr
     PublicKeyAuthentication no

   Host *.su
     Cipher none
     PasswordAuthentication no

   # Defaults for various options
   Host *
     ForwardAgent no
     ForwardX11 no
     RhostsAuthentication yes
     PasswordAuthentication yes
     RSAAuthentication yes
     RhostsRSAAuthentication yes
     StrictHostKeyChecking yes
     KeepAlives no
     IdentityFile ~/.ssh/identity
     Port 22
     EscapeChar ~

*/

/* Keyword tokens. */

typedef enum {
	oBadOption,
	oForwardAgent, oForwardX11, oGatewayPorts, oRhostsAuthentication,
	oPasswordAuthentication, oRSAAuthentication,
	oChallengeResponseAuthentication, oXAuthLocation,
#if defined(KRB4) || defined(KRB5)
	oKerberosAuthentication,
#endif
#if defined(AFS) || defined(KRB5)
	oKerberosTgtPassing,
#endif
#ifdef AFS
	oAFSTokenPassing,
#endif
	oIdentityFile, oHostName, oPort, oCipher, oRemoteForward, oLocalForward,
	oUser, oHost, oEscapeChar, oRhostsRSAAuthentication, oProxyCommand,
	oGlobalKnownHostsFile, oUserKnownHostsFile, oConnectionAttempts,
	oBatchMode, oCheckHostIP, oStrictHostKeyChecking, oCompression,
	oCompressionLevel, oKeepAlives, oNumberOfPasswordPrompts,
	oUsePrivilegedPort, oLogLevel, oCiphers, oProtocol, oMacs,
	oGlobalKnownHostsFile2, oUserKnownHostsFile2, oPubkeyAuthentication,
	oKbdInteractiveAuthentication, oKbdInteractiveDevices, oHostKeyAlias,
	oDynamicForward, oPreferredAuthentications, oHostbasedAuthentication,
	oHostKeyAlgorithms, oBindAddress, oSmartcardDevice,
	oClearAllForwardings, oNoHostAuthenticationForLocalhost,
	oDeprecated
} OpCodes;

/* Textual representations of the tokens. */

static struct {
	const char *name;
	OpCodes opcode;
} keywords[] = {
	{ "forwardagent", oForwardAgent },
	{ "forwardx11", oForwardX11 },
	{ "xauthlocation", oXAuthLocation },
	{ "gatewayports", oGatewayPorts },
	{ "useprivilegedport", oUsePrivilegedPort },
	{ "rhostsauthentication", oRhostsAuthentication },
	{ "passwordauthentication", oPasswordAuthentication },
	{ "kbdinteractiveauthentication", oKbdInteractiveAuthentication },
	{ "kbdinteractivedevices", oKbdInteractiveDevices },
	{ "rsaauthentication", oRSAAuthentication },
	{ "pubkeyauthentication", oPubkeyAuthentication },
	{ "dsaauthentication", oPubkeyAuthentication },		    /* alias */
	{ "rhostsrsaauthentication", oRhostsRSAAuthentication },
	{ "hostbasedauthentication", oHostbasedAuthentication },
	{ "challengeresponseauthentication", oChallengeResponseAuthentication },
	{ "skeyauthentication", oChallengeResponseAuthentication }, /* alias */
	{ "tisauthentication", oChallengeResponseAuthentication },  /* alias */
#if defined(KRB4) || defined(KRB5)
	{ "kerberosauthentication", oKerberosAuthentication },
#endif
#if defined(AFS) || defined(KRB5)
	{ "kerberostgtpassing", oKerberosTgtPassing },
	{ "kerberos5tgtpassing", oKerberosTgtPassing },		/* alias */
	{ "kerberos4tgtpassing", oKerberosTgtPassing },		/* alias */
#endif
#ifdef AFS
	{ "afstokenpassing", oAFSTokenPassing },
#endif
	{ "fallbacktorsh", oDeprecated },
	{ "usersh", oDeprecated },
	{ "identityfile", oIdentityFile },
	{ "identityfile2", oIdentityFile },			/* alias */
	{ "hostname", oHostName },
	{ "hostkeyalias", oHostKeyAlias },
	{ "proxycommand", oProxyCommand },
	{ "port", oPort },
	{ "cipher", oCipher },
	{ "ciphers", oCiphers },
	{ "macs", oMacs },
	{ "protocol", oProtocol },
	{ "remoteforward", oRemoteForward },
	{ "localforward", oLocalForward },
	{ "user", oUser },
	{ "host", oHost },
	{ "escapechar", oEscapeChar },
	{ "globalknownhostsfile", oGlobalKnownHostsFile },
	{ "userknownhostsfile", oUserKnownHostsFile },		/* obsolete */
	{ "globalknownhostsfile2", oGlobalKnownHostsFile2 },
	{ "userknownhostsfile2", oUserKnownHostsFile2 },	/* obsolete */
	{ "connectionattempts", oConnectionAttempts },
	{ "batchmode", oBatchMode },
	{ "checkhostip", oCheckHostIP },
	{ "stricthostkeychecking", oStrictHostKeyChecking },
	{ "compression", oCompression },
	{ "compressionlevel", oCompressionLevel },
	{ "keepalive", oKeepAlives },
	{ "numberofpasswordprompts", oNumberOfPasswordPrompts },
	{ "loglevel", oLogLevel },
	{ "dynamicforward", oDynamicForward },
	{ "preferredauthentications", oPreferredAuthentications },
	{ "hostkeyalgorithms", oHostKeyAlgorithms },
	{ "bindaddress", oBindAddress },
	{ "smartcarddevice", oSmartcardDevice },
	{ "clearallforwardings", oClearAllForwardings },
	{ "nohostauthenticationforlocalhost", oNoHostAuthenticationForLocalhost },
	{ NULL, oBadOption }
};

/*
 * Adds a local TCP/IP port forward to options.  Never returns if there is an
 * error.
 */

void
add_local_forward(Options *options, u_short port, const char *host,
		  u_short host_port)
{
	Forward *fwd;
	extern uid_t original_real_uid;
	if (port < IPPORT_RESERVED && original_real_uid != 0)
		fatal("Privileged ports can only be forwarded by root.");
	if (options->num_local_forwards >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("Too many local forwards (max %d).", SSH_MAX_FORWARDS_PER_DIRECTION);
	fwd = &options->local_forwards[options->num_local_forwards++];
	fwd->port = port;
	fwd->host = xstrdup(host);
	fwd->host_port = host_port;
}

/*
 * Adds a remote TCP/IP port forward to options.  Never returns if there is
 * an error.
 */

void
add_remote_forward(Options *options, u_short port, const char *host,
		   u_short host_port)
{
	Forward *fwd;
	if (options->num_remote_forwards >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("Too many remote forwards (max %d).",
		    SSH_MAX_FORWARDS_PER_DIRECTION);
	fwd = &options->remote_forwards[options->num_remote_forwards++];
	fwd->port = port;
	fwd->host = xstrdup(host);
	fwd->host_port = host_port;
}

static void
clear_forwardings(Options *options)
{
	int i;

	for (i = 0; i < options->num_local_forwards; i++)
		xfree(options->local_forwards[i].host);
	options->num_local_forwards = 0;
	for (i = 0; i < options->num_remote_forwards; i++)
		xfree(options->remote_forwards[i].host);
	options->num_remote_forwards = 0;
}

/*
 * Returns the number of the token pointed to by cp or oBadOption.
 */

static OpCodes
parse_token(const char *cp, const char *filename, int linenum)
{
	u_int i;

	for (i = 0; keywords[i].name; i++)
		if (strcasecmp(cp, keywords[i].name) == 0)
			return keywords[i].opcode;

	error("%s: line %d: Bad configuration option: %s",
	    filename, linenum, cp);
	return oBadOption;
}

/*
 * Processes a single option line as used in the configuration files. This
 * only sets those values that have not already been set.
 */

int
process_config_line(Options *options, const char *host,
		    char *line, const char *filename, int linenum,
		    int *activep)
{
	char buf[256], *s, *string, **charptr, *endofnumber, *keyword, *arg;
	int opcode, *intptr, value;
	u_short fwd_port, fwd_host_port;
	char sfwd_host_port[6];

	s = line;
	/* Get the keyword. (Each line is supposed to begin with a keyword). */
	keyword = strdelim(&s);
	/* Ignore leading whitespace. */
	if (*keyword == '\0')
		keyword = strdelim(&s);
	if (keyword == NULL || !*keyword || *keyword == '\n' || *keyword == '#')
		return 0;

	opcode = parse_token(keyword, filename, linenum);

	switch (opcode) {
	case oBadOption:
		/* don't panic, but count bad options */
		return -1;
		/* NOTREACHED */
	case oForwardAgent:
		intptr = &options->forward_agent;
parse_flag:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing yes/no argument.", filename, linenum);
		value = 0;	/* To avoid compiler warning... */
		if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = 0;
		else
			fatal("%.200s line %d: Bad yes/no argument.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oForwardX11:
		intptr = &options->forward_x11;
		goto parse_flag;

	case oGatewayPorts:
		intptr = &options->gateway_ports;
		goto parse_flag;

	case oUsePrivilegedPort:
		intptr = &options->use_privileged_port;
		goto parse_flag;

	case oRhostsAuthentication:
		intptr = &options->rhosts_authentication;
		goto parse_flag;

	case oPasswordAuthentication:
		intptr = &options->password_authentication;
		goto parse_flag;

	case oKbdInteractiveAuthentication:
		intptr = &options->kbd_interactive_authentication;
		goto parse_flag;

	case oKbdInteractiveDevices:
		charptr = &options->kbd_interactive_devices;
		goto parse_string;

	case oPubkeyAuthentication:
		intptr = &options->pubkey_authentication;
		goto parse_flag;

	case oRSAAuthentication:
		intptr = &options->rsa_authentication;
		goto parse_flag;

	case oRhostsRSAAuthentication:
		intptr = &options->rhosts_rsa_authentication;
		goto parse_flag;

	case oHostbasedAuthentication:
		intptr = &options->hostbased_authentication;
		goto parse_flag;

	case oChallengeResponseAuthentication:
		intptr = &options->challenge_response_authentication;
		goto parse_flag;
#if defined(KRB4) || defined(KRB5)
	case oKerberosAuthentication:
		intptr = &options->kerberos_authentication;
		goto parse_flag;
#endif
#if defined(AFS) || defined(KRB5)
	case oKerberosTgtPassing:
		intptr = &options->kerberos_tgt_passing;
		goto parse_flag;
#endif
#ifdef AFS
	case oAFSTokenPassing:
		intptr = &options->afs_token_passing;
		goto parse_flag;
#endif
	case oBatchMode:
		intptr = &options->batch_mode;
		goto parse_flag;

	case oCheckHostIP:
		intptr = &options->check_host_ip;
		goto parse_flag;

	case oStrictHostKeyChecking:
		intptr = &options->strict_host_key_checking;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing yes/no/ask argument.",
			    filename, linenum);
		value = 0;	/* To avoid compiler warning... */
		if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = 0;
		else if (strcmp(arg, "ask") == 0)
			value = 2;
		else
			fatal("%.200s line %d: Bad yes/no/ask argument.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oCompression:
		intptr = &options->compression;
		goto parse_flag;

	case oKeepAlives:
		intptr = &options->keepalives;
		goto parse_flag;

	case oNoHostAuthenticationForLocalhost:
		intptr = &options->no_host_authentication_for_localhost;
		goto parse_flag;

	case oNumberOfPasswordPrompts:
		intptr = &options->number_of_password_prompts;
		goto parse_int;

	case oCompressionLevel:
		intptr = &options->compression_level;
		goto parse_int;

	case oIdentityFile:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (*activep) {
			intptr = &options->num_identity_files;
			if (*intptr >= SSH_MAX_IDENTITY_FILES)
				fatal("%.200s line %d: Too many identity files specified (max %d).",
				    filename, linenum, SSH_MAX_IDENTITY_FILES);
			charptr =  &options->identity_files[*intptr];
			*charptr = xstrdup(arg);
			*intptr = *intptr + 1;
		}
		break;

	case oXAuthLocation:
		charptr=&options->xauth_location;
		goto parse_string;

	case oUser:
		charptr = &options->user;
parse_string:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case oGlobalKnownHostsFile:
		charptr = &options->system_hostfile;
		goto parse_string;

	case oUserKnownHostsFile:
		charptr = &options->user_hostfile;
		goto parse_string;

	case oGlobalKnownHostsFile2:
		charptr = &options->system_hostfile2;
		goto parse_string;

	case oUserKnownHostsFile2:
		charptr = &options->user_hostfile2;
		goto parse_string;

	case oHostName:
		charptr = &options->hostname;
		goto parse_string;

	case oHostKeyAlias:
		charptr = &options->host_key_alias;
		goto parse_string;

	case oPreferredAuthentications:
		charptr = &options->preferred_authentications;
		goto parse_string;

	case oBindAddress:
		charptr = &options->bind_address;
		goto parse_string;

	case oSmartcardDevice:
		charptr = &options->smartcard_device;
		goto parse_string;

	case oProxyCommand:
		charptr = &options->proxy_command;
		string = xstrdup("");
		while ((arg = strdelim(&s)) != NULL && *arg != '\0') {
			string = xrealloc(string, strlen(string) + strlen(arg) + 2);
			strcat(string, " ");
			strcat(string, arg);
		}
		if (*activep && *charptr == NULL)
			*charptr = string;
		else
			xfree(string);
		return 0;

	case oPort:
		intptr = &options->port;
parse_int:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] < '0' || arg[0] > '9')
			fatal("%.200s line %d: Bad number.", filename, linenum);

		/* Octal, decimal, or hex format? */
		value = strtol(arg, &endofnumber, 0);
		if (arg == endofnumber)
			fatal("%.200s line %d: Bad number.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oConnectionAttempts:
		intptr = &options->connection_attempts;
		goto parse_int;

	case oCipher:
		intptr = &options->cipher;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		value = cipher_number(arg);
		if (value == -1)
			fatal("%.200s line %d: Bad cipher '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oCiphers:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!ciphers_valid(arg))
			fatal("%.200s line %d: Bad SSH2 cipher spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->ciphers == NULL)
			options->ciphers = xstrdup(arg);
		break;

	case oMacs:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!mac_valid(arg))
			fatal("%.200s line %d: Bad SSH2 Mac spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->macs == NULL)
			options->macs = xstrdup(arg);
		break;

	case oHostKeyAlgorithms:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!key_names_valid2(arg))
			fatal("%.200s line %d: Bad protocol 2 host key algorithms '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->hostkeyalgorithms == NULL)
			options->hostkeyalgorithms = xstrdup(arg);
		break;

	case oProtocol:
		intptr = &options->protocol;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		value = proto_spec(arg);
		if (value == SSH_PROTO_UNKNOWN)
			fatal("%.200s line %d: Bad protocol spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *intptr == SSH_PROTO_UNKNOWN)
			*intptr = value;
		break;

	case oLogLevel:
		intptr = (int *) &options->log_level;
		arg = strdelim(&s);
		value = log_level_number(arg);
		if (value == SYSLOG_LEVEL_NOT_SET)
			fatal("%.200s line %d: unsupported log level '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && (LogLevel) *intptr == SYSLOG_LEVEL_NOT_SET)
			*intptr = (LogLevel) value;
		break;

	case oLocalForward:
	case oRemoteForward:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing port argument.",
			    filename, linenum);
		if ((fwd_port = a2port(arg)) == 0)
			fatal("%.200s line %d: Bad listen port.",
			    filename, linenum);
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing second argument.",
			    filename, linenum);
		if (sscanf(arg, "%255[^:]:%5[0-9]", buf, sfwd_host_port) != 2 &&
		    sscanf(arg, "%255[^/]/%5[0-9]", buf, sfwd_host_port) != 2)
			fatal("%.200s line %d: Bad forwarding specification.",
			    filename, linenum);
		if ((fwd_host_port = a2port(sfwd_host_port)) == 0)
			fatal("%.200s line %d: Bad forwarding port.",
			    filename, linenum);
		if (*activep) {
			if (opcode == oLocalForward)
				add_local_forward(options, fwd_port, buf,
				    fwd_host_port);
			else if (opcode == oRemoteForward)
				add_remote_forward(options, fwd_port, buf,
				    fwd_host_port);
		}
		break;

	case oDynamicForward:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing port argument.",
			    filename, linenum);
		fwd_port = a2port(arg);
		if (fwd_port == 0)
			fatal("%.200s line %d: Badly formatted port number.",
			    filename, linenum);
		if (*activep)
			add_local_forward(options, fwd_port, "socks4", 0);
		break;

	case oClearAllForwardings:
		intptr = &options->clear_forwardings;
		goto parse_flag;

	case oHost:
		*activep = 0;
		while ((arg = strdelim(&s)) != NULL && *arg != '\0')
			if (match_pattern(host, arg)) {
				debug("Applying options for %.100s", arg);
				*activep = 1;
				break;
			}
		/* Avoid garbage check below, as strdelim is done. */
		return 0;

	case oEscapeChar:
		intptr = &options->escape_char;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] == '^' && arg[2] == 0 &&
		    (u_char) arg[1] >= 64 && (u_char) arg[1] < 128)
			value = (u_char) arg[1] & 31;
		else if (strlen(arg) == 1)
			value = (u_char) arg[0];
		else if (strcmp(arg, "none") == 0)
			value = SSH_ESCAPECHAR_NONE;
		else {
			fatal("%.200s line %d: Bad escape character.",
			    filename, linenum);
			/* NOTREACHED */
			value = 0;	/* Avoid compiler warning. */
		}
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oDeprecated:
		debug("%s line %d: Deprecated option \"%s\"",
		    filename, linenum, keyword);
		return 0;

	default:
		fatal("process_config_line: Unimplemented opcode %d", opcode);
	}

	/* Check that there is no garbage at end of line. */
	if ((arg = strdelim(&s)) != NULL && *arg != '\0') {
		fatal("%.200s line %d: garbage at end of line; \"%.200s\".",
		     filename, linenum, arg);
	}
	return 0;
}


/*
 * Reads the config file and modifies the options accordingly.  Options
 * should already be initialized before this call.  This never returns if
 * there is an error.  If the file does not exist, this returns 0.
 */

int
read_config_file(const char *filename, const char *host, Options *options)
{
	FILE *f;
	char line[1024];
	int active, linenum;
	int bad_options = 0;

	/* Open the file. */
	f = fopen(filename, "r");
	if (!f)
		return 0;

	debug("Reading configuration data %.200s", filename);

	/*
	 * Mark that we are now processing the options.  This flag is turned
	 * on/off by Host specifications.
	 */
	active = 1;
	linenum = 0;
	while (fgets(line, sizeof(line), f)) {
		/* Update line number counter. */
		linenum++;
		if (process_config_line(options, host, line, filename, linenum, &active) != 0)
			bad_options++;
	}
	fclose(f);
	if (bad_options > 0)
		fatal("%s: terminating, %d bad configuration options",
		    filename, bad_options);
	return 1;
}

/*
 * Initializes options to special values that indicate that they have not yet
 * been set.  Read_config_file will only set options with this value. Options
 * are processed in the following order: command line, user config file,
 * system config file.  Last, fill_default_options is called.
 */

void
initialize_options(Options * options)
{
	memset(options, 'X', sizeof(*options));
	options->forward_agent = -1;
	options->forward_x11 = -1;
	options->xauth_location = NULL;
	options->gateway_ports = -1;
	options->use_privileged_port = -1;
	options->rhosts_authentication = -1;
	options->rsa_authentication = -1;
	options->pubkey_authentication = -1;
	options->challenge_response_authentication = -1;
#if defined(KRB4) || defined(KRB5)
	options->kerberos_authentication = -1;
#endif
#if defined(AFS) || defined(KRB5)
	options->kerberos_tgt_passing = -1;
#endif
#ifdef AFS
	options->afs_token_passing = -1;
#endif
	options->password_authentication = -1;
	options->kbd_interactive_authentication = -1;
	options->kbd_interactive_devices = NULL;
	options->rhosts_rsa_authentication = -1;
	options->hostbased_authentication = -1;
	options->batch_mode = -1;
	options->check_host_ip = -1;
	options->strict_host_key_checking = -1;
	options->compression = -1;
	options->keepalives = -1;
	options->compression_level = -1;
	options->port = -1;
	options->connection_attempts = -1;
	options->number_of_password_prompts = -1;
	options->cipher = -1;
	options->ciphers = NULL;
	options->macs = NULL;
	options->hostkeyalgorithms = NULL;
	options->protocol = SSH_PROTO_UNKNOWN;
	options->num_identity_files = 0;
	options->hostname = NULL;
	options->host_key_alias = NULL;
	options->proxy_command = NULL;
	options->user = NULL;
	options->escape_char = -1;
	options->system_hostfile = NULL;
	options->user_hostfile = NULL;
	options->system_hostfile2 = NULL;
	options->user_hostfile2 = NULL;
	options->num_local_forwards = 0;
	options->num_remote_forwards = 0;
	options->clear_forwardings = -1;
	options->log_level = SYSLOG_LEVEL_NOT_SET;
	options->preferred_authentications = NULL;
	options->bind_address = NULL;
	options->smartcard_device = NULL;
	options->no_host_authentication_for_localhost = - 1;
}

/*
 * Called after processing other sources of option data, this fills those
 * options for which no value has been specified with their default values.
 */

void
fill_default_options(Options * options)
{
	int len;

	if (options->forward_agent == -1)
		options->forward_agent = 0;
	if (options->forward_x11 == -1)
		options->forward_x11 = 0;
	if (options->xauth_location == NULL)
		options->xauth_location = _PATH_XAUTH;
	if (options->gateway_ports == -1)
		options->gateway_ports = 0;
	if (options->use_privileged_port == -1)
		options->use_privileged_port = 0;
	if (options->rhosts_authentication == -1)
		options->rhosts_authentication = 0;
	if (options->rsa_authentication == -1)
		options->rsa_authentication = 1;
	if (options->pubkey_authentication == -1)
		options->pubkey_authentication = 1;
	if (options->challenge_response_authentication == -1)
		options->challenge_response_authentication = 1;
#if defined(KRB4) || defined(KRB5)
	if (options->kerberos_authentication == -1)
		options->kerberos_authentication = 1;
#endif
#if defined(AFS) || defined(KRB5)
	if (options->kerberos_tgt_passing == -1)
		options->kerberos_tgt_passing = 1;
#endif
#ifdef AFS
	if (options->afs_token_passing == -1)
		options->afs_token_passing = 1;
#endif
	if (options->password_authentication == -1)
		options->password_authentication = 1;
	if (options->kbd_interactive_authentication == -1)
		options->kbd_interactive_authentication = 1;
	if (options->rhosts_rsa_authentication == -1)
		options->rhosts_rsa_authentication = 0;
	if (options->hostbased_authentication == -1)
		options->hostbased_authentication = 0;
	if (options->batch_mode == -1)
		options->batch_mode = 0;
	if (options->check_host_ip == -1)
		options->check_host_ip = 1;
	if (options->strict_host_key_checking == -1)
		options->strict_host_key_checking = 2;	/* 2 is default */
	if (options->compression == -1)
		options->compression = 0;
	if (options->keepalives == -1)
		options->keepalives = 1;
	if (options->compression_level == -1)
		options->compression_level = 6;
	if (options->port == -1)
		options->port = 0;	/* Filled in ssh_connect. */
	if (options->connection_attempts == -1)
		options->connection_attempts = 1;
	if (options->number_of_password_prompts == -1)
		options->number_of_password_prompts = 3;
	/* Selected in ssh_login(). */
	if (options->cipher == -1)
		options->cipher = SSH_CIPHER_NOT_SET;
	/* options->ciphers, default set in myproposals.h */
	/* options->macs, default set in myproposals.h */
	/* options->hostkeyalgorithms, default set in myproposals.h */
	if (options->protocol == SSH_PROTO_UNKNOWN)
		options->protocol = SSH_PROTO_1|SSH_PROTO_2;
	if (options->num_identity_files == 0) {
		if (options->protocol & SSH_PROTO_1) {
			len = 2 + strlen(_PATH_SSH_CLIENT_IDENTITY) + 1;
			options->identity_files[options->num_identity_files] =
			    xmalloc(len);
			snprintf(options->identity_files[options->num_identity_files++],
			    len, "~/%.100s", _PATH_SSH_CLIENT_IDENTITY);
		}
		if (options->protocol & SSH_PROTO_2) {
			len = 2 + strlen(_PATH_SSH_CLIENT_ID_RSA) + 1;
			options->identity_files[options->num_identity_files] =
			    xmalloc(len);
			snprintf(options->identity_files[options->num_identity_files++],
			    len, "~/%.100s", _PATH_SSH_CLIENT_ID_RSA);

			len = 2 + strlen(_PATH_SSH_CLIENT_ID_DSA) + 1;
			options->identity_files[options->num_identity_files] =
			    xmalloc(len);
			snprintf(options->identity_files[options->num_identity_files++],
			    len, "~/%.100s", _PATH_SSH_CLIENT_ID_DSA);
		}
	}
	if (options->escape_char == -1)
		options->escape_char = '~';
	if (options->system_hostfile == NULL)
		options->system_hostfile = _PATH_SSH_SYSTEM_HOSTFILE;
	if (options->user_hostfile == NULL)
		options->user_hostfile = _PATH_SSH_USER_HOSTFILE;
	if (options->system_hostfile2 == NULL)
		options->system_hostfile2 = _PATH_SSH_SYSTEM_HOSTFILE2;
	if (options->user_hostfile2 == NULL)
		options->user_hostfile2 = _PATH_SSH_USER_HOSTFILE2;
	if (options->log_level == SYSLOG_LEVEL_NOT_SET)
		options->log_level = SYSLOG_LEVEL_INFO;
	if (options->clear_forwardings == 1)
		clear_forwardings(options);
	if (options->no_host_authentication_for_localhost == - 1)
		options->no_host_authentication_for_localhost = 0;
	/* options->proxy_command should not be set by default */
	/* options->user will be set in the main program if appropriate */
	/* options->hostname will be set in the main program if appropriate */
	/* options->host_key_alias should not be set by default */
	/* options->preferred_authentications will be set in ssh */
}
