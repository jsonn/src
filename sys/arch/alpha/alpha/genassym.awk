# $NetBSD: genassym.awk,v 1.1.2.3 1997/08/12 05:54:41 cgd Exp $

#
# Copyright (c) 1997 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Gordon W. Ross
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# This parses the assembly file genassym.s and translates each
# element of the assyms[] array into a cpp define.  Credit for
# this idea goes to Matthias Pfaller. 

BEGIN {
	len = 0;
	val = 0;
	str = "";

	translate = 0;
}

# This marks the beginning of the part we should translate.
# Note: leading _ is not allowed on all platforms (e.g. alpha).
/assyms:/ {
	translate = 1;
}
/\t\.ascii/ {
	if (!translate)
		next;
	len = length($2);
	str = substr($2,2,len-4);
	printf("#define\t%s\t", str);
	next;
}

/\t\.zero/ {
	next;
}

/\t\.quad/ {
	if (!translate)
		next;
	printf("%s\n", $2);
	next;
}

# This marks the end of the part we should translate.
# Note: leading _ is not allowed on all platforms (e.g. alpha).
/nassyms:/ {
	exit;
}
