/*	$NetBSD: net.c,v 1.58.2.4 2000/10/18 17:51:15 tv Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* net.c -- routines to fetch files off the network. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef INET6
#include <sys/sysctl.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"
#include "txtwalk.h"

int network_up = 0;

/* URL encode unsafe characters.  */

static char *url_encode __P((char *dst, const char *src, size_t len,
				const char *safe_chars,
				int encode_leading_slash));

/* Get the list of network interfaces. */

static void get_ifconfig_info __P((void));
static void get_ifinterface_info __P((void));

static void write_etc_hosts(FILE *f);

#define DHCLIENT_EX "/sbin/dhclient"
#include <signal.h>
static int config_dhcp __P((char *));
static void get_command_out __P((char *, int, char *, char *));
static void get_dhcp_value __P(( char *, char *));

#ifdef INET6
static int is_v6kernel __P((void));
static void init_v6kernel __P((int));
static int get_v6wait __P((void));
#endif

/*
 * URL encode unsafe characters.  See RFC 1738.
 *
 * Copies src string to dst, encoding unsafe or reserved characters
 * in %hex form as it goes, and returning a pointer to the result.
 * The result is always a nul-terminated string even if it had to be
 * truncated to avoid overflowing the available space.
 *
 * This url_encode() function does not operate on complete URLs, it
 * operates on strings that make up parts of URLs.  For example, in a
 * URL like "ftp://username:password@host/path", the username, password,
 * host and path should each be encoded separately before they are
 * joined together with the punctuation characters.
 *
 * In most ordinary use, the path portion of a URL does not start with
 * a slash; the slash is a separator between the host portion and the
 * path portion, and is dealt with by software outside the url_encode()
 * function.  However, it is valid for url_encode() to be passed a
 * string that does begin with a slash.  For example, the string might
 * represent a password, or a path part of a URL that the user really
 * does want to begin with a slash.
 *
 * len is the length of the destination buffer.  The result will be
 * truncated if necessary to fit in the destination buffer.
 *
 * safe_chars is a string of characters that should not be encoded.  If
 * safe_chars is non-NULL, any characters in safe_chars as well as any
 * alphanumeric characters will be copied from src to dst without
 * encoding.  Some potentially useful settings for this parameter are:
 *
 *	NULL		Everything is encoded (even alphanumerics)
 *	""		Everything except alphanumerics are encoded
 *	"/"		Alphanumerics and '/' remain unencoded
 *	"$-_.+!*'(),"	Consistent with a strict reading of RFC 1738
 *	"$-_.+!*'(),/"	As above, except '/' is not encoded
 *	"-_.+!,/"	As above, except shell special characters are encoded
 *
 * encode_leading_slash is a flag that determines whether or not to
 * encode a leading slash in a string.  If this flag is set, and if the
 * first character in the src string is '/', then the leading slash will
 * be encoded (as "%2F"), even if '/' is one of the characters in the
 * safe_chars string.  Note that only the first character of the src
 * string is affected by this flag, and that leading slashes are never
 * deleted, but either retained unchanged or encoded.
 *
 * Unsafe and reserved characters are defined in RFC 1738 section 2.2.
 * The most important parts are:
 *
 *      The characters ";", "/", "?", ":", "@", "=" and "&" are the
 *      characters which may be reserved for special meaning within a
 *      scheme. No other characters may be reserved within a scheme.
 *      [...]
 *
 *      Thus, only alphanumerics, the special characters "$-_.+!*'(),",
 *      and reserved characters used for their reserved purposes may be
 *      used unencoded within a URL.
 *
 */

#define RFC1738_SAFE				"$-_.+!*'(),"
#define RFC1738_SAFE_LESS_SHELL			"-_.+!,"
#define RFC1738_SAFE_LESS_SHELL_PLUS_SLASH	"-_.+!,/"

