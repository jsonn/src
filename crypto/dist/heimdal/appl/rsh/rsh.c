/*
 * Copyright (c) 1997, 1998, 1999 Kungliga Tekniska H�gskolan
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
RCSID("$Id: rsh.c,v 1.1.1.1.4.2 2000/06/16 18:32:06 thorpej Exp $");

enum auth_method auth_method;
int do_encrypt;
int do_forward;
int do_forwardable;
int do_unique_tkfile = 0;
char *unique_tkfile  = NULL;
char tkfile[MAXPATHLEN];
krb5_context context;
krb5_keyblock *keyblock;
krb5_crypto crypto;
#ifdef KRB4
des_key_schedule schedule;
des_cblock iv;
#endif


/*
 *
 */

static int input = 1;		/* Read from stdin */

static int
loop (int s, int errsock)
{
    fd_set real_readset;
    int count = 1;

    FD_ZERO(&real_readset);
    FD_SET(s, &real_readset);
    if (errsock != -1) {
	FD_SET(errsock, &real_readset);
	++count;
    }
    if(input)
	FD_SET(STDIN_FILENO, &real_readset);

    for (;;) {
	int ret;
	fd_set readset;
	char buf[RSH_BUFSIZ];

	readset = real_readset;
	ret = select (max(s, errsock) + 1, &readset, NULL, NULL, NULL);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    else
		err (1, "select");
	}
	if (FD_ISSET(s, &readset)) {
	    ret = do_read (s, buf, sizeof(buf));
	    if (ret < 0)
		err (1, "read");
	    else if (ret == 0) {
		close (s);
		FD_CLR(s, &real_readset);
		if (--count == 0)
		    return 0;
	    } else
		net_write (STDOUT_FILENO, buf, ret);
	}
	if (errsock != -1 && FD_ISSET(errsock, &readset)) {
	    ret = do_read (errsock, buf, sizeof(buf));
	    if (ret < 0)
		err (1, "read");
	    else if (ret == 0) {
		close (errsock);
		FD_CLR(errsock, &real_readset);
		if (--count == 0)
		    return 0;
	    } else
		net_write (STDERR_FILENO, buf, ret);
	}
	if (FD_ISSET(STDIN_FILENO, &readset)) {
	    ret = read (STDIN_FILENO, buf, sizeof(buf));
	    if (ret < 0)
		err (1, "read");
	    else if (ret == 0) {
		close (STDIN_FILENO);
		FD_CLR(STDIN_FILENO, &real_readset);
		shutdown (s, SHUT_WR);
	    } else
		do_write (s, buf, ret);
	}
    }
}

#ifdef KRB4
static int
send_krb4_auth(int s,
	       struct sockaddr *thisaddr,
	       struct sockaddr *thataddr,
	       const char *hostname,
	       const char *remote_user,
	       const char *local_user,
	       size_t cmd_len,
	       const char *cmd)
{
    KTEXT_ST text;
    CREDENTIALS cred;
    MSG_DAT msg;
    int status;
    size_t len;

    status = krb_sendauth (do_encrypt ? KOPT_DO_MUTUAL : 0,
			   s, &text, "rcmd",
			   (char *)hostname, krb_realmofhost (hostname),
			   getpid(), &msg, &cred, schedule,
			   (struct sockaddr_in *)thisaddr,
			   (struct sockaddr_in *)thataddr,
			   KCMD_VERSION);
    if (status != KSUCCESS) {
	warnx ("%s: %s", hostname, krb_get_err_text(status));
	return 1;
    }
    memcpy (iv, cred.session, sizeof(iv));

    len = strlen(remote_user) + 1;
    if (net_write (s, remote_user, len) != len) {
	warn("write");
	return 1;
    }
    if (net_write (s, cmd, cmd_len) != cmd_len) {
	warn("write");
	return 1;
    }
    return 0;
}
#endif /* KRB4 */

/*
 * Send forward information on `s' for host `hostname', them being
 * forwardable themselves if `forwardable'
 */

