/*
 * Copyright (c) 1997-1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "rsh_locl.h"
RCSID("$Id: rshd.c,v 1.1.1.1.4.2 2000/06/16 18:32:06 thorpej Exp $");

enum auth_method auth_method;

krb5_context context;
krb5_keyblock *keyblock;
krb5_crypto crypto;

#ifdef KRB4
des_key_schedule schedule;
des_cblock iv;
#endif

krb5_ccache ccache, ccache2;
int kerberos_status = 0;

int do_encrypt = 0;

static int do_unique_tkfile           = 0;
static char tkfile[MAXPATHLEN] = "";

static int do_inetd = 1;
static char *port_str;
static int do_rhosts;
static int do_kerberos = 0;
static int do_vacuous = 0;
static int do_log = 1;
static int do_newpag = 1;
static int do_version;
static int do_help = 0;

static void
syslog_and_die (const char *m, ...)
{
    va_list args;

    va_start(args, m);
    vsyslog (LOG_ERR, m, args);
    va_end(args);
    exit (1);
}

static void
fatal (int sock, const char *m, ...)
{
    va_list args;
    char buf[BUFSIZ];
    size_t len;

    *buf = 1;
    va_start(args, m);
    len = vsnprintf (buf + 1, sizeof(buf) - 1, m, args);
    va_end(args);
    syslog (LOG_ERR, buf + 1);
    net_write (sock, buf, len + 1);
    exit (1);
}

static void
read_str (int s, char *str, size_t sz, char *expl)
{
    while (sz > 0) {
	if (net_read (s, str, 1) != 1)
	    syslog_and_die ("read: %m");
	if (*str == '\0')
	    return;
	--sz;
	++str;
    }
    fatal (s, "%s too long", expl);
}

static int
recv_bsd_auth (int s, u_char *buf,
	       struct sockaddr_in *thisaddr,
	       struct sockaddr_in *thataddr,
	       char *client_username,
	       char *server_username,
	       char *cmd)
{
    struct passwd *pwd;

    read_str (s, client_username, USERNAME_SZ, "local username");
    read_str (s, server_username, USERNAME_SZ, "remote username");
    read_str (s, cmd, COMMAND_SZ, "command");
    pwd = getpwnam(server_username);
    if (pwd == NULL)
	fatal(s, "Login incorrect.");
    if (iruserok(thataddr->sin_addr.s_addr, pwd->pw_uid == 0,
		 client_username, server_username))
	fatal(s, "Login incorrect.");
    return 0;
}

#ifdef KRB4
static int
recv_krb4_auth (int s, u_char *buf,
		struct sockaddr *thisaddr,
		struct sockaddr *thataddr,
		char *client_username,
		char *server_username,
		char *cmd)
{
    int status;
    int32_t options;
    KTEXT_ST ticket;
    AUTH_DAT auth;
    char instance[INST_SZ + 1];
    char version[KRB_SENDAUTH_VLEN + 1];

    if (memcmp (buf, KRB_SENDAUTH_VERS, 4) != 0)
	return -1;
    if (net_read (s, buf + 4, KRB_SENDAUTH_VLEN - 4) !=
	KRB_SENDAUTH_VLEN - 4)
	syslog_and_die ("reading auth info: %m");
    if (memcmp (buf, KRB_SENDAUTH_VERS, KRB_SENDAUTH_VLEN) != 0)
	syslog_and_die("unrecognized auth protocol: %.8s", buf);

    options = KOPT_IGNORE_PROTOCOL;
    if (do_encrypt)
	options |= KOPT_DO_MUTUAL;
    k_getsockinst (s, instance, sizeof(instance));
    status = krb_recvauth (options,
			   s,
			   &ticket,
			   "rcmd",
			   instance,
			   (struct sockaddr_in *)thataddr,
			   (struct sockaddr_in *)thisaddr,
			   &auth,
			   "",
			   schedule,
			   version);
    if (status != KSUCCESS)
	syslog_and_die ("recvauth: %s", krb_get_err_text(status));
    if (strncmp (version, KCMD_VERSION, KRB_SENDAUTH_VLEN) != 0)
	syslog_and_die ("bad version: %s", version);

    read_str (s, server_username, USERNAME_SZ, "remote username");
    if (kuserok (&auth, server_username) != 0)
	fatal (s, "Permission denied");
    read_str (s, cmd, COMMAND_SZ, "command");

    syslog(LOG_INFO|LOG_AUTH,
	   "kerberos v4 shell from %s on %s as %s, cmd '%.80s'",
	   krb_unparse_name_long(auth.pname, auth.pinst, auth.prealm),

	   inet_ntoa(((struct sockaddr_in *)thataddr)->sin_addr),
	   server_username,
	   cmd);

    memcpy (iv, auth.session, sizeof(iv));

    return 0;
}

#endif /* KRB4 */

