/*
** Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
**	All rights reserved.
**
** By using this file, you agree to the terms and conditions set
** forth in the LICENSE file which can be found at the top level of
** the sendmail distribution.
**
** Id: smdb.h,v 8.29.2.1 2000/04/08 20:40:42 ca Exp
*/

#ifndef _SMDB_H_
# define _SMDB_H_

# include <sys/types.h>
# include <sys/stat.h>
# ifndef __P
#  include "sendmail/cdefs.h"
# endif /* __P */

# ifndef NDBM
#  ifndef NEWDB
ERROR	NDBM or NEWDB must be defined.
#  endif /* ! NEWDB */
# endif /* ! NDBM */

# ifdef NDBM
#  include <ndbm.h>
# endif /* NDBM */

# ifdef NEWDB
#  include <db.h>
#  ifndef DB_VERSION_MAJOR
#   define DB_VERSION_MAJOR 1
#  endif /* ! DB_VERSION_MAJOR */
# endif /* NEWDB */

/*
** Some size constants
*/
#define SMDB_MAX_USER_NAME_LEN	1024
#define SMDB_MAX_NAME_LEN	1024

/*
** This file defines the abstraction for database lookups. It is pretty
** much a copy of the db2 interface with the exception that every function
** returns 0 on success and non-zero on failure. The non-zero return code
** is meaningful.
**
** I'm going to put the function comments in this file since the interface
** MUST be the same for all inheritors of this interface.
*/

typedef struct database_struct SMDB_DATABASE;
typedef struct cursor_struct SMDB_CURSOR;
typedef union database_entity_union SMDB_DBENT;


/*
** DB_CLOSE_FUNC -- close the database
**
**	Parameters:
**		db -- The database to close.
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_close_func) __P((SMDB_DATABASE *db));



/*
** DB_DEL_FUNC -- removes a key and data pair from the database
**
**	Parameters:
**		db -- The database to close.
**		key -- The key to remove.
**		flags -- delete options. There are currently no defined
**			 flags for delete.
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_del_func) __P((SMDB_DATABASE *db,
			   SMDB_DBENT *key, u_int flags));



/*
** DB_FD_FUNC -- Returns a pointer to a file used for the database.
**
**	Parameters:
**		db -- The database to close.
**		fd -- A pointer to store the returned fd in.
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_fd_func) __P((SMDB_DATABASE *db, int* fd));



/*
** DB_GET_FUNC -- Gets the data associated with a key.
**
**	Parameters:
**		db -- The database to close.
**		key -- The key to access.
**		data -- A place to store the returned data.
**		flags -- get options. There are currently no defined
**			 flags for get.
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_get_func) __P((SMDB_DATABASE *db,
			   SMDB_DBENT *key,
			   SMDB_DBENT *data, u_int flags));



/*
** DB_PUT_FUNC -- Sets some data according to the key.
**
**	Parameters:
**		db -- The database to close.
**		key -- The key to use.
**		data -- The data to store.
**		flags -- put options:
**			SMDBF_NO_OVERWRITE - Return an error if key alread
**					     exists.
**			SMDBF_ALLOW_DUP - Allow duplicates in btree maps.
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_put_func) __P((SMDB_DATABASE *db,
			   SMDB_DBENT *key,
			   SMDB_DBENT *data, u_int flags));


/*
** DB_SYNC_FUNC -- Flush any cached information to disk.
**
**	Parameters:
**		db -- The database to sync.
**		flags -- sync options:
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_sync_func) __P((SMDB_DATABASE *db, u_int flags));


/*
** DB_SET_OWNER_FUNC -- Set the owner and group of the database files.
**
**	Parameters:
**		db -- The database to set.
**		uid -- The UID for the new owner (-1 for no change)
**		gid -- The GID for the new owner (-1 for no change)
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_set_owner_func) __P((SMDB_DATABASE *db, uid_t uid,
				 gid_t gid));


/*
** DB_CURSOR -- Obtain a cursor for sequential access
**
**	Parameters:
**		db -- The database to use.
**		cursor -- The address of a cursor pointer.
**		flags -- sync options:
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_cursor_func) __P((SMDB_DATABASE *db,
			      SMDB_CURSOR **cursor, u_int flags));


struct database_struct
{
	db_close_func		smdb_close;
	db_del_func		smdb_del;
	db_fd_func		smdb_fd;
	db_get_func		smdb_get;
	db_put_func		smdb_put;
	db_sync_func		smdb_sync;
	db_set_owner_func	smdb_set_owner;
	db_cursor_func		smdb_cursor;
	void			*smdb_impl;
};



/*
** DB_CURSOR_CLOSE -- Close a cursor
**
**	Parameters:
**		cursor -- The cursor to close.
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_cursor_close_func) __P((SMDB_CURSOR *cursor));


/*
** DB_CURSOR_DEL -- Delete the key/value pair of this cursor
**
**	Parameters:
**		cursor -- The cursor.
**		flags -- flags
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_cursor_del_func) __P((SMDB_CURSOR *cursor, u_int flags));


/*
** DB_CURSOR_GET -- Get the key/value of this cursor.
**
**	Parameters:
**		cursor -- The cursor.
**		key -- The current key.
**		value -- The current value
**		flags -- flags
**
**	Returns:
**		0 - Success, otherwise errno.
**		SMDBE_LAST_ENTRY - This is a success condition that
**				   gets returned when the end of the
**				   database is hit.
**
*/
typedef int (*db_cursor_get_func) __P((SMDB_CURSOR *cursor,
				  SMDB_DBENT *key,
				  SMDB_DBENT *data,
				  u_int flags));