static int
krb5_forward_cred (krb5_auth_context auth_context,
		   int s,
		   const char *hostname,
		   int forwardable)
{
    krb5_error_code ret;
    krb5_ccache     ccache;
    krb5_creds      creds;
    krb5_kdc_flags  flags;
    krb5_data       out_data;
    krb5_principal  principal;

    memset (&creds, 0, sizeof(creds));

    ret = krb5_cc_default (context, &ccache);
    if (ret) {
	warnx ("could not forward creds: krb5_cc_default: %s",
	       krb5_get_err_text (context, ret));
	return 1;
    }

    ret = krb5_cc_get_principal (context, ccache, &principal);
    if (ret) {
	warnx ("could not forward creds: krb5_cc_get_principal: %s",
	       krb5_get_err_text (context, ret));
	return 1;
    }

    creds.client = principal;
    
    ret = krb5_build_principal (context,
				&creds.server,
				strlen(principal->realm),
				principal->realm,
				"krbtgt",
				principal->realm,
				NULL);

    if (ret) {
	warnx ("could not forward creds: krb5_build_principal: %s",
	       krb5_get_err_text (context, ret));
	return 1;
    }

    creds.times.endtime = 0;

    flags.i = 0;
    flags.b.forwarded   = 1;
    flags.b.forwardable = forwardable;

    ret = krb5_get_forwarded_creds (context,
				    auth_context,
				    ccache,
				    flags.i,
				    hostname,
				    &creds,
				    &out_data);
    if (ret) {
	warnx ("could not forward creds: krb5_get_forwarded_creds: %s",
	       krb5_get_err_text (context, ret));
	return 1;
    }

    ret = krb5_write_message (context,
			      (void *)&s,
			      &out_data);
    krb5_data_free (&out_data);

    if (ret)
	warnx ("could not forward creds: krb5_write_message: %s",
	       krb5_get_err_text (context, ret));
    return 0;
}

static int
send_krb5_auth(int s,
	       struct sockaddr *thisaddr,
	       struct sockaddr *thataddr,
	       const char *hostname,
	       const char *remote_user,
	       const char *local_user,
	       size_t cmd_len,
	       const char *cmd)
{
    krb5_principal server;
    krb5_data cksum_data;
    int status;
    size_t len;
    krb5_auth_context auth_context = NULL;

    status = krb5_sname_to_principal(context,
				     hostname,
				     "host",
				     KRB5_NT_SRV_HST,
				     &server);
    if (status) {
	warnx ("%s: %s", hostname, krb5_get_err_text(context, status));
	return 1;
    }

    cksum_data.length = asprintf ((char **)&cksum_data.data,
				  "%u:%s%s%s",
				  ntohs(socket_get_port(thataddr)),
				  do_encrypt ? "-x " : "",
				  cmd,
				  remote_user);

    status = krb5_sendauth (context,
			    &auth_context,
			    &s,
			    KCMD_VERSION,
			    NULL,
			    server,
			    do_encrypt ? AP_OPTS_MUTUAL_REQUIRED : 0,
			    &cksum_data,
			    NULL,
			    NULL,
			    NULL,
			    NULL,
			    NULL);
    if (status) {
	warnx ("%s: %s", hostname, krb5_get_err_text(context, status));
	return 1;
    }

    status = krb5_auth_con_getkey (context, auth_context, &keyblock);
    if (status) {
	warnx ("krb5_auth_con_getkey: %s", krb5_get_err_text(context, status));
	return 1;
    }

    status = krb5_auth_con_setaddrs_from_fd (context,
					     auth_context,
					     &s);
    if (status) {
        warnx("krb5_auth_con_setaddrs_from_fd: %s",
	      krb5_get_err_text(context, status));
        return(1);
    }

    status = krb5_crypto_init(context, keyblock, 0, &crypto);
    if(status) {
	warnx ("krb5_crypto_init: %s", krb5_get_err_text(context, status));
	return 1;
    }

    len = strlen(remote_user) + 1;
    if (net_write (s, remote_user, len) != len) {
	warn ("write");
	return 1;
    }
    if (do_encrypt && net_write (s, "-x ", 3) != 3) {
	warn ("write");
	return 1;
    }
    if (net_write (s, cmd, cmd_len) != cmd_len) {
	warn ("write");
	return 1;
    }

    if (do_unique_tkfile) {
	if (net_write (s, tkfile, strlen(tkfile)) != strlen(tkfile)) {
	    warn ("write");
	    return 1;
	}
    }
    len = strlen(local_user) + 1;
    if (net_write (s, local_user, len) != len) {
	warn ("write");
	return 1;
    }

    if (!do_forward
	|| krb5_forward_cred (auth_context, s, hostname, do_forwardable)) {
	/* Empty forwarding info */

	u_char zero[4] = {0, 0, 0, 0};
	write (s, &zero, 4);
    }
    krb5_auth_con_free (context, auth_context);
    return 0;
}

