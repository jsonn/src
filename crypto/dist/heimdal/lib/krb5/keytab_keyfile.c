/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska H�gskolan
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

#include "krb5_locl.h"

RCSID("$Id: keytab_keyfile.c,v 1.1.1.1.4.2 2000/06/16 18:32:59 thorpej Exp $");

/* afs keyfile operations --------------------------------------- */

/*
 * Minimum tools to handle the AFS KeyFile.
 * 
 * Format of the KeyFile is:
 * <int32_t numkeys> {[<int32_t kvno> <char[8] deskey>] * numkeys}
 *
 * It just adds to the end of the keyfile, deleting isn't implemented.
 * Use your favorite text/hex editor to delete keys.
 *
 */

#define AFS_SERVERTHISCELL "/usr/afs/etc/ThisCell"
#define AFS_SERVERMAGICKRBCONF "/usr/afs/etc/krb.conf"

struct akf_data {
    int num_entries;
    char *filename;
    char *cell;
    char *realm;
};

/*
 * set `d->cell' and `d->realm'
 */

static int
get_cell_and_realm (struct akf_data *d)
{
    FILE *f;
    char buf[BUFSIZ], *cp;

    f = fopen (AFS_SERVERTHISCELL, "r");
    if (f == NULL)
	return errno;
    if (fgets (buf, sizeof(buf), f) == NULL) {
	fclose (f);
	return EINVAL;
    }
    if (buf[strlen(buf) - 1] == '\n')
	buf[strlen(buf) - 1] = '\0';
    fclose(f);

    d->cell = strdup (buf);
    if (d->cell == NULL)
	return errno;

    f = fopen (AFS_SERVERMAGICKRBCONF, "r");
    if (f != NULL) {
	if (fgets (buf, sizeof(buf), f) == NULL) {
	    fclose (f);
	    return EINVAL;
	}
	if (buf[strlen(buf)-1] == '\n')
	    buf[strlen(buf)-1] = '\0';
	fclose(f);
    }
    /* uppercase */
    for (cp = buf; *cp != '\0'; cp++)
	*cp = toupper(*cp);
    
    d->realm = strdup (buf);
    if (d->realm == NULL) {
	free (d->cell);
	return errno;
    }
    return 0;
}

/*
 * init and get filename
 */

static krb5_error_code
akf_resolve(krb5_context context, const char *name, krb5_keytab id)
{
    int ret;
    struct akf_data *d = malloc(sizeof (struct akf_data));

    if (d == NULL)
	return errno;
    
    d->num_entries = 0;
    ret = get_cell_and_realm (d);
    if (ret) {
	free (d);
	return ret;
    }
    d->filename = strdup (name);
    if (d->filename == NULL) {
	free (d->cell);
	free (d->realm);
	free (d);
	return ENOMEM;
    }
    id->data = d;
    
    return 0;
}

/*
 * cleanup
 */

static krb5_error_code
akf_close(krb5_context context, krb5_keytab id)
{
    struct akf_data *d = id->data;

    free (d->filename);
    free (d->cell);
    free (d);
    return 0;
}

/*
 * Return filename
 */

static krb5_error_code 
akf_get_name(krb5_context context, 
	     krb5_keytab id, 
	     char *name, 
	     size_t name_sz)
{
    struct akf_data *d = id->data;

    strlcpy (name, d->filename, name_sz);
    return 0;
}

/*
 * Init 
 */

static krb5_error_code
akf_start_seq_get(krb5_context context, 
		  krb5_keytab id, 
		  krb5_kt_cursor *c)
{
    int32_t ret;
    struct akf_data *d = id->data;

    c->fd = open (d->filename, O_RDONLY|O_BINARY, 0600);
    if (c->fd < 0)
	return errno;

    c->sp = krb5_storage_from_fd(c->fd);
    ret = krb5_ret_int32(c->sp, &d->num_entries);
    if(ret) {
	krb5_storage_free(c->sp);
	close(c->fd);
	return ret;
    }

    return 0;
}

static krb5_error_code
akf_next_entry(krb5_context context, 
	       krb5_keytab id, 
	       krb5_keytab_entry *entry, 
	       krb5_kt_cursor *cursor)
{
    struct akf_data *d = id->data;
    int32_t kvno;
    off_t pos;
    int ret;

    pos = cursor->sp->seek(cursor->sp, 0, SEEK_CUR);

    if ((pos - 4) / (4 + 8) >= d->num_entries)
	return KRB5_KT_END;

    ret = krb5_make_principal (context, &entry->principal,
			       d->realm, "afs", d->cell, NULL);
    if (ret)
	goto out;

    ret = krb5_ret_int32(cursor->sp, &kvno);
    if (ret) {
	krb5_free_principal (context, entry->principal);
	goto out;
    }

    entry->vno = (int8_t) kvno;

    entry->keyblock.keytype         = ETYPE_DES_CBC_MD5;
    entry->keyblock.keyvalue.length = 8;
    entry->keyblock.keyvalue.data   = malloc (8);
    if (entry->keyblock.keyvalue.data == NULL) {
	krb5_free_principal (context, entry->principal);
	ret = ENOMEM;
	goto out;
    }

    ret = cursor->sp->fetch(cursor->sp, entry->keyblock.keyvalue.data, 8);
    if(ret != 8)
	ret = (ret < 0) ? errno : KRB5_KT_END;

    entry->timestamp = time(NULL);

 out:
    cursor->sp->seek(cursor->sp, pos + 4 + 8, SEEK_SET);
    return ret;
}

static krb5_error_code
akf_end_seq_get(krb5_context context, 
		krb5_keytab id,
		krb5_kt_cursor *cursor)
{
    krb5_storage_free(cursor->sp);
    close(cursor->fd);
    return 0;
}

static krb5_error_code
akf_add_entry(krb5_context context,
	      krb5_keytab id,
	      krb5_keytab_entry *entry)
{
    struct akf_data *d = id->data;
    int fd, created = 0;
    int32_t kvno;

    fd = open (d->filename, O_RDWR | O_BINARY);
    if (fd < 0) {
	fd = open (d->filename,
		   O_RDWR | O_BINARY | O_CREAT, 0600);
	if (fd < 0)
	    return errno;
	created = 1;
    }

    if (entry->keyblock.keyvalue.length == 8
	&& entry->keyblock.keytype == ETYPE_DES_CBC_MD5) {

	int32_t len = 0;

	if (!created) {
	    if (lseek (fd, 0, SEEK_SET))
		return errno;
	    
	    if (read (fd, &len, sizeof(len)) != sizeof(len))
		return errno;
	}
	len += 1;

	if (lseek (fd, 0, SEEK_SET))
	    return errno;

	if (write (fd, &len, sizeof(len)) != sizeof(len))
	    return errno;

	if (lseek (fd, 4 + (len-1) * (8+4), SEEK_SET))
	    return errno;

	kvno = entry->vno;
	write(fd, &kvno, sizeof(kvno));
	write(fd, entry->keyblock.keyvalue.data, 8);
    }
    close (fd);
    return 0;
}

const krb5_kt_ops krb5_akf_ops = {
    "AFSKEYFILE",
    akf_resolve,
    akf_get_name,
    akf_close,
    NULL, /* get */
    akf_start_seq_get,
    akf_next_entry,
    akf_end_seq_get,
    akf_add_entry,
    NULL /* remove */
};