static char *
url_encode(char *dst, const char *src, size_t len,
	const char *safe_chars, int encode_leading_slash)
{
	char *p = dst;

	/*
	 * If encoding of a leading slash was desired, and there was in
	 * fact one or more leading slashes, encode one in the output string.
	 */
	if (encode_leading_slash && *src == '/') {
		if (len < 3)
			goto done;
		sprintf(p, "%%%02X", '/');
		src++;
		p += 3;
		len -= 3;
	}

	while (--len > 0 && *src != '\0') {
		if (safe_chars != NULL &&
		    (isalnum(*src) || strchr(safe_chars, *src))) {
			*p++ = *src++;
		} else {
			/* encode this char */
			if (len < 3)
				break;
			sprintf(p, "%%%02X", *src++);
			p += 3;
			len -= 2;
		}
	}
done:
	*p = '\0';
	return dst;
}

static const char *ignored_if_names[] = {
	"eon",			/* netiso */
	"gre",			/* net */
	"ipip",			/* netinet */
	"gif",			/* netinet6 */
	"faith",		/* netinet6 */
	"lo",			/* net */
#if 0
	"mdecap",		/* netinet -- never in IF list (?) XXX */
#endif
	"nsip",			/* netns */
	"ppp",			/* net */
	"sl",			/* net */
	"strip",		/* net */
	"tun",			/* net */
	/* XXX others? */
	NULL,
};

static void
get_ifconfig_info()
{
	char *textbuf;
	char *t, *nt, *ndest;
	const char **ignore;
	int textsize, len;

	/* Get ifconfig information */
	
	textsize = collect(T_OUTPUT, &textbuf, "/sbin/ifconfig -l 2>/dev/null");
	if (textsize < 0) {
		if (logging)
			(void)fprintf(log, "Aborting: Could not run ifconfig.\n");
		(void)fprintf(stderr, "Could not run ifconfig.");
		exit(1);
	}
	(void)strtok(textbuf,"\n");

	nt = textbuf;
	ndest = net_devices;
	*ndest = '\0';
	while ((t = strsep(&nt, " ")) != NULL) {
		for (ignore = ignored_if_names; *ignore != NULL; ignore++) {
			len = strlen(*ignore);
			if (strncmp(t, *ignore, len) == 0 &&
			    isdigit((unsigned char)t[len]))
				goto loop;
		}

		if (strlen(ndest) + 1 + strlen(t) + 1 > STRSIZE)
			break;			/* would overflow */

		strcat(ndest, t);
		strcat(ndest, " ");	/* net_devices needs trailing space! */
loop:
		t = nt;
	}
	free(textbuf);
}

/* Fill in defaults network values for the selected interface */
static void
get_ifinterface_info()
{
	char *textbuf;
	int textsize;
	char *t;
	char hostname[MAXHOSTNAMELEN + 1];
	int max_len;
	char *dot;

	/* First look to see if the selected interface is already configured. */
	textsize = collect(T_OUTPUT, &textbuf,
	    "/sbin/ifconfig %s inet 2>/dev/null", net_dev);
	if (textsize >= 0) {
		(void)strtok(textbuf, " \t\n"); /* ignore interface name */
		while ((t = strtok(NULL, " \t\n")) != NULL) {
			if (strcmp(t, "inet") == 0) {
				t = strtok(NULL, " \t\n");
				if (strcmp(t, "0.0.0.0") != 0)
					strcpy(net_ip, t);
			} else if (strcmp(t, "netmask") == 0) {
				t = strtok(NULL, " \t\n");
				if (strcmp(t, "0x0") != 0)
					strcpy(net_mask, t);
			} else if (strcmp(t, "media:") == 0) {
				t = strtok(NULL, " \t\n");
				/* handle "media: Ethernet manual" */
				if (strcmp(t, "Ethernet") == 0)
					t = strtok(NULL, " \t\n");
				if (strcmp(t, "none") != 0 &&
				    strcmp(t, "manual") != 0)
					strcpy(net_media, t);
			}
		}
	}
#ifdef INET6
	textsize = collect(T_OUTPUT, &textbuf,
	    "/sbin/ifconfig %s inet6 2>/dev/null", net_dev);
	if (textsize >= 0) {
		char *p;

		(void)strtok(textbuf, "\n"); /* ignore first line */
		while ((t = strtok(NULL, "\n")) != NULL) {
			if (strncmp(t, "\tinet6 ", 7) != 0)
				continue;
			t += 7;
			if (strstr(t, "tentative") || strstr(t, "duplicated"))
				continue;
			if (strncmp(t, "fe80:", 5) == 0)
				continue;

			p = t;
			while (*p && *p != ' ' && *p != '\n')
				p++;
			*p = '\0';
			strcpy(net_ip6, t);
			break;
		}
	}
#endif

	/* Check host (and domain?) name */
	if (gethostname(hostname, sizeof(hostname)) == 0) {
		hostname[sizeof(hostname) - 1] = '\0';
		/* check for a . */
		dot = strchr(hostname, '.');
		if ( dot == NULL ) {
			/* if not found its just a host, punt on domain */
			strncpy(net_host, hostname, sizeof(net_host));
		} else {
			/* split hostname into host/domain parts */
			max_len = dot - hostname;
			max_len = (sizeof(net_host)<max_len)?sizeof(net_host):max_len;
			*dot = '\0';
			dot++;
			strncpy(net_host, hostname, max_len);
			max_len = strlen(dot);
			max_len = (sizeof(net_host)<max_len)?sizeof(net_host):max_len;
			strncpy(net_domain, dot, max_len);
		}
	}
}

