/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
 */
#include "config.h"

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif

#include <string.h>

#include "packet-parse.h"
#include "crypto.h"
#include "errors.h"
#include "netpgpdefs.h"
#include "parse_local.h"
#include "memory.h"
#include "writer.h"

#define DECOMPRESS_BUFFER	1024

typedef struct {
	__ops_compression_type_t type;
	__ops_region_t   *region;
	unsigned char   in[DECOMPRESS_BUFFER];
	unsigned char   out[DECOMPRESS_BUFFER];
	z_stream        zstream;/* ZIP and ZLIB */
	size_t          offset;
	int             inflate_ret;
}               z_decompress_t;

typedef struct {
	__ops_compression_type_t type;
	__ops_region_t   *region;
	char            in[DECOMPRESS_BUFFER];
	char            out[DECOMPRESS_BUFFER];
	bz_stream       bzstream;	/* BZIP2 */
	size_t          offset;
	int             inflate_ret;
}               bz_decompress_t;

typedef struct {
	z_stream        stream;
	unsigned char  *src;
	unsigned char  *dst;
}               compress_t;

/*
 * \todo remove code duplication between this and
 * bzip2_compressed_data_reader
 */
static int 
zlib_compressed_data_reader(void *dest, size_t length,
			    __ops_error_t ** errors,
			    __ops_reader_info_t * rinfo,
			    __ops_parse_cb_info_t * cbinfo)
{
	z_decompress_t *z = __ops_reader_get_arg(rinfo);
	size_t           len;
	char		*cdest = dest;
	int              cc;

	assert(z->type == OPS_C_ZIP || z->type == OPS_C_ZLIB);

	if (z->inflate_ret == Z_STREAM_END &&
	    z->zstream.next_out == &z->out[z->offset]) {
		return 0;
	}

	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "zlib_compressed_data_reader: length %" PRIsize "d\n", length);
	}

	if (z->region->length_read == z->region->length) {
		if (z->inflate_ret != Z_STREAM_END) {
			OPS_ERROR(cbinfo->errors, OPS_E_P_DECOMPRESSION_ERROR,
				"Compressed data didn't end when region ended.");
		}
	}
	for (cc = 0 ; cc < length ; cc += len) {
		if (&z->out[z->offset] == z->zstream.next_out) {
			int             ret;

			z->zstream.next_out = z->out;
			z->zstream.avail_out = sizeof(z->out);
			z->offset = 0;
			if (z->zstream.avail_in == 0) {
				unsigned        n = z->region->length;

				if (!z->region->indeterminate) {
					n -= z->region->length_read;
					if (n > sizeof(z->in)) {
						n = sizeof(z->in);
					}
				} else {
					n = sizeof(z->in);
				}

				if (!__ops_stacked_limited_read(z->in, n, z->region,
						     errors, rinfo, cbinfo)) {
					return -1;
				}

				z->zstream.next_in = z->in;
				z->zstream.avail_in = (z->region->indeterminate) ?
					z->region->last_read : n;
			}
			ret = inflate(&z->zstream, Z_SYNC_FLUSH);
			if (ret == Z_STREAM_END) {
				if (!z->region->indeterminate &&
				    z->region->length_read != z->region->length) {
					OPS_ERROR(cbinfo->errors,
						OPS_E_P_DECOMPRESSION_ERROR,
						"Compressed stream ended before packet end.");
				}
			} else if (ret != Z_OK) {
				(void) fprintf(stderr, "ret=%d\n", ret);
				OPS_ERROR(cbinfo->errors, OPS_E_P_DECOMPRESSION_ERROR, z->zstream.msg);
			}
			z->inflate_ret = ret;
		}
		assert(z->zstream.next_out > &z->out[z->offset]);
		len = z->zstream.next_out - &z->out[z->offset];
		if (len > length) {
			len = length;
		}
		(void) memcpy(&cdest[cc], &z->out[z->offset], len);
		z->offset += len;
	}

	return length;
}