static int 
save_krb5_creds (int s,
                 krb5_auth_context auth_context,
                 krb5_principal client)

{
    int ret;
    krb5_data remote_cred;
 
    krb5_data_zero (&remote_cred);
    ret= krb5_read_message (context, (void *)&s, &remote_cred);
    if (ret) {
	krb5_data_free(&remote_cred);
	return 0;
    }
    if (remote_cred.length == 0)
	return 0;
 
    ret = krb5_cc_gen_new(context, &krb5_mcc_ops, &ccache);
    if (ret) {
	krb5_data_free(&remote_cred);
	return 0;
    }
  
    krb5_cc_initialize(context,ccache,client);
    ret = krb5_rd_cred(context, auth_context, ccache,&remote_cred);
    krb5_data_free (&remote_cred);
    if (ret)
	return 0;
    return 1;
}

static void
krb5_start_session (void)
{
    krb5_error_code ret;

    ret = krb5_cc_resolve (context, tkfile, &ccache2);
    if (ret) {
	krb5_cc_destroy(context, ccache);
	return;
    }

    ret = krb5_cc_copy_cache (context, ccache, ccache2);
    if (ret) {
	krb5_cc_destroy(context, ccache);
	return ;
    }

    krb5_cc_close(context, ccache2);
    krb5_cc_destroy(context, ccache);
    return;
}