#ifdef INET6
static int
is_v6kernel()
{
	int s;

	s = socket(PF_INET6, SOCK_DGRAM, 0);
	if (s < 0)
		return 0;
	else {
		close(s);
		return 1;
	}
}

/*
 * initialize as v6 client.
 * we are sure that we will never become router with boot floppy :-)
 * (include and use sysctl(8) if you are willing to)
 */
static void
init_v6kernel(autoconf)
	int autoconf;
{
	int v;
	int mib[4] = {CTL_NET, PF_INET6, IPPROTO_IPV6, 0};

	mib[3] = IPV6CTL_FORWARDING;
	v = 0;
	if (sysctl(mib, 4, NULL, NULL, (void *)&v, sizeof(v)) < 0)
		; /* warn("sysctl(net.inet6.ip6.fowarding"); */

	mib[3] = IPV6CTL_ACCEPT_RTADV;
	v = autoconf ? 1 : 0;
	if (sysctl(mib, 4, NULL, NULL, (void *)&v, sizeof(v)) < 0)
		; /* warn("sysctl(net.inet6.ip6.accept_rtadv"); */
}

static int
get_v6wait()
{
	long len = sizeof(int);
	int v;
	int mib[4] = {CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_DAD_COUNT};

	len = sizeof(v);
	if (sysctl(mib, 4, (void *)&v, (size_t *)&len, NULL, 0) < 0) {
		/* warn("sysctl(net.inet6.ip6.dadcount)"); */
		return 1;	/* guess */
	}
	return v;
}
#endif

/*
 * Get the information to configure the network, configure it and
 * make sure both the gateway and the name server are up.
 */
