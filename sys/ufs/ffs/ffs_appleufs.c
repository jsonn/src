/* $NetBSD: ffs_appleufs.c,v 1.1.2.2 2002/10/10 18:44:52 jdolecek Exp $ */
/*
 * Copyright (c) 2002 Darrin B. Jewell
 * All rights reserved.
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
 *      This product includes software developed by Darrin B. Jewell
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_appleufs.c,v 1.1.2.2 2002/10/10 18:44:52 jdolecek Exp $");

#include <sys/param.h>
#include <sys/time.h>
#if defined(_KERNEL)
#include <sys/kernel.h>
#include <sys/systm.h>
#endif

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#if !defined(_KERNEL) && !defined(STANDALONE)
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#define KASSERT(x) assert(x)
#endif

/*
 * this is the same calculation as in_cksum
 */
u_int16_t
ffs_appleufs_cksum(appleufs)
	const struct appleufslabel *appleufs;
{
	const u_int16_t *p = (const u_int16_t *)appleufs;
	int len = sizeof(struct appleufslabel);
	long res = 0;
	while (len > 1)  {
		res += *p++;
		len -= 2;
	}
#if 0 /* sizeof(struct appleufslabel) is guaranteed to be even */
	if (len == 1)
		res += htons(*(u_char *)p<<8);
#endif
	res = (res >> 16) + (res & 0xffff);
	res += (res >> 16);
	return (~res);
}

/* copies o to n, validating and byteswapping along the way
 * returns 0 if ok, EINVAL if not valid
 */
int
ffs_appleufs_validate(name,o,n)
	const char *name;
	const struct appleufslabel *o;
	struct appleufslabel *n;
{
	struct appleufslabel tmp;
	if (!n) n = &tmp;

	if (o->ul_magic != ntohl(APPLEUFS_LABEL_MAGIC)) {
		return EINVAL;
	}
	*n = *o;
	n->ul_checksum = 0;
	n->ul_checksum = ffs_appleufs_cksum(n);
	if (n->ul_checksum != o->ul_checksum) {
#if defined(DIAGNOSTIC) || !defined(_KERNEL)
		printf("%s: invalid APPLE UFS checksum. found 0x%x, expecting 0x%x",
			name,o->ul_checksum,n->ul_checksum);
#endif
		return EINVAL;
	}
	n->ul_magic = ntohl(o->ul_magic);
	n->ul_version = ntohl(o->ul_version);
	n->ul_time = ntohl(o->ul_time);
	n->ul_namelen = ntohs(o->ul_namelen);

	if (n->ul_namelen > APPLEUFS_MAX_LABEL_NAME) {
#if defined(DIAGNOSTIC) || !defined(_KERNEL)
		printf("%s: APPLE UFS label name too long, truncated.\n",
				name);
#endif
		n->ul_namelen = APPLEUFS_MAX_LABEL_NAME;
	}
	/* if len is max, will set ul_reserved[0] */
	n->ul_name[n->ul_namelen] = '\0';	
#ifdef DEBUG
	printf("%s: found APPLE UFS label v%d: \"%s\"\n",
			name,n->ul_version,n->ul_name);
#endif
	
	return 0;
}

void
ffs_appleufs_set(appleufs,name,t)
	struct appleufslabel *appleufs;
	const char *name;
	time_t t;
{
	size_t namelen;
	if (!name) name = "untitled";
	if (t == ((time_t)-1)) {
#if defined(_KERNEL)
		t = time.tv_sec;
#elif defined(STANDALONE)
		t = 0;
#else
		(void)time(&t);
#endif
	}
	namelen = strlen(name);
	if (namelen > APPLEUFS_MAX_LABEL_NAME)
		namelen = APPLEUFS_MAX_LABEL_NAME;
	memset(appleufs, 0, sizeof(*appleufs));
	appleufs->ul_magic   = htonl(APPLEUFS_LABEL_MAGIC);
	appleufs->ul_version = htonl(APPLEUFS_LABEL_VERSION);
	appleufs->ul_time    = htonl((u_int32_t)t);
	appleufs->ul_namelen = htons(namelen);
	strncpy(appleufs->ul_name,name,namelen);
	appleufs->ul_checksum = ffs_appleufs_cksum(appleufs);
}
