/*
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: servconf.c,v 1.67 2001/02/12 16:16:23 markus Exp $");

#ifdef KRB4
#include <krb.h>
#endif
#ifdef AFS
#include <kafs.h>
#endif

#include "ssh.h"
#include "log.h"
#include "servconf.h"
#include "xmalloc.h"
#include "compat.h"
#include "pathnames.h"
#include "tildexpand.h"
#include "misc.h"
#include "cipher.h"
#include "kex.h"
#include "mac.h"

/* add listen address */
void add_listen_addr(ServerOptions *options, char *addr);

/* AF_UNSPEC or AF_INET or AF_INET6 */
extern int IPv4or6;

/* Initializes the server options to their default values. */

void
initialize_server_options(ServerOptions *options)
{
	memset(options, 0, sizeof(*options));
	options->num_ports = 0;
	options->ports_from_cmdline = 0;
	options->listen_addrs = NULL;
	options->num_host_key_files = 0;
	options->pid_file = NULL;
	options->server_key_bits = -1;
	options->login_grace_time = -1;
	options->key_regeneration_time = -1;
	options->permit_root_login = PERMIT_NOT_SET;
	options->ignore_rhosts = -1;
	options->ignore_root_rhosts = -1;
	options->ignore_user_known_hosts = -1;
	options->print_motd = -1;
	options->check_mail = -1;
	options->x11_forwarding = -1;
	options->x11_display_offset = -1;
	options->xauth_location = NULL;
	options->strict_modes = -1;
	options->keepalives = -1;
	options->log_facility = (SyslogFacility) - 1;
	options->log_level = (LogLevel) - 1;
	options->rhosts_authentication = -1;
	options->rhosts_rsa_authentication = -1;
	options->rsa_authentication = -1;
	options->pubkey_authentication = -1;
#ifdef KRB4
	options->kerberos_authentication = -1;
	options->kerberos_or_local_passwd = -1;
	options->kerberos_ticket_cleanup = -1;
#endif
#ifdef AFS
	options->kerberos_tgt_passing = -1;
	options->afs_token_passing = -1;
#endif
	options->password_authentication = -1;
	options->kbd_interactive_authentication = -1;
	options->challenge_reponse_authentication = -1;
	options->permit_empty_passwd = -1;
	options->use_login = -1;
	options->allow_tcp_forwarding = -1;
	options->num_allow_users = 0;
	options->num_deny_users = 0;
	options->num_allow_groups = 0;
	options->num_deny_groups = 0;
	options->ciphers = NULL;
	options->macs = NULL;
	options->protocol = SSH_PROTO_UNKNOWN;
	options->gateway_ports = -1;
	options->num_subsystems = 0;
	options->max_startups_begin = -1;
	options->max_startups_rate = -1;
	options->max_startups = -1;
	options->banner = NULL;
	options->reverse_mapping_check = -1;
}

void
fill_default_server_options(ServerOptions *options)
{
	if (options->protocol == SSH_PROTO_UNKNOWN)
		options->protocol = SSH_PROTO_1|SSH_PROTO_2;
	if (options->num_host_key_files == 0) {
		/* fill default hostkeys for protocols */
		if (options->protocol & SSH_PROTO_1)
			options->host_key_files[options->num_host_key_files++] = _PATH_HOST_KEY_FILE;
		if (options->protocol & SSH_PROTO_2)
			options->host_key_files[options->num_host_key_files++] = _PATH_HOST_DSA_KEY_FILE;
	}
	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	if (options->listen_addrs == NULL)
		add_listen_addr(options, NULL);
	if (options->pid_file == NULL)
		options->pid_file = _PATH_SSH_DAEMON_PID_FILE;
	if (options->server_key_bits == -1)
		options->server_key_bits = 768;
	if (options->login_grace_time == -1)
		options->login_grace_time = 600;
	if (options->key_regeneration_time == -1)
		options->key_regeneration_time = 3600;
	if (options->permit_root_login == PERMIT_NOT_SET)
		options->permit_root_login = PERMIT_YES;
	if (options->ignore_rhosts == -1)
		options->ignore_rhosts = 1;
	if (options->ignore_root_rhosts == -1)
		options->ignore_root_rhosts = options->ignore_rhosts;
	if (options->ignore_user_known_hosts == -1)
		options->ignore_user_known_hosts = 0;
	if (options->check_mail == -1)
		options->check_mail = 0;
	if (options->print_motd == -1)
		options->print_motd = 1;
	if (options->x11_forwarding == -1)
		options->x11_forwarding = 0;
	if (options->x11_display_offset == -1)
		options->x11_display_offset = 10;
#ifdef XAUTH_PATH
	if (options->xauth_location == NULL)
		options->xauth_location = XAUTH_PATH;
#endif /* XAUTH_PATH */
	if (options->strict_modes == -1)
		options->strict_modes = 1;
	if (options->keepalives == -1)
		options->keepalives = 1;
	if (options->log_facility == (SyslogFacility) (-1))
		options->log_facility = SYSLOG_FACILITY_AUTH;
	if (options->log_level == (LogLevel) (-1))
		options->log_level = SYSLOG_LEVEL_INFO;
	if (options->rhosts_authentication == -1)
		options->rhosts_authentication = 0;
	if (options->rhosts_rsa_authentication == -1)
		options->rhosts_rsa_authentication = 0;
	if (options->rsa_authentication == -1)
		options->rsa_authentication = 1;
	if (options->pubkey_authentication == -1)
		options->pubkey_authentication = 1;
#ifdef KRB4
	if (options->kerberos_authentication == -1)
		options->kerberos_authentication = (access(KEYFILE, R_OK) == 0);
	if (options->kerberos_or_local_passwd == -1)
		options->kerberos_or_local_passwd = 1;
	if (options->kerberos_ticket_cleanup == -1)
		options->kerberos_ticket_cleanup = 1;
#endif /* KRB4 */
#ifdef AFS
	if (options->kerberos_tgt_passing == -1)
		options->kerberos_tgt_passing = 0;
	if (options->afs_token_passing == -1)
		options->afs_token_passing = k_hasafs();
#endif /* AFS */
	if (options->password_authentication == -1)
		options->password_authentication = 1;
	if (options->kbd_interactive_authentication == -1)
		options->kbd_interactive_authentication = 0;
	if (options->challenge_reponse_authentication == -1)
		options->challenge_reponse_authentication = 1;
	if (options->permit_empty_passwd == -1)
		options->permit_empty_passwd = 0;
	if (options->use_login == -1)
		options->use_login = 0;
	if (options->allow_tcp_forwarding == -1)
		options->allow_tcp_forwarding = 1;
	if (options->gateway_ports == -1)
		options->gateway_ports = 0;
	if (options->max_startups == -1)
		options->max_startups = 10;
	if (options->max_startups_rate == -1)
		options->max_startups_rate = 100;		/* 100% */
	if (options->max_startups_begin == -1)
		options->max_startups_begin = options->max_startups;
	if (options->reverse_mapping_check == -1)
		options->reverse_mapping_check = 0;
}

/* Keyword tokens. */
typedef enum {
	sBadOption,		/* == unknown option */
	sPort, sHostKeyFile, sServerKeyBits, sLoginGraceTime, sKeyRegenerationTime,
	sPermitRootLogin, sLogFacility, sLogLevel,
	sRhostsAuthentication, sRhostsRSAAuthentication, sRSAAuthentication,
#ifdef KRB4
	sKerberosAuthentication, sKerberosOrLocalPasswd, sKerberosTicketCleanup,
#endif
#ifdef AFS
	sKerberosTgtPassing, sAFSTokenPassing,
#endif
	sChallengeResponseAuthentication,
	sPasswordAuthentication, sKbdInteractiveAuthentication, sListenAddress,
	sPrintMotd, sIgnoreRhosts, sX11Forwarding, sX11DisplayOffset,
	sStrictModes, sEmptyPasswd, sRandomSeedFile, sKeepAlives, sCheckMail,
	sUseLogin, sAllowTcpForwarding,
	sAllowUsers, sDenyUsers, sAllowGroups, sDenyGroups,
	sIgnoreUserKnownHosts, sCiphers, sMacs, sProtocol, sPidFile,
	sGatewayPorts, sPubkeyAuthentication, sXAuthLocation, sSubsystem, sMaxStartups,
	sBanner, sReverseMappingCheck,
	sIgnoreRootRhosts
} ServerOpCodes;

/* Textual representation of the tokens. */
static struct {
	const char *name;
	ServerOpCodes opcode;
} keywords[] = {
	{ "port", sPort },
	{ "hostkey", sHostKeyFile },
	{ "hostdsakey", sHostKeyFile },					/* alias */
	{ "pidfile", sPidFile },
	{ "serverkeybits", sServerKeyBits },
	{ "logingracetime", sLoginGraceTime },
	{ "keyregenerationinterval", sKeyRegenerationTime },
	{ "permitrootlogin", sPermitRootLogin },
	{ "syslogfacility", sLogFacility },
	{ "loglevel", sLogLevel },
	{ "rhostsauthentication", sRhostsAuthentication },
	{ "rhostsrsaauthentication", sRhostsRSAAuthentication },
	{ "rsaauthentication", sRSAAuthentication },
	{ "pubkeyauthentication", sPubkeyAuthentication },
	{ "dsaauthentication", sPubkeyAuthentication },			/* alias */
#ifdef KRB4
	{ "kerberosauthentication", sKerberosAuthentication },
	{ "kerberosorlocalpasswd", sKerberosOrLocalPasswd },
	{ "kerberosticketcleanup", sKerberosTicketCleanup },
#endif
#ifdef AFS
	{ "kerberostgtpassing", sKerberosTgtPassing },
	{ "afstokenpassing", sAFSTokenPassing },
#endif
	{ "passwordauthentication", sPasswordAuthentication },
	{ "kbdinteractiveauthentication", sKbdInteractiveAuthentication },
	{ "challengeresponseauthentication", sChallengeResponseAuthentication },
	{ "skeyauthentication", sChallengeResponseAuthentication }, /* alias */
	{ "checkmail", sCheckMail },
	{ "listenaddress", sListenAddress },
	{ "printmotd", sPrintMotd },
	{ "ignorerhosts", sIgnoreRhosts },
	{ "ignorerootrhosts", sIgnoreRootRhosts },
	{ "ignoreuserknownhosts", sIgnoreUserKnownHosts },
	{ "x11forwarding", sX11Forwarding },
	{ "x11displayoffset", sX11DisplayOffset },
	{ "xauthlocation", sXAuthLocation },
	{ "strictmodes", sStrictModes },
	{ "permitemptypasswords", sEmptyPasswd },
	{ "uselogin", sUseLogin },
	{ "randomseed", sRandomSeedFile },
	{ "keepalive", sKeepAlives },
	{ "allowtcpforwarding", sAllowTcpForwarding },
	{ "allowusers", sAllowUsers },
	{ "denyusers", sDenyUsers },
	{ "allowgroups", sAllowGroups },
	{ "denygroups", sDenyGroups },
	{ "ciphers", sCiphers },
	{ "macs", sMacs },
	{ "protocol", sProtocol },
	{ "gatewayports", sGatewayPorts },
	{ "subsystem", sSubsystem },
	{ "maxstartups", sMaxStartups },
	{ "banner", sBanner },
	{ "reversemappingcheck", sReverseMappingCheck },
	{ NULL, 0 }
};

/*
 * Returns the number of the token pointed to by cp of length len. Never
 * returns if the token is not known.
 */

static ServerOpCodes
parse_token(const char *cp, const char *filename,
	    int linenum)
{
	u_int i;

	for (i = 0; keywords[i].name; i++)
		if (strcasecmp(cp, keywords[i].name) == 0)
			return keywords[i].opcode;

	fprintf(stderr, "%s: line %d: Bad configuration option: %s\n",
		filename, linenum, cp);
	return sBadOption;
}

/*
 * add listen address
 */
void
add_listen_addr(ServerOptions *options, char *addr)
{
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;
	int i;

	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	for (i = 0; i < options->num_ports; i++) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = IPv4or6;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = (addr == NULL) ? AI_PASSIVE : 0;
		snprintf(strport, sizeof strport, "%d", options->ports[i]);
		if ((gaierr = getaddrinfo(addr, strport, &hints, &aitop)) != 0)
			fatal("bad addr or host: %s (%s)\n",
			    addr ? addr : "<NULL>",
			    gai_strerror(gaierr));
		for (ai = aitop; ai->ai_next; ai = ai->ai_next)
			;
		ai->ai_next = options->listen_addrs;
		options->listen_addrs = aitop;
	}
}

/* Reads the server configuration file. */

void
read_server_config(ServerOptions *options, const char *filename)
{
	FILE *f;
	char line[1024];
	char *cp, **charptr, *arg;
	int linenum, *intptr, value;
	int bad_options = 0;
	ServerOpCodes opcode;
	int i;

	f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		exit(1);
	}
	linenum = 0;
	while (fgets(line, sizeof(line), f)) {
		linenum++;
		cp = line;
		arg = strdelim(&cp);
		/* Ignore leading whitespace */
		if (*arg == '\0')
			arg = strdelim(&cp);
		if (!arg || !*arg || *arg == '#')
			continue;
		intptr = NULL;
		charptr = NULL;
		opcode = parse_token(arg, filename, linenum);
		switch (opcode) {
		case sBadOption:
			bad_options++;
			continue;
		case sPort:
			/* ignore ports from configfile if cmdline specifies ports */
			if (options->ports_from_cmdline)
				continue;
			if (options->listen_addrs != NULL)
				fatal("%s line %d: ports must be specified before "
				    "ListenAdress.\n", filename, linenum);
			if (options->num_ports >= MAX_PORTS)
				fatal("%s line %d: too many ports.\n",
				    filename, linenum);
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: missing port number.\n",
				    filename, linenum);
			options->ports[options->num_ports++] = atoi(arg);
			break;

		case sServerKeyBits:
			intptr = &options->server_key_bits;
parse_int:
			arg = strdelim(&cp);
			if (!arg || *arg == '\0') {
				fprintf(stderr, "%s line %d: missing integer value.\n",
					filename, linenum);
				exit(1);
			}
			value = atoi(arg);
			if (*intptr == -1)
				*intptr = value;
			break;

		case sLoginGraceTime:
			intptr = &options->login_grace_time;
			goto parse_int;

		case sKeyRegenerationTime:
			intptr = &options->key_regeneration_time;
			goto parse_int;

		case sListenAddress:
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: missing inet addr.\n",
				    filename, linenum);
			add_listen_addr(options, arg);
			break;

		case sHostKeyFile:
			intptr = &options->num_host_key_files;
			if (*intptr >= MAX_HOSTKEYS) {
				fprintf(stderr, "%s line %d: to many host keys specified (max %d).\n",
				    filename, linenum, MAX_HOSTKEYS);
				exit(1);
			}
			charptr = &options->host_key_files[*intptr];
parse_filename:
			arg = strdelim(&cp);
			if (!arg || *arg == '\0') {
				fprintf(stderr, "%s line %d: missing file name.\n",
				    filename, linenum);
				exit(1);
			}
			if (*charptr == NULL) {
				*charptr = tilde_expand_filename(arg, getuid());
				/* increase optional counter */
				if (intptr != NULL)
					*intptr = *intptr + 1;
			}
			break;

		case sPidFile:
			charptr = &options->pid_file;
			goto parse_filename;

		case sRandomSeedFile:
			fprintf(stderr, "%s line %d: \"randomseed\" option is obsolete.\n",
				filename, linenum);
			arg = strdelim(&cp);
			break;

		case sPermitRootLogin:
			intptr = &options->permit_root_login;
			arg = strdelim(&cp);
			if (!arg || *arg == '\0') {
				fprintf(stderr, "%s line %d: missing yes/without-password/no argument.\n",
					filename, linenum);
				exit(1);
			}
			if (strcmp(arg, "without-password") == 0)
				value = PERMIT_NO_PASSWD;
			else if (strcmp(arg, "forced-commands-only") == 0)
				value = PERMIT_FORCED_ONLY;
			else if (strcmp(arg, "yes") == 0)
				value = PERMIT_YES;
			else if (strcmp(arg, "no") == 0)
				value = PERMIT_NO;
			else {
				fprintf(stderr, "%s line %d: Bad yes/"
				    "without-password/forced-commands-only/no "
				    "argument: %s\n", filename, linenum, arg);
				exit(1);
			}
			if (*intptr == -1)
				*intptr = value;
			break;

		case sIgnoreRhosts:
			intptr = &options->ignore_rhosts;