int
config_network()
{	char *tp;
	char defname[255];
	int  octet0;
	int  pass, v6config, dhcp_config;

	FILE *f;
	time_t now;

	if (network_up)
		return (1);

	net_devices[0] = '\0';
	get_ifconfig_info();
	if (strlen(net_devices) == 0) {
		/* No network interfaces found! */
		msg_display(MSG_nonet);
		process_menu(MENU_ok);
		return (-1);
	}
	network_up = 1;

	strncpy(defname, net_devices, 255);
	tp = defname;
	strsep(&tp, " ");
	msg_prompt(MSG_asknetdev, defname, net_dev, 255, net_devices);
	tp = net_dev;
	strsep(&tp, " ");
	net_dev[strlen(net_dev)+1] = 0;
	net_dev[strlen(net_dev)] = ' ';
	while (strstr(net_devices, net_dev) == NULL) {
		msg_prompt(MSG_badnet, defname,  net_dev, 255, net_devices);
		tp = net_dev;
		strsep(&tp, " ");
		net_dev[strlen(net_dev)+1] = 0;
		net_dev[strlen(net_dev)] = ' ';
	}

	/* Remove that space we added. */
	net_dev[strlen(net_dev) - 1] = 0;

again:

#ifdef INET6
	v6config = 1;
#else
	v6config = 0;
#endif

	/* Preload any defaults we can find */
	get_ifinterface_info();
	pass = strlen(net_mask) == 0 ? 0 : 1;

	/* domain and host */
	msg_display(MSG_netinfo);

	/* ethernet medium */
	if (strlen(net_media) == 0)
		msg_prompt_add(MSG_net_media, net_media, net_media, STRSIZE);

	/* try a dhcp configuration */
	dhcp_config = config_dhcp(net_dev);
	if (dhcp_config) {
		net_dhcpconf |= DHCPCONF_IPADDR;

		/* run route show and extract data */
		get_command_out(net_defroute, AF_INET,
		    "/sbin/route -n show -inet 2>/dev/null", "default");

		/* pull nameserver info out of /etc/resolv.conf */
		get_command_out(net_namesvr, AF_INET,
		    "cat /etc/resolv.conf 2> /dev/null", "nameserver");
		if (strlen(net_namesvr) != 0)
			net_dhcpconf |= DHCPCONF_NAMESVR;

		/* pull domainname out of leases file */
		get_dhcp_value(net_domain, "domain-name");
		if (strlen(net_domain) != 0)
			net_dhcpconf |= DHCPCONF_DOMAIN;

		/* pull hostname out of leases file */
		get_dhcp_value(net_host, "hostname");
		if (strlen(net_host) != 0)
			net_dhcpconf |= DHCPCONF_HOST;
	}

	msg_prompt_add(MSG_net_domain, net_domain, net_domain, STRSIZE);
	msg_prompt_add(MSG_net_host, net_host, net_host, STRSIZE);

	if (!dhcp_config) {
		/* Manually configure IPv4 */
		msg_prompt_add(MSG_net_ip, net_ip, net_ip, STRSIZE);
		octet0 = atoi(net_ip);
		if (!pass) {
			if (0 <= octet0 && octet0 <= 127)
				strcpy(net_mask, "0xff000000");
			else if (128 <= octet0 && octet0 <= 191)
				strcpy(net_mask, "0xffff0000");
			else if (192 <= octet0 && octet0 <= 223)
				strcpy(net_mask, "0xffffff00");
		}
		msg_prompt_add(MSG_net_mask, net_mask, net_mask, STRSIZE);
		msg_prompt_add(MSG_net_defroute, net_defroute, net_defroute,
		    STRSIZE);
		msg_prompt_add(MSG_net_namesrv, net_namesvr, net_namesvr,
		    STRSIZE);
	}

#ifdef INET6
	/* IPv6 autoconfiguration */
	if (!is_v6kernel())
		v6config = 0;
	else if (v6config) {  /* dhcp config will disable this */
		process_menu(MENU_ip6autoconf);
		v6config = yesno ? 1 : 0;
	}

	if (v6config) {
		process_menu(MENU_namesrv6);
		if (!yesno)
			msg_prompt_add(MSG_net_namesrv6, net_namesvr6,
			    net_namesvr6, STRSIZE);
	}
#endif

	/* confirm the setting */
	msg_display(MSG_netok, net_domain, net_host,
		     *net_ip == '\0' ? "<none>" : net_ip,
		     *net_mask == '\0' ? "<none>" : net_mask,
		     *net_namesvr == '\0' ? "<none>" : net_namesvr,
		     *net_defroute == '\0' ? "<none>" : net_defroute,
		     *net_media == '\0' ? "<default>" : net_media,
#ifdef INET6
		     !is_v6kernel() ? "<not supported>" :
			(v6config ? "yes" : "no"),
		     *net_namesvr6 == '\0' ? "<none>" : net_namesvr6
#else
		     "<not supported>",
		     "<not supported>"
#endif
		     );
	process_menu(MENU_yesno);
	if (!yesno)
		msg_display(MSG_netagain);
	pass++;
	if (!yesno)
		goto again;

	/*
	 * we may want to perform checks against inconsistent configuration,
	 * like IPv4 DNS server without IPv4 configuration.
	 */

	/* Create /etc/resolv.conf if a nameserver was given */
	if (strcmp(net_namesvr, "") != 0
#ifdef INET6
	 || strcmp(net_namesvr6, "") != 0
#endif
		) {
#ifdef DEBUG
		f = fopen("/tmp/resolv.conf", "w");
#else
		f = fopen("/etc/resolv.conf", "w");
#endif
		if (f == NULL) {
			if (logging)
				(void)fprintf(log, "%s", msg_string(MSG_resolv));
			(void)fprintf(stderr, "%s", msg_string(MSG_resolv));
			exit(1);
		}
		if (scripting)
			(void)fprintf(script, "cat <<EOF >/etc/resolv.conf\n");
		time(&now);
		/* NB: ctime() returns a string ending in  '\n' */
		(void)fprintf(f, ";\n; BIND data file\n; %s %s;\n", 
		    "Created by NetBSD sysinst on", ctime(&now)); 
		if (strcmp(net_namesvr, "") != 0)
			(void)fprintf(f, "nameserver %s\n", net_namesvr);
#ifdef INET6
		if (strcmp(net_namesvr6, "") != 0)
			(void)fprintf(f, "nameserver %s\n", net_namesvr6);
#endif
		(void)fprintf(f, "search %s\n", net_domain);
		if (scripting) {
			(void)fprintf(script, ";\n; BIND data file\n; %s %s;\n", 
			    "Created by NetBSD sysinst on", ctime(&now)); 
			if (strcmp(net_namesvr, "") != 0)
				(void)fprintf(script, "nameserver %s\n",
				    net_namesvr);
#ifdef INET6
			if (strcmp(net_namesvr6, "") != 0)
				(void)fprintf(script, "nameserver %s\n",
				    net_namesvr6);
#endif
			(void)fprintf(script, "search %s\n", net_domain);
		}
		fflush(NULL);
		fclose(f);
	}

	run_prog(0, NULL, "/sbin/ifconfig lo0 127.0.0.1");

	/*
	 * ifconfig does not allow media specifiers on IFM_MANUAL interfaces.
	 * Our UI gies no way to set an option back to null-string if it
	 * gets accidentally set.
	 * good way to reset the media to null-string.
	 * Check for plausible alternatives.
	 */
	if (strcmp(net_media, "<default>") == 0 ||
	    strcmp(net_media, "default") == 0 ||
	    strcmp(net_media, "<manual>") == 0 ||
	    strcmp(net_media, "manual") == 0 ||
	    strcmp(net_media, "<none>") == 0 ||
	    strcmp(net_media, "none") == 0 ||
	    strcmp(net_media, " ") == 0) {
		*net_media = '\0';
	}

	if (*net_media != '\0')
		run_prog(0, NULL, "/sbin/ifconfig %s media %s",
		    net_dev, net_media);

#ifdef INET6
	if (v6config) {
		init_v6kernel(1);
		run_prog(0, NULL, "/sbin/ifconfig %s up", net_dev);
		sleep(get_v6wait() + 1);
		run_prog(RUN_DISPLAY, NULL, "/sbin/rtsol -D %s", net_dev);
		sleep(get_v6wait() + 1);
	}
#endif

	if (strcmp(net_ip, "") != 0) {
		if (strcmp(net_mask, "") != 0) {
			run_prog(0, NULL, 
			    "/sbin/ifconfig %s inet %s netmask %s",
			    net_dev, net_ip, net_mask);
		} else {
			run_prog(0, NULL, 
			    "/sbin/ifconfig %s inet %s", net_dev, net_ip);
		}
	}

	/* Set host name */
	if (strcmp(net_host, "") != 0)
	  	sethostname(net_host, strlen(net_host));

	/* Set a default route if one was given */
	if (strcmp(net_defroute, "") != 0) {
		run_prog(0, NULL, 
		    "/sbin/route -n flush -inet");
		run_prog(0, NULL, 
		    "/sbin/route -n add default %s",
			  net_defroute);
	}

	/*
	 * ping should be verbose, so users can see the cause
	 * of a network failure.
	 */

#ifdef INET6
	if (v6config && network_up) {
		network_up = !run_prog(0, NULL, 
		    "/sbin/ping6 -v -c 3 -n -I %s ff02::2", net_dev);

		if (strcmp(net_namesvr6, "") != 0)
			network_up = !run_prog(RUN_DISPLAY, NULL, 
			    "/sbin/ping6 -v -c 3 -n %s", net_namesvr6);
	}
#endif

	if (strcmp(net_namesvr, "") != 0 && network_up)
		network_up = !run_prog(0, NULL, 
		    "/sbin/ping -v -c 5 -w 5 -o -n %s",
					net_namesvr);

	if (strcmp(net_defroute, "") != 0 && network_up)
		network_up = !run_prog(0, NULL, 
		    "/sbin/ping -v -c 5 -w 5 -o -n %s",
					net_defroute);
	fflush(NULL);

	return network_up;
}

