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
RCSID("$OpenBSD: sshconnect1.c,v 1.27 2001/02/15 23:19:59 markus Exp $");

#include <openssl/bn.h>
#include <openssl/evp.h>

#ifdef KRB4
#include <krb.h>
#endif
#ifdef AFS
#include <kafs.h>
#include "radix.h"
#endif

#include "ssh.h"
#include "ssh1.h"
#include "xmalloc.h"
#include "rsa.h"
#include "buffer.h"
#include "packet.h"
#include "mpaux.h"
#include "uidswap.h"
#include "log.h"
#include "readconf.h"
#include "key.h"
#include "authfd.h"
#include "sshconnect.h"
#include "authfile.h"
#include "readpass.h"
#include "cipher.h"
#include "canohost.h"

/* Session id for the current session. */
u_char session_id[16];
u_int supported_authentications = 0;

extern Options options;
extern char *__progname;

/*
 * Checks if the user has an authentication agent, and if so, tries to
 * authenticate using the agent.
 */
static int
try_agent_authentication(void)
{
	int type;
	char *comment;
	AuthenticationConnection *auth;
	u_char response[16];
	u_int i;
	int plen, clen;
	Key *key;
	BIGNUM *challenge;

	/* Get connection to the agent. */
	auth = ssh_get_authentication_connection();
	if (!auth)
		return 0;

	challenge = BN_new();

	/* Loop through identities served by the agent. */
	for (key = ssh_get_first_identity(auth, &comment, 1);
	     key != NULL;
	     key = ssh_get_next_identity(auth, &comment, 1)) {

		/* Try this identity. */
		debug("Trying RSA authentication via agent with '%.100s'", comment);
		xfree(comment);

		/* Tell the server that we are willing to authenticate using this key. */
		packet_start(SSH_CMSG_AUTH_RSA);
		packet_put_bignum(key->rsa->n);
		packet_send();
		packet_write_wait();

		/* Wait for server's response. */
		type = packet_read(&plen);

		/* The server sends failure if it doesn\'t like our key or
		   does not support RSA authentication. */
		if (type == SSH_SMSG_FAILURE) {
			debug("Server refused our key.");
			key_free(key);
			continue;
		}
		/* Otherwise it should have sent a challenge. */
		if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
			packet_disconnect("Protocol error during RSA authentication: %d",
					  type);

		packet_get_bignum(challenge, &clen);

		packet_integrity_check(plen, clen, type);

		debug("Received RSA challenge from server.");

		/* Ask the agent to decrypt the challenge. */
		if (!ssh_decrypt_challenge(auth, key, challenge, session_id, 1, response)) {
			/*
			 * The agent failed to authenticate this identifier
			 * although it advertised it supports this.  Just
			 * return a wrong value.
			 */
			log("Authentication agent failed to decrypt challenge.");
			memset(response, 0, sizeof(response));
		}
		key_free(key);
		debug("Sending response to RSA challenge.");

		/* Send the decrypted challenge back to the server. */
		packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
		for (i = 0; i < 16; i++)
			packet_put_char(response[i]);
		packet_send();
		packet_write_wait();

		/* Wait for response from the server. */
		type = packet_read(&plen);

		/* The server returns success if it accepted the authentication. */
		if (type == SSH_SMSG_SUCCESS) {
			ssh_close_authentication_connection(auth);
			BN_clear_free(challenge);
			debug("RSA authentication accepted by server.");
			return 1;
		}
		/* Otherwise it should return failure. */
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error waiting RSA auth response: %d",
					  type);
	}
	ssh_close_authentication_connection(auth);
	BN_clear_free(challenge);
	debug("RSA authentication using agent refused.");
	return 0;
}

/*
 * Computes the proper response to a RSA challenge, and sends the response to
 * the server.
 */
