/*
 * Copyright (c) 1999, 2000, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software  nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hdb_locl.h"

RCSID("$Id: hdb-ldap.c,v 1.1.1.1.4.2 2000/06/16 18:32:50 thorpej Exp $");

#ifdef OPENLDAP

#include <ldap.h>
#include <lber.h>
#include <ctype.h>
#include <sys/un.h>

static krb5_error_code LDAP__connect(krb5_context context, HDB * db);

static krb5_error_code
LDAP_message2entry(krb5_context context, HDB * db, LDAPMessage * msg,
		   hdb_entry * ent);

static char *krb5kdcentry_attrs[] =
    { "krb5PrincipalName", "cn", "krb5PrincipalRealm",
    "krb5KeyVersionNumber", "krb5Key",
    "krb5ValidStart", "krb5ValidEnd", "krb5PasswordEnd",
    "krb5MaxLife", "krb5MaxRenew", "krb5KDCFlags", "krb5EncryptionType",
    "modifiersName", "modifyTimestamp", "creatorsName", "createTimestamp",
    NULL
};

static char *krb5principal_attrs[] =
    { "krb5PrincipalName", "cn", "krb5PrincipalRealm",
    "modifiersName", "modifyTimestamp", "creatorsName", "createTimestamp",
    NULL
};

/* based on samba: source/passdb/ldap.c */
static krb5_error_code
LDAP_addmod_len(LDAPMod *** modlist, int modop, const char *attribute,
		unsigned char *value, size_t len)
{
    LDAPMod **mods = *modlist;
    int i, j;

    if (mods == NULL) {
	mods = (LDAPMod **) calloc(1, sizeof(LDAPMod *));
	if (mods == NULL) {
	    return ENOMEM;
	}
	mods[0] = NULL;
    }

    for (i = 0; mods[i] != NULL; ++i) {
	if ((mods[i]->mod_op & (~LDAP_MOD_BVALUES)) == modop
	    && (!strcasecmp(mods[i]->mod_type, attribute))) {
	    break;
	}
    }

    if (mods[i] == NULL) {
	mods = (LDAPMod **) realloc(mods, (i + 2) * sizeof(LDAPMod *));
	if (mods == NULL) {
	    return ENOMEM;
	}
	mods[i] = (LDAPMod *) malloc(sizeof(LDAPMod));
	if (mods[i] == NULL) {
	    return ENOMEM;
	}
	mods[i]->mod_op = modop | LDAP_MOD_BVALUES;
	mods[i]->mod_bvalues = NULL;
	mods[i]->mod_type = strdup(attribute);
	if (mods[i]->mod_type == NULL) {
	    return ENOMEM;
	}
	mods[i + 1] = NULL;
    }

    if (value != NULL) {
	j = 0;
	if (mods[i]->mod_bvalues != NULL) {
	    for (; mods[i]->mod_bvalues[j] != NULL; j++);
	}
	mods[i]->mod_bvalues =
	    (struct berval **) realloc(mods[i]->mod_bvalues,
				       (j + 2) * sizeof(struct berval *));
	if (mods[i]->mod_bvalues == NULL) {
	    return ENOMEM;
	}
	/* Caller allocates memory on our behalf, unlike LDAP_addmod. */
	mods[i]->mod_bvalues[j] =
	    (struct berval *) malloc(sizeof(struct berval));
	if (mods[i]->mod_bvalues[j] == NULL) {
	    return ENOMEM;
	}
	mods[i]->mod_bvalues[j]->bv_val = value;
	mods[i]->mod_bvalues[j]->bv_len = len;
	mods[i]->mod_bvalues[j + 1] = NULL;
    }
    *modlist = mods;
    return 0;
}

