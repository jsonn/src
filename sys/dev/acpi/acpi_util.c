/*	$NetBSD: acpi_util.c,v 1.4.4.2 2010/05/30 05:17:17 rmind Exp $ */

/*-
 * Copyright (c) 2003, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum of By Noon Software, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright 2001, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_util.c,v 1.4.4.2 2010/05/30 05:17:17 rmind Exp $");

#include <sys/param.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT		ACPI_BUS_COMPONENT
ACPI_MODULE_NAME		("acpi_util")

/*
 * Evaluate an integer object.
 */
ACPI_STATUS
acpi_eval_integer(ACPI_HANDLE handle, const char *path, ACPI_INTEGER *valp)
{
	ACPI_OBJECT obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	buf.Pointer = &obj;
	buf.Length = sizeof(obj);

	rv = AcpiEvaluateObject(handle, path, NULL, &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	if (obj.Type != ACPI_TYPE_INTEGER)
		return AE_TYPE;

	if (valp != NULL)
		*valp = obj.Integer.Value;

	return AE_OK;
}

/*
 * Evaluate an integer object with a single integer input parameter.
 */
ACPI_STATUS
acpi_eval_set_integer(ACPI_HANDLE handle, const char *path, ACPI_INTEGER val)
{
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT obj;

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	obj.Type = ACPI_TYPE_INTEGER;
	obj.Integer.Value = val;

	arg.Count = 1;
	arg.Pointer = &obj;

	return AcpiEvaluateObject(handle, path, &arg, NULL);
}

/*
 * Evaluate a (Unicode) string object.
 */
ACPI_STATUS
acpi_eval_string(ACPI_HANDLE handle, const char *path, char **stringp)
{
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	rv = acpi_eval_struct(handle, path, &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_STRING) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->String.Length == 0) {
		rv = AE_BAD_DATA;
		goto out;
	}

	*stringp = ACPI_ALLOCATE(obj->String.Length + 1);

	if (*stringp == NULL) {
		rv = AE_NO_MEMORY;
		goto out;
	}

	(void)memcpy(*stringp, obj->String.Pointer, obj->String.Length);

	(*stringp)[obj->String.Length] = '\0';

out:
	ACPI_FREE(buf.Pointer);

	return rv;
}

/*
 * Evaluate a structure. Caller must free buf.Pointer by ACPI_FREE().
 */
ACPI_STATUS
acpi_eval_struct(ACPI_HANDLE handle, const char *path, ACPI_BUFFER *buf)
{

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	buf->Pointer = NULL;
	buf->Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	return AcpiEvaluateObject(handle, path, NULL, buf);
}

/*
 * Evaluate a reference handle from an element in a package.
 */
ACPI_STATUS
acpi_eval_reference_handle(ACPI_OBJECT *elm, ACPI_HANDLE *handle)
{

	if (elm == NULL || handle == NULL)
		return AE_BAD_PARAMETER;

	switch (elm->Type) {

	case ACPI_TYPE_ANY:
	case ACPI_TYPE_LOCAL_REFERENCE:

		if (elm->Reference.Handle == NULL)
			return AE_NULL_ENTRY;

		*handle = elm->Reference.Handle;

		return AE_OK;

	case ACPI_TYPE_STRING:
		return AcpiGetHandle(NULL, elm->String.Pointer, handle);

	default:
		return AE_TYPE;
	}
}

/*
 * Iterate over all objects in a package, and pass them all
 * to a function. If the called function returns non-AE_OK,
 * the iteration is stopped and that value is returned.
 */
ACPI_STATUS
acpi_foreach_package_object(ACPI_OBJECT *pkg,
    ACPI_STATUS (*func)(ACPI_OBJECT *, void *), void *arg)
{
	ACPI_STATUS rv = AE_OK;
	uint32_t i;

	if (pkg == NULL)
		return AE_BAD_PARAMETER;

	if (pkg->Type != ACPI_TYPE_PACKAGE)
		return AE_TYPE;

	for (i = 0; i < pkg->Package.Count; i++) {

		rv = (*func)(&pkg->Package.Elements[i], arg);

		if (ACPI_FAILURE(rv))
			break;
	}

	return rv;
}

/*
 * Fetch data info the specified (empty) ACPI buffer.
 * Caller must free buf.Pointer by ACPI_FREE().
 */
ACPI_STATUS
acpi_get(ACPI_HANDLE handle, ACPI_BUFFER *buf,
    ACPI_STATUS (*getit)(ACPI_HANDLE, ACPI_BUFFER *))
{

	buf->Pointer = NULL;
	buf->Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	return (*getit)(handle, buf);
}

/*
 * Get a device node from a handle.
 */
struct acpi_devnode *
acpi_get_node(ACPI_HANDLE handle)
{
	struct acpi_softc *sc = acpi_softc; /* XXX. */
	struct acpi_devnode *ad;

	if (sc == NULL || handle == NULL)
		return NULL;

	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		if (ad->ad_handle == handle)
			return ad;
	}

	aprint_debug_dev(sc->sc_dev, "%s: failed to "
	    "find node %s\n", __func__, acpi_name(handle));

	return NULL;
}

/*
 * Return a complete pathname from a handle.
 *
 * Note that the function uses static data storage;
 * if the data is needed for future use, it should be
 * copied before any subsequent calls overwrite it.
 */
const char *
acpi_name(ACPI_HANDLE handle)
{
	static char name[80];
	ACPI_BUFFER buf;
	ACPI_STATUS rv;

	if (handle == NULL)
		handle = ACPI_ROOT_OBJECT;

	buf.Pointer = name;
	buf.Length = sizeof(name);

	rv = AcpiGetName(handle, ACPI_FULL_PATHNAME, &buf);

	if (ACPI_FAILURE(rv))
		return "UNKNOWN";

	return name;
}

/*
 * Match given IDs against _HID and _CIDs.
 */
int
acpi_match_hid(ACPI_DEVICE_INFO *ad, const char * const *ids)
{
	uint32_t i, n;
	char *id;

	while (*ids) {

		if ((ad->Valid & ACPI_VALID_HID) != 0) {

			if (pmatch(ad->HardwareId.String, *ids, NULL) == 2)
				return 1;
		}

		if ((ad->Valid & ACPI_VALID_CID) != 0) {

			n = ad->CompatibleIdList.Count;

			for (i = 0; i < n; i++) {

				id = ad->CompatibleIdList.Ids[i].String;

				if (pmatch(id, *ids, NULL) == 2)
					return 1;
			}
		}

		ids++;
	}

	return 0;
}