static void
respond_to_rsa_challenge(BIGNUM * challenge, RSA * prv)
{
	u_char buf[32], response[16];
	MD5_CTX md;
	int i, len;

	/* Decrypt the challenge using the private key. */
	/* XXX think about Bleichenbacher, too */
	if (rsa_private_decrypt(challenge, challenge, prv) <= 0)
		packet_disconnect(
		    "respond_to_rsa_challenge: rsa_private_decrypt failed");

	/* Compute the response. */
	/* The response is MD5 of decrypted challenge plus session id. */
	len = BN_num_bytes(challenge);
	if (len <= 0 || len > sizeof(buf))
		packet_disconnect(
		    "respond_to_rsa_challenge: bad challenge length %d", len);

	memset(buf, 0, sizeof(buf));
	BN_bn2bin(challenge, buf + sizeof(buf) - len);
	MD5_Init(&md);
	MD5_Update(&md, buf, 32);
	MD5_Update(&md, session_id, 16);
	MD5_Final(response, &md);

	debug("Sending response to host key RSA challenge.");

	/* Send the response back to the server. */
	packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
	for (i = 0; i < 16; i++)
		packet_put_char(response[i]);
	packet_send();
	packet_write_wait();

	memset(buf, 0, sizeof(buf));
	memset(response, 0, sizeof(response));
	memset(&md, 0, sizeof(md));
}

/*
 * Checks if the user has authentication file, and if so, tries to authenticate
 * the user using it.
 */
static int
try_rsa_authentication(const char *authfile)
{
	BIGNUM *challenge;
	Key *public;
	Key *private;
	char *passphrase, *comment;
	int type, i;
	int plen, clen;

	/* Try to load identification for the authentication key. */
	public = key_new(KEY_RSA1);
	if (!load_public_key(authfile, public, &comment)) {
		key_free(public);
		/* Could not load it.  Fail. */
		return 0;
	}
	debug("Trying RSA authentication with key '%.100s'", comment);

	/* Tell the server that we are willing to authenticate using this key. */
	packet_start(SSH_CMSG_AUTH_RSA);
	packet_put_bignum(public->rsa->n);
	packet_send();
	packet_write_wait();

	/* We no longer need the public key. */
	key_free(public);

	/* Wait for server's response. */
	type = packet_read(&plen);

	/*
	 * The server responds with failure if it doesn\'t like our key or
	 * doesn\'t support RSA authentication.
	 */
	if (type == SSH_SMSG_FAILURE) {
		debug("Server refused our key.");
		xfree(comment);
		return 0;
	}
	/* Otherwise, the server should respond with a challenge. */
	if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
		packet_disconnect("Protocol error during RSA authentication: %d", type);

	/* Get the challenge from the packet. */
	challenge = BN_new();
	packet_get_bignum(challenge, &clen);

	packet_integrity_check(plen, clen, type);

	debug("Received RSA challenge from server.");

	private = key_new(KEY_RSA1);
	/*
	 * Load the private key.  Try first with empty passphrase; if it
	 * fails, ask for a passphrase.
	 */
	if (!load_private_key(authfile, "", private, NULL)) {
		char buf[300];
		snprintf(buf, sizeof buf, "Enter passphrase for RSA key '%.100s': ",
		    comment);
		if (!options.batch_mode)
			passphrase = read_passphrase(buf, 0);
		else {
			debug("Will not query passphrase for %.100s in batch mode.",
			      comment);
			passphrase = xstrdup("");
		}

		/* Load the authentication file using the pasphrase. */
		if (!load_private_key(authfile, passphrase, private, NULL)) {
			memset(passphrase, 0, strlen(passphrase));
			xfree(passphrase);
			error("Bad passphrase.");

			/* Send a dummy response packet to avoid protocol error. */
			packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
			for (i = 0; i < 16; i++)
				packet_put_char(0);
			packet_send();
			packet_write_wait();

			/* Expect the server to reject it... */
			packet_read_expect(&plen, SSH_SMSG_FAILURE);
			xfree(comment);
			key_free(private);
			BN_clear_free(challenge);
			return 0;
		}
		/* Destroy the passphrase. */
		memset(passphrase, 0, strlen(passphrase));
		xfree(passphrase);
	}
	/* We no longer need the comment. */
	xfree(comment);

	/* Compute and send a response to the challenge. */
	respond_to_rsa_challenge(challenge, private->rsa);

	/* Destroy the private key. */
	key_free(private);

	/* We no longer need the challenge. */
	BN_clear_free(challenge);

	/* Wait for response from the server. */
	type = packet_read(&plen);
	if (type == SSH_SMSG_SUCCESS) {
		debug("RSA authentication accepted by server.");
		return 1;
	}
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error waiting RSA auth response: %d", type);
	debug("RSA authentication refused.");
	return 0;
}

