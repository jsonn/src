/* buffer.c

   Buffer access functions for the object management protocol... */

/*
 * Copyright (c) 1999-2000 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include <omapip/omapip_p.h>

/* Make sure that at least len bytes are in the input buffer, and if not,
   read enough bytes to make up the difference. */

isc_result_t omapi_connection_reader (omapi_object_t *h)
{
	omapi_buffer_t *buffer;
	isc_result_t status;
	unsigned read_len;
	int read_status;
	omapi_connection_object_t *c;
	unsigned bytes_to_read;

	if (!h || h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	/* Make sure c -> bytes_needed is valid. */
	if (c -> bytes_needed < 0)
		return ISC_R_INVALIDARG;

	/* See if there are enough bytes. */
	if (c -> in_bytes >= OMAPI_BUF_SIZE - 1 &&
	    c -> in_bytes > c -> bytes_needed)
		return ISC_R_SUCCESS;

	if (c -> inbufs) {
		for (buffer = c -> inbufs; buffer -> next;
		     buffer = buffer -> next)
			;
		if (!BUFFER_BYTES_FREE (buffer)) {
			status = omapi_buffer_new (&buffer -> next, MDL);
			if (status != ISC_R_SUCCESS)
				return status;
			buffer = buffer -> next;
		}
	} else {
		status = omapi_buffer_new (&c -> inbufs, MDL);
		if (status != ISC_R_SUCCESS)
			return status;
		buffer = c -> inbufs;
	}

	bytes_to_read = BUFFER_BYTES_FREE (buffer);

	while (bytes_to_read) {
		if (buffer -> tail > buffer -> head)
			read_len = sizeof (buffer -> buf) - buffer -> tail;
		else
			read_len = buffer -> head - buffer -> tail;

		read_status = read (c -> socket,
				    &buffer -> buf [buffer -> tail], read_len);
		if (read_status < 0) {
			if (errno == EWOULDBLOCK)
				break;
			else if (errno == EIO)
				return ISC_R_IOERROR;
			else if (errno == EINVAL)
				return ISC_R_INVALIDARG;
			else if (errno == ECONNRESET) {
				omapi_disconnect (h, 0);
				return ISC_R_SHUTTINGDOWN;
			} else
				return ISC_R_UNEXPECTED;
		}
		/* If we got a zero-length read, as opposed to EWOULDBLOCK,
		   the remote end closed the connection. */
		if (read_status == 0) {
			omapi_disconnect (h, 0);
			return ISC_R_SHUTTINGDOWN;
		}
		buffer -> tail += read_status;
		c -> in_bytes += read_status;
		if (buffer -> tail == sizeof buffer -> buf)
			buffer -> tail = 0;
		if (read_status < read_len)
			break;
		bytes_to_read -= read_status;
	}

	if (c -> bytes_needed <= c -> in_bytes) {
		omapi_signal (h, "ready", c);
	}
	return ISC_R_SUCCESS;
}

/* Put some bytes into the output buffer for a connection. */

isc_result_t omapi_connection_copyin (omapi_object_t *h,
				      const unsigned char *bufp,
				      unsigned len)
{
	omapi_buffer_t *buffer;
	isc_result_t status;
	int bytes_copied = 0;
	unsigned copy_len;
	int sig_flags = SIG_MODE_UPDATE;
	omapi_connection_object_t *c;

	/* Make sure len is valid. */
	if (len < 0)
		return ISC_R_INVALIDARG;
	if (!h || h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	if (c -> outbufs) {
		for (buffer = c -> outbufs;
		     buffer -> next; buffer = buffer -> next)
			;
	} else {
		status = omapi_buffer_new (&c -> outbufs, MDL);
		if (status != ISC_R_SUCCESS)
			return status;
		buffer = c -> outbufs;
	}

	while (bytes_copied < len) {
		/* If there is no space available in this buffer,
                   allocate a new one. */
		if (!BUFFER_BYTES_FREE (buffer)) {
			status = (omapi_buffer_new (&buffer -> next, MDL));
			if (status != ISC_R_SUCCESS)
				return status;
			buffer = buffer -> next;
		}

		if (buffer -> tail > buffer -> head)
			copy_len = sizeof (buffer -> buf) - buffer -> tail;
		else
			copy_len = buffer -> head - buffer -> tail;

		if (copy_len > (len - bytes_copied))
			copy_len = len - bytes_copied;

		if (c -> out_key) {
			if (!c -> out_context)
				sig_flags |= SIG_MODE_INIT;
			status = omapi_connection_sign_data
				(sig_flags, c -> out_key, &c -> out_context,
				 &bufp [bytes_copied], copy_len,
				 (omapi_typed_data_t **)0);
			if (status != ISC_R_SUCCESS)
				return status;
		}

		memcpy (&buffer -> buf [buffer -> tail],
			&bufp [bytes_copied], copy_len);
		buffer -> tail += copy_len;
		c -> out_bytes += copy_len;
		bytes_copied += copy_len;
		if (buffer -> tail == sizeof buffer -> buf)
			buffer -> tail = 0;
	}
	return ISC_R_SUCCESS;
}

/* Copy some bytes from the input buffer, and advance the input buffer
   pointer beyond the bytes copied out. */

isc_result_t omapi_connection_copyout (unsigned char *buf,
				       omapi_object_t *h,
				       unsigned size)
{
	unsigned bytes_remaining;
	unsigned bytes_this_copy;
	unsigned first_byte;
	omapi_buffer_t *buffer;
	unsigned char *bufp;
	int sig_flags = SIG_MODE_UPDATE;
	omapi_connection_object_t *c;
	isc_result_t status;

	if (!h || h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	if (size > c -> in_bytes)
		return ISC_R_NOMORE;
	bufp = buf;
	bytes_remaining = size;
	buffer = c -> inbufs;

	while (bytes_remaining) {
		if (!buffer)
			return ISC_R_UNEXPECTED;
		if (BYTES_IN_BUFFER (buffer)) {
			if (buffer -> head == (sizeof buffer -> buf) - 1)
				first_byte = 0;
			else
				first_byte = buffer -> head + 1;

			if (first_byte > buffer -> tail) {
				bytes_this_copy = (sizeof buffer -> buf -
						   first_byte);
			} else {
				bytes_this_copy =
					buffer -> tail - first_byte;
			}
			if (bytes_this_copy > bytes_remaining)
				bytes_this_copy = bytes_remaining;
			if (bufp) {
				if (c -> in_key) {
					if (!c -> in_context)
						sig_flags |= SIG_MODE_INIT;
					status = omapi_connection_sign_data
						(sig_flags,
						 c -> in_key,
						 &c -> in_context,
						 (unsigned char *)
						 &buffer -> buf [first_byte],
						 bytes_this_copy,
						 (omapi_typed_data_t **)0);
					if (status != ISC_R_SUCCESS)
						return status;
				}

				memcpy (bufp, &buffer -> buf [first_byte],
					bytes_this_copy);
				bufp += bytes_this_copy;
			}
			bytes_remaining -= bytes_this_copy;
			buffer -> head = first_byte + bytes_this_copy - 1;
			c -> in_bytes -= bytes_this_copy;
		}
			
		if (!BYTES_IN_BUFFER (buffer))
			buffer = buffer -> next;
	}

	/* Get rid of any input buffers that we emptied. */
	buffer = (omapi_buffer_t *)0;
	while (c -> inbufs &&
	       !BYTES_IN_BUFFER (c -> inbufs)) {
		if (c -> inbufs -> next) {
			omapi_buffer_reference (&buffer,
						c -> inbufs -> next, MDL);
			omapi_buffer_dereference (&c -> inbufs -> next, MDL);
		}
		omapi_buffer_dereference (&c -> inbufs, MDL);
		if (buffer) {
			omapi_buffer_reference
				(&c -> inbufs, buffer, MDL);
			omapi_buffer_dereference (&buffer, MDL);
		}
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_writer (omapi_object_t *h)
{
	unsigned bytes_this_write;
	int bytes_written;
	unsigned first_byte;
	omapi_buffer_t *buffer;
	unsigned char *bufp;
	omapi_connection_object_t *c;

	if (!h || h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	/* Already flushed... */
	if (!c -> out_bytes)
		return ISC_R_SUCCESS;

	buffer = c -> outbufs;

	while (c -> out_bytes) {
		if (!buffer)
			return ISC_R_UNEXPECTED;
		if (BYTES_IN_BUFFER (buffer)) {
			if (buffer -> head == (sizeof buffer -> buf) - 1)
				first_byte = 0;
			else
				first_byte = buffer -> head + 1;

			if (first_byte > buffer -> tail) {
				bytes_this_write = (sizeof buffer -> buf -
						   first_byte);
			} else {
				bytes_this_write =
					buffer -> tail - first_byte;
			}
			bytes_written = write (c -> socket,
					       &buffer -> buf [first_byte],
					       bytes_this_write);
			/* If the write failed with EWOULDBLOCK or we wrote
			   zero bytes, a further write would block, so we have
			   flushed as much as we can for now.   Other errors
			   are really errors. */
			if (bytes_written < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN)
					return ISC_R_SUCCESS;
				else if (errno == EPIPE)
					return ISC_R_NOCONN;
				else if (errno == EFBIG || errno == EDQUOT)
					return ISC_R_NORESOURCES;
				else if (errno == ENOSPC)
					return ISC_R_NOSPACE;
				else if (errno == EIO)
					return ISC_R_IOERROR;
				else if (errno == EINVAL)
					return ISC_R_INVALIDARG;
				else if (errno == ECONNRESET)
					return ISC_R_SHUTTINGDOWN;
				else
					return ISC_R_UNEXPECTED;
			}
			if (bytes_written == 0)
				return ISC_R_SUCCESS;

			buffer -> head = first_byte + bytes_written - 1;
			c -> out_bytes -= bytes_written;

			/* If we didn't finish out the write, we filled the
			   O.S. output buffer and a further write would block,
			   so stop trying to flush now. */
			if (bytes_written != bytes_this_write)
				return ISC_R_SUCCESS;
		}
			
		if (!BYTES_IN_BUFFER (buffer))
			buffer = buffer -> next;
	}
		
	/* Get rid of any output buffers we emptied. */
	buffer = (omapi_buffer_t *)0;
	while (c -> outbufs &&
	       !BYTES_IN_BUFFER (c -> outbufs)) {
		if (c -> outbufs -> next) {
			omapi_buffer_reference (&buffer,
						c -> outbufs -> next, MDL);
			omapi_buffer_dereference (&c -> outbufs -> next, MDL);
		}
		omapi_buffer_dereference (&c -> outbufs, MDL);
		if (buffer) {
			omapi_buffer_reference (&c -> outbufs, buffer, MDL);
			omapi_buffer_dereference (&buffer, MDL);
		}
	}
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_get_uint32 (omapi_object_t *c,
					  u_int32_t *result)
{
	u_int32_t inbuf;
	isc_result_t status;

	status = omapi_connection_copyout ((unsigned char *)&inbuf,
					   c, sizeof inbuf);
	if (status != ISC_R_SUCCESS)
		return status;

	*result = ntohl (inbuf);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_put_uint32 (omapi_object_t *c,
					  u_int32_t value)
{
	u_int32_t inbuf;
	isc_result_t status;

	inbuf = htonl (value);
	
	return omapi_connection_copyin (c, (unsigned char *)&inbuf,
					sizeof inbuf);
}

isc_result_t omapi_connection_get_uint16 (omapi_object_t *c,
					  u_int16_t *result)
{
	u_int16_t inbuf;
	isc_result_t status;

	status = omapi_connection_copyout ((unsigned char *)&inbuf,
					   c, sizeof inbuf);
	if (status != ISC_R_SUCCESS)
		return status;

	*result = ntohs (inbuf);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_put_uint16 (omapi_object_t *c,
					  u_int32_t value)
{
	u_int16_t inbuf;
	isc_result_t status;

	inbuf = htons (value);
	
	return omapi_connection_copyin (c, (unsigned char *)&inbuf,
					sizeof inbuf);
}

isc_result_t omapi_connection_write_typed_data (omapi_object_t *c,
						omapi_typed_data_t *data)
{
	isc_result_t status;
	omapi_handle_t handle;

	/* Null data is valid. */
	if (!data)
		return omapi_connection_put_uint32 (c, 0);

	switch (data -> type) {
	      case omapi_datatype_int:
		status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
		if (status != ISC_R_SUCCESS)
			return status;
		return omapi_connection_put_uint32 (c, ((u_int32_t)
							(data -> u.integer)));

	      case omapi_datatype_string:
	      case omapi_datatype_data:
		status = omapi_connection_put_uint32 (c, data -> u.buffer.len);
		if (status != ISC_R_SUCCESS)
			return status;
		if (data -> u.buffer.len)
			return omapi_connection_copyin
				(c, data -> u.buffer.value,
				 data -> u.buffer.len);
		return ISC_R_SUCCESS;

	      case omapi_datatype_object:
		if (data -> u.object) {
			status = omapi_object_handle (&handle,
						      data -> u.object);
			if (status != ISC_R_SUCCESS)
				return status;
		} else
			handle = 0;
		status = omapi_connection_put_uint32 (c, sizeof handle);
		if (status != ISC_R_SUCCESS)
			return status;
		return omapi_connection_put_uint32 (c, handle);

	}
	return ISC_R_INVALIDARG;
}

isc_result_t omapi_connection_put_name (omapi_object_t *c, const char *name)
{
	isc_result_t status;
	unsigned len = strlen (name);

	status = omapi_connection_put_uint16 (c, len);
	if (status != ISC_R_SUCCESS)
		return status;
	return omapi_connection_copyin (c, (const unsigned char *)name, len);
}

isc_result_t omapi_connection_put_string (omapi_object_t *c,
					  const char *string)
{
	isc_result_t status;
	unsigned len;

	if (string)
		len = strlen (string);
	else
		len = 0;

	status = omapi_connection_put_uint32 (c, len);
	if (status != ISC_R_SUCCESS)
		return status;
	if (len)
		return omapi_connection_copyin
			(c, (const unsigned char *)string, len);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_put_handle (omapi_object_t *c, omapi_object_t *h)
{
	isc_result_t status;
	omapi_handle_t handle;

	if (h) {
		status = omapi_object_handle (&handle, h);
		if (status != ISC_R_SUCCESS)
			return status;
	} else
		handle = 0;	/* The null handle. */
	status = omapi_connection_put_uint32 (c, sizeof handle);
	if (status != ISC_R_SUCCESS)
		return status;
	return omapi_connection_put_uint32 (c, handle);
}
