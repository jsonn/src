/* omapi.c

   OMAPI object interfaces for the DHCP server. */

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

/* Many, many thanks to Brian Murrell and BCtel for this code - BCtel
   provided the funding that resulted in this code and the entire
   OMAPI support library being written, and Brian helped brainstorm
   and refine the requirements.  To the extent that this code is
   useful, you have Brian and BCtel to thank.  Any limitations in the
   code are a result of mistakes on my part.  -- Ted Lemon */

#ifndef lint
static char copyright[] =
"$Id: comapi.c,v 1.1.1.1.2.2 2000/10/18 04:11:01 tv Exp $ Copyright (c) 1999-2000 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include <omapip/omapip_p.h>

int (*dhcp_interface_shutdown_hook) (struct interface_info *);

omapi_object_type_t *dhcp_type_interface;
omapi_object_type_t *dhcp_type_group;
omapi_object_type_t *dhcp_type_shared_network;
omapi_object_type_t *dhcp_type_subnet;

void dhcp_common_objects_setup ()
{
	isc_result_t status;

	status = omapi_object_type_register (&dhcp_type_group,
					     "group",
					     dhcp_group_set_value,
					     dhcp_group_get_value,
					     dhcp_group_destroy,
					     dhcp_group_signal_handler,
					     dhcp_group_stuff_values,
					     dhcp_group_lookup, 
					     dhcp_group_create,
					     dhcp_group_remove, 0, 0, 0,
					     sizeof (struct group_object), 0);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register group object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_subnet,
					     "subnet",
					     dhcp_subnet_set_value,
					     dhcp_subnet_get_value,
					     dhcp_subnet_destroy,
					     dhcp_subnet_signal_handler,
					     dhcp_subnet_stuff_values,
					     dhcp_subnet_lookup, 
					     dhcp_subnet_create,
					     dhcp_subnet_remove, 0, 0, 0,
					     sizeof (struct subnet), 0);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register subnet object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register
		(&dhcp_type_shared_network,
		 "shared-network",
		 dhcp_shared_network_set_value,
		 dhcp_shared_network_get_value,
		 dhcp_shared_network_destroy,
		 dhcp_shared_network_signal_handler,
		 dhcp_shared_network_stuff_values,
		 dhcp_shared_network_lookup, 
		 dhcp_shared_network_create,
		 dhcp_shared_network_remove, 0, 0, 0,
		 sizeof (struct shared_network), 0);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register shared network object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_interface,
					     "interface",
					     dhcp_interface_set_value,
					     dhcp_interface_get_value,
					     dhcp_interface_destroy,
					     dhcp_interface_signal_handler,
					     dhcp_interface_stuff_values,
					     dhcp_interface_lookup, 
					     dhcp_interface_create,
					     dhcp_interface_remove,
					     0, 0, 0,
					     sizeof (struct interface_info),
					     0);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register interface object type: %s",
			   isc_result_totext (status));
}