/*
** Flags for DB_CURSOR_GET
*/
#define SMDB_CURSOR_GET_FIRST	0
#define SMDB_CURSOR_GET_LAST	1
#define SMDB_CURSOR_GET_NEXT	2
#define SMDB_CURSOR_GET_RANGE	3


/*
** DB_CURSOR_PUT -- Put the key/value at this cursor.
**
**	Parameters:
**		cursor -- The cursor.
**		key -- The current key.
**		value -- The current value
**		flags -- flags
**
**	Returns:
**		0 - Success, otherwise errno.
**
*/
typedef int (*db_cursor_put_func) __P((SMDB_CURSOR *cursor,
				  SMDB_DBENT *key,
				  SMDB_DBENT *data,
				  u_int flags));



struct cursor_struct
{
	db_cursor_close_func	smdbc_close;
	db_cursor_del_func	smdbc_del;
	db_cursor_get_func	smdbc_get;
	db_cursor_put_func	smdbc_put;
	void			*smdbc_impl;
};


struct database_params_struct
{
	u_int	smdbp_num_elements;
	u_int	smdbp_cache_size;
	bool	smdbp_allow_dup;
};

typedef struct database_params_struct SMDB_DBPARAMS;

struct database_user_struct
{
	uid_t	smdbu_id;
	gid_t	smdbu_group_id;
	char	smdbu_name[SMDB_MAX_USER_NAME_LEN];
};

typedef struct database_user_struct SMDB_USER_INFO;

union database_entity_union
{
# ifdef NDBM
	datum	dbm;
# endif /* NDBM */
# ifdef NEWDB
	DBT	db;
# endif /* NEWDB */
	struct
	{
		char	*data;
		size_t	size;
	} data;
};


typedef char *SMDB_DBTYPE;
typedef u_int SMDB_FLAG;

/*
** These are types of databases.
*/

# define SMDB_TYPE_DEFAULT	NULL
# define SMDB_TYPE_DEFAULT_LEN	0
# define SMDB_TYPE_HASH		"hash"
# define SMDB_TYPE_HASH_LEN	5
# define SMDB_TYPE_BTREE	"btree"
# define SMDB_TYPE_BTREE_LEN	6
# define SMDB_TYPE_NDBM		"dbm"
# define SMDB_TYPE_NDBM_LEN	4

/*
** These are flags
*/
/* Flags for put */
# define SMDBF_NO_OVERWRITE	0x00000001
# define SMDBF_ALLOW_DUP	0x00000002


extern SMDB_DATABASE	*smdb_malloc_database __P((void));
extern void		smdb_free_database __P((SMDB_DATABASE *));
extern int		smdb_open_database __P((SMDB_DATABASE **, char *, int,
						int, long, SMDB_DBTYPE,
						SMDB_USER_INFO *,
						SMDB_DBPARAMS *));
# ifdef NEWDB
extern int		smdb_db_open __P((SMDB_DATABASE **, char *, int, int,
					  long, SMDB_DBTYPE, SMDB_USER_INFO *,
					  SMDB_DBPARAMS *));
# endif /* NEWDB */
# ifdef NDBM
extern int		smdb_ndbm_open __P((SMDB_DATABASE **, char *, int, int,
					    long, SMDB_DBTYPE,
					    SMDB_USER_INFO *,
					    SMDB_DBPARAMS *));
# endif /* NDBM */
extern int		smdb_add_extension __P((char *, int, char *, char *));
extern int		smdb_setup_file __P((char *, char *, int, long,
					     SMDB_USER_INFO *, struct stat *));
extern int		smdb_lock_file __P((int *, char *, int, long, char *));
extern int		smdb_unlock_file __P((int));
extern int		smdb_filechanged __P((char *, char *, int,
					      struct stat *));
extern void		smdb_print_available_types __P((void));
extern char		*smdb_db_definition __P((SMDB_DBTYPE));
#endif /* ! _SMDB_H_ */