/*
 * Tries to authenticate the user using combined rhosts or /etc/hosts.equiv
 * authentication and RSA host authentication.
 */
static int
try_rhosts_rsa_authentication(const char *local_user, RSA * host_key)
{
	int type;
	BIGNUM *challenge;
	int plen, clen;

	debug("Trying rhosts or /etc/hosts.equiv with RSA host authentication.");

	/* Tell the server that we are willing to authenticate using this key. */
	packet_start(SSH_CMSG_AUTH_RHOSTS_RSA);
	packet_put_string(local_user, strlen(local_user));
	packet_put_int(BN_num_bits(host_key->n));
	packet_put_bignum(host_key->e);
	packet_put_bignum(host_key->n);
	packet_send();
	packet_write_wait();

	/* Wait for server's response. */
	type = packet_read(&plen);

	/* The server responds with failure if it doesn't admit our
	   .rhosts authentication or doesn't know our host key. */
	if (type == SSH_SMSG_FAILURE) {
		debug("Server refused our rhosts authentication or host key.");
		return 0;
	}
	/* Otherwise, the server should respond with a challenge. */
	if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
		packet_disconnect("Protocol error during RSA authentication: %d", type);

	/* Get the challenge from the packet. */
	challenge = BN_new();
	packet_get_bignum(challenge, &clen);

	packet_integrity_check(plen, clen, type);

	debug("Received RSA challenge for host key from server.");

	/* Compute a response to the challenge. */
	respond_to_rsa_challenge(challenge, host_key);

	/* We no longer need the challenge. */
	BN_clear_free(challenge);

	/* Wait for response from the server. */
	type = packet_read(&plen);
	if (type == SSH_SMSG_SUCCESS) {
		debug("Rhosts or /etc/hosts.equiv with RSA host authentication accepted by server.");
		return 1;
	}
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error waiting RSA auth response: %d", type);
	debug("Rhosts or /etc/hosts.equiv with RSA host authentication refused.");
	return 0;
}

