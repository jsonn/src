#!/bin/sh -
copyright="\
/*
 * Copyright (c) 1992, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS \`\`AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
"
SCRIPT_ID='$NetBSD: vnode_if.sh,v 1.32.2.3 2004/09/18 14:53:04 skrll Exp $'

# Script to produce VFS front-end sugar.
#
# usage: vnode_if.sh srcfile
#	(where srcfile is currently /sys/kern/vnode_if.src)
#

if [ $# -ne 1 ] ; then
	echo 'usage: vnode_if.sh srcfile'
	exit 1
fi

# Name and revision of the source file.
src=$1
SRC_ID=`head -1 $src | sed -e 's/.*\$\(.*\)\$.*/\1/'`

# Names of the created files.
out_c=vnode_if.c
out_h=../sys/vnode_if.h

# Awk program (must support nawk extensions)
# Use "awk" at Berkeley, "nawk" or "gawk" elsewhere.
awk=${AWK:-awk}

# Does this awk have a "toupper" function? (i.e. is it GNU awk)
isgawk=`$awk 'BEGIN { print toupper("true"); exit; }' 2>/dev/null`

# If this awk does not define "toupper" then define our own.
if [ "$isgawk" = TRUE ] ; then
	# GNU awk provides it.
	toupper=
else
	# Provide our own toupper()
	toupper='
function toupper(str) {
	_toupper_cmd = "echo "str" |tr a-z A-Z"
	_toupper_cmd | getline _toupper_str;
	close(_toupper_cmd);
	return _toupper_str;
}'
fi

#
# This is the common part of all awk programs that read $src
# This parses the input for one function into the arrays:
#	argdir, argtype, argname, willrele
# and calls "doit()" to generate output for the function.
#
# Input to this parser is pre-processed slightly by sed
# so this awk parser doesn't have to work so hard.  The
# changes done by the sed pre-processing step are:
#	insert a space beween * and pointer name
#	replace semicolons with spaces
#
sed_prep='s:\*\([^\*/]\):\* \1:g
s/;/ /'
awk_parser='
# Comment line
/^#/	{ next; }
# First line of description
/^vop_/	{
	name=$1;
	argc=0;
	next;
}
# Last line of description
/^}/	{
	doit();
	next;
}
# Middle lines of description
{
	argdir[argc] = $1; i=2;
	if ($2 == "WILLRELE") {
		willrele[argc] = 1;
		i++;
	} else if ($2 == "WILLUNLOCK") {
		willrele[argc] = 2;
		i++;
	} else if ($2 == "WILLPUT") {
		willrele[argc] = 3;
		i++;
	} else
		willrele[argc] = 0;
	argtype[argc] = $i; i++;
	while (i < NF) {
		argtype[argc] = argtype[argc]" "$i;
		i++;
	}
	argname[argc] = $i;
	argc++;
	next;
}
'

# This is put before the copyright on each generated file.
warning="\
/*	@NetBSD@	*/

/*
 * Warning: This file is generated automatically.
 * (Modifications made here may easily be lost!)
 *
 * Created from the file:
 *	${SRC_ID}
 * by the script:
 *	${SCRIPT_ID}
 */
" 

# This is to satisfy McKusick (get rid of evil spaces 8^)
anal_retentive='s:\([^/]\*\) :\1:g'

#
# Redirect stdout to the H file.
#
echo "$0: Creating $out_h" 1>&2
exec > $out_h

# Begin stuff
echo -n "$warning" | sed -e 's/\$//g;s/@/\$/g'
echo ""
echo -n "$copyright"
echo ''
echo '#ifndef _SYS_VNODE_IF_H_'
echo '#define _SYS_VNODE_IF_H_'
echo ''
echo '#ifdef _KERNEL'
echo '#if defined(_LKM) || defined(LKM)'
echo '/* LKMs always use non-inlined vnode ops. */'
echo '#define	VNODE_OP_NOINLINE'
echo '#else'
echo '#include "opt_vnode_op_noinline.h"'
echo '#endif'
echo '#endif /* _KERNEL */'
echo '
extern const struct vnodeop_desc vop_default_desc;
'

# Body stuff
# This awk program needs toupper() so define it if necessary.
sed -e "$sed_prep" $src | $awk "$toupper"'
function doit() {
	# Declare arg struct, descriptor.
	printf("\nstruct %s_args {\n", name);
	printf("\tconst struct vnodeop_desc * a_desc;\n");
	for (i=0; i<argc; i++) {
		printf("\t%s a_%s;\n", argtype[i], argname[i]);
	}
	printf("};\n");
	printf("extern const struct vnodeop_desc %s_desc;\n", name);
	# Prototype it.
	printf("#ifndef VNODE_OP_NOINLINE\n");
	printf("static __inline\n");
	printf("#endif\n");
	protoarg = sprintf("int %s(", toupper(name));
	protolen = length(protoarg);
	printf("%s", protoarg);
	for (i=0; i<argc; i++) {
		protoarg = sprintf("%s", argtype[i]);
		if (i < (argc-1)) protoarg = (protoarg ", ");
		arglen = length(protoarg);
		if ((protolen + arglen) > 77) {
			protoarg = ("\n    " protoarg);
			arglen += 4;
			protolen = 0;
		}
		printf("%s", protoarg);
		protolen += arglen;
	}
	printf(")\n");
	printf("#ifndef VNODE_OP_NOINLINE\n");
	printf("__attribute__((__unused__))\n");
	printf("#endif\n");
	printf(";\n");
	# Define inline function.
	printf("#ifndef VNODE_OP_NOINLINE\n");
	printf("static __inline int %s(", toupper(name));
	for (i=0; i<argc; i++) {
		printf("%s", argname[i]);
		if (i < (argc-1)) printf(", ");
	}
	printf(")\n");
	for (i=0; i<argc; i++) {
		printf("\t%s %s;\n", argtype[i], argname[i]);
	}
	printf("{\n\tstruct %s_args a;\n", name);
	printf("\ta.a_desc = VDESC(%s);\n", name);
	for (i=0; i<argc; i++) {
		printf("\ta.a_%s = %s;\n", argname[i], argname[i]);
	}
	printf("\treturn (VCALL(%s%s, VOFFSET(%s), &a));\n}\n",
		argname[0], arg0special, name);
	printf("#endif\n");
	vops++;
}
BEGIN	{
	arg0special="";
	vops = 1; # start at 1, to count the 'default' op
}
END	{
	printf("\n/* Special cases: */\n#include <sys/buf.h>\n");
	argc=1;
	argtype[0]="struct buf *";
	argname[0]="bp";
	arg0special="->b_vp";
	name="vop_bwrite";
	doit();

	printf("\n#define VNODE_OPS_COUNT\t%d\n", vops);
}
'"$awk_parser" | sed -e "$anal_retentive"

# End stuff
echo '
/* End of special cases. */'
echo ''
echo '#endif /* !_SYS_VNODE_IF_H_ */'

#
# Redirect stdout to the C file.
#
echo "$0: Creating $out_c" 1>&2
exec > $out_c

# Begin stuff
echo -n "$warning" | sed -e 's/\$//g;s/@/\$/g'
echo ""
echo -n "$copyright"
echo "
#include <sys/cdefs.h>
__KERNEL_RCSID(0, \"\$NetBSD\$\");
"

echo '
/*
 * If we have LKM support, always include the non-inline versions for
 * LKMs.  Otherwise, do it based on the option.
 */
#ifdef LKM
#define	VNODE_OP_NOINLINE
#else
#include "opt_vnode_op_noinline.h"
#endif'
echo '
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/vnode.h>

const struct vnodeop_desc vop_default_desc = {
	0,
	"default",
	0,
	NULL,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
'

# Body stuff
sed -e "$sed_prep" $src | $awk '
function do_offset(typematch) {
	for (i=0; i<argc; i++) {
		if (argtype[i] == typematch) {
			printf("\tVOPARG_OFFSETOF(struct %s_args, a_%s),\n",
				name, argname[i]);
			return i;
		};
	};
	print "\tVDESC_NO_OFFSET,";
	return -1;
}

function doit() {
	# Define offsets array
	printf("\nconst int %s_vp_offsets[] = {\n", name);
	for (i=0; i<argc; i++) {
		if (argtype[i] == "struct vnode *") {
			printf ("\tVOPARG_OFFSETOF(struct %s_args,a_%s),\n",
				name, argname[i]);
		}
	}
	print "\tVDESC_NO_OFFSET";
	print "};";
	# Define F_desc
	printf("const struct vnodeop_desc %s_desc = {\n", name);
	# offset
	printf ("\t%d,\n", vop_offset++);
	# printable name
	printf ("\t\"%s\",\n", name);
	# flags
	printf("\t0");
	vpnum = 0;
	for (i=0; i<argc; i++) {
		if (willrele[i]) {
			if (willrele[i] == 2) {
				word = "UNLOCK";
			} else if (willrele[i] == 3) {
				word = "PUT";
			} else {
				word = "RELE";
			}
			if (argdir[i] ~ /OUT/) {
				printf(" | VDESC_VPP_WILL%s", word);
			} else {
				printf(" | VDESC_VP%s_WILL%s", vpnum, word);
			};
			vpnum++;
		}
	}
	print ",";
	# vp offsets
	printf ("\t%s_vp_offsets,\n", name);
	# vpp (if any)
	do_offset("struct vnode **");
	# cred (if any)
	do_offset("struct ucred *");
	# proc (if any)
	do_offset("struct proc *");
	# componentname
	do_offset("struct componentname *");
	# transport layer information
	printf ("\tNULL,\n};\n");

	# Define function.
	printf("#ifdef VNODE_OP_NOINLINE\n");
	printf("int\n%s(", toupper(name));
	for (i=0; i<argc; i++) {
		printf("%s", argname[i]);
		if (i < (argc-1)) printf(", ");
	}
	printf(")\n");
	for (i=0; i<argc; i++) {
		printf("\t%s %s;\n", argtype[i], argname[i]);
	}
	printf("{\n\tstruct %s_args a;\n", name);
	printf("\ta.a_desc = VDESC(%s);\n", name);
	for (i=0; i<argc; i++) {
		printf("\ta.a_%s = %s;\n", argname[i], argname[i]);
	}
	printf("\treturn (VCALL(%s%s, VOFFSET(%s), &a));\n}\n",
		argname[0], arg0special, name);
	printf("#endif\n");
}
BEGIN	{
	printf("\n/* Special cases: */\n");
	# start from 1 (vop_default is at 0)
	vop_offset=1;
	argc=1;
	argdir[0]="IN";
	argtype[0]="struct buf *";
	argname[0]="bp";
	arg0special="->b_vp";
	willrele[0]=0;
	name="vop_bwrite";
	doit();
	printf("\n/* End of special cases */\n");

	arg0special="";
}
'"$awk_parser" | sed -e "$anal_retentive"

# End stuff
echo '
/* End of special cases. */'

# Add the vfs_op_descs array to the C file.
# Begin stuff
echo '
const struct vnodeop_desc * const vfs_op_descs[] = {
	&vop_default_desc,	/* MUST BE FIRST */
	&vop_bwrite_desc,	/* XXX: SPECIAL CASE */
'

# Body stuff
sed -e "$sed_prep" $src | $awk '
function doit() {
	printf("\t&%s_desc,\n", name);
}
'"$awk_parser"

# End stuff
echo '	NULL
};
'

exit 0

# Local Variables:
# tab-width: 4
# End:
