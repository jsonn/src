/*	$NetBSD: readconf.h,v 1.1.1.1.2.3 2001/12/11 00:00:49 he Exp $	*/
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for reading the configuration file.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: readconf.h,v 1.40 2001/10/01 21:51:16 markus Exp $"); */

#ifndef READCONF_H
#define READCONF_H

#include "key.h"

/* Data structure for representing a forwarding request. */

typedef struct {
	u_short	  port;		/* Port to forward. */
	char	 *host;		/* Host to connect. */
	u_short	  host_port;	/* Port to connect on host. */
}       Forward;
/* Data structure for representing option data. */

typedef struct {
	int     forward_agent;	/* Forward authentication agent. */
	int     forward_x11;	/* Forward X11 display. */
	char   *xauth_location;	/* Location for xauth program */
	int     gateway_ports;	/* Allow remote connects to forwarded ports. */
	int     use_privileged_port;	/* Don't use privileged port if false. */
	int     rhosts_authentication;	/* Try rhosts authentication. */
	int     rhosts_rsa_authentication;	/* Try rhosts with RSA
						 * authentication. */
	int     rsa_authentication;	/* Try RSA authentication. */
	int     pubkey_authentication;	/* Try ssh2 pubkey authentication. */
	int     hostbased_authentication;	/* ssh2's rhosts_rsa */
	int     challenge_response_authentication;
					/* Try S/Key or TIS, authentication. */
#if defined(KRB4) || defined(KRB5)
	int     kerberos_authentication;	/* Try Kerberos authentication. */
#endif
#if defined(AFS) || defined(KRB5)
	int     kerberos_tgt_passing;	/* Try Kerberos TGT passing. */
#endif
#ifdef AFS
	int     afs_token_passing;	/* Try AFS token passing. */
#endif
	int     password_authentication;	/* Try password
						 * authentication. */
	int     kbd_interactive_authentication; /* Try keyboard-interactive auth. */
	char	*kbd_interactive_devices; /* Keyboard-interactive auth devices. */
	int     fallback_to_rsh;/* Use rsh if cannot connect with ssh. */
	int     use_rsh;	/* Always use rsh (don\'t try ssh). */
	int     batch_mode;	/* Batch mode: do not ask for passwords. */
	int     check_host_ip;	/* Also keep track of keys for IP address */
	int     strict_host_key_checking;	/* Strict host key checking. */
	int     compression;	/* Compress packets in both directions. */
	int     compression_level;	/* Compression level 1 (fast) to 9
					 * (best). */
	int     keepalives;	/* Set SO_KEEPALIVE. */
	LogLevel log_level;	/* Level for logging. */

	int     port;		/* Port to connect. */
	int     connection_attempts;	/* Max attempts (seconds) before
					 * giving up */
	int     number_of_password_prompts;	/* Max number of password
						 * prompts. */
	int     cipher;		/* Cipher to use. */
	char   *ciphers;	/* SSH2 ciphers in order of preference. */
	char   *macs;		/* SSH2 macs in order of preference. */
	char   *hostkeyalgorithms;	/* SSH2 server key types in order of preference. */
	int	protocol;	/* Protocol in order of preference. */
	char   *hostname;	/* Real host to connect. */
	char   *host_key_alias;	/* hostname alias for .ssh/known_hosts */
	char   *proxy_command;	/* Proxy command for connecting the host. */
	char   *user;		/* User to log in as. */
	int     escape_char;	/* Escape character; -2 = none */

	char   *system_hostfile;/* Path for /etc/ssh_known_hosts. */
	char   *user_hostfile;	/* Path for $HOME/.ssh/known_hosts. */
	char   *system_hostfile2;
	char   *user_hostfile2;
	char   *preferred_authentications;
	char   *bind_address;	/* local socket address for connection to sshd */
	char   *smartcard_device; /* Smartcard reader device */

	int     num_identity_files;	/* Number of files for RSA/DSA identities. */
	char   *identity_files[SSH_MAX_IDENTITY_FILES];
	Key    *identity_keys[SSH_MAX_IDENTITY_FILES];

	/* Local TCP/IP forward requests. */
	int     num_local_forwards;
	Forward local_forwards[SSH_MAX_FORWARDS_PER_DIRECTION];

	/* Remote TCP/IP forward requests. */
	int     num_remote_forwards;
	Forward remote_forwards[SSH_MAX_FORWARDS_PER_DIRECTION];
	int	clear_forwardings;
	int	no_host_authentication_for_localhost;
}       Options;


void     initialize_options(Options *);
void     fill_default_options(Options *);
int	 read_config_file(const char *, const char *, Options *);

int
process_config_line(Options *, const char *, char *, const char *, int, int *);

void	 add_local_forward(Options *, u_short, const char *, u_short);
void	 add_remote_forward(Options *, u_short, const char *, u_short);

#endif				/* READCONF_H */