static int
recv_krb5_auth (int s, u_char *buf,
		struct sockaddr *thisaddr,
		struct sockaddr *thataddr,
		char *client_username,
		char *server_username,
		char *cmd)
{
    u_int32_t len;
    krb5_auth_context auth_context = NULL;
    krb5_ticket *ticket;
    krb5_error_code status;
    krb5_data cksum_data;
    krb5_principal server;

    if (memcmp (buf, "\x00\x00\x00\x13", 4) != 0)
	return -1;
    len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
	
    if (net_read(s, buf, len) != len)
	syslog_and_die ("reading auth info: %m");
    if (len != sizeof(KRB5_SENDAUTH_VERSION)
	|| memcmp (buf, KRB5_SENDAUTH_VERSION, len) != 0)
	syslog_and_die ("bad sendauth version: %.8s", buf);
    
    status = krb5_sock_to_principal (context,
				     s,
				     "host",
				     KRB5_NT_SRV_HST,
				     &server);
    if (status)
	syslog_and_die ("krb5_sock_to_principal: %s",
			krb5_get_err_text(context, status));

    status = krb5_recvauth(context,
			   &auth_context,
			   &s,
			   KCMD_VERSION,
			   server,
			   KRB5_RECVAUTH_IGNORE_VERSION,
			   NULL,
			   &ticket);
    krb5_free_principal (context, server);
    if (status)
	syslog_and_die ("krb5_recvauth: %s",
			krb5_get_err_text(context, status));

    read_str (s, server_username, USERNAME_SZ, "remote username");
    read_str (s, cmd, COMMAND_SZ, "command");
    read_str (s, client_username, COMMAND_SZ, "local username");

    status = krb5_auth_con_getkey (context, auth_context, &keyblock);
    if (status)
       syslog_and_die ("krb5_auth_con_getkey: %s",
                       krb5_get_err_text(context, status));

    status = krb5_crypto_init(context, keyblock, 0, &crypto);
    if(status)
	syslog_and_die("krb5_crypto_init: %s", 
		       krb5_get_err_text(context, status));

    
    cksum_data.length = asprintf ((char **)&cksum_data.data,
				  "%u:%s%s",
				  ntohs(socket_get_port (thisaddr)),
				  cmd,
				  server_username);

    status = krb5_verify_authenticator_checksum(context, 
						auth_context,
						cksum_data.data, 
						cksum_data.length);

    if (status)
	syslog_and_die ("krb5_verify_authenticator_checksum: %s",
			krb5_get_err_text(context, status));

    free (cksum_data.data);

    if (strncmp (client_username, "-u ", 3) == 0) {
	do_unique_tkfile = 1;
	memmove (client_username, client_username + 3,
		 strlen(client_username) - 2);
    }

    if (strncmp (client_username, "-U ", 3) == 0) {
	char *end, *temp_tkfile;

	do_unique_tkfile = 1;
	if (strncmp (server_username + 3, "FILE:", 5) == 0) {
	    temp_tkfile = tkfile;
	} else {
	    strcpy (tkfile, "FILE:");
	    temp_tkfile = tkfile + 5;
	}
	end = strchr(client_username + 3,' ');
	strncpy(temp_tkfile, client_username + 3, end - client_username - 3);
	temp_tkfile[end - client_username - 3] = '\0';
	memmove (client_username, end +1, strlen(end+1)+1);
    }

    kerberos_status = save_krb5_creds (s, auth_context, ticket->client);

    if(!krb5_kuserok (context,
		     ticket->client,
		     server_username))
	fatal (s, "Permission denied");

    if (strncmp (cmd, "-x ", 3) == 0) {
	do_encrypt = 1;
	memmove (cmd, cmd + 3, strlen(cmd) - 2);
    } else {
	do_encrypt = 0;
    }

    {
	char *name;

	if (krb5_unparse_name (context, ticket->client, &name) == 0) {
	    char addr_str[256];

	    if (inet_ntop (thataddr->sa_family,
			   socket_get_address (thataddr),
			   addr_str, sizeof(addr_str)) == NULL)
		strlcpy (addr_str, "unknown address",
				 sizeof(addr_str));

	    syslog(LOG_INFO|LOG_AUTH,
		   "kerberos v5 shell from %s on %s as %s, cmd '%.80s'",
		   name,
		   addr_str,
		   server_username,
		   cmd);
	    free (name);
	}
    }	   

    return 0;
}

static void
loop (int from0, int to0,
      int to1,   int from1,
      int to2,   int from2)
{
    fd_set real_readset;
    int max_fd;
    int count = 2;

    FD_ZERO(&real_readset);
    FD_SET(from0, &real_readset);
    FD_SET(from1, &real_readset);
    FD_SET(from2, &real_readset);
    max_fd = max(from0, max(from1, from2)) + 1;
    for (;;) {
	int ret;
	fd_set readset = real_readset;
	char buf[RSH_BUFSIZ];

	ret = select (max_fd, &readset, NULL, NULL, NULL);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    else
		syslog_and_die ("select: %m");
	}
	if (FD_ISSET(from0, &readset)) {
	    ret = do_read (from0, buf, sizeof(buf));
	    if (ret < 0)
		syslog_and_die ("read: %m");
	    else if (ret == 0) {
		close (from0);
		close (to0);
		FD_CLR(from0, &real_readset);
	    } else
		net_write (to0, buf, ret);
	}
	if (FD_ISSET(from1, &readset)) {
	    ret = read (from1, buf, sizeof(buf));
	    if (ret < 0)
		syslog_and_die ("read: %m");
	    else if (ret == 0) {
		close (from1);
		close (to1);
		FD_CLR(from1, &real_readset);
		if (--count == 0)
		    exit (0);
	    } else
		do_write (to1, buf, ret);
	}
	if (FD_ISSET(from2, &readset)) {
	    ret = read (from2, buf, sizeof(buf));
	    if (ret < 0)
		syslog_and_die ("read: %m");
	    else if (ret == 0) {
		close (from2);
		close (to2);
		FD_CLR(from2, &real_readset);
		if (--count == 0)
		    exit (0);
	    } else
		do_write (to2, buf, ret);
	}
   }
}

