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

#include "kadmin_locl.h"
#include <parse_units.h>

RCSID("$Id: get.c,v 1.1.1.1.4.2 2000/06/16 18:32:08 thorpej Exp $");

struct get_entry_data {
    void (*header)(void);
    void (*format)(kadm5_principal_ent_t);
};

static void
print_entry_terse(kadm5_principal_ent_t princ)
{
    char *p;
    krb5_unparse_name(context, princ->principal, &p);
    printf("  %s\n", p);
    free(p);
}

static void
print_header_short(void)
{
    printf("%-20s ", "Principal");
    
    printf("%-10s ", "Expires");
	    
    printf("%-10s ", "PW-exp");
	    
    printf("%-10s ", "PW-change");
	
    printf("%-9s ", "Max life");

    printf("%-9s ", "Max renew");
    
    printf("\n");
}

static void
print_entry_short(kadm5_principal_ent_t princ)
{
    char buf[1024];
    
    krb5_unparse_name_fixed_short(context, princ->principal, buf, sizeof(buf));
    printf("%-20s ", buf);
    
    time_t2str(princ->princ_expire_time, buf, sizeof(buf), 0);
    printf("%-10s ", buf);
	    
    time_t2str(princ->pw_expiration, buf, sizeof(buf), 0);
    printf("%-10s ", buf);
	    
    time_t2str(princ->last_pwd_change, buf, sizeof(buf), 0);
    printf("%-10s ", buf);
	
    deltat2str(princ->max_life, buf, sizeof(buf));
    printf("%-9s ", buf);

    deltat2str(princ->max_renewable_life, buf, sizeof(buf));
    printf("%-9s ", buf);

#if 0
    time_t2str(princ->mod_date, buf, sizeof(buf), 0);
    printf("%-10s ", buf);

    krb5_unparse_name_fixed(context, princ->mod_name, buf, sizeof(buf));
    printf("%-24s", buf);
#endif
    
    printf("\n");
}

static void
print_entry_long(kadm5_principal_ent_t princ)
{
    char buf[1024];
    int i;
    
    krb5_unparse_name_fixed(context, princ->principal, buf, sizeof(buf));
    printf("%24s: %s\n", "Principal", buf);
    time_t2str(princ->princ_expire_time, buf, sizeof(buf), 1);
    printf("%24s: %s\n", "Principal expires", buf);
	    
    time_t2str(princ->pw_expiration, buf, sizeof(buf), 1);
    printf("%24s: %s\n", "Password expires", buf);
	    
    time_t2str(princ->last_pwd_change, buf, sizeof(buf), 1);
    printf("%24s: %s\n", "Last password change", buf);
	
    deltat2str(princ->max_life, buf, sizeof(buf));
    printf("%24s: %s\n", "Max ticket life", buf);

    deltat2str(princ->max_renewable_life, buf, sizeof(buf));
    printf("%24s: %s\n", "Max renewable life", buf);
    printf("%24s: %d\n", "Kvno", princ->kvno);
    printf("%24s: %d\n", "Mkvno", princ->mkvno);
    printf("%24s: %s\n", "Policy", princ->policy ? princ->policy : "none");
    time_t2str(princ->last_success, buf, sizeof(buf), 1);
    printf("%24s: %s\n", "Last successful login", buf);
    time_t2str(princ->last_failed, buf, sizeof(buf), 1);
    printf("%24s: %s\n", "Last failed login", buf);
    printf("%24s: %d\n", "Failed login count", princ->fail_auth_count);
    time_t2str(princ->mod_date, buf, sizeof(buf), 1);
    printf("%24s: %s\n", "Last modified", buf);
    krb5_unparse_name_fixed(context, princ->mod_name, buf, sizeof(buf));
    printf("%24s: %s\n", "Modifier", buf);
    attributes2str (princ->attributes, buf, sizeof(buf));
    printf("%24s: %s\n", "Attributes", buf);

    printf("%24s: ", "Keytypes(salts)");

    for (i = 0; i < princ->n_key_data; ++i) {
	krb5_key_data *k = &princ->key_data[i];
	krb5_error_code ret;
	char *e_string, *s_string;

	ret = krb5_enctype_to_string (context,
				      k->key_data_type[0],
				      &e_string);
	if (ret)
	    asprintf (&e_string, "unknown(%d)", k->key_data_type[0]);

	ret = krb5_salttype_to_string (context,
				       k->key_data_type[0],
				       k->key_data_type[1],
				       &s_string);
	if (ret)
	    asprintf (&s_string, "unknown(%d)", k->key_data_type[1]);

	printf ("%s%s(%s)", (i != 0) ? ", " : "", e_string, s_string);
	free (e_string);
	free (s_string);
    }
    printf("\n\n");
}

static int
do_get_entry(krb5_principal principal, void *data)
{
    kadm5_principal_ent_rec princ;
    krb5_error_code ret;
    struct get_entry_data *e = data;
    
    memset(&princ, 0, sizeof(princ));
    ret = kadm5_get_principal(kadm_handle, principal, 
			      &princ,
			      KADM5_PRINCIPAL_NORMAL_MASK|KADM5_KEY_DATA);
    if(ret)
	return ret;
    else {
	if(e->header) {
	    (*e->header)();
	    e->header = NULL; /* XXX only once */
	}
	(e->format)(&princ);
	kadm5_free_principal_ent(kadm_handle, &princ);
    }
    return 0;
}

int
get_entry(int argc, char **argv)
{
    int i;
    krb5_error_code ret;
    struct get_entry_data data;
    struct getargs args[] = {
	{ "long",	'l',	arg_flag,	NULL, "long format" },
	{ "terse",	't',	arg_flag,	NULL, "terse format" },
    };
    int num_args = sizeof(args) / sizeof(args[0]);
    int optind = 0;
    int long_flag = 0;
    int terse_flag = 0;
    
    args[0].value = &long_flag;
    args[1].value = &terse_flag;
    if(getarg(args, num_args, argc, argv, &optind))
	goto usage;
    if(optind == argc)
	goto usage;

    if(long_flag) {
	data.format = print_entry_long;
	data.header = NULL;
    } else if(terse_flag) {
	data.format = print_entry_terse;
	data.header = NULL;
    } else {
	data.format = print_entry_short;
	data.header = print_header_short;
    }

    argc -= optind;
    argv += optind;

    for(i = 0; i < argc; i++)
	ret = foreach_principal(argv[i], do_get_entry, &data);
    return 0;
usage:
    arg_printusage (args, num_args, "get", "principal...");
    return 0;
}

int
list_princs(int argc, char **argv)
{
    int i;
    krb5_error_code ret;
    struct get_entry_data data;

    data.format = print_entry_terse;
    data.header = NULL;
    
    for(i = 1; i < argc; i++)
	ret = foreach_principal(argv[i], do_get_entry, &data);
    return 0;
}