int
get_via_ftp()
{ 
	distinfo *list;
	char ftp_user_encoded[STRSIZE];
	char ftp_pass_encoded[STRSIZE];
	char ftp_dir_encoded[STRSIZE];
	char filename[SSTRSIZE];
	int  ret;

	while ((ret = config_network()) <= 0) {
		if (ret < 0)
			return (-1);
		msg_display(MSG_netnotup);
		process_menu(MENU_yesno);
		if (!yesno)
			return 0;
	}

	cd_dist_dir("ftp");

	/* Fill in final values for ftp_dir. */
	strncat(ftp_dir, rel, STRSIZE - strlen(ftp_dir));
	strcat(ftp_dir, "/");
	strncat(ftp_dir, machine, STRSIZE - strlen(ftp_dir));
	strncat(ftp_dir, ftp_prefix, STRSIZE - strlen(ftp_dir));
	process_menu(MENU_ftpsource);

	list = dist_list;
	while (list->name) {
		if (!list->getit) {
			list++;
			continue;
		}
		(void)snprintf(filename, SSTRSIZE, "%s%s", list->name,
		    dist_postfix);
		/*
		 * Invoke ftp to fetch the file.
		 *
		 * ftp_pass is quite likely to contain unsafe characters
		 * that need to be encoded in the URL (for example,
		 * "@", ":" and "/" need quoting).  Let's be
		 * paranoid and also encode ftp_user and ftp_dir.  (For
		 * example, ftp_dir could easily contain '~', which is
		 * unsafe by a strict reading of RFC 1738).
		 */
		if (strcmp ("ftp", ftp_user) == 0)
			ret = run_prog(RUN_DISPLAY, NULL, 
			    "/usr/bin/ftp -a ftp://%s/%s/%s",
			    ftp_host,
			    url_encode(ftp_dir_encoded, ftp_dir, STRSIZE,
					RFC1738_SAFE_LESS_SHELL_PLUS_SLASH, 1),
			    filename);
		else {
			ret = run_prog(RUN_DISPLAY, NULL, 
			    "/usr/bin/ftp ftp://%s:%s@%s/%s/%s",
			    url_encode(ftp_user_encoded, ftp_user, STRSIZE,
					RFC1738_SAFE_LESS_SHELL, 0),
			    url_encode(ftp_pass_encoded, ftp_pass, STRSIZE,
					NULL, 0),
			    ftp_host,
			    url_encode(ftp_dir_encoded, ftp_dir, STRSIZE,
					RFC1738_SAFE_LESS_SHELL_PLUS_SLASH, 1),
			    filename);
		}
		if (ret) {
			/* Error getting the file.  Bad host name ... ? */
			msg_display(MSG_ftperror_cont);
			getchar();
			puts(CL);		/* XXX */
			touchwin(stdscr);
			wclear(stdscr);
			wrefresh(stdscr);
			msg_display(MSG_ftperror);
			process_menu(MENU_yesno);
			if (yesno)
				process_menu(MENU_ftpsource);
			else
				return 0;
		} else 
			list++;

	}
	puts(CL);		/* XXX */
	touchwin(stdscr);
	wclear(stdscr);
	wrefresh(stdscr);
#ifndef DEBUG
	chdir("/");	/* back to current real root */
#endif
	return (1);
}