#ifdef KRB4
static int
try_kerberos_authentication(void)
{
	KTEXT_ST auth;		/* Kerberos data */
	char *reply;
	char inst[INST_SZ];
	char *realm;
	CREDENTIALS cred;
	int r, type, plen;
	socklen_t slen;
	Key_schedule schedule;
	u_long checksum, cksum;
	MSG_DAT msg_data;
	struct sockaddr_in local, foreign;
	struct stat st;

	/* Don't do anything if we don't have any tickets. */
	if (stat(tkt_string(), &st) < 0)
		return 0;

	strncpy(inst, (char *) krb_get_phost(get_canonical_hostname(1)), INST_SZ);

	realm = (char *) krb_realmofhost(get_canonical_hostname(1));
	if (!realm) {
		debug("Kerberos V4: no realm for %s", get_canonical_hostname(1));
		return 0;
	}
	/* This can really be anything. */
	checksum = (u_long) getpid();

	r = krb_mk_req(&auth, KRB4_SERVICE_NAME, inst, realm, checksum);
	if (r != KSUCCESS) {
		debug("Kerberos V4 krb_mk_req failed: %s", krb_err_txt[r]);
		return 0;
	}
	/* Get session key to decrypt the server's reply with. */
	r = krb_get_cred(KRB4_SERVICE_NAME, inst, realm, &cred);
	if (r != KSUCCESS) {
		debug("get_cred failed: %s", krb_err_txt[r]);
		return 0;
	}
	des_key_sched((des_cblock *) cred.session, schedule);

	/* Send authentication info to server. */
	packet_start(SSH_CMSG_AUTH_KERBEROS);
	packet_put_string((char *) auth.dat, auth.length);
	packet_send();
	packet_write_wait();

	/* Zero the buffer. */
	(void) memset(auth.dat, 0, MAX_KTXT_LEN);

	slen = sizeof(local);
	memset(&local, 0, sizeof(local));
	if (getsockname(packet_get_connection_in(),
			(struct sockaddr *) & local, &slen) < 0)
		debug("getsockname failed: %s", strerror(errno));

	slen = sizeof(foreign);
	memset(&foreign, 0, sizeof(foreign));
	if (getpeername(packet_get_connection_in(),
			(struct sockaddr *) & foreign, &slen) < 0) {
		debug("getpeername failed: %s", strerror(errno));
		fatal_cleanup();
	}
	/* Get server reply. */
	type = packet_read(&plen);
	switch (type) {
	case SSH_SMSG_FAILURE:
		/* Should really be SSH_SMSG_AUTH_KERBEROS_FAILURE */
		debug("Kerberos V4 authentication failed.");
		return 0;
		break;

	case SSH_SMSG_AUTH_KERBEROS_RESPONSE:
		/* SSH_SMSG_AUTH_KERBEROS_SUCCESS */
		debug("Kerberos V4 authentication accepted.");

		/* Get server's response. */
		reply = packet_get_string((u_int *) &auth.length);
		memcpy(auth.dat, reply, auth.length);
		xfree(reply);

		packet_integrity_check(plen, 4 + auth.length, type);

		/*
		 * If his response isn't properly encrypted with the session
		 * key, and the decrypted checksum fails to match, he's
		 * bogus. Bail out.
		 */
		r = krb_rd_priv(auth.dat, auth.length, schedule, &cred.session,
				&foreign, &local, &msg_data);
		if (r != KSUCCESS) {
			debug("Kerberos V4 krb_rd_priv failed: %s", krb_err_txt[r]);
			packet_disconnect("Kerberos V4 challenge failed!");
		}
		/* Fetch the (incremented) checksum that we supplied in the request. */
		(void) memcpy((char *) &cksum, (char *) msg_data.app_data, sizeof(cksum));
		cksum = ntohl(cksum);

		/* If it matches, we're golden. */
		if (cksum == checksum + 1) {
			debug("Kerberos V4 challenge successful.");
			return 1;
		} else
			packet_disconnect("Kerberos V4 challenge failed!");
		break;

	default:
		packet_disconnect("Protocol error on Kerberos V4 response: %d", type);
	}
	return 0;
}

#endif /* KRB4 */

#ifdef AFS
int
send_kerberos_tgt(void)
{
	CREDENTIALS *creds;
	char pname[ANAME_SZ], pinst[INST_SZ], prealm[REALM_SZ];
	int r, type, plen;
	char buffer[8192];
	struct stat st;

	/* Don't do anything if we don't have any tickets. */
	if (stat(tkt_string(), &st) < 0)
		return 0;

	creds = xmalloc(sizeof(*creds));

	if ((r = krb_get_tf_fullname(TKT_FILE, pname, pinst, prealm)) != KSUCCESS) {
		debug("Kerberos V4 tf_fullname failed: %s", krb_err_txt[r]);
		return 0;
	}
	if ((r = krb_get_cred("krbtgt", prealm, prealm, creds)) != GC_OK) {
		debug("Kerberos V4 get_cred failed: %s", krb_err_txt[r]);
		return 0;
	}
	if (time(0) > krb_life_to_time(creds->issue_date, creds->lifetime)) {
		debug("Kerberos V4 ticket expired: %s", TKT_FILE);
		return 0;
	}
	creds_to_radix(creds, (u_char *)buffer, sizeof buffer);
	xfree(creds);

	packet_start(SSH_CMSG_HAVE_KERBEROS_TGT);
	packet_put_string(buffer, strlen(buffer));
	packet_send();
	packet_write_wait();

	type = packet_read(&plen);

	if (type == SSH_SMSG_FAILURE)
		debug("Kerberos TGT for realm %s rejected.", prealm);
	else if (type != SSH_SMSG_SUCCESS)
		packet_disconnect("Protocol error on Kerberos TGT response: %d", type);

	return 1;
}