isc_result_t dhcp_group_set_value  (omapi_object_t *h,
				    omapi_object_t *id,
				    omapi_data_string_t *name,
				    omapi_typed_data_t *value)
{
	struct group_object *group;
	isc_result_t status;
	int foo;

	if (h -> type != dhcp_type_group)
		return ISC_R_INVALIDARG;
	group = (struct group_object *)h;

	/* XXX For now, we can only set these values on new group objects. 
	   XXX Soon, we need to be able to update group objects. */
	if (!omapi_ds_strcmp (name, "name")) {
		if (group -> name)
			return ISC_R_EXISTS;
		if (value -> type == omapi_datatype_data ||
		    value -> type == omapi_datatype_string) {
			group -> name = dmalloc (value -> u.buffer.len + 1,
						 MDL);
			if (!group -> name)
				return ISC_R_NOMEMORY;
			memcpy (group -> name,
				value -> u.buffer.value,
				value -> u.buffer.len);
			group -> name [value -> u.buffer.len] = 0;
		} else
			return ISC_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp (name, "statements")) {
		if (group -> group && group -> group -> statements)
			return ISC_R_EXISTS;
		if (!group -> group) {
			if (!clone_group (&group -> group, root_group, MDL))
				return ISC_R_NOMEMORY;
		}
		if (value -> type == omapi_datatype_data ||
		    value -> type == omapi_datatype_string) {
			struct parse *parse;
			int lose = 0;
			parse = (struct parse *)0;
			status = new_parse (&parse, -1,
					    (char *)value -> u.buffer.value,
					    value -> u.buffer.len,
					    "network client");
			if (status != ISC_R_SUCCESS)
				return status;
			if (!(parse_executable_statements
			      (&group -> group -> statements, parse, &lose,
			       context_any))) {
				end_parse (&parse);
				return ISC_R_BADPARSE;
			}
			end_parse (&parse);
			return ISC_R_SUCCESS;
		} else
			return ISC_R_INVALIDARG;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == ISC_R_UNCHANGED)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_group_get_value (omapi_object_t *h, omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_value_t **value)
{
	struct group_object *group;
	isc_result_t status;
	struct data_string ip_addrs;

	if (h -> type != dhcp_type_group)
		return ISC_R_INVALIDARG;
	group = (struct group_object *)h;

	if (!omapi_ds_strcmp (name, "name"))
		return omapi_make_string_value (value,
						name, group -> name, MDL);

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_group_destroy (omapi_object_t *h, const char *file, int line)
{
	struct group_object *group, *t;
	isc_result_t status;

	if (h -> type != dhcp_type_group)
		return ISC_R_INVALIDARG;
	group = (struct group_object *)h;

	if (group -> name) {
		if (group_name_hash) {
			t = (struct group_object *)0;
			if (group_hash_lookup (&t, group_name_hash,
					       group -> name,
					       strlen (group -> name), MDL)) {
				group_hash_delete (group_name_hash,
						   group -> name,
						   strlen (group -> name),
						   MDL);
				group_object_dereference (&t, MDL);
			}
		}
		dfree (group -> name, file, line);
		group -> name = (char *)0;
	}
	if (group -> group)
		group_dereference (&group -> group, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_group_signal_handler (omapi_object_t *h,
					const char *name, va_list ap)
{
	struct group_object *group, *t;
	isc_result_t status;
	int updatep = 0;

	if (h -> type != dhcp_type_group)
		return ISC_R_INVALIDARG;
	group = (struct group_object *)h;

	if (!strcmp (name, "updated")) {
		/* A group object isn't valid if a subgroup hasn't yet been
		   associated with it. */
		if (!group -> group)
			return ISC_R_INVALIDARG;

		/* Group objects always have to have names. */
		if (!group -> name) {
			char hnbuf [64];
			sprintf (hnbuf, "ng%08lx%08lx",
				 cur_time, (unsigned long)group);
			group -> name = dmalloc (strlen (hnbuf) + 1, MDL);
			if (!group -> name)
				return ISC_R_NOMEMORY;
			strcpy (group -> name, hnbuf);
		}

		supersede_group (group, 1);
		updatep = 1;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	if (updatep)
		return ISC_R_SUCCESS;
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_group_stuff_values (omapi_object_t *c,
				      omapi_object_t *id,
				      omapi_object_t *h)
{
	struct group_object *group;
	isc_result_t status;

	if (h -> type != dhcp_type_group)
		return ISC_R_INVALIDARG;
	group = (struct group_object *)h;

	/* Write out all the values. */
	if (group -> name) {
		status = omapi_connection_put_name (c, "name");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_string (c, group -> name);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_group_lookup (omapi_object_t **lp,
				omapi_object_t *id, omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct group_object *group;

	/* First see if we were sent a handle. */
	status = omapi_get_value_str (ref, id, "handle", &tv);
	if (status == ISC_R_SUCCESS) {
		status = omapi_handle_td_lookup (lp, tv -> value);

		omapi_value_dereference (&tv, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Don't return the object if the type is wrong. */
		if ((*lp) -> type != dhcp_type_group) {
			omapi_object_dereference (lp, MDL);
			return ISC_R_INVALIDARG;
		}
	}

	/* Now look for a name. */
	status = omapi_get_value_str (ref, id, "name", &tv);
	if (status == ISC_R_SUCCESS) {
		group = (struct group_object *)0;
		if (group_name_hash &&
		    group_hash_lookup (&group, group_name_hash,
				       (const char *)
				       tv -> value -> u.buffer.value,
				       tv -> value -> u.buffer.len, MDL)) {
			omapi_value_dereference (&tv, MDL);

			if (*lp && *lp != (omapi_object_t *)group) {
			    group_object_dereference (&group, MDL);
			    omapi_object_dereference (lp, MDL);
			    return ISC_R_KEYCONFLICT;
			} else if (!*lp) {
			    /* XXX fix so that hash lookup itself creates
			       XXX the reference. */
			    omapi_object_reference (lp,
						    (omapi_object_t *)group,
						    MDL);
			    group_object_dereference (&group, MDL);
			}
		} else if (!*lp)
			return ISC_R_NOTFOUND;
	}

	/* If we get to here without finding a group, no valid key was
	   specified. */
	if (!*lp)
		return ISC_R_NOKEYS;

	if (((struct group_object *)(*lp)) -> flags & GROUP_OBJECT_DELETED) {
		omapi_object_dereference (lp, MDL);
		return ISC_R_NOTFOUND;
	}
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_group_create (omapi_object_t **lp,
			       omapi_object_t *id)
{
	struct group_object *group;
	isc_result_t status;
	group = (struct group_object *)0;

	status = group_object_allocate (&group, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	group -> flags = GROUP_OBJECT_DYNAMIC;
	status = omapi_object_reference (lp, (omapi_object_t *)group, MDL);
	group_object_dereference (&group, MDL);
	return status;
}

isc_result_t dhcp_group_remove (omapi_object_t *lp,
				omapi_object_t *id)
{
	struct group_object *group;
	isc_result_t status;
	if (lp -> type != dhcp_type_group)
		return ISC_R_INVALIDARG;
	group = (struct group_object *)lp;

	group -> flags |= GROUP_OBJECT_DELETED;
	if (group_write_hook) {
		if (!(*group_write_hook) (group))
			return ISC_R_IOERROR;
	}

	status = dhcp_group_destroy ((omapi_object_t *)group, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subnet_set_value  (omapi_object_t *h,
				     omapi_object_t *id,
				     omapi_data_string_t *name,
				     omapi_typed_data_t *value)
{
	struct subnet *subnet;
	isc_result_t status;
	int foo;

	if (h -> type != dhcp_type_subnet)
		return ISC_R_INVALIDARG;
	subnet = (struct subnet *)h;

	/* No values to set yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == ISC_R_UNCHANGED)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_subnet_get_value (omapi_object_t *h, omapi_object_t *id,
				    omapi_data_string_t *name,
				    omapi_value_t **value)
{
	struct subnet *subnet;
	isc_result_t status;

	if (h -> type != dhcp_type_subnet)
		return ISC_R_INVALIDARG;
	subnet = (struct subnet *)h;

	/* No values to get yet. */

	/* Try to find some inner object that can provide the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_subnet_destroy (omapi_object_t *h, const char *file, int line)
{
	struct subnet *subnet;
	isc_result_t status;

	if (h -> type != dhcp_type_subnet)
		return ISC_R_INVALIDARG;
	subnet = (struct subnet *)h;

	/* Can't destroy subnets yet. */

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subnet_signal_handler (omapi_object_t *h,
					 const char *name, va_list ap)
{
	struct subnet *subnet;
	isc_result_t status;
	int updatep = 0;

	if (h -> type != dhcp_type_subnet)
		return ISC_R_INVALIDARG;
	subnet = (struct subnet *)h;

	/* Can't write subnets yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	if (updatep)
		return ISC_R_SUCCESS;
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_subnet_stuff_values (omapi_object_t *c,
				       omapi_object_t *id,
				       omapi_object_t *h)
{
	struct subnet *subnet;
	isc_result_t status;

	if (h -> type != dhcp_type_subnet)
		return ISC_R_INVALIDARG;
	subnet = (struct subnet *)h;

	/* Can't stuff subnet values yet. */

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subnet_lookup (omapi_object_t **lp,
				 omapi_object_t *id,
				 omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct subnet *subnet;

	/* Can't look up subnets yet. */

	/* If we get to here without finding a subnet, no valid key was
	   specified. */
	if (!*lp)
		return ISC_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subnet_create (omapi_object_t **lp,
				 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_subnet_remove (omapi_object_t *lp,
			       omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_shared_network_set_value  (omapi_object_t *h,
					     omapi_object_t *id,
					     omapi_data_string_t *name,
					     omapi_typed_data_t *value)
{
	struct shared_network *shared_network;
	isc_result_t status;
	int foo;

	if (h -> type != dhcp_type_shared_network)
		return ISC_R_INVALIDARG;
	shared_network = (struct shared_network *)h;

	/* No values to set yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == ISC_R_UNCHANGED)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_shared_network_get_value (omapi_object_t *h,
					    omapi_object_t *id,
					    omapi_data_string_t *name,
					    omapi_value_t **value)
{
	struct shared_network *shared_network;
	isc_result_t status;

	if (h -> type != dhcp_type_shared_network)
		return ISC_R_INVALIDARG;
	shared_network = (struct shared_network *)h;

	/* No values to get yet. */

	/* Try to find some inner object that can provide the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_shared_network_destroy (omapi_object_t *h,
					  const char *file, int line)
{
	struct shared_network *shared_network;
	isc_result_t status;

	if (h -> type != dhcp_type_shared_network)
		return ISC_R_INVALIDARG;
	shared_network = (struct shared_network *)h;

	/* Can't destroy shared_networks yet. */

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_shared_network_signal_handler (omapi_object_t *h,
						 const char *name,
						 va_list ap)
{
	struct shared_network *shared_network;
	isc_result_t status;
	int updatep = 0;

	if (h -> type != dhcp_type_shared_network)
		return ISC_R_INVALIDARG;
	shared_network = (struct shared_network *)h;

	/* Can't write shared_networks yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	if (updatep)
		return ISC_R_SUCCESS;
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_shared_network_stuff_values (omapi_object_t *c,
					       omapi_object_t *id,
					       omapi_object_t *h)
{
	struct shared_network *shared_network;
	isc_result_t status;

	if (h -> type != dhcp_type_shared_network)
		return ISC_R_INVALIDARG;
	shared_network = (struct shared_network *)h;

	/* Can't stuff shared_network values yet. */

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_shared_network_lookup (omapi_object_t **lp,
					 omapi_object_t *id,
					 omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct shared_network *shared_network;

	/* Can't look up shared_networks yet. */

	/* If we get to here without finding a shared_network, no valid key was
	   specified. */
	if (!*lp)
		return ISC_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_shared_network_create (omapi_object_t **lp,
					 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_shared_network_remove (omapi_object_t *lp,
					 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_interface_set_value  (omapi_object_t *h,
					omapi_object_t *id,
					omapi_data_string_t *name,
					omapi_typed_data_t *value)
{
	struct interface_info *interface;
	isc_result_t status;
	int foo;

	if (h -> type != dhcp_type_interface)
		return ISC_R_INVALIDARG;
	interface = (struct interface_info *)h;

	if (!omapi_ds_strcmp (name, "name")) {
		if ((value -> type == omapi_datatype_data ||
		     value -> type == omapi_datatype_string) &&
		    value -> u.buffer.len < sizeof interface -> name) {
			memcpy (interface -> name,
				value -> u.buffer.value,
				value -> u.buffer.len);
			interface -> name [value -> u.buffer.len] = 0;
		} else
			return ISC_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == ISC_R_UNCHANGED)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_interface_get_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_value_t **value)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_interface_destroy (omapi_object_t *h,
					 const char *file, int line)
{
	struct interface_info *interface;
	isc_result_t status;

	if (h -> type != dhcp_type_interface)
		return ISC_R_INVALIDARG;
	interface = (struct interface_info *)h;

	if (interface -> ifp)
		dfree (interface -> ifp, file, line);
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_interface_signal_handler (omapi_object_t *h,
					    const char *name, va_list ap)
{
	struct interface_info *ip, *interface;
	struct client_config *config;
	struct client_state *client;
	isc_result_t status;

	if (h -> type != dhcp_type_interface)
		return ISC_R_INVALIDARG;

	/* This code needs some rethinking.   It doesn't test against
	   a signal name, and it just kind of bulls into doing something
	   that may or may not be appropriate. */
#if 0
	interface = (struct interface_info *)h;

	if (interfaces) {
		interface_reference (&interface -> next, interfaces, MDL);
		interface_dereference (&interfaces, MDL);
	}
	interface_reference (&interfaces, interface, MDL);

	discover_interfaces (DISCOVER_UNCONFIGURED);

	for (ip = interfaces; ip; ip = ip -> next) {
		/* If interfaces were specified, don't configure
		   interfaces that weren't specified! */
		if (ip -> flags & INTERFACE_RUNNING ||
		   (ip -> flags & (INTERFACE_REQUESTED |
				     INTERFACE_AUTOMATIC)) !=
		     INTERFACE_REQUESTED)
			continue;
		script_init (ip -> client,
			     "PREINIT", (struct string_list *)0);
		if (ip -> client -> alias)
			script_write_params (ip -> client, "alias_",
					     ip -> client -> alias);
		script_go (ip -> client);
	}
	
	discover_interfaces (interfaces_requested
			     ? DISCOVER_REQUESTED
			     : DISCOVER_RUNNING);

	for (ip = interfaces; ip; ip = ip -> next) {
		if (ip -> flags & INTERFACE_RUNNING)
			continue;
		ip -> flags |= INTERFACE_RUNNING;
		for (client = ip -> client; client; client = client -> next) {
			client -> state = S_INIT;
			/* Set up a timeout to start the initialization
			   process. */
			add_timeout (cur_time + random () % 5,
				     state_reboot, client, 0, 0);
		}
	}
	return ISC_R_SUCCESS;
#endif
	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_interface_stuff_values (omapi_object_t *c,
					  omapi_object_t *id,
					  omapi_object_t *h)
{
	struct interface_info *interface;
	isc_result_t status;

	if (h -> type != dhcp_type_interface)
		return ISC_R_INVALIDARG;
	interface = (struct interface_info *)h;

	/* Write out all the values. */

	status = omapi_connection_put_name (c, "state");
	if (status != ISC_R_SUCCESS)
		return status;
	if (interface -> flags && INTERFACE_REQUESTED)
	    status = omapi_connection_put_string (c, "up");
	else
	    status = omapi_connection_put_string (c, "down");
	if (status != ISC_R_SUCCESS)
		return status;

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_interface_lookup (omapi_object_t **ip,
				    omapi_object_t *id,
				    omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct interface_info *interface;

	/* First see if we were sent a handle. */
	status = omapi_get_value_str (ref, id, "handle", &tv);
	if (status == ISC_R_SUCCESS) {
		status = omapi_handle_td_lookup (ip, tv -> value);

		omapi_value_dereference (&tv, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Don't return the object if the type is wrong. */
		if ((*ip) -> type != dhcp_type_interface) {
			omapi_object_dereference (ip, MDL);
			return ISC_R_INVALIDARG;
		}
	}

	/* Now look for an interface name. */
	status = omapi_get_value_str (ref, id, "name", &tv);
	if (status == ISC_R_SUCCESS) {
		for (interface = interfaces; interface;
		     interface = interface -> next) {
		    if (strncmp (interface -> name,
				 (char *)tv -> value -> u.buffer.value,
				 tv -> value -> u.buffer.len) == 0)
			    break;
		}
		omapi_value_dereference (&tv, MDL);
		if (*ip && *ip != (omapi_object_t *)interface) {
			omapi_object_dereference (ip, MDL);
			return ISC_R_KEYCONFLICT;
		} else if (!interface) {
			if (*ip)
				omapi_object_dereference (ip, MDL);
			return ISC_R_NOTFOUND;
		} else if (!*ip)
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (ip,
						(omapi_object_t *)interface,
						MDL);
	}

	/* If we get to here without finding an interface, no valid key was
	   specified. */
	if (!*ip)
		return ISC_R_NOKEYS;
	return ISC_R_SUCCESS;
}

/* actually just go discover the interface */
isc_result_t dhcp_interface_create (omapi_object_t **lp,
				    omapi_object_t *id)
{
 	struct interface_info *hp;
	isc_result_t status;
	
	hp = (struct interface_info *)0;
	status = interface_allocate (&hp, MDL);
 	if (status != ISC_R_SUCCESS)
		return status;
 	hp -> flags = INTERFACE_REQUESTED;
	status = interface_reference ((struct interface_info **)lp, hp, MDL);
	interface_dereference (&hp, MDL);
	return status;
}

isc_result_t dhcp_interface_remove (omapi_object_t *lp,
				    omapi_object_t *id)
{
 	struct interface_info *interface, *ip, *last;

	interface = (struct interface_info *)lp;

	/* remove from interfaces */
	last = 0;
	for (ip = interfaces; ip; ip = ip -> next) {
		if (ip == interface) {
			if (last) {
				interface_dereference (&last -> next, MDL);
				if (ip -> next)
					interface_reference (&last -> next,
							     ip -> next, MDL);
			} else {
				interface_dereference (&interfaces, MDL);
				if (ip -> next)
					interface_reference (&interfaces,
							     ip -> next, MDL);
			}
			if (ip -> next)
				interface_dereference (&ip -> next, MDL);
			break;
		}
		last = ip;
	}
	if (!ip)
		return ISC_R_NOTFOUND;

	/* add the interface to the dummy_interface list */
	if (dummy_interfaces) {
		interface_reference (&interface -> next,
				     dummy_interfaces, MDL);
		interface_dereference (&dummy_interfaces, MDL);
	}
	interface_reference (&dummy_interfaces, interface, MDL);

	/* do a DHCPRELEASE */
	if (dhcp_interface_shutdown_hook)
		(*dhcp_interface_shutdown_hook) (interface);

	/* remove the io object */
	omapi_unregister_io_object ((omapi_object_t *)interface);

	if_deregister_send (interface);
	if_deregister_receive (interface);

	return ISC_R_SUCCESS;
}