int
get_via_nfs()
{
	int ret;

        while ((ret = config_network()) <= 0) {
		if (ret < 0)
			return (-1);
                msg_display(MSG_netnotup);
                process_menu(MENU_yesno);
                if (!yesno)
                        return (0);
        }

	/* Get server and filepath */
	process_menu(MENU_nfssource);
again:

	run_prog(0, NULL, 
	    "/sbin/umount /mnt2");
	
	/* Mount it */
	if (run_prog(0, NULL, 
	    "/sbin/mount -r -o -i,-r=1024 -t nfs %s:%s /mnt2",
	    nfs_host, nfs_dir)) {
		msg_display(MSG_nfsbadmount, nfs_host, nfs_dir);
		process_menu(MENU_nfsbadmount);
		if (!yesno)
			return (0);
		if (!ignorerror)
			goto again;
	}

	/* Verify distribution files exist.  */
	if (distribution_sets_exist_p("/mnt2") == 0) {
		msg_display(MSG_badsetdir, "/mnt2");
		process_menu (MENU_nfsbadmount);
		if (!yesno)
			return (0);
		if (!ignorerror)
			goto again;
	}

	/* return location, don't clean... */
	strcpy(ext_dir, "/mnt2");
	clean_dist_dir = 0;
	mnt2_mounted = 1;
	return 1;
}