void
send_afs_tokens(void)
{
	CREDENTIALS creds;
	struct ViceIoctl parms;
	struct ClearToken ct;
	int i, type, len, plen;
	char buf[2048], *p, *server_cell;
	char buffer[8192];

	/* Move over ktc_GetToken, here's something leaner. */
	for (i = 0; i < 100; i++) {	/* just in case */
		parms.in = (char *) &i;
		parms.in_size = sizeof(i);
		parms.out = buf;
		parms.out_size = sizeof(buf);
		if (k_pioctl(0, VIOCGETTOK, &parms, 0) != 0)
			break;
		p = buf;

		/* Get secret token. */
		memcpy(&creds.ticket_st.length, p, sizeof(u_int));
		if (creds.ticket_st.length > MAX_KTXT_LEN)
			break;
		p += sizeof(u_int);
		memcpy(creds.ticket_st.dat, p, creds.ticket_st.length);
		p += creds.ticket_st.length;

		/* Get clear token. */
		memcpy(&len, p, sizeof(len));
		if (len != sizeof(struct ClearToken))
			break;
		p += sizeof(len);
		memcpy(&ct, p, len);
		p += len;
		p += sizeof(len);	/* primary flag */
		server_cell = p;

		/* Flesh out our credentials. */
		strlcpy(creds.service, "afs", sizeof creds.service);
		creds.instance[0] = '\0';
		strlcpy(creds.realm, server_cell, REALM_SZ);
		memcpy(creds.session, ct.HandShakeKey, DES_KEY_SZ);
		creds.issue_date = ct.BeginTimestamp;
		creds.lifetime = krb_time_to_life(creds.issue_date, ct.EndTimestamp);
		creds.kvno = ct.AuthHandle;
		snprintf(creds.pname, sizeof(creds.pname), "AFS ID %d", ct.ViceId);
		creds.pinst[0] = '\0';

		/* Encode token, ship it off. */
		if (creds_to_radix(&creds, (u_char *) buffer, sizeof buffer) <= 0)
			break;
		packet_start(SSH_CMSG_HAVE_AFS_TOKEN);
		packet_put_string(buffer, strlen(buffer));
		packet_send();
		packet_write_wait();

		/* Roger, Roger. Clearance, Clarence. What's your vector,
		   Victor? */
		type = packet_read(&plen);

		if (type == SSH_SMSG_FAILURE)
			debug("AFS token for cell %s rejected.", server_cell);
		else if (type != SSH_SMSG_SUCCESS)
			packet_disconnect("Protocol error on AFS token response: %d", type);
	}
}

#endif /* AFS */

/*
 * Tries to authenticate with any string-based challenge/response system.
 * Note that the client code is not tied to s/key or TIS.
 */
static int
try_challenge_reponse_authentication(void)
{
	int type, i;
	int payload_len;
	u_int clen;
	char prompt[1024];
	char *challenge, *response;

	debug("Doing challenge reponse authentication.");

	for (i = 0; i < options.number_of_password_prompts; i++) {
		/* request a challenge */
		packet_start(SSH_CMSG_AUTH_TIS);
		packet_send();
		packet_write_wait();

		type = packet_read(&payload_len);
		if (type != SSH_SMSG_FAILURE &&
		    type != SSH_SMSG_AUTH_TIS_CHALLENGE) {
			packet_disconnect("Protocol error: got %d in response "
			    "to SSH_CMSG_AUTH_TIS", type);
		}
		if (type != SSH_SMSG_AUTH_TIS_CHALLENGE) {
			debug("No challenge.");
			return 0;
		}
		challenge = packet_get_string(&clen);
		packet_integrity_check(payload_len, (4 + clen), type);
		snprintf(prompt, sizeof prompt, "%s%s", challenge,
		     strchr(challenge, '\n') ? "" : "\nResponse: ");
		xfree(challenge);
		if (i != 0)
			error("Permission denied, please try again.");
		if (options.cipher == SSH_CIPHER_NONE)
			log("WARNING: Encryption is disabled! "
			    "Reponse will be transmitted in clear text.");
		response = read_passphrase(prompt, 0);
		if (strcmp(response, "") == 0) {
			xfree(response);
			break;
		}
		packet_start(SSH_CMSG_AUTH_TIS_RESPONSE);
		ssh_put_password(response);
		memset(response, 0, strlen(response));
		xfree(response);
		packet_send();
		packet_write_wait();
		type = packet_read(&payload_len);
		if (type == SSH_SMSG_SUCCESS)
			return 1;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response "
			    "to SSH_CMSG_AUTH_TIS_RESPONSE", type);
	}
	/* failure */
	return 0;
}

