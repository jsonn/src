/*	$NetBSD: dict_static.c,v 1.1.1.1.4.2 2010/04/21 05:24:17 matt Exp $	*/

/*++
/* NAME
/*	dict_static 3
/* SUMMARY
/*	dictionary manager interface to static variables
/* SYNOPSIS
/*	#include <dict_static.h>
/*
/*	DICT	*dict_static_open(name, dummy, dict_flags)
/*	const char *name;
/*	int	dummy;
/*	int	dict_flags;
/* DESCRIPTION
/*	dict_static_open() implements a dummy dictionary that returns
/*	as lookup result the dictionary name, regardless of the lookup
/*	key value.
/*
/*	The \fIdummy\fR argument is ignored.
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	jeffm
/*	ghostgun.com
/*--*/

/* System library. */

#include "sys_defs.h"
#include <stdio.h>			/* sprintf() prototype */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Utility library. */

#include "mymalloc.h"
#include "msg.h"
#include "dict.h"
#include "dict_static.h"

/* dict_static_lookup - access static value*/

static const char *dict_static_lookup(DICT *dict, const char *unused_name)
{
    dict_errno = 0;

    return (dict->name);
}

/* dict_static_close - close static dictionary */

static void dict_static_close(DICT *dict)
{
    dict_free(dict);
}

/* dict_static_open - make association with static variable */

DICT   *dict_static_open(const char *name, int unused_flags, int dict_flags)
{
    DICT   *dict;

    dict = dict_alloc(DICT_TYPE_STATIC, name, sizeof(*dict));
    dict->lookup = dict_static_lookup;
    dict->close = dict_static_close;
    dict->flags = dict_flags | DICT_FLAG_FIXED;
    return (DICT_DEBUG (dict));
}