/*
 * Used by `setup_copier' to create some pipe-like means of
 * communcation.  Real pipes would probably be the best thing, but
 * then the shell doesn't understand it's talking to rshd.  If
 * socketpair doesn't work everywhere, some autoconf magic would have
 * to be added here.
 *
 * If it fails creating the `pipe', it aborts by calling fatal.
 */

static void
pipe_a_like (int fd[2])
{
    if (socketpair (AF_UNIX, SOCK_STREAM, 0, fd) < 0)
	fatal (STDOUT_FILENO, "socketpair: %m");
}

/*
 * Start a child process and leave the parent copying data to and from it.  */

static void
setup_copier (void)
{
    int p0[2], p1[2], p2[2];
    pid_t pid;

    pipe_a_like(p0);
    pipe_a_like(p1);
    pipe_a_like(p2);
    pid = fork ();
    if (pid < 0)
	fatal (STDOUT_FILENO, "fork: %m");
    if (pid == 0) { /* child */
	close (p0[1]);
	close (p1[0]);
	close (p2[0]);
	dup2 (p0[0], STDIN_FILENO);
	dup2 (p1[1], STDOUT_FILENO);
	dup2 (p2[1], STDERR_FILENO);
	close (p0[0]);
	close (p1[1]);
	close (p2[1]);
    } else { /* parent */
	close (p0[0]);
	close (p1[1]);
	close (p2[1]);

	if (net_write (STDOUT_FILENO, "", 1) != 1)
	    fatal (STDOUT_FILENO, "write failed");

	loop (STDIN_FILENO, p0[1],
	      STDOUT_FILENO, p1[0],
	      STDERR_FILENO, p2[0]);
    }
}

/*
 * Is `port' a ``reserverd'' port?
 */

static int
is_reserved(u_short port)
{
    return ntohs(port) < IPPORT_RESERVED;
}

/*
 * Set the necessary part of the environment in `env'.
 */

static void
setup_environment (char *env[7], struct passwd *pwd)
{
    asprintf (&env[0], "USER=%s",  pwd->pw_name);
    asprintf (&env[1], "HOME=%s",  pwd->pw_dir);
    asprintf (&env[2], "SHELL=%s", pwd->pw_shell);
    asprintf (&env[3], "PATH=%s",  _PATH_DEFPATH);
    asprintf (&env[4], "SSH_CLIENT=only_to_make_bash_happy");
    if (do_unique_tkfile)
       asprintf (&env[5], "KRB5CCNAME=%s", tkfile);
       else env[5] = NULL;
    env[6] = NULL;
}