/*
 * Tries to authenticate with plain passwd authentication.
 */
static int
try_password_authentication(char *prompt)
{
	int type, i, payload_len;
	char *password;

	debug("Doing password authentication.");
	if (options.cipher == SSH_CIPHER_NONE)
		log("WARNING: Encryption is disabled! Password will be transmitted in clear text.");
	for (i = 0; i < options.number_of_password_prompts; i++) {
		if (i != 0)
			error("Permission denied, please try again.");
		password = read_passphrase(prompt, 0);
		packet_start(SSH_CMSG_AUTH_PASSWORD);
		ssh_put_password(password);
		memset(password, 0, strlen(password));
		xfree(password);
		packet_send();
		packet_write_wait();

		type = packet_read(&payload_len);
		if (type == SSH_SMSG_SUCCESS)
			return 1;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response to passwd auth", type);
	}
	/* failure */
	return 0;
}

/*
 * SSH1 key exchange
 */
void
ssh_kex(char *host, struct sockaddr *hostaddr)
{
	int i;
	BIGNUM *key;
	RSA *host_key;
	RSA *public_key;
	Key k;
	int bits, rbits;
	int ssh_cipher_default = SSH_CIPHER_3DES;
	u_char session_key[SSH_SESSION_KEY_LENGTH];
	u_char cookie[8];
	u_int supported_ciphers;
	u_int server_flags, client_flags;
	int payload_len, clen, sum_len = 0;
	u_int32_t rand = 0;

	debug("Waiting for server public key.");

	/* Wait for a public key packet from the server. */
	packet_read_expect(&payload_len, SSH_SMSG_PUBLIC_KEY);

	/* Get cookie from the packet. */
	for (i = 0; i < 8; i++)
		cookie[i] = packet_get_char();

	/* Get the public key. */
	public_key = RSA_new();
	bits = packet_get_int();/* bits */
	public_key->e = BN_new();
	packet_get_bignum(public_key->e, &clen);
	sum_len += clen;
	public_key->n = BN_new();
	packet_get_bignum(public_key->n, &clen);
	sum_len += clen;

	rbits = BN_num_bits(public_key->n);
	if (bits != rbits) {
		log("Warning: Server lies about size of server public key: "
		    "actual size is %d bits vs. announced %d.", rbits, bits);
		log("Warning: This may be due to an old implementation of ssh.");
	}
	/* Get the host key. */
	host_key = RSA_new();
	bits = packet_get_int();/* bits */
	host_key->e = BN_new();
	packet_get_bignum(host_key->e, &clen);
	sum_len += clen;
	host_key->n = BN_new();
	packet_get_bignum(host_key->n, &clen);
	sum_len += clen;

	rbits = BN_num_bits(host_key->n);
	if (bits != rbits) {
		log("Warning: Server lies about size of server host key: "
		    "actual size is %d bits vs. announced %d.", rbits, bits);
		log("Warning: This may be due to an old implementation of ssh.");
	}

	/* Get protocol flags. */
	server_flags = packet_get_int();
	packet_set_protocol_flags(server_flags);

	supported_ciphers = packet_get_int();
	supported_authentications = packet_get_int();

	debug("Received server public key (%d bits) and host key (%d bits).",
	      BN_num_bits(public_key->n), BN_num_bits(host_key->n));

	packet_integrity_check(payload_len,
			       8 + 4 + sum_len + 0 + 4 + 0 + 0 + 4 + 4 + 4,
			       SSH_SMSG_PUBLIC_KEY);
	k.type = KEY_RSA1;
	k.rsa = host_key;
	check_host_key(host, hostaddr, &k,
	    options.user_hostfile, options.system_hostfile);

	client_flags = SSH_PROTOFLAG_SCREEN_NUMBER | SSH_PROTOFLAG_HOST_IN_FWD_OPEN;

	compute_session_id(session_id, cookie, host_key->n, public_key->n);

	/* Generate a session key. */
	arc4random_stir();

	/*
	 * Generate an encryption key for the session.   The key is a 256 bit
	 * random number, interpreted as a 32-byte key, with the least
	 * significant 8 bits being the first byte of the key.
	 */
	for (i = 0; i < 32; i++) {
		if (i % 4 == 0)
			rand = arc4random();
		session_key[i] = rand & 0xff;
		rand >>= 8;
	}

	/*
	 * According to the protocol spec, the first byte of the session key
	 * is the highest byte of the integer.  The session key is xored with
	 * the first 16 bytes of the session id.
	 */
	key = BN_new();
	BN_set_word(key, 0);
	for (i = 0; i < SSH_SESSION_KEY_LENGTH; i++) {
		BN_lshift(key, key, 8);
		if (i < 16)
			BN_add_word(key, session_key[i] ^ session_id[i]);
		else
			BN_add_word(key, session_key[i]);
	}

	/*
	 * Encrypt the integer using the public key and host key of the
	 * server (key with smaller modulus first).
	 */
	if (BN_cmp(public_key->n, host_key->n) < 0) {
		/* Public key has smaller modulus. */
		if (BN_num_bits(host_key->n) <
		    BN_num_bits(public_key->n) + SSH_KEY_BITS_RESERVED) {
			fatal("respond_to_rsa_challenge: host_key %d < public_key %d + "
			      "SSH_KEY_BITS_RESERVED %d",
			      BN_num_bits(host_key->n),
			      BN_num_bits(public_key->n),
			      SSH_KEY_BITS_RESERVED);
		}
		rsa_public_encrypt(key, key, public_key);
		rsa_public_encrypt(key, key, host_key);
	} else {
		/* Host key has smaller modulus (or they are equal). */
		if (BN_num_bits(public_key->n) <
		    BN_num_bits(host_key->n) + SSH_KEY_BITS_RESERVED) {
			fatal("respond_to_rsa_challenge: public_key %d < host_key %d + "
			      "SSH_KEY_BITS_RESERVED %d",
			      BN_num_bits(public_key->n),
			      BN_num_bits(host_key->n),
			      SSH_KEY_BITS_RESERVED);
		}
		rsa_public_encrypt(key, key, host_key);
		rsa_public_encrypt(key, key, public_key);
	}

	/* Destroy the public keys since we no longer need them. */
	RSA_free(public_key);
	RSA_free(host_key);

	if (options.cipher == SSH_CIPHER_NOT_SET) {
		if (cipher_mask_ssh1(1) & supported_ciphers & (1 << ssh_cipher_default))
			options.cipher = ssh_cipher_default;
	} else if (options.cipher == SSH_CIPHER_ILLEGAL ||
	    !(cipher_mask_ssh1(1) & (1 << options.cipher))) {
		log("No valid SSH1 cipher, using %.100s instead.",
		    cipher_name(ssh_cipher_default));
		options.cipher = ssh_cipher_default;
	}
	/* Check that the selected cipher is supported. */
	if (!(supported_ciphers & (1 << options.cipher)))
		fatal("Selected cipher type %.100s not supported by server.",
		      cipher_name(options.cipher));

	debug("Encryption type: %.100s", cipher_name(options.cipher));

	/* Send the encrypted session key to the server. */
	packet_start(SSH_CMSG_SESSION_KEY);
	packet_put_char(options.cipher);

	/* Send the cookie back to the server. */
	for (i = 0; i < 8; i++)
		packet_put_char(cookie[i]);

	/* Send and destroy the encrypted encryption key integer. */
	packet_put_bignum(key);
	BN_clear_free(key);

	/* Send protocol flags. */
	packet_put_int(client_flags);

	/* Send the packet now. */
	packet_send();
	packet_write_wait();

	debug("Sent encrypted session key.");

	/* Set the encryption key. */
	packet_set_encryption_key(session_key, SSH_SESSION_KEY_LENGTH, options.cipher);

	/* We will no longer need the session key here.  Destroy any extra copies. */
	memset(session_key, 0, sizeof(session_key));

	/*
	 * Expect a success message from the server.  Note that this message
	 * will be received in encrypted form.
	 */
	packet_read_expect(&payload_len, SSH_SMSG_SUCCESS);

	debug("Received encrypted confirmation.");
}