/* \todo remove code duplication between this and zlib_compressed_data_reader */
static int 
bzip2_compressed_data_reader(void *dest, size_t length,
			     __ops_error_t ** errors,
			     __ops_reader_info_t * rinfo,
			     __ops_parse_cb_info_t * cbinfo)
{
	bz_decompress_t *bz = __ops_reader_get_arg(rinfo);
	size_t		len;
	char		*cdest = dest;
	int		cc;

	assert(bz->type == OPS_C_BZIP2);

	if (bz->inflate_ret == BZ_STREAM_END &&
	    bz->bzstream.next_out == &bz->out[bz->offset]) {
		return 0;
	}
	if (bz->region->length_read == bz->region->length) {
		if (bz->inflate_ret != BZ_STREAM_END) {
			OPS_ERROR(cbinfo->errors, OPS_E_P_DECOMPRESSION_ERROR,
				"Compressed data didn't end when region ended.");
		}
	}
	for (cc = 0 ; cc < length ; cc += len) {
		if (&bz->out[bz->offset] == bz->bzstream.next_out) {
			int             ret;

			bz->bzstream.next_out = (char *) bz->out;
			bz->bzstream.avail_out = sizeof(bz->out);
			bz->offset = 0;
			if (bz->bzstream.avail_in == 0) {
				unsigned        n = bz->region->length;

				if (!bz->region->indeterminate) {
					n -= bz->region->length_read;
					if (n > sizeof(bz->in))
						n = sizeof(bz->in);
				} else
					n = sizeof(bz->in);

				if (!__ops_stacked_limited_read((unsigned char *) bz->in, n, bz->region,
						     errors, rinfo, cbinfo))
					return -1;

				bz->bzstream.next_in = bz->in;
				bz->bzstream.avail_in = bz->region->indeterminate
					? bz->region->last_read : n;
			}
			ret = BZ2_bzDecompress(&bz->bzstream);
			if (ret == BZ_STREAM_END) {
				if (!bz->region->indeterminate &&
				    bz->region->length_read != bz->region->length)
					OPS_ERROR(cbinfo->errors,
						OPS_E_P_DECOMPRESSION_ERROR,
						"Compressed stream ended before packet end.");
			} else if (ret != BZ_OK) {
				OPS_ERROR_1(cbinfo->errors,
					OPS_E_P_DECOMPRESSION_ERROR,
					"Invalid return %d from BZ2_bzDecompress", ret);
			}
			bz->inflate_ret = ret;
		}
		assert(bz->bzstream.next_out > &bz->out[bz->offset]);
		len = bz->bzstream.next_out - &bz->out[bz->offset];
		if (len > length)
			len = length;
		(void) memcpy(&cdest[cc], &bz->out[bz->offset], len);
		bz->offset += len;
	}

	return length;
}

/**
 * \ingroup Core_Compress
 *
 * \param *region 	Pointer to a region
 * \param *parse_info 	How to parse
 * \param type Which compression type to expect
*/