static int
send_broken_auth(int s,
		 struct sockaddr *thisaddr,
		 struct sockaddr *thataddr,
		 const char *hostname,
		 const char *remote_user,
		 const char *local_user,
		 size_t cmd_len,
		 const char *cmd)
{
    size_t len;

    len = strlen(local_user) + 1;
    if (net_write (s, local_user, len) != len) {
	warn ("write");
	return 1;
    }
    len = strlen(remote_user) + 1;
    if (net_write (s, remote_user, len) != len) {
	warn ("write");
	return 1;
    }
    if (net_write (s, cmd, cmd_len) != cmd_len) {
	warn ("write");
	return 1;
    }
    return 0;
}

static int
proto (int s, int errsock,
       const char *hostname, const char *local_user, const char *remote_user,
       const char *cmd, size_t cmd_len,
       int (*auth_func)(int s,
			struct sockaddr *this, struct sockaddr *that,
			const char *hostname, const char *remote_user,
			const char *local_user, size_t cmd_len,
			const char *cmd))
{
    int errsock2;
    char buf[BUFSIZ];
    char *p;
    size_t len;
    char reply;
    struct sockaddr_storage thisaddr_ss;
    struct sockaddr *thisaddr = (struct sockaddr *)&thisaddr_ss;
    struct sockaddr_storage thataddr_ss;
    struct sockaddr *thataddr = (struct sockaddr *)&thataddr_ss;
    struct sockaddr_storage erraddr_ss;
    struct sockaddr *erraddr = (struct sockaddr *)&erraddr_ss;
    int addrlen;
    int ret;

    addrlen = sizeof(thisaddr_ss);
    if (getsockname (s, thisaddr, &addrlen) < 0) {
	warn ("getsockname(%s)", hostname);
	return 1;
    }
    addrlen = sizeof(thataddr_ss);
    if (getpeername (s, thataddr, &addrlen) < 0) {
	warn ("getpeername(%s)", hostname);
	return 1;
    }

    if (errsock != -1) {

	addrlen = sizeof(erraddr_ss);
	if (getsockname (errsock, erraddr, &addrlen) < 0) {
	    warn ("getsockname");
	    return 1;
	}

	if (listen (errsock, 1) < 0) {
	    warn ("listen");
	    return 1;
	}

	p = buf;
	snprintf (p, sizeof(buf), "%u",
		  ntohs(socket_get_port(erraddr)));
	len = strlen(buf) + 1;
	if(net_write (s, buf, len) != len) {
	    warn ("write");
	    close (errsock);
	    return 1;
	}

	errsock2 = accept (errsock, NULL, NULL);
	if (errsock2 < 0) {
	    warn ("accept");
	    close (errsock);
	    return 1;
	}
	close (errsock);

    } else {
	if (net_write (s, "0", 2) != 2) {
	    warn ("write");
	    return 1;
	}
	errsock2 = -1;
    }

    if ((*auth_func)(s, thisaddr, thataddr, hostname,
		     remote_user, local_user,
		     cmd_len, cmd)) {
	close (errsock2);
	return 1;
    } 

    ret = net_read (s, &reply, 1);
    if (ret < 0) {
	warn ("read");
	close (errsock2);
	return 1;
    } else if (ret == 0) {
	warnx ("unexpected EOF from %s", hostname);
	close (errsock2);
	return 1;
    }
    if (reply != 0) {

	warnx ("Error from rshd at %s:", hostname);

	while ((ret = read (s, buf, sizeof(buf))) > 0)
	    write (STDOUT_FILENO, buf, ret);
        write (STDOUT_FILENO,"\n",1);
	close (errsock2);
	return 1;
    }

    return loop (s, errsock2);
}

