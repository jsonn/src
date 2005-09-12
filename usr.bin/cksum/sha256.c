/* $NetBSD: sha256.c,v 1.2.2.2 2005/09/12 12:18:33 tron Exp $ */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <crypto/sha2.h>	/* this hash type */
#include <md5.h>		/* the hash we're replacing */

#define HASHTYPE	"SHA256"
#define HASHLEN		64

#define MD5Filter	SHA256_Filter
#define MD5String	SHA256_String
#define MD5TestSuite	SHA256_TestSuite
#define MD5TimeTrial	SHA256_TimeTrial

#define MD5Data		SHA256_Data
#define MD5Init		SHA256_Init
#define MD5Update	SHA256_Update
#define MD5End		SHA256_End

#define MD5_CTX		SHA256_CTX

#include "md5.c"