int 
__ops_decompress(__ops_region_t *region, __ops_parse_info_t *parse_info,
	       __ops_compression_type_t type)
{
	z_decompress_t z;
	bz_decompress_t bz;
	int             ret;

	switch (type) {
	case OPS_C_ZIP:
	case OPS_C_ZLIB:
		(void) memset(&z, 0x0, sizeof(z));

		z.region = region;
		z.offset = 0;
		z.type = type;

		z.zstream.next_in = Z_NULL;
		z.zstream.avail_in = 0;
		z.zstream.next_out = z.out;
		z.zstream.zalloc = Z_NULL;
		z.zstream.zfree = Z_NULL;
		z.zstream.opaque = Z_NULL;

		break;

	case OPS_C_BZIP2:
		(void) memset(&bz, 0x0, sizeof(bz));

		bz.region = region;
		bz.offset = 0;
		bz.type = type;

		bz.bzstream.next_in = NULL;
		bz.bzstream.avail_in = 0;
		bz.bzstream.next_out = bz.out;
		bz.bzstream.bzalloc = NULL;
		bz.bzstream.bzfree = NULL;
		bz.bzstream.opaque = NULL;

		break;

	default:
		OPS_ERROR_1(&parse_info->errors,
			OPS_E_ALG_UNSUPPORTED_COMPRESS_ALG,
			"Compression algorithm %d is not yet supported", type);
		return 0;
	}

	switch (type) {
	case OPS_C_ZIP:
		ret = inflateInit2(&z.zstream, -15);
		break;

	case OPS_C_ZLIB:
		ret = inflateInit(&z.zstream);
		break;

	case OPS_C_BZIP2:
		ret = BZ2_bzDecompressInit(&bz.bzstream, 1, 0);
		break;

	default:
		OPS_ERROR_1(&parse_info->errors,
			OPS_E_ALG_UNSUPPORTED_COMPRESS_ALG,
			"Compression algorithm %d is not yet supported", type);
		return 0;
	}

	switch (type) {
	case OPS_C_ZIP:
	case OPS_C_ZLIB:
		if (ret != Z_OK) {
			OPS_ERROR_1(&parse_info->errors,
				OPS_E_P_DECOMPRESSION_ERROR,
				"Cannot initialise ZIP or ZLIB stream for decompression: error=%d", ret);
			return 0;
		}
		__ops_reader_push(parse_info, zlib_compressed_data_reader, NULL, &z);
		break;

	case OPS_C_BZIP2:
		if (ret != BZ_OK) {
			OPS_ERROR_1(&parse_info->errors,
				OPS_E_P_DECOMPRESSION_ERROR,
				"Cannot initialise BZIP2 stream for decompression: error=%d", ret);
			return 0;
		}
		__ops_reader_push(parse_info, bzip2_compressed_data_reader, NULL, &bz);
		break;

	default:
		OPS_ERROR_1(&parse_info->errors,
			OPS_E_ALG_UNSUPPORTED_COMPRESS_ALG,
			"Compression algorithm %d is not yet supported", type);
		return 0;
	}

	ret = __ops_parse(parse_info);

	__ops_reader_pop(parse_info);

	return ret;
}

/**
\ingroup Core_WritePackets
\brief Writes Compressed packet
\param data Data to write out
\param len Length of data
\param cinfo Write settings
\return true if OK; else false
*/

bool 
__ops_write_compressed(const unsigned char *data,
		     const unsigned int len,
		     __ops_create_info_t *cinfo)
{
	compress_t	*zip = calloc(1, sizeof(compress_t));
	size_t		 sz_in = 0;
	size_t		 sz_out = 0;
	int              r = 0;

	/* compress the data */
	const int       level = Z_DEFAULT_COMPRESSION;	/* \todo allow varying
							 * levels */
	zip->stream.zalloc = Z_NULL;
	zip->stream.zfree = Z_NULL;
	zip->stream.opaque = NULL;

	/* all other fields set to zero by use of calloc */

	if (deflateInit(&zip->stream, level) != Z_OK) {
		/* can't initialise */
		assert(/* CONSTCOND */0);
	}
	/* do necessary transformation */
	/* copy input to maintain const'ness of src */
	assert(zip->src == NULL);
	assert(zip->dst == NULL);

	sz_in = len * sizeof(unsigned char);
	sz_out = (sz_in * 1.01) + 12;	/* from zlib webpage */
	zip->src = calloc(1, sz_in);
	zip->dst = calloc(1, sz_out);
	(void) memcpy(zip->src, data, len);

	/* setup stream */
	zip->stream.next_in = zip->src;
	zip->stream.avail_in = sz_in;
	zip->stream.total_in = 0;

	zip->stream.next_out = zip->dst;
	zip->stream.avail_out = sz_out;
	zip->stream.total_out = 0;

	r = deflate(&zip->stream, Z_FINISH);
	assert(r == Z_STREAM_END);	/* need to loop if not */

	/* write it out */
	return (__ops_write_ptag(OPS_PTAG_CT_COMPRESSED, cinfo) &&
		__ops_write_length((unsigned)(zip->stream.total_out + 1), cinfo) &&
		__ops_write_scalar(OPS_C_ZLIB, 1, cinfo) &&
		__ops_write(zip->dst, (unsigned)zip->stream.total_out, cinfo));
}
