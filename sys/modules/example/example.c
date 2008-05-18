/*	$NetBSD: example.c,v 1.2.4.1 2008/05/18 12:35:26 yamt Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: example.c,v 1.2.4.1 2008/05/18 12:35:26 yamt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>

MODULE(MODULE_CLASS_MISC, example, NULL);

static
void
handle_props(prop_dictionary_t props)
{
	prop_string_t str;

	str = prop_dictionary_get(props, "msg");
	if (str == NULL)
		printf("The 'msg' property was not given.\n");
	else if (prop_object_type(str) != PROP_TYPE_STRING)
		printf("The 'msg' property is not a string.\n");
	else {
		const char *msg = prop_string_cstring_nocopy(str);
		if (msg == NULL)
			printf("Failed to process the 'msg' property.\n");
		else
			printf("The 'msg' property is: %s\n", msg);
	}
}

static int
example_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		printf("Example module loaded.\n");
		handle_props(arg);
		break;

	case MODULE_CMD_FINI:
		printf("Example module unloaded.\n");
		break;

	case MODULE_CMD_STAT:
		printf("Example module status queried.\n");
		return ENOTTY;

	default:
		return ENOTTY;
	}

	return 0;
}