parse_flag:
			arg = strdelim(&cp);
			if (!arg || *arg == '\0') {
				fprintf(stderr, "%s line %d: missing yes/no argument.\n",
					filename, linenum);
				exit(1);
			}
			if (strcmp(arg, "yes") == 0)
				value = 1;
			else if (strcmp(arg, "no") == 0)
				value = 0;
			else {
				fprintf(stderr, "%s line %d: Bad yes/no argument: %s\n",
					filename, linenum, arg);
				exit(1);
			}
			if (*intptr == -1)
				*intptr = value;
			break;

		case sIgnoreRootRhosts:
			intptr = &options->ignore_root_rhosts;
			goto parse_flag;

		case sIgnoreUserKnownHosts:
			intptr = &options->ignore_user_known_hosts;
			goto parse_flag;

		case sRhostsAuthentication:
			intptr = &options->rhosts_authentication;
			goto parse_flag;

		case sRhostsRSAAuthentication:
			intptr = &options->rhosts_rsa_authentication;
			goto parse_flag;

		case sRSAAuthentication:
			intptr = &options->rsa_authentication;
			goto parse_flag;

		case sPubkeyAuthentication:
			intptr = &options->pubkey_authentication;
			goto parse_flag;

#ifdef KRB4
		case sKerberosAuthentication:
			intptr = &options->kerberos_authentication;
			goto parse_flag;

		case sKerberosOrLocalPasswd:
			intptr = &options->kerberos_or_local_passwd;
			goto parse_flag;

		case sKerberosTicketCleanup:
			intptr = &options->kerberos_ticket_cleanup;
			goto parse_flag;
#endif

#ifdef AFS
		case sKerberosTgtPassing:
			intptr = &options->kerberos_tgt_passing;
			goto parse_flag;

		case sAFSTokenPassing:
			intptr = &options->afs_token_passing;
			goto parse_flag;
