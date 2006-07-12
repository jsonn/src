/*	$NetBSD: match_ops.c,v 1.7.2.1 2006/07/12 15:06:44 tron Exp $	*/

/*++
/* NAME
/*	match_ops 3
/* SUMMARY
/*	simple string or host pattern matching
/* SYNOPSIS
/*	#include <match_ops.h>
/*
/*	int	match_string(flags, string, pattern)
/*	int	flags;
/*	const char *string;
/*	const char *pattern;
/*
/*	int	match_hostname(flags, name, pattern)
/*	int	flags;
/*	const char *name;
/*	const char *pattern;
/*
/*	int	match_hostaddr(flags, addr, pattern)
/*	int	flags;
/*	const char *addr;
/*	const char *pattern;
/* DESCRIPTION
/*	This module implements simple string and host name or address
/*	matching. The matching process is case insensitive. If a pattern
/*	has the form type:name, table lookup is used instead of string
/*	or address comparison.
/*
/*	match_string() matches the string against the pattern, requiring
/*	an exact (case-insensitive) match. The flags argument is not used.
/*
/*	match_hostname() matches the host name when the hostname matches
/*	the pattern exactly, or when the pattern matches a parent domain
/*	of the named host. The flags argument specifies the bit-wise OR
/*	of zero or more of the following:
/* .IP MATCH_FLAG_PARENT
/*	The hostname pattern foo.com matches itself and any name below
/*	the domain foo.com. If this flag is cleared, foo.com matches itself
/*	only, and .foo.com matches any name below the domain foo.com.
/* .RE
/*	Specify MATCH_FLAG_NONE to request none of the above.
/*
/*	match_hostaddr() matches a host address when the pattern is
/*	identical to the host address, or when the pattern is a net/mask
/*	that contains the address. The mask specifies the number of
/*	bits in the network part of the pattern. The flags argument is
/*	not used.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <split_at.h>
#include <dict.h>
#include <match_ops.h>
#include <stringops.h>
#include <cidr_match.h>

#define MATCH_DICTIONARY(pattern) \
    ((pattern)[0] != '[' && strchr((pattern), ':') != 0)

/* match_string - match a string literal */

int     match_string(int unused_flags, const char *string, const char *pattern)
{
    char   *myname = "match_string";
    int     match;
    char   *key;

    if (msg_verbose)
	msg_info("%s: %s ~? %s", myname, string, pattern);

    /*
     * Try dictionary lookup: exact match.
     */
    if (MATCH_DICTIONARY(pattern)) {
	key = lowercase(mystrdup(string));
	match = (dict_lookup(pattern, key) != 0);
	myfree(key);
	if (match != 0)
	    return (1);
	if (dict_errno != 0)
	    msg_fatal("%s: table lookup problem", pattern);
	return (0);
    }

    /*
     * Try an exact string match.
     */
    if (strcasecmp(string, pattern) == 0) {
	return (1);
    }

    /*
     * No match found.
     */
    return (0);
}

/* match_hostname - match a host by name */

int     match_hostname(int flags, const char *name, const char *pattern)
{
    char   *myname = "match_hostname";
    const char *pd;
    const char *entry;
    char   *next;
    char   *temp;
    int     match;

    if (msg_verbose)
	msg_info("%s: %s ~? %s", myname, name, pattern);

    /*
     * Try dictionary lookup: exact match and parent domains.
     */
    if (MATCH_DICTIONARY(pattern)) {
	temp = lowercase(mystrdup(name));
	match = 0;
	for (entry = temp; *entry != 0; entry = next) {
	    if ((match = (dict_lookup(pattern, entry) != 0)) != 0)
		break;
	    if (dict_errno != 0)
		msg_fatal("%s: table lookup problem", pattern);
	    if ((next = strchr(entry + 1, '.')) == 0)
		break;
	    if (flags & MATCH_FLAG_PARENT)
		next += 1;
	}
	myfree(temp);
	return (match);
    }

    /*
     * Try an exact match with the host name.
     */
    if (strcasecmp(name, pattern) == 0) {
	return (1);
    }

    /*
     * See if the pattern is a parent domain of the hostname.
     */
    else {
	if (flags & MATCH_FLAG_PARENT) {
	    pd = name + strlen(name) - strlen(pattern);
	    if (pd > name && pd[-1] == '.' && strcasecmp(pd, pattern) == 0)
		return (1);
	} else if (pattern[0] == '.') {
	    pd = name + strlen(name) - strlen(pattern);
	    if (pd > name && strcasecmp(pd, pattern) == 0)
		return (1);
	}
    }
    return (0);
}

/* match_hostaddr - match host by address */

int     match_hostaddr(int unused_flags, const char *addr, const char *pattern)
{
    char   *myname = "match_hostaddr";
    char   *saved_patt;
    CIDR_MATCH match_info;
    VSTRING *err;

    if (msg_verbose)
	msg_info("%s: %s ~? %s", myname, addr, pattern);

#define V4_ADDR_STRING_CHARS	"01234567890."
#define V6_ADDR_STRING_CHARS	V4_ADDR_STRING_CHARS "abcdefABCDEF:"

    if (addr[strspn(addr, V6_ADDR_STRING_CHARS)] != 0)
	return (0);

    /*
     * Try dictionary lookup. This can be case insensitive.
     */
    if (MATCH_DICTIONARY(pattern)) {
	if (dict_lookup(pattern, addr) != 0)
	    return (1);
	if (dict_errno != 0)
	    msg_fatal("%s: table lookup problem", pattern);
	return (0);
    }

    /*
     * Try an exact match with the host address.
     */
    if (pattern[0] != '[') {
	if (strcasecmp(addr, pattern) == 0)
	    return (1);
    } else {
	int     addr_len = strlen(addr);

	if (strncasecmp(addr, pattern + 1, addr_len) == 0
	    && strcmp(pattern + 1 + addr_len, "]") == 0)
	    return (1);
    }

    /*
     * Light-weight tests before we get into expensive operations.
     * 
     * - Don't bother matching IPv4 against IPv6. Postfix transforms
     * IPv4-in-IPv6 to native IPv4 form when IPv4 support is enabled in
     * Postfix; if not, then Postfix has no business dealing with IPv4
     * addresses anyway.
     * 
     * - Don't bother if the pattern is a bare IPv4 address. That form would
     * have been matched with the strcasecmp() call above.
     * 
     * - Don't bother if the pattern isn't an address or address/mask.
     */
    if (!strchr(addr, ':') != !strchr(pattern, ':')
	|| pattern[strspn(pattern, V4_ADDR_STRING_CHARS)] == 0
	|| pattern[strspn(pattern, V6_ADDR_STRING_CHARS "[]/")] != 0)
	return (0);

    /*
     * No escape from expensive operations: either we have a net/mask
     * pattern, or we have an address that can have multiple valid
     * representations (e.g., 0:0:0:0:0:0:0:1 versus ::1, etc.). The only way
     * to find out if the address matches the pattern is to transform
     * everything into to binary form, and to do the comparison there.
     */
    saved_patt = mystrdup(pattern);
    if ((err = cidr_match_parse(&match_info, saved_patt, (VSTRING *) 0)) != 0)
	msg_fatal("%s", vstring_str(err));
    myfree(saved_patt);
    return (cidr_match_execute(&match_info, addr) != 0);
}