static void
doit (int do_kerberos, int check_rhosts)
{
    u_char buf[BUFSIZ];
    u_char *p;
    struct sockaddr_storage thisaddr_ss;
    struct sockaddr *thisaddr = (struct sockaddr *)&thisaddr_ss;
    struct sockaddr_storage thataddr_ss;
    struct sockaddr *thataddr = (struct sockaddr *)&thataddr_ss;
    struct sockaddr_storage erraddr_ss;
    struct sockaddr *erraddr = (struct sockaddr *)&erraddr_ss;
    int addrlen;
    int port;
    int errsock = -1;
    char client_user[COMMAND_SZ], server_user[USERNAME_SZ];
    char cmd[COMMAND_SZ];
    struct passwd *pwd;
    int s = STDIN_FILENO;
    char *env[7];

    addrlen = sizeof(thisaddr_ss);
    if (getsockname (s, thisaddr, &addrlen) < 0)
	syslog_and_die("getsockname: %m");
    addrlen = sizeof(thataddr_ss);
    if (getpeername (s, thataddr, &addrlen) < 0)
	syslog_and_die ("getpeername: %m");

    if (!do_kerberos && !is_reserved(socket_get_port(thataddr)))
	fatal(s, "Permission denied");

    p = buf;
    port = 0;
    for(;;) {
	if (net_read (s, p, 1) != 1)
	    syslog_and_die ("reading port number: %m");
	if (*p == '\0')
	    break;
	else if (isdigit(*p))
	    port = port * 10 + *p - '0';
	else
	    syslog_and_die ("non-digit in port number: %c", *p);
    }

    if (!do_kerberos && !is_reserved(htons(port)))
	fatal(s, "Permission denied");

    if (port) {
	int priv_port = IPPORT_RESERVED - 1;

	/* 
	 * There's no reason to require a ``privileged'' port number
	 * here, but for some reason the brain dead rsh clients
	 * do... :-(
	 */

	erraddr->sa_family = thataddr->sa_family;
	socket_set_address_and_port (erraddr,
				     socket_get_address (thataddr),
				     htons(port));

	/*
	 * we only do reserved port for IPv4
	 */

	if (erraddr->sa_family == AF_INET)
	    errsock = rresvport (&priv_port);
	else
	    errsock = socket (erraddr->sa_family, SOCK_STREAM, 0);
	if (errsock < 0)
	    syslog_and_die ("socket: %m");
	if (connect (errsock,
		     erraddr,
		     socket_sockaddr_size (erraddr)) < 0)
	    syslog_and_die ("connect: %m");
    }
    
    if(do_kerberos) {
	if (net_read (s, buf, 4) != 4)
	    syslog_and_die ("reading auth info: %m");
    
#ifdef KRB4
	if (recv_krb4_auth (s, buf, thisaddr, thataddr,
			    client_user,
			    server_user,
			    cmd) == 0)
	    auth_method = AUTH_KRB4;
	else
#endif /* KRB4 */
	    if(recv_krb5_auth (s, buf, thisaddr, thataddr,
			       client_user,
			       server_user,
			       cmd) == 0)
		auth_method = AUTH_KRB5;
	    else
		syslog_and_die ("unrecognized auth protocol: %x %x %x %x",
				buf[0], buf[1], buf[2], buf[3]);
    } else {
	if(recv_bsd_auth (s, buf,
			  (struct sockaddr_in *)thisaddr,
			  (struct sockaddr_in *)thataddr,
			  client_user,
			  server_user,
			  cmd) == 0) {
	    auth_method = AUTH_BROKEN;
	    if(do_vacuous) {
		printf("Remote host requires Kerberos authentication\n");
		exit(0);
	    }
	} else
	    syslog_and_die("recv_bsd_auth failed");
    }

    pwd = getpwnam (server_user);
    if (pwd == NULL)
	fatal (s, "Login incorrect.");

    if (*pwd->pw_shell == '\0')
	pwd->pw_shell = _PATH_BSHELL;

    if (pwd->pw_uid != 0 && access (_PATH_NOLOGIN, F_OK) == 0)
	fatal (s, "Login disabled.");

#ifdef HAVE_GETSPNAM
    {
	struct spwd *sp;
	long    today;
    
	sp = getspnam(server_user);
	today = time(0)/(24L * 60 * 60);
	if (sp->sp_expire > 0) 
	    if (today > sp->sp_expire) 
		fatal(s, "Account has expired.");
    }
#endif
    
#ifdef HAVE_SETLOGIN
    if (setlogin(pwd->pw_name) < 0)
	syslog(LOG_ERR, "setlogin() failed: %m");
#endif

#ifdef HAVE_SETPCRED
    if (setpcred (pwd->pw_name, NULL) == -1)
	syslog(LOG_ERR, "setpcred() failure: %m");
#endif /* HAVE_SETPCRED */
    if (initgroups (pwd->pw_name, pwd->pw_gid) < 0)
	fatal (s, "Login incorrect.");

    if (setgid(pwd->pw_gid) < 0)
	fatal (s, "Login incorrect.");

    if (setuid (pwd->pw_uid) < 0)
	fatal (s, "Login incorrect.");

#ifdef KRB5
    {
	int fd;
 
	if (!do_unique_tkfile)
	    snprintf(tkfile,sizeof(tkfile),"FILE:/tmp/krb5cc_%u",pwd->pw_uid);
	else if (*tkfile=='\0') {
	    snprintf(tkfile,sizeof(tkfile),"FILE:/tmp/krb5cc_XXXXXX");
	    fd = mkstemp(tkfile+5);
	    close(fd);
	    unlink(tkfile+5);
	}
 
	if (kerberos_status)
	    krb5_start_session();
    }
#endif

    if (chdir (pwd->pw_dir) < 0)
	fatal (s, "Remote directory.");

    if (errsock >= 0) {
	if (dup2 (errsock, STDERR_FILENO) < 0)
	    fatal (s, "Dup2 failed.");
	close (errsock);
    }

    setup_environment (env, pwd);

    if (do_encrypt) {
	setup_copier ();
    } else {
	if (net_write (s, "", 1) != 1)
	    fatal (s, "write failed");
    }

#ifdef KRB4
    if(k_hasafs()) {
	char cell[64];

	if(do_newpag)
	    k_setpag();
	if (k_afs_cell_of_file (pwd->pw_dir, cell, sizeof(cell)) == 0)
	    krb_afslog_uid_home (cell, NULL, pwd->pw_uid, pwd->pw_dir);

	krb_afslog_uid_home(NULL, NULL, pwd->pw_uid, pwd->pw_dir);

#ifdef KRB5
	/* XXX */
       {
	   krb5_ccache ccache;
	   krb5_error_code status;

	   status = krb5_cc_resolve (context, tkfile, &ccache);
	   if (!status) {
	       krb5_afslog_uid_home(context,ccache,NULL,NULL,
				    pwd->pw_uid, pwd->pw_dir);
	       krb5_cc_close (context, ccache);
	   }
       }
#endif /* KRB5 */
    }
#endif /* KRB4 */
    execle (pwd->pw_shell, pwd->pw_shell, "-c", cmd, NULL, env);
    err(1, "exec %s", pwd->pw_shell);
}