#endif

		case sPasswordAuthentication:
			intptr = &options->password_authentication;
			goto parse_flag;

		case sKbdInteractiveAuthentication:
			intptr = &options->kbd_interactive_authentication;
			goto parse_flag;

		case sCheckMail:
			intptr = &options->check_mail;
			goto parse_flag;

		case sChallengeResponseAuthentication:
			intptr = &options->challenge_reponse_authentication;
			goto parse_flag;

		case sPrintMotd:
			intptr = &options->print_motd;
			goto parse_flag;

		case sX11Forwarding:
			intptr = &options->x11_forwarding;
			goto parse_flag;

		case sX11DisplayOffset:
			intptr = &options->x11_display_offset;
			goto parse_int;

		case sXAuthLocation:
			charptr = &options->xauth_location;
			goto parse_filename;

		case sStrictModes:
			intptr = &options->strict_modes;
			goto parse_flag;

		case sKeepAlives:
			intptr = &options->keepalives;
			goto parse_flag;

		case sEmptyPasswd:
			intptr = &options->permit_empty_passwd;
			goto parse_flag;

		case sUseLogin:
			intptr = &options->use_login;
			goto parse_flag;

		case sGatewayPorts:
			intptr = &options->gateway_ports;
			goto parse_flag;

		case sReverseMappingCheck:
			intptr = &options->reverse_mapping_check;
			goto parse_flag;

		case sLogFacility:
			intptr = (int *) &options->log_facility;
			arg = strdelim(&cp);
			value = log_facility_number(arg);
			if (value == (SyslogFacility) - 1)
				fatal("%.200s line %d: unsupported log facility '%s'\n",
				    filename, linenum, arg ? arg : "<NONE>");
			if (*intptr == -1)
				*intptr = (SyslogFacility) value;
			break;

		case sLogLevel:
			intptr = (int *) &options->log_level;
			arg = strdelim(&cp);
			value = log_level_number(arg);
			if (value == (LogLevel) - 1)
				fatal("%.200s line %d: unsupported log level '%s'\n",
				    filename, linenum, arg ? arg : "<NONE>");
			if (*intptr == -1)
				*intptr = (LogLevel) value;
			break;

		case sAllowTcpForwarding:
			intptr = &options->allow_tcp_forwarding;
			goto parse_flag;

		case sAllowUsers:
			while ((arg = strdelim(&cp)) && *arg != '\0') {
				if (options->num_allow_users >= MAX_ALLOW_USERS)
					fatal("%s line %d: too many allow users.\n",
					    filename, linenum);
				options->allow_users[options->num_allow_users++] = xstrdup(arg);
			}
			break;

		case sDenyUsers:
			while ((arg = strdelim(&cp)) && *arg != '\0') {
				if (options->num_deny_users >= MAX_DENY_USERS)
					fatal( "%s line %d: too many deny users.\n",
					    filename, linenum);
				options->deny_users[options->num_deny_users++] = xstrdup(arg);
			}
			break;

		case sAllowGroups:
			while ((arg = strdelim(&cp)) && *arg != '\0') {
				if (options->num_allow_groups >= MAX_ALLOW_GROUPS)
					fatal("%s line %d: too many allow groups.\n",
					    filename, linenum);
				options->allow_groups[options->num_allow_groups++] = xstrdup(arg);
			}
			break;

		case sDenyGroups:
			while ((arg = strdelim(&cp)) && *arg != '\0') {
				if (options->num_deny_groups >= MAX_DENY_GROUPS)
					fatal("%s line %d: too many deny groups.\n",
					    filename, linenum);
				options->deny_groups[options->num_deny_groups++] = xstrdup(arg);
			}
			break;

		case sCiphers:
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: Missing argument.", filename, linenum);
			if (!ciphers_valid(arg))
				fatal("%s line %d: Bad SSH2 cipher spec '%s'.",
				    filename, linenum, arg ? arg : "<NONE>");
			if (options->ciphers == NULL)
				options->ciphers = xstrdup(arg);
			break;

		case sMacs:
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: Missing argument.", filename, linenum);
			if (!mac_valid(arg))
				fatal("%s line %d: Bad SSH2 mac spec '%s'.",
				    filename, linenum, arg ? arg : "<NONE>");
			if (options->macs == NULL)
				options->macs = xstrdup(arg);
			break;

		case sProtocol:
			intptr = &options->protocol;
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: Missing argument.", filename, linenum);
			value = proto_spec(arg);
			if (value == SSH_PROTO_UNKNOWN)
				fatal("%s line %d: Bad protocol spec '%s'.",
				      filename, linenum, arg ? arg : "<NONE>");
			if (*intptr == SSH_PROTO_UNKNOWN)
				*intptr = value;
			break;

		case sSubsystem:
			if(options->num_subsystems >= MAX_SUBSYSTEMS) {
				fatal("%s line %d: too many subsystems defined.",
				      filename, linenum);
			}
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: Missing subsystem name.",
				      filename, linenum);
			for (i = 0; i < options->num_subsystems; i++)
				if(strcmp(arg, options->subsystem_name[i]) == 0)
					fatal("%s line %d: Subsystem '%s' already defined.",
					      filename, linenum, arg);
			options->subsystem_name[options->num_subsystems] = xstrdup(arg);
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: Missing subsystem command.",
				      filename, linenum);
			options->subsystem_command[options->num_subsystems] = xstrdup(arg);
			options->num_subsystems++;
			break;

		case sMaxStartups:
			arg = strdelim(&cp);
			if (!arg || *arg == '\0')
				fatal("%s line %d: Missing MaxStartups spec.",
				      filename, linenum);
			if (sscanf(arg, "%d:%d:%d",
			    &options->max_startups_begin,
			    &options->max_startups_rate,
			    &options->max_startups) == 3) {
				if (options->max_startups_begin >
				    options->max_startups ||
				    options->max_startups_rate > 100 ||
				    options->max_startups_rate < 1)
				fatal("%s line %d: Illegal MaxStartups spec.",
				      filename, linenum);
				break;
			}
			intptr = &options->max_startups;
			goto parse_int;

		case sBanner:
			charptr = &options->banner;
			goto parse_filename;

		default:
			fprintf(stderr, "%s line %d: Missing handler for opcode %s (%d)\n",
				filename, linenum, arg, opcode);
			exit(1);
		}
		if ((arg = strdelim(&cp)) != NULL && *arg != '\0') {
			fprintf(stderr,
				"%s line %d: garbage at end of line; \"%.200s\".\n",
				filename, linenum, arg);
			exit(1);
		}
	}
	fclose(f);
	if (bad_options > 0) {
		fprintf(stderr, "%s: terminating, %d bad configuration options\n",
			filename, bad_options);
		exit(1);
	}
}