/*
 * write the new contents of /etc/hosts to the specified file
 */
static void
write_etc_hosts(FILE *f)
{
	int l;

	fprintf(f, "#\n");
	fprintf(f, "# Added by NetBSD sysinst\n");
	fprintf(f, "#\n");

	fprintf(f, "127.0.0.1	localhost\n");

	fprintf(f, "%s\t", net_ip);
	l = strlen(net_host) - strlen(net_domain);
	if (l <= 0 ||
	    net_host[l - 1] != '.' ||
	    strcasecmp(net_domain, net_host + l) != 0) {
		/* net_host isn't an FQDN. */
		fprintf(f, "%s.%s ", net_host, net_domain);
	}
	fprintf(f, "%s\n", net_host);
}

/*
 * Write the network config info the user entered via menus into the
 * config files in the target disk.  Be careful not to lose any
 * information we don't immediately add back, in case the install
 * target is the currently-active root. 
 *
 * XXXX rc.conf support is needed here!
 */
void
mnt_net_config(void)
{
	char ans [5] = "y";
	char ifconfig_fn [STRSIZE];
	FILE *f;

	if (!network_up)
		return;
	msg_prompt(MSG_mntnetconfig, ans, ans, 5);
	if (*ans != 'y')
		return;

	/* Write hostname to /etc/myname */
	if ((net_dhcpconf & DHCPCONF_HOST) == 0) {
		f = target_fopen("/etc/myname", "w");
		if (f != 0) {
			(void)fprintf(f, "%s\n", net_host);
			if (scripting) {
				(void)fprintf(script,
				    "echo \"%s\" >%s/etc/myname\n",
				    net_host, target_prefix());
			}
			(void)fclose(f);
		}
	}

	/* If not running in target, copy resolv.conf there. */
	if ((net_dhcpconf & DHCPCONF_NAMESVR) == 0) {
		if (strcmp(net_namesvr, "") != 0)
			dup_file_into_target("/etc/resolv.conf");
	}

	if ((net_dhcpconf & DHCPCONF_IPADDR) == 0) {
		/* 
		 * Add IPaddr/hostname to  /etc/hosts.
		 * Be careful not to clobber any existing contents.
		 * Relies on ordered seach of /etc/hosts. XXX YP?
		 */
		f = target_fopen("/etc/hosts", "a");
		if (f != 0) {
			write_etc_hosts(f);
			(void)fclose(f);
			if (scripting) {
				(void)fprintf(script,
				    "cat <<EOF >>%s/etc/hosts\n",
				    target_prefix());
				write_etc_hosts(script);
				(void)fprintf(script, "EOF\n");
			}
		}

		/* Write IPaddr and netmask to /etc/ifconfig.if[0-9] */
		snprintf(ifconfig_fn, STRSIZE, "/etc/ifconfig.%s",
		    net_dev);
		f = target_fopen(ifconfig_fn, "w");
		if (f != 0) {
			if (*net_media != '\0') {
				fprintf(f, "%s netmask %s media %s\n",
					net_ip, net_mask, net_media);
				if (scripting) {
					fprintf(script,
					    "echo \"%s netmask %s media %s\">%s%s\n",
					    net_ip, net_mask, net_media,
					    target_prefix(), ifconfig_fn);
				}
			} else {
				fprintf(f, "%s netmask %s\n",
					net_ip, net_mask);
				if (scripting) {
					fprintf(script,
					    "echo \"%s netmask %s\">%s%s\n",
					    net_ip, net_mask, target_prefix(),
					    ifconfig_fn);
				}
			}
			fclose(f);
		}

		f = target_fopen("/etc/mygate", "w");
		if (f != 0) {
			fprintf(f, "%s\n", net_defroute);
			if (scripting) {
				fprintf(script,
				    "echo \"%s\" >%s/etc/mygate\n",
				    net_defroute, target_prefix());
			}
			fclose(f);
		}
	}

	fflush(NULL);
}

