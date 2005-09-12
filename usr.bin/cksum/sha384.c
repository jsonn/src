/* $NetBSD: sha384.c,v 1.2.2.2 2005/09/12 12:18:33 tron Exp $ */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <crypto/sha2.h>	/* this hash type */
#include <md5.h>		/* the hash we're replacing */

#define HASHTYPE	"SHA384"
#define HASHLEN		96

#define MD5Filter	SHA384_Filter
#define MD5String	SHA384_String
#define MD5TestSuite	SHA384_TestSuite
#define MD5TimeTrial	SHA384_TimeTrial

#define MD5Data		SHA384_Data
#define MD5Init		SHA384_Init
#define MD5Update	SHA384_Update
#define MD5End		SHA384_End

#define MD5_CTX		SHA384_CTX

#include "md5.c"