static krb5_error_code
LDAP_addmod(LDAPMod *** modlist, int modop, const char *attribute,
	    const char *value)
{
    LDAPMod **mods = *modlist;
    int i, j;

    if (mods == NULL) {
	mods = (LDAPMod **) calloc(1, sizeof(LDAPMod *));
	if (mods == NULL) {
	    return ENOMEM;
	}
	mods[0] = NULL;
    }

    for (i = 0; mods[i] != NULL; ++i) {
	if (mods[i]->mod_op == modop
	    && (!strcasecmp(mods[i]->mod_type, attribute))) {
	    break;
	}
    }

    if (mods[i] == NULL) {
	mods = (LDAPMod **) realloc(mods, (i + 2) * sizeof(LDAPMod *));
	if (mods == NULL) {
	    return ENOMEM;
	}
	mods[i] = (LDAPMod *) malloc(sizeof(LDAPMod));
	if (mods[i] == NULL) {
	    return ENOMEM;
	}
	mods[i]->mod_op = modop;
	mods[i]->mod_values = NULL;
	mods[i]->mod_type = strdup(attribute);
	if (mods[i]->mod_type == NULL) {
	    return ENOMEM;
	}
	mods[i + 1] = NULL;
    }

    if (value != NULL) {
	j = 0;
	if (mods[i]->mod_values != NULL) {
	    for (; mods[i]->mod_values[j] != NULL; j++);
	}
	mods[i]->mod_values = (char **) realloc(mods[i]->mod_values,
						(j + 2) * sizeof(char *));
	if (mods[i]->mod_values == NULL) {
	    return ENOMEM;
	}
	mods[i]->mod_values[j] = strdup(value);
	if (mods[i]->mod_values[j] == NULL) {
	    return ENOMEM;
	}
	mods[i]->mod_values[j + 1] = NULL;
    }
    *modlist = mods;
    return 0;
}

static krb5_error_code
LDAP_addmod_generalized_time(LDAPMod *** mods, int modop,
			     const char *attribute, KerberosTime * time)
{
    char buf[22];
    struct tm *tm;

    /* XXX not threadsafe */
    tm = gmtime(time);
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%SZ", tm);

    return LDAP_addmod(mods, modop, attribute, buf);
}

static krb5_error_code
LDAP_get_string_value(HDB * db, LDAPMessage * entry,
		      const char *attribute, char **ptr)
{
    char **vals;
    int ret;

    vals = ldap_get_values((LDAP *) db->db, entry, (char *) attribute);
    if (vals == NULL) {
	return HDB_ERR_NOENTRY;
    }
    *ptr = strdup(vals[0]);
    if (*ptr == NULL) {
	ret = ENOMEM;
    } else {
	ret = 0;
    }

    ldap_value_free(vals);

    return ret;
}

static krb5_error_code
LDAP_get_integer_value(HDB * db, LDAPMessage * entry,
		       const char *attribute, int *ptr)
{
    char **vals;

    vals = ldap_get_values((LDAP *) db->db, entry, (char *) attribute);
    if (vals == NULL) {
	return HDB_ERR_NOENTRY;
    }
    *ptr = atoi(vals[0]);
    ldap_value_free(vals);
    return 0;
}

static krb5_error_code
LDAP_get_generalized_time_value(HDB * db, LDAPMessage * entry,
				const char *attribute, KerberosTime * kt)
{
    char *tmp, *gentime;
    struct tm tm;
    int ret;

    *kt = 0;

    ret = LDAP_get_string_value(db, entry, attribute, &gentime);
    if (ret != 0) {
	return ret;
    }

    tmp = strptime(gentime, "%Y%m%d%H%M%SZ", &tm);
    if (tmp == NULL) {
	free(gentime);
	return HDB_ERR_NOENTRY;
    }

    free(gentime);

    *kt = timegm(&tm);

    return 0;
}