int
config_dhcp(inter)
char *inter;
{
	int dhcpautoconf;
	int result;
	char *textbuf;
	int pid;

	/* check if dhclient is running, if so, kill it */
	result = collect(T_FILE, &textbuf, "/tmp/dhclient.pid");
	if (result >=0) {
		pid = atoi(textbuf);
		if (pid > 0) {
			kill(pid,15);
			sleep(1);
			kill(pid,9);
		}
	}

	if (!file_mode_match(DHCLIENT_EX, S_IFREG))
		return 0;
	process_menu(MENU_dhcpautoconf);
	if (yesno) {
		/* spawn off dhclient and wait for parent to exit */
		dhcpautoconf = run_prog(RUN_DISPLAY, NULL, "%s -pf /tmp/dhclient.pid -lf /tmp/dhclient.leases %s", DHCLIENT_EX,inter);
		return dhcpautoconf?0:1;
	}
	return 0;
}

static void
get_command_out(targ, af, command, search)
char *targ;
int af;
char *command;
char *search;
{
	int textsize;
	char *textbuf;
	char *t;
#ifndef INET6
	struct in_addr in;
#else
	struct in6_addr in;
#endif

	textsize = collect(T_OUTPUT, &textbuf, command);
	if (textsize < 0) {
		if (logging)
			(void)fprintf(log, "Aborting: Could not run %s.\n", command);
		(void)fprintf(stderr, "Could not run ifconfig.");
		exit(1);
	}
	if (textsize >= 0) {
		(void)strtok(textbuf, " \t\n"); /* ignore interface name */
		while ((t = strtok(NULL, " \t\n")) != NULL) {
			if (strcmp(t, search) == 0) {
				t = strtok(NULL, " \t\n");
				if (inet_pton(af, t, &in) == 1 &&
				    strcmp(t, "0.0.0.0") != 0) {
					strcpy(targ, t);
				}
			}
		}
	}
	return;
}

static void
get_dhcp_value(targ, line)
char *targ;
char *line;
{
	int textsize;
	char *textbuf;
	char *t;
	char *walk;

	textsize = collect(T_FILE, &textbuf, "/tmp/dhclient.leases");
	if (textsize < 0) {
		if (logging)
			(void)fprintf(log, "Could not open file /tmp/dhclient.leases.\n");
		(void)fprintf(stderr, "Could not open /tmp/dhclient.leases\n");
		/* not fatal, just assume value not found */
	}
	if (textsize >= 0) {
		(void)strtok(textbuf, " \t\n"); /* jump past 'lease' */
		while ((t=strtok(NULL, " \t\n")) !=NULL) {
			if (strcmp(t, line) == 0) {
				t = strtok(NULL, " \t\n");
				/* found the tag, extract the value */
				/* last char should be a ';' */
				walk = strrchr(t,';');
				if (walk != NULL ) {
					*walk = '\0';
				}
				/* strip any " from the string */
				walk = strrchr(t,'"');
				if (walk != NULL ) {
					*walk = '\0';
					t++;
				}
				strcpy(targ, t);
				return;
			}
		}
	}
	return;
}