struct getargs args[] = {
    { "inetd",		'i',	arg_negative_flag,	&do_inetd,
      "Not started from inetd" },
    { "kerberos",	'k',	arg_flag,	&do_kerberos,
      "Implement kerberised services" },
    { "encrypt",	'x',	arg_flag,		&do_encrypt,
      "Implement encrypted service" },
    { "rhosts",		'l',	arg_flag, &do_rhosts,
      "Check users .rhosts" },
    { "port",		'p',	arg_string,	&port_str,	"Use this port",
      "port" },
    { "vacuous",	'v',	arg_flag, &do_vacuous,
      "Don't accept non-kerberised connections" },
    { NULL,		'P',	arg_negative_flag, &do_newpag,
      "Don't put process in new PAG" },
    /* compatibility flag: */
    { NULL,		'L',	arg_flag, &do_log },
    { "version",	0, 	arg_flag,		&do_version },
    { "help",		0, 	arg_flag,		&do_help }
};

static void
usage (int ret)
{
    if(isatty(STDIN_FILENO))
	arg_printusage (args,
			sizeof(args) / sizeof(args[0]),
			NULL,
			"");
    else
	syslog (LOG_ERR, "Usage: %s [-ikxlvPL] [-p port]", __progname);
    exit (ret);
}


int
main(int argc, char **argv)
{
    int optind = 0;
    int port = 0;

    set_progname (argv[0]);
    roken_openlog ("rshd", LOG_ODELAY | LOG_PID, LOG_AUTH);

    if (getarg(args, sizeof(args) / sizeof(args[0]), argc, argv,
	       &optind))
	usage(1);

    if(do_help)
	usage (0);

    if (do_version) {
	print_version(NULL);
	exit(0);
    }

#ifdef KRB5
    krb5_init_context (&context);
#endif

    if(port_str) {
	struct servent *s = roken_getservbyname (port_str, "tcp");

	if (s)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		syslog_and_die("Bad port `%s'", port_str);
	    port = htons(port);
	}
    }

    if (do_encrypt)
	do_kerberos = 1;

    if (!do_inetd) {
	if (port == 0) {
	    if (do_kerberos) {
		if (do_encrypt)
		    port = krb5_getportbyname (context, "ekshell", "tcp", 545);
		else
		    port = krb5_getportbyname (context, "kshell",  "tcp", 544);
	    } else {
		port = krb5_getportbyname(context, "shell", "tcp", 514);
	    }
	}
	mini_inetd (port);
    }

    signal (SIGPIPE, SIG_IGN);

    doit (do_kerberos, do_rhosts);
    return 0;
}