/*
 * Return in `res' a copy of the concatenation of `argc, argv' into
 * malloced space.
 */

static size_t
construct_command (char **res, int argc, char **argv)
{
    int i;
    size_t len = 0;
    char *tmp;

    for (i = 0; i < argc; ++i)
	len += strlen(argv[i]) + 1;
    len = max (1, len);
    tmp = malloc (len);
    if (tmp == NULL)
	errx (1, "malloc %u failed", len);

    *tmp = '\0';
    for (i = 0; i < argc - 1; ++i) {
	strcat (tmp, argv[i]);
	strcat (tmp, " ");
    }
    if (argc > 0)
	strcat (tmp, argv[argc-1]);
    *res = tmp;
    return len;
}

static char *
print_addr (const struct sockaddr_in *sin)
{
    char addr_str[256];
    char *res;

    inet_ntop (AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
    res = strdup(addr_str);
    if (res == NULL)
	errx (1, "malloc: out of memory");
    return res;
}

static int
doit_broken (int argc,
	     char **argv,
	     int optind,
	     const char *host,
	     const char *remote_user,
	     const char *local_user,
	     int port,
	     int priv_socket1,
	     int priv_socket2,
	     const char *cmd,
	     size_t cmd_len)
{
    struct addrinfo *ai, *a;
    struct addrinfo hints;
    int error;
    char portstr[NI_MAXSERV];

    if (priv_socket1 < 0) {
	warnx ("unable to bind reserved port: is rsh setuid root?");
	return 1;
    }

    memset (&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family   = AF_INET;

    snprintf (portstr, sizeof(portstr), "%u", ntohs(port));

    error = getaddrinfo (host, portstr, &hints, &ai);
    if (error) {
	warnx ("%s: %s", host, gai_strerror(error));
	return 1;
    }
    
    if (connect (priv_socket1, ai->ai_addr, ai->ai_addrlen) < 0) {
	if (ai->ai_next == NULL) {
	    freeaddrinfo (ai);
	    return 1;
	}

	close(priv_socket1);
	close(priv_socket2);

	for (a = ai->ai_next; a != NULL; a = a->ai_next) {
	    pid_t pid;

	    pid = fork();
	    if (pid < 0)
		err (1, "fork");
	    else if(pid == 0) {
		char **new_argv;
		int i = 0;
		struct sockaddr_in *sin = (struct sockaddr_in *)a->ai_addr;

		new_argv = malloc((argc + 2) * sizeof(*new_argv));
		if (new_argv == NULL)
		    errx (1, "malloc: out of memory");
		new_argv[i] = argv[i];
		++i;
		if (optind == i)
		    new_argv[i++] = print_addr (sin);
		new_argv[i++] = "-K";
		for(; i <= argc; ++i)
		    new_argv[i] = argv[i - 1];
		if (optind > 1)
		    new_argv[optind + 1] = print_addr(sin);
		new_argv[argc + 1] = NULL;
		execv(PATH_RSH, new_argv);
		err(1, "execv(%s)", PATH_RSH);
	    } else {
		int status;

		freeaddrinfo (ai);

		while(waitpid(pid, &status, 0) < 0)
		    ;
		if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
		    return 0;
	    }
	}
	return 1;
    } else {
	int ret;

	freeaddrinfo (ai);

	ret = proto (priv_socket1, priv_socket2,
		     argv[optind],
		     local_user, remote_user,
		     cmd, cmd_len,
		     send_broken_auth);
	return ret;
    }
}

static int
doit (const char *hostname,
      const char *remote_user,
      const char *local_user,
      int port,
      const char *cmd,
      size_t cmd_len,
      int do_errsock,
      int (*auth_func)(int s,
		       struct sockaddr *this, struct sockaddr *that,
		       const char *hostname, const char *remote_user,
		       const char *local_user, size_t cmd_len,
		       const char *cmd))
{
    struct addrinfo *ai, *a;
    struct addrinfo hints;
    int error;
    char portstr[NI_MAXSERV];
    int ret;

    memset (&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    snprintf (portstr, sizeof(portstr), "%u", ntohs(port));

    error = getaddrinfo (hostname, portstr, &hints, &ai);
    if (error) {
	errx (1, "%s: %s", hostname, gai_strerror(error));
	return -1;
    }
    
    for (a = ai; a != NULL; a = a->ai_next) {
	int s;
	int errsock;

	s = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (s < 0)
	    continue;
	if (connect (s, a->ai_addr, a->ai_addrlen) < 0) {
	    warn ("connect(%s)", hostname);
	    close (s);
	    continue;
	}
	if (do_errsock) {
	    struct addrinfo *ea;
	    struct addrinfo hints;

	    memset (&hints, 0, sizeof(hints));
	    hints.ai_socktype = a->ai_socktype;
	    hints.ai_protocol = a->ai_protocol;
	    hints.ai_family   = a->ai_family;
	    hints.ai_flags    = AI_PASSIVE;

	    error = getaddrinfo (NULL, "0", &hints, &ea);
	    if (error)
		errx (1, "getaddrinfo: %s", gai_strerror(error));
	    errsock = socket (ea->ai_family, ea->ai_socktype, ea->ai_protocol);
	    if (errsock < 0)
		err (1, "socket");
	    if (bind (errsock, ea->ai_addr, ea->ai_addrlen) < 0)
		err (1, "bind");
	    freeaddrinfo (ea);
	} else
	    errsock = -1;
    
	freeaddrinfo (ai);
	ret = proto (s, errsock,
		     hostname,
		     local_user, remote_user,
		     cmd, cmd_len, auth_func);
	close (s);
	return ret;
    }
    warnx ("failed to contact %s", hostname);
    freeaddrinfo (ai);
    return -1;
}

#ifdef KRB4
static int use_v4 = -1;
#endif
static int use_v5 = -1;
static int use_only_broken = 0;
static int use_broken = 1;
static char *port_str;
static const char *user;
static int do_version;
static int do_help;
static int do_errsock = 1;

struct getargs args[] = {
#ifdef KRB4
    { "krb4",	'4', arg_flag,		&use_v4,	"Use Kerberos V4",
      NULL },
#endif
    { "krb5",	'5', arg_flag,		&use_v5,	"Use Kerberos V5",
      NULL },
    { "broken", 'K', arg_flag,		&use_only_broken, "Use priv port",
      NULL },
    { "input",	'n', arg_negative_flag,	&input,		"Close stdin",
      NULL },
    { "encrypt", 'x', arg_flag,		&do_encrypt,	"Encrypt connection",
      NULL },
    { "encrypt", 'z', arg_negative_flag,      &do_encrypt,
      "Don't encrypt connection", NULL },
    { "forward", 'f', arg_flag,		&do_forward,	"Forward credentials",
      NULL },
    { "forward", 'G', arg_negative_flag,&do_forward,	"Forward credentials",
      NULL },
    { "forwardable", 'F', arg_flag,	&do_forwardable,
      "Forward forwardable credentials", NULL },
    { "unique", 'u', arg_flag,	&do_unique_tkfile,
      "Use unique remote tkfile", NULL },
    { "tkfile", 'U', arg_string,  &unique_tkfile,
      "Use that remote tkfile", NULL },
    { "port",	'p', arg_string,	&port_str,	"Use this port",
      "number-or-service" },
    { "user",	'l', arg_string,	&user,		"Run as this user",
      NULL },
    { "stderr", 'e', arg_negative_flag, &do_errsock,	"don't open stderr"},
    { "version", 0,  arg_flag,		&do_version,	"Print version",
      NULL },
    { "help",	 0,  arg_flag,		&do_help,	NULL,
      NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args) / sizeof(args[0]),
		    NULL,
		    "host [command]");
    exit (ret);
}

/*
 *
 */

int
main(int argc, char **argv)
{
    int priv_port1, priv_port2;
    int priv_socket1, priv_socket2;
    int port = 0;
    int optind = 0;
    int ret = 1;
    char *cmd;
    size_t cmd_len;
    const char *local_user;
    char *host = NULL;
    int host_index = -1;
    int status; 

    priv_port1 = priv_port2 = IPPORT_RESERVED-1;
    priv_socket1 = rresvport(&priv_port1);
    priv_socket2 = rresvport(&priv_port2);
    setuid(getuid());
    
    set_progname (argv[0]);

    if (argc >= 2 && argv[1][0] != '-') {
	host = argv[host_index = 1];
	optind = 1;
    }
    
    status = krb5_init_context (&context);
    if (status)
        errx(1, "krb5_init_context failed: %u", status);
      
    do_forwardable = krb5_config_get_bool (context, NULL,
					   "libdefaults",
					   "forwardable",
					   NULL);
	
    do_forward = krb5_config_get_bool (context, NULL,
				       "libdefaults",
				       "forward",
				       NULL);

    do_encrypt = krb5_config_get_bool (context, NULL,
				       "libdefaults",
				       "encrypt",
				       NULL);

    if (getarg (args, sizeof(args) / sizeof(args[0]), argc, argv,
		&optind))
	usage (1);

    if (do_forwardable)
	do_forward = 1;

#if defined(KRB4) && defined(KRB5)
    if(use_v4 == -1 && use_v5 == 1)
	use_v4 = 0;
    if(use_v5 == -1 && use_v4 == 1)
	use_v5 = 0;
#endif    

    if (use_only_broken) {
#ifdef KRB4
	use_v4 = 0;
#endif
	use_v5 = 0;
    }

    if (do_help)
	usage (0);

    if (do_version) {
	print_version (NULL);
	return 0;
    }
	
    if (do_unique_tkfile && unique_tkfile != NULL)
	errx (1, "Only one of -u and -U allowed.");

    if (do_unique_tkfile)
	strcpy(tkfile,"-u ");
    else if (unique_tkfile != NULL) {
	if (strchr(unique_tkfile,' ') != NULL) {
	    warnx("Space is not allowed in tkfilename");
	    usage(1);
	}
	do_unique_tkfile = 1;
	snprintf (tkfile, sizeof(tkfile), "-U %s ", unique_tkfile);
    }

    if (host == NULL) {
	if (argc - optind < 1)
	    usage (1);
	else
	    host = argv[host_index = optind++];
    }

    if (optind == argc) {
	close (priv_socket1);
	close (priv_socket2);
	argv[0] = "rlogin";
	execvp ("rlogin", argv);
	err (1, "execvp rlogin");
    }

    if (port_str) {
	struct servent *s = roken_getservbyname (port_str, "tcp");

	if (s)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		errx (1, "Bad port `%s'", port_str);
	    port = htons(port);
	}
    }

    local_user = get_default_username ();
    if (local_user == NULL)
	errx (1, "who are you?");

    if (user == NULL)
	user = local_user;

    cmd_len = construct_command(&cmd, argc - optind, argv + optind);

    /*
     * Try all different authentication methods
     */

    if (ret && use_v5) {
	int tmp_port;

	if (port)
	    tmp_port = port;
	else
	    tmp_port = krb5_getportbyname (context, "kshell", "tcp", 544);

	auth_method = AUTH_KRB5;
	ret = doit (host, user, local_user, tmp_port, cmd, cmd_len,
		    do_errsock,
		    send_krb5_auth);
    }
#ifdef KRB4
    if (ret && use_v4) {
	int tmp_port;

	if (port)
	    tmp_port = port;
	else if (do_encrypt)
	    tmp_port = krb5_getportbyname (context, "ekshell", "tcp", 545);
	else
	    tmp_port = krb5_getportbyname (context, "kshell", "tcp", 544);

	auth_method = AUTH_KRB4;
	ret = doit (host, user, local_user, tmp_port, cmd, cmd_len,
		    do_errsock,
		    send_krb4_auth);
    }
#endif
    if (ret && use_broken) {
	int tmp_port;

	if(port)
	    tmp_port = port;
	else
	    tmp_port = krb5_getportbyname(context, "shell", "tcp", 514);
	auth_method = AUTH_BROKEN;
	ret = doit_broken (argc, argv, host_index, host,
			   user, local_user,
			   tmp_port,
			   priv_socket1,
			   do_errsock ? priv_socket2 : -1,
			   cmd, cmd_len);
    }
    return ret;
}