/*
 * Authenticate user
 */
void
ssh_userauth(
    const char *local_user,
    const char *server_user,
    char *host,
    int host_key_valid, RSA *own_host_key)
{
	int i, type;
	int payload_len;

	if (supported_authentications == 0)
		fatal("ssh_userauth: server supports no auth methods");

	/* Send the name of the user to log in as on the server. */
	packet_start(SSH_CMSG_USER);
	packet_put_string(server_user, strlen(server_user));
	packet_send();
	packet_write_wait();

	/*
	 * The server should respond with success if no authentication is
	 * needed (the user has no password).  Otherwise the server responds
	 * with failure.
	 */
	type = packet_read(&payload_len);

	/* check whether the connection was accepted without authentication. */
	if (type == SSH_SMSG_SUCCESS)
		return;
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error: got %d in response to SSH_CMSG_USER",
				  type);

#ifdef AFS
	/* Try Kerberos tgt passing if the server supports it. */
	if ((supported_authentications & (1 << SSH_PASS_KERBEROS_TGT)) &&
	    options.kerberos_tgt_passing) {
		if (options.cipher == SSH_CIPHER_NONE)
			log("WARNING: Encryption is disabled! Ticket will be transmitted in the clear!");
		(void) send_kerberos_tgt();
	}
	/* Try AFS token passing if the server supports it. */
	if ((supported_authentications & (1 << SSH_PASS_AFS_TOKEN)) &&
	    options.afs_token_passing && k_hasafs()) {
		if (options.cipher == SSH_CIPHER_NONE)
			log("WARNING: Encryption is disabled! Token will be transmitted in the clear!");
		send_afs_tokens();
	}