static krb5_error_code
LDAP_entry2mods(krb5_context context, HDB * db, hdb_entry * ent,
		LDAPMessage * msg, LDAPMod *** pmods)
{
    krb5_error_code ret;
    krb5_boolean is_new_entry;
    int rc, i;
    char *tmp = NULL;
    LDAPMod **mods = NULL;
    hdb_entry orig;
    unsigned long oflags, nflags;

    if (msg != NULL) {
	ret = LDAP_message2entry(context, db, msg, &orig);
	if (ret != 0) {
	    goto out;
	}
	is_new_entry = FALSE;
    } else {
	/* to make it perfectly obvious we're depending on
	 * orig being intiialized to zero */
	memset(&orig, 0, sizeof(orig));
	is_new_entry = TRUE;
    }

    if (is_new_entry) {
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", "top");
	if (ret != 0) {
	    goto out;
	}
	/* person is the structural object class */
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", "person");
	if (ret != 0) {
	    goto out;
	}
	ret =
	    LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass",
			"krb5Principal");
	if (ret != 0) {
	    goto out;
	}
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass",
			  "krb5KDCEntry");
	if (ret != 0) {
	    goto out;
	}
    }

    if (is_new_entry ||
	krb5_principal_compare(context, ent->principal, orig.principal) ==
	FALSE) {
	ret = krb5_unparse_name(context, ent->principal, &tmp);
	if (ret != 0) {
	    goto out;
	}
	ret =
	    LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5PrincipalName", tmp);
	if (ret != 0) {
	    free(tmp);
	    goto out;
	}
	free(tmp);
    }

    if (ent->kvno != orig.kvno) {
	rc = asprintf(&tmp, "%d", ent->kvno);
	if (rc < 0) {
	    ret = ENOMEM;
	    goto out;
	}
	ret =
	    LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5KeyVersionNumber",
			tmp);
	free(tmp);
	if (ret != 0) {
	    goto out;
	}
    }

    if (ent->valid_start) {
	if (orig.valid_end == NULL
	    || (*(ent->valid_start) != *(orig.valid_start))) {
	    ret =
		LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
					     "krb5ValidStart",
					     ent->valid_start);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->valid_end) {
	if (orig.valid_end == NULL
	    || (*(ent->valid_end) != *(orig.valid_end))) {
	    ret =
		LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
					     "krb5ValidEnd",
					     ent->valid_end);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->pw_end) {
	if (orig.pw_end == NULL || (*(ent->pw_end) != *(orig.pw_end))) {
	    ret =
		LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
					     "krb5PasswordEnd",
					     ent->pw_end);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->max_life) {
	if (orig.max_life == NULL
	    || (*(ent->max_life) != *(orig.max_life))) {
	    rc = asprintf(&tmp, "%d", *(ent->max_life));
	    if (rc < 0) {
		ret = ENOMEM;
		goto out;
	    }
	    ret = LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5MaxLife", tmp);
	    free(tmp);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->max_renew) {
	if (orig.max_renew == NULL
	    || (*(ent->max_renew) != *(orig.max_renew))) {
	    rc = asprintf(&tmp, "%d", *(ent->max_renew));
	    if (rc < 0) {
		ret = ENOMEM;
		goto out;
	    }
	    ret =
		LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5MaxRenew", tmp);
	    free(tmp);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    memset(&oflags, 0, sizeof(oflags));
    memcpy(&oflags, &orig.flags, sizeof(HDBFlags));
    memset(&nflags, 0, sizeof(nflags));
    memcpy(&nflags, &ent->flags, sizeof(HDBFlags));

    if (memcmp(&oflags, &nflags, sizeof(HDBFlags))) {
	rc = asprintf(&tmp, "%lu", nflags);
	if (rc < 0) {
	    ret = ENOMEM;
	    goto out;
	}
	ret = LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5KDCFlags", tmp);
	free(tmp);
	if (ret != 0) {
	    goto out;
	}
    }

    if (is_new_entry == FALSE && orig.keys.len > 0) {
	/* for the moment, clobber and replace keys. */
	ret = LDAP_addmod(&mods, LDAP_MOD_DELETE, "krb5Key", NULL);
	if (ret != 0) {
	    goto out;
	}
    }

    for (i = 0; i < ent->keys.len; i++) {
	unsigned char *buf;
	size_t len;
	Key new;

	ret = copy_Key(&ent->keys.val[i], &new);
	if (ret != 0) {
	    goto out;
	}

	len = length_Key(&new);
	buf = malloc(len);
	if (buf == NULL) {
	    ret = ENOMEM;
	    free_Key(&new);
	    goto out;
	}

	ret = encode_Key(buf + len - 1, len, &new, &len);
	if (ret != 0) {
	    free(buf);
	    free_Key(&new);
	    goto out;
	}
	free_Key(&new);

	/* addmod_len _owns_ the key, doesn't need to copy it */
	ret = LDAP_addmod_len(&mods, LDAP_MOD_ADD, "krb5Key", buf, len);
	if (ret != 0) {
	    goto out;
	}
    }

    if (ent->etypes) {
	/* clobber and replace encryption types. */
	if (is_new_entry == FALSE) {
	    ret =
		LDAP_addmod(&mods, LDAP_MOD_DELETE, "krb5EncryptionType",
			    NULL);
	}
	for (i = 0; i < ent->etypes->len; i++) {
	    rc = asprintf(&tmp, "%d", ent->etypes->val[i]);
	    if (rc < 0) {
		ret = ENOMEM;
		goto out;
	    }
	    free(tmp);
	    ret =
		LDAP_addmod(&mods, LDAP_MOD_ADD, "krb5EncryptionType",
			    tmp);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    /* for clarity */
    ret = 0;

  out:

    if (ret == 0) {
	*pmods = mods;
    } else if (mods != NULL) {
	ldap_mods_free(mods, 1);
	*pmods = NULL;
    }

    if (msg != NULL) {
	hdb_free_entry(context, &orig);
    }

    return ret;
}

static krb5_error_code
LDAP_dn2principal(krb5_context context, HDB * db, const char *dn,
		  krb5_principal * principal)
{
    krb5_error_code ret;
    int rc;
    char **values;
    LDAPMessage *res = NULL, *e;

    rc = 1;
    (void) ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, &rc);
    rc = ldap_search_s((LDAP *) db->db, db->name, LDAP_SCOPE_BASE,
		       "(objectclass=krb5Principal)", krb5principal_attrs,
		       0, &res);

    if (rc != LDAP_SUCCESS) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    e = ldap_first_entry((LDAP *) db->db, res);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    values = ldap_get_values((LDAP *) db->db, e, "krb5PrincipalName");
    if (values == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = krb5_parse_name(context, values[0], principal);
    ldap_value_free(values);

  out:
    if (res != NULL) {
	ldap_msgfree(res);
    }
    return ret;
}

static krb5_error_code
LDAP__lookup_princ(krb5_context context, HDB * db, const char *princname,
		   LDAPMessage ** msg)
{
    krb5_error_code ret;
    int rc;
    char *filter = NULL;

    (void) LDAP__connect(context, db);

    rc =
	asprintf(&filter,
		 "(&(objectclass=krb5KDCEntry)(krb5PrincipalName=%s))",
		 princname);
    if (rc < 0) {
	ret = ENOMEM;
	goto out;
    }

    rc = 1;
    (void) ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, (void *) &rc);

    rc = ldap_search_s((LDAP *) db->db, db->name,
		       LDAP_SCOPE_ONELEVEL, filter, NULL, 0, msg);
    if (rc != LDAP_SUCCESS) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = 0;

  out:
    if (filter != NULL) {
	free(filter);
    }
    return ret;
}

static krb5_error_code
LDAP_principal2message(krb5_context context, HDB * db,
		       krb5_principal princ, LDAPMessage ** msg)
{
    char *princname = NULL;
    krb5_error_code ret;

    ret = krb5_unparse_name(context, princ, &princname);
    if (ret != 0) {
	return ret;
    }

    ret = LDAP__lookup_princ(context, db, princname, msg);
    free(princname);

    return ret;
}

/*
 * Construct an hdb_entry from a directory entry.
 */
static krb5_error_code
LDAP_message2entry(krb5_context context, HDB * db, LDAPMessage * msg,
		   hdb_entry * ent)
{
    char *unparsed_name = NULL, *dn = NULL;
    int ret;
    unsigned long tmp;
    struct berval **keys;
    char **values;

    memset(ent, 0, sizeof(*ent));
    memset(&ent->flags, 0, sizeof(HDBFlags));

    ret =
	LDAP_get_string_value(db, msg, "krb5PrincipalName",
			      &unparsed_name);
    if (ret != 0) {
	return ret;
    }

    ret = krb5_parse_name(context, unparsed_name, &ent->principal);
    if (ret != 0) {
	goto out;
    }

    ret =
	LDAP_get_integer_value(db, msg, "krb5KeyVersionNumber",
			       &ent->kvno);
    if (ret != 0) {
	ent->kvno = 0;
    }

    keys = ldap_get_values_len((LDAP *) db->db, msg, "krb5Key");
    if (keys != NULL) {
	int i;
	size_t l;

	ent->keys.len = ldap_count_values_len(keys);
	ent->keys.val = (Key *) calloc(ent->keys.len, sizeof(Key));
	for (i = 0; i < ent->keys.len; i++) {
	    decode_Key((unsigned char *) keys[i]->bv_val,
		       (size_t) keys[i]->bv_len, &ent->keys.val[i], &l);
	}
	ber_bvecfree(keys);
    } else {
#if 1
	/*
	 * This violates the ASN1 but it allows a principal to
	 * be related to a general directory entry without creating
	 * the keys. Hopefully it's OK.
	 */
	ent->keys.len = 0;
	ent->keys.val = NULL;
#else
	ret = HDB_ERR_NOENTRY;
	goto out;
#endif
    }

    ret =
	LDAP_get_generalized_time_value(db, msg, "createTimestamp",
					&ent->created_by.time);
    if (ret != 0) {
	ent->created_by.time = time(NULL);
    }

    ent->created_by.principal = NULL;

    ret = LDAP_get_string_value(db, msg, "creatorsName", &dn);
    if (ret == 0) {
	if (LDAP_dn2principal(context, db, dn, &ent->created_by.principal)
	    != 0) {
	    ent->created_by.principal = NULL;
	}
	free(dn);
    }

    ent->modified_by = (Event *) malloc(sizeof(Event));
    if (ent->modified_by == NULL) {
	ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "modifyTimestamp",
					&ent->modified_by->time);
    if (ret == 0) {
	ret = LDAP_get_string_value(db, msg, "modifiersName", &dn);
	if (LDAP_dn2principal
	    (context, db, dn, &ent->modified_by->principal) != 0) {
	    ent->modified_by->principal = NULL;
	}
	free(dn);
    } else {
	free(ent->modified_by);
	ent->modified_by = NULL;
    }

    if ((ent->valid_start = (KerberosTime *) malloc(sizeof(KerberosTime)))
	== NULL) {
	ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "krb5ValidStart",
					ent->valid_start);
    if (ret != 0) {
	/* OPTIONAL */
	free(ent->valid_start);
	ent->valid_start = NULL;
    }

    if ((ent->valid_end = (KerberosTime *) malloc(sizeof(KerberosTime))) ==
	NULL) {ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "krb5ValidEnd",
					ent->valid_end);
    if (ret != 0) {
	/* OPTIONAL */
	free(ent->valid_end);
	ent->valid_end = NULL;
    }

    if ((ent->pw_end = (KerberosTime *) malloc(sizeof(KerberosTime))) ==
	NULL) {ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "krb5PasswordEnd",
					ent->pw_end);
    if (ret != 0) {
	/* OPTIONAL */
	free(ent->pw_end);
	ent->pw_end = NULL;
    }

    ent->max_life = (int *) malloc(sizeof(int));
    if (ent->max_life == NULL) {
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_integer_value(db, msg, "krb5MaxLife", ent->max_life);
    if (ret != 0) {
	free(ent->max_life);
	ent->max_life = NULL;
    }

    ent->max_renew = (int *) malloc(sizeof(int));
    if (ent->max_renew == NULL) {
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_integer_value(db, msg, "krb5MaxRenew", ent->max_renew);
    if (ret != 0) {
	free(ent->max_renew);
	ent->max_renew = NULL;
    }

    values = ldap_get_values((LDAP *) db->db, msg, "krb5KDCFlags");
    if (values != NULL) {
	tmp = strtoul(values[0], (char **) NULL, 10);
	if (tmp == ULONG_MAX && errno == ERANGE) {
	    ret = ERANGE;
	    goto out;
	}
    } else {
	tmp = 0;
    }
    memcpy(&ent->flags, &tmp, sizeof(HDBFlags));

    values = ldap_get_values((LDAP *) db->db, msg, "krb5EncryptionType");
    if (values != NULL) {
	int i;

	ent->etypes = malloc(sizeof(*(ent->etypes)));
	if (ent->etypes == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	ent->etypes->len = ldap_count_values(values);
	ent->etypes->val = calloc(ent->etypes->len, sizeof(int));
	for (i = 0; i < ent->etypes->len; i++) {
	    ent->etypes->val[i] = atoi(values[i]);
	}
	ldap_value_free(values);
    }

    ret = 0;

  out:
    if (unparsed_name != NULL) {
	free(unparsed_name);
    }

    if (ret != 0) {
	/* I don't think this frees ent itself. */
	hdb_free_entry(context, ent);
    }

    return ret;
}

static krb5_error_code LDAP_close(krb5_context context, HDB * db)
{
    LDAP *ld = (LDAP *) db->db;

    ldap_unbind(ld);
    db->db = NULL;
    return 0;
}

static krb5_error_code
LDAP_lock(krb5_context context, HDB * db, int operation)
{
    return 0;
}

static krb5_error_code LDAP_unlock(krb5_context context, HDB * db)
{
    return 0;
}

static krb5_error_code
LDAP_seq(krb5_context context, HDB * db, unsigned flags, hdb_entry * entry)
{
    int msgid, rc, parserc;
    krb5_error_code ret;
    LDAPMessage *e;

    msgid = db->openp;		/* BOGUS OVERLOADING */
    if (msgid < 0) {
	return HDB_ERR_NOENTRY;
    }

    do {
	rc = ldap_result((LDAP *) db->db, msgid, LDAP_MSG_ONE, NULL, &e);
	switch (rc) {
	case LDAP_RES_SEARCH_ENTRY:
	    /* We have an entry. Parse it. */
	    ret = LDAP_message2entry(context, db, e, entry);
	    ldap_msgfree(e);
	    break;
	case LDAP_RES_SEARCH_RESULT:
	    /* We're probably at the end of the results. If not, abandon. */
	    parserc =
		ldap_parse_result((LDAP *) db->db, e, NULL, NULL, NULL,
				  NULL, NULL, 1);
	    if (parserc != LDAP_SUCCESS
		&& parserc != LDAP_MORE_RESULTS_TO_RETURN) {
		ldap_abandon((LDAP *) db->db, msgid);
	    }
	    ret = HDB_ERR_NOENTRY;
	    db->openp = -1;
	    break;
	case 0:
	case -1:
	default:
	    /* Some unspecified error (timeout?). Abandon. */
	    ldap_msgfree(e);
	    ldap_abandon((LDAP *) db->db, msgid);
	    ret = HDB_ERR_NOENTRY;
	    db->openp = -1;
	    break;
	}
    } while (rc == LDAP_RES_SEARCH_REFERENCE);

    if (ret == 0) {
	if (db->master_key_set && (flags & HDB_F_DECRYPT))
	    hdb_unseal_keys(db, entry);
    }

    return ret;
}

static krb5_error_code
LDAP_firstkey(krb5_context context, HDB * db, unsigned flags,
	      hdb_entry * entry)
{
    int msgid;

    (void) LDAP__connect(context, db);

    msgid = LDAP_NO_LIMIT;
    (void) ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, &msgid);

    msgid = ldap_search((LDAP *) db->db, db->name,
			LDAP_SCOPE_ONELEVEL, "(objectclass=krb5KDCEntry)",
			krb5kdcentry_attrs, 0);
    if (msgid < 0) {
	return HDB_ERR_NOENTRY;
    }

    db->openp = msgid;

    return LDAP_seq(context, db, flags, entry);
}

static krb5_error_code
LDAP_nextkey(krb5_context context, HDB * db, unsigned flags,
	     hdb_entry * entry)
{
    return LDAP_seq(context, db, flags, entry);
}

static krb5_error_code
LDAP_rename(krb5_context context, HDB * db, const char *new_name)
{
    return HDB_ERR_DB_INUSE;
}

static krb5_boolean LDAP__is_user_namingcontext(const char *ctx,
						char *const *subschema)
{
    char *const *p;

    if (!strcasecmp(ctx, "CN=MONITOR")
	|| !strcasecmp(ctx, "CN=CONFIG")) {
	return FALSE;
    }

    if (subschema != NULL) {
	for (p = subschema; *p != NULL; p++) {
	    if (!strcasecmp(ctx, *p)) {
		return FALSE;
	    }
	}
    }

    return TRUE;
}

static krb5_error_code LDAP__connect(krb5_context context, HDB * db)
{
    int rc;
    krb5_error_code ret;
    char *attrs[] = { "namingContexts", "subschemaSubentry", NULL };
    LDAPMessage *res = NULL, *e;

    if (db->db != NULL) {
	/* connection has been opened. ping server. */
	struct sockaddr_un addr;
	int sd, len;

	if (ldap_get_option((LDAP *) db->db, LDAP_OPT_DESC, &sd) == 0 &&
	    getpeername(sd, (struct sockaddr *) &addr, &len) < 0) {
	    /* the other end has died. reopen. */
	    LDAP_close(context, db);
	}
    }

    if (db->db != NULL) {
	/* server is UP */
	return 0;
    }

    rc = ldap_initialize((LDAP **) & db->db, "ldapi:///");
    if (rc != LDAP_SUCCESS) {
	return HDB_ERR_NOENTRY;
    }

    rc = LDAP_VERSION3;
    (void) ldap_set_option((LDAP *) db->db, LDAP_OPT_PROTOCOL_VERSION, &rc);

    /* XXX set db->name to the search base */
    rc = ldap_search_s((LDAP *) db->db, "", LDAP_SCOPE_BASE,
		       "(objectclass=*)", attrs, 0, &res);
    if (rc != LDAP_SUCCESS) {
	ret = HDB_ERR_BADVERSION;
	goto out;
    }

    e = ldap_first_entry((LDAP *) db->db, res);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    if (db->name == NULL) {
	char **contexts = NULL, **schema_contexts, **p;

	contexts = ldap_get_values((LDAP *) db->db, e, "namingContexts");
	if (contexts == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}

	schema_contexts =
	    ldap_get_values((LDAP *) db->db, e, "subschemaSubentry");

	if (db->name != NULL) {
	    free(db->name);
	    db->name = NULL;
	}

	for (p = contexts; *p != NULL; p++) {
	    if (LDAP__is_user_namingcontext(*p, schema_contexts)) {
		break;
	    }
	}

	db->name = strdup(*p);
	if (db->name == NULL) {
	    ldap_value_free(contexts);
	    ret = ENOMEM;
	    goto out;
	}

	ldap_value_free(contexts);
	if (schema_contexts != NULL) {
	    ldap_value_free(schema_contexts);
	}
    }

    ret = 0;

  out:

    if (res != NULL) {
	ldap_msgfree(res);
    }

    if (ret != 0) {
	if (db->db != NULL) {
	    ldap_unbind((LDAP *) db->db);
	    db->db = NULL;
	}
    }

    return ret;
}

static krb5_error_code
LDAP_open(krb5_context context, HDB * db, int flags, mode_t mode)
{
    krb5_error_code ret;

    /* Not the right place for this. */
#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGPIPE, &sa, NULL);
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    if (db->name != NULL) {
	free(db->name);
	db->name = NULL;
    }

    ret = LDAP__connect(context, db);
    if (ret != 0) {
	return ret;
    }

    return ret;
}

static krb5_error_code
LDAP_fetch(krb5_context context, HDB * db, unsigned flags,
	   hdb_entry * entry)
{
    LDAPMessage *msg, *e;
    krb5_error_code ret;

    ret = LDAP_principal2message(context, db, entry->principal, &msg);
    if (ret != 0) {
	return ret;
    }

    e = ldap_first_entry((LDAP *) db->db, msg);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = LDAP_message2entry(context, db, e, entry);
    if (ret == 0) {
	if (db->master_key_set && (flags & HDB_F_DECRYPT))
	    hdb_unseal_keys(db, entry);
    }

  out:
    ldap_msgfree(msg);

    return ret;
}

static krb5_error_code
LDAP_store(krb5_context context, HDB * db, unsigned flags,
	   hdb_entry * entry)
{
    LDAPMod **mods = NULL;
    krb5_error_code ret;
    LDAPMessage *msg = NULL, *e = NULL;
    char *dn = NULL, *name = NULL;

    ret = krb5_unparse_name(context, entry->principal, &name);
    if (ret != 0) {
	goto out;
    }

    ret = LDAP__lookup_princ(context, db, name, &msg);
    if (ret == 0) {
	e = ldap_first_entry((LDAP *) db->db, msg);
    }

    hdb_seal_keys(db, entry);

    /* turn new entry into LDAPMod array */
    ret = LDAP_entry2mods(context, db, entry, e, &mods);
    if (ret != 0) {
	goto out;
    }

    if (e == NULL) {
	/* Doesn't exist yet. */
	char *p;

	e = NULL;

	/* normalize the naming attribute */
	for (p = name; *p != '\0'; p++) {
	    *p = (char) tolower((int) *p);
	}

	/*
	 * We could do getpwnam() on the local component of
	 * the principal to find cn/sn but that's probably
	 * bad thing to do from inside a KDC. Better leave
	 * it to management tools.
	 */
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "cn", name);
	if (ret < 0) {
	    goto out;
	}

	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "sn", name);
	if (ret < 0) {
	    goto out;
	}

	ret = asprintf(&dn, "cn=%s,%s", name, db->name);
	if (ret < 0) {
	    ret = ENOMEM;
	    goto out;
	}
    } else if (flags & HDB_F_REPLACE) {
	/* Entry exists, and we're allowed to replace it. */
	dn = ldap_get_dn((LDAP *) db->db, e);
    } else {
	/* Entry exists, but we're not allowed to replace it. Bail. */
	ret = HDB_ERR_EXISTS;
	goto out;
    }

    /* write entry into directory */
    if (e == NULL) {
	/* didn't exist before */
	ret = ldap_add_s((LDAP *) db->db, dn, mods);
    } else {
	/* already existed, send deltas only */
	ret = ldap_modify_s((LDAP *) db->db, dn, mods);
    }

    if (ret == LDAP_SUCCESS) {
	ret = 0;
    } else {
	ret = HDB_ERR_CANT_LOCK_DB;
    }

  out:
    /* free stuff */
    if (dn != NULL) {
	free(dn);
    }

    if (msg != NULL) {
	ldap_msgfree(msg);
    }

    if (mods != NULL) {
	ldap_mods_free(mods, 1);
    }

    if (name != NULL) {
	free(name);
    }

    return ret;
}

static krb5_error_code
LDAP_remove(krb5_context context, HDB * db, hdb_entry * entry)
{
    krb5_error_code ret;
    LDAPMessage *msg, *e;
    char *dn = NULL;

    ret = LDAP_principal2message(context, db, entry->principal, &msg);
    if (ret != 0) {
	goto out;
    }

    e = ldap_first_entry((LDAP *) db->db, msg);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    dn = ldap_get_dn((LDAP *) db->db, e);
    if (dn == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = LDAP_NO_LIMIT;
    (void) ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, &ret);

    ret = ldap_delete_s((LDAP *) db->db, dn);
    if (ret == LDAP_SUCCESS) {
	ret = 0;
    } else {
	ret = HDB_ERR_CANT_LOCK_DB;
    }

  out:
    if (dn != NULL) {
	free(dn);
    }

    if (msg != NULL) {
	ldap_msgfree(msg);
    }

    return ret;
}

static krb5_error_code
LDAP__get(krb5_context context, HDB * db, krb5_data key, krb5_data * reply)
{
    fprintf(stderr, "LDAP__get not implemented\n");
    abort();
    return 0;
}

static krb5_error_code
LDAP__put(krb5_context context, HDB * db, int replace,
	  krb5_data key, krb5_data value)
{
    fprintf(stderr, "LDAP__put not implemented\n");
    abort();
    return 0;
}

static krb5_error_code
LDAP__del(krb5_context context, HDB * db, krb5_data key)
{
    fprintf(stderr, "LDAP__del not implemented\n");
    abort();
    return 0;
}

static krb5_error_code LDAP_destroy(krb5_context context, HDB * db)
{
    krb5_error_code ret;

    ret = hdb_clear_master_key(context, db);
    free(db->name);
    free(db);

    return ret;
}

krb5_error_code
hdb_ldap_create(krb5_context context, HDB ** db, const char *filename)
{
    *db = malloc(sizeof(**db));
    if (*db == NULL)
	return ENOMEM;

    (*db)->db = NULL;
/*    (*db)->name = strdup(filename); */
    (*db)->name = NULL;
    (*db)->master_key_set = 0;
    (*db)->openp = 0;
    (*db)->open = LDAP_open;
    (*db)->close = LDAP_close;
    (*db)->fetch = LDAP_fetch;
    (*db)->store = LDAP_store;
    (*db)->remove = LDAP_remove;
    (*db)->firstkey = LDAP_firstkey;
    (*db)->nextkey = LDAP_nextkey;
    (*db)->lock = LDAP_lock;
    (*db)->unlock = LDAP_unlock;
    (*db)->rename = LDAP_rename;
    /* can we ditch these? */
    (*db)->_get = LDAP__get;
    (*db)->_put = LDAP__put;
    (*db)->_del = LDAP__del;
    (*db)->destroy = LDAP_destroy;

    return 0;
}

#endif				/* OPENLDAP */
