/* This is a generated file */
#ifndef __hdb_protos_h__
#define __hdb_protos_h__

#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

krb5_error_code
hdb_check_db_format __P((
	krb5_context context,
	HDB *db));

krb5_error_code
hdb_clear_master_key __P((
	krb5_context context,
	HDB *db));

krb5_error_code
hdb_create __P((
	krb5_context context,
	HDB **db,
	const char *filename));

krb5_error_code
hdb_db_create __P((
	krb5_context context,
	HDB **db,
	const char *filename));

krb5_error_code
hdb_enctype2key __P((
	krb5_context context,
	hdb_entry *e,
	krb5_enctype enctype,
	Key **key));

krb5_error_code
hdb_entry2string __P((
	krb5_context context,
	hdb_entry *ent,
	char **str));

int
hdb_entry2value __P((
	krb5_context context,
	hdb_entry *ent,
	krb5_data *value));

krb5_error_code
hdb_foreach __P((
	krb5_context context,
	HDB *db,
	unsigned flags,
	hdb_foreach_func_t func,
	void *data));

void
hdb_free_entry __P((
	krb5_context context,
	hdb_entry *ent));

void
hdb_free_key __P((Key *key));

krb5_error_code
hdb_init_db __P((
	krb5_context context,
	HDB *db));

int
hdb_key2principal __P((
	krb5_context context,
	krb5_data *key,
	krb5_principal p));

krb5_error_code
hdb_ldap_create __P((
	krb5_context context,
	HDB ** db,
	const char *filename));

krb5_error_code
hdb_lock __P((
	int fd,
	int operation));

krb5_error_code
hdb_ndbm_create __P((
	krb5_context context,
	HDB **db,
	const char *filename));

krb5_error_code
hdb_next_enctype2key __P((
	krb5_context context,
	hdb_entry *e,
	krb5_enctype enctype,
	Key **key));

int
hdb_principal2key __P((
	krb5_context context,
	krb5_principal p,
	krb5_data *key));

krb5_error_code
hdb_print_entry __P((
	krb5_context context,
	HDB *db,
	hdb_entry *entry,
	void *data));

krb5_error_code
hdb_process_master_key __P((
	krb5_context context,
	EncryptionKey key,
	krb5_data *schedule));

krb5_error_code
hdb_read_master_key __P((
	krb5_context context,
	const char *filename,
	EncryptionKey *key));

void
hdb_seal_keys __P((
	HDB *db,
	hdb_entry *ent));

krb5_error_code
hdb_set_master_key __P((
	krb5_context context,
	HDB *db,
	EncryptionKey key));

krb5_error_code
hdb_set_master_keyfile __P((
	krb5_context context,
	HDB *db,
	const char *keyfile));

krb5_error_code
hdb_unlock __P((int fd));

void
hdb_unseal_keys __P((
	HDB *db,
	hdb_entry *ent));

int
hdb_value2entry __P((
	krb5_context context,
	krb5_data *value,
	hdb_entry *ent));

#endif /* __hdb_protos_h__ */