#endif /* AFS */

#ifdef KRB4
	if ((supported_authentications & (1 << SSH_AUTH_KERBEROS)) &&
	    options.kerberos_authentication) {
		debug("Trying Kerberos authentication.");
		if (try_kerberos_authentication()) {
			/* The server should respond with success or failure. */
			type = packet_read(&payload_len);
			if (type == SSH_SMSG_SUCCESS)
				return;
			if (type != SSH_SMSG_FAILURE)
				packet_disconnect("Protocol error: got %d in response to Kerberos auth", type);
		}
	}
#endif /* KRB4 */

	/*
	 * Use rhosts authentication if running in privileged socket and we
	 * do not wish to remain anonymous.
	 */
	if ((supported_authentications & (1 << SSH_AUTH_RHOSTS)) &&
	    options.rhosts_authentication) {
		debug("Trying rhosts authentication.");
		packet_start(SSH_CMSG_AUTH_RHOSTS);
		packet_put_string(local_user, strlen(local_user));
		packet_send();
		packet_write_wait();

		/* The server should respond with success or failure. */
		type = packet_read(&payload_len);
		if (type == SSH_SMSG_SUCCESS)
			return;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response to rhosts auth",
					  type);
	}
	/*
	 * Try .rhosts or /etc/hosts.equiv authentication with RSA host
	 * authentication.
	 */
	if ((supported_authentications & (1 << SSH_AUTH_RHOSTS_RSA)) &&
	    options.rhosts_rsa_authentication && host_key_valid) {
		if (try_rhosts_rsa_authentication(local_user, own_host_key))
			return;
	}
	/* Try RSA authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_RSA)) &&
	    options.rsa_authentication) {
		/*
		 * Try RSA authentication using the authentication agent. The
		 * agent is tried first because no passphrase is needed for
		 * it, whereas identity files may require passphrases.
		 */
		if (try_agent_authentication())
			return;

		/* Try RSA authentication for each identity. */
		for (i = 0; i < options.num_identity_files; i++)
			if (options.identity_files_type[i] == KEY_RSA1 &&
			    try_rsa_authentication(options.identity_files[i]))
				return;
	}
	/* Try challenge response authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_TIS)) &&
	    options.challenge_reponse_authentication && !options.batch_mode) {
		if (try_challenge_reponse_authentication())
			return;
	}
	/* Try password authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_PASSWORD)) &&
	    options.password_authentication && !options.batch_mode) {
		char prompt[80];

		snprintf(prompt, sizeof(prompt), "%.30s@%.128s's password: ",
		    server_user, host);
		if (try_password_authentication(prompt))
			return;
	}
	/* All authentication methods have failed.  Exit with an error message. */
	fatal("Permission denied.");
	/* NOTREACHED */
}
