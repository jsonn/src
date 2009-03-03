#! /bin/sh -
#	$NetBSD: makesyscalls.sh,v 1.73.2.2 2009/03/03 18:32:56 skrll Exp $
#
# Copyright (c) 1994, 1996, 2000 Christopher G. Demetriou
# All rights reserved.
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
#      This product includes software developed for the NetBSD Project
#      by Christopher G. Demetriou.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#	@(#)makesyscalls.sh	8.1 (Berkeley) 6/10/93

set -e

case $# in
    2)	;;
    *)	echo "Usage: $0 config-file input-file" 1>&2
	exit 1
	;;
esac

# the config file sets the following variables:
#	sysalign	check for alignment of off_t
#	sysnames	the syscall names file
#	sysnumhdr	the syscall numbers file
#	syssw		the syscall switch file
#	sysarghdr	the syscall argument struct definitions
#	compatopts	those syscall types that are for 'compat' syscalls
#	switchname	the name for the 'struct sysent' we define
#	namesname	the name for the 'const char *[]' we define
#	constprefix	the prefix for the system call constants
#	registertype	the type for register_t
#	nsysent		the size of the sysent table
#	sys_nosys	[optional] name of function called for unsupported
#			syscalls, if not sys_nosys()
#       maxsysargs	[optiona] the maximum number or arguments
#
# NOTE THAT THIS makesyscalls.sh DOES NOT SUPPORT 'SYSLIBCOMPAT'.

# source the config file.
sys_nosys="sys_nosys"	# default is sys_nosys(), if not specified otherwise
maxsysargs=8		# default limit is 8 (32bit) arguments
rumpcalls="/dev/null"
rumpcallshdr="/dev/null"
rumpsysent="rumpsysent.tmp"
. ./$1

# tmp files:
sysdcl="sysent.dcl"
sysprotos="sys.protos"
syscompat_pref="sysent."
sysent="sysent.switch"
sysnamesbottom="sysnames.bottom"

trap "rm $sysdcl $sysprotos $sysent $sysnamesbottom $rumpsysent" 0

# Awk program (must support nawk extensions)
# Use "awk" at Berkeley, "nawk" or "gawk" elsewhere.
awk=${AWK:-awk}

# Does this awk have a "toupper" function?
have_toupper=`$awk 'BEGIN { print toupper("true"); exit; }' 2>/dev/null`

# If this awk does not define "toupper" then define our own.
if [ "$have_toupper" = TRUE ] ; then
	# Used awk (GNU awk or nawk) provides it
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

# before handing it off to awk, make a few adjustments:
#	(1) insert spaces around {, }, (, ), *, and commas.
#	(2) get rid of any and all dollar signs (so that rcs id use safe)
#
# The awk script will deal with blank lines and lines that
# start with the comment character (';').

sed -e '
s/\$//g
:join
	/\\$/{a\

	N
	s/\\\n//
	b join
	}
2,${
	/^#/!s/\([{}()*,|]\)/ \1 /g
}
' < $2 | $awk "
$toupper
BEGIN {
	# Create a NetBSD tag that does not get expanded when checking
	# this script out of CVS.  (This part of the awk script is in a
	# shell double-quoted string, so the backslashes are eaten by
	# the shell.)
	tag = \"\$\" \"NetBSD\" \"\$\"

	# to allow nested #if/#else/#endif sets
	savedepth = 0
	# to track already processed syscalls

	sysnames = \"$sysnames\"
	sysprotos = \"$sysprotos\"
	sysnumhdr = \"$sysnumhdr\"
	sysarghdr = \"$sysarghdr\"
	rumpcalls = \"$rumpcalls\"
	rumpcallshdr = \"$rumpcallshdr\"
	rumpsysent = \"$rumpsysent\"
	switchname = \"$switchname\"
	namesname = \"$namesname\"
	constprefix = \"$constprefix\"
	registertype = \"$registertype\"
	sysalign=\"$sysalign\"
	if (!registertype) {
	    registertype = \"register_t\"
	}
	nsysent = \"$nsysent\"

	sysdcl = \"$sysdcl\"
	syscompat_pref = \"$syscompat_pref\"
	sysent = \"$sysent\"
	sysnamesbottom = \"$sysnamesbottom\"
	sys_nosys = \"$sys_nosys\"
	maxsysargs = \"$maxsysargs\"
	infile = \"$2\"

	compatopts = \"$compatopts\"
	"'

	printf "/* %s */\n\n", tag > sysdcl
	printf "/*\n * System call switch table.\n *\n" > sysdcl
	printf " * DO NOT EDIT-- this file is automatically generated.\n" > sysdcl

	ncompat = split(compatopts,compat)
	for (i = 1; i <= ncompat; i++) {
		compat_upper[i] = toupper(compat[i])

		printf "\n#ifdef %s\n", compat_upper[i] > sysent
		printf "#define	%s(func) __CONCAT(%s_,func)\n", compat[i], \
		    compat[i] > sysent
		printf "#else\n" > sysent
		printf "#define	%s(func) %s\n", compat[i], sys_nosys > sysent
		printf "#endif\n" > sysent
	}

	printf "\n#define\ts(type)\tsizeof(type)\n" > sysent
	printf "#define\tn(type)\t(sizeof(type)/sizeof (%s))\n", registertype > sysent
	printf "#define\tns(type)\tn(type), s(type)\n\n", registertype > sysent
	printf "struct sysent %s[] = {\n",switchname > sysent

	printf "/* %s */\n\n", tag > sysnames
	printf "/*\n * System call names.\n *\n" > sysnames
	printf " * DO NOT EDIT-- this file is automatically generated.\n" > sysnames

	printf "\n/*\n * System call prototypes.\n */\n\n" > sysprotos

	printf "/* %s */\n\n", tag > sysnumhdr
	printf "/*\n * System call numbers.\n *\n" > sysnumhdr
	printf " * DO NOT EDIT-- this file is automatically generated.\n" > sysnumhdr

	printf "/* %s */\n\n", tag > sysarghdr
	printf "/*\n * System call argument lists.\n *\n" > sysarghdr
	printf " * DO NOT EDIT-- this file is automatically generated.\n" > sysarghdr

	printf "/* %s */\n\n", tag > rumpcalls
	printf "/*\n * System call vector and marshalling for rump.\n *\n" > rumpcalls
	printf " * DO NOT EDIT-- this file is automatically generated.\n" > rumpcalls

	printf "/* %s */\n\n", tag > rumpcallshdr
	printf "/*\n * System call protos in rump namespace.\n *\n" > rumpcallshdr
	printf " * DO NOT EDIT-- this file is automatically generated.\n" > rumpcallshdr
}
NR == 1 {
	sub(/ $/, "")
	printf " * created from%s\n */\n\n", $0 > sysdcl
	printf "#include <sys/cdefs.h>\n__KERNEL_RCSID(0, \"%s\");\n\n", tag > sysdcl

	printf " * created from%s\n */\n\n", $0 > sysnames
	printf "#include <sys/cdefs.h>\n__KERNEL_RCSID(0, \"%s\");\n\n", tag > sysnames

	printf " * created from%s\n */\n\n", $0 > rumpcalls
	printf "#include <sys/cdefs.h>\n__KERNEL_RCSID(0, \"%s\");\n\n", tag > rumpcalls
	printf "#include <sys/types.h>\n" > rumpcalls
	printf "#include <sys/param.h>\n" > rumpcalls
	printf "#include <sys/proc.h>\n" > rumpcalls
	printf "#include <sys/syscall.h>\n" > rumpcalls
	printf "#include <sys/syscallargs.h>\n" > rumpcalls
	printf "#include <rump/rumpuser.h>\n" > rumpcalls
	printf "#include \"rump_private.h\"\n\n" > rumpcalls
	printf "#if\tBYTE_ORDER == BIG_ENDIAN\n" > rumpcalls
	printf "#define SPARG(p,k)\t((p)->k.be.datum)\n" > rumpcalls
	printf "#else /* LITTLE_ENDIAN, I hope dearly */\n" > rumpcalls
	printf "#define SPARG(p,k)\t((p)->k.le.datum)\n" > rumpcalls
	printf "#endif\n\n" > rumpcalls
	printf "int rump_enosys(void);\n" > rumpcalls
	printf "int\nrump_enosys()\n{\n\n\treturn ENOSYS;\n}\n" > rumpcalls

	printf "\n#define\ts(type)\tsizeof(type)\n" > rumpsysent
	printf "#define\tn(type)\t(sizeof(type)/sizeof (%s))\n", registertype > rumpsysent
	printf "#define\tns(type)\tn(type), s(type)\n\n", registertype > rumpsysent
	printf "struct sysent rump_sysent[] = {\n" > rumpsysent

	# System call names are included by userland (kdump(1)), so
	# hide the include files from it.
	printf "#if defined(_KERNEL_OPT)\n" > sysnames

	printf "#endif /* _KERNEL_OPT */\n\n" > sysnamesbottom
	printf "const char *const %s[] = {\n",namesname > sysnamesbottom

	printf " * created from%s\n */\n\n", $0 > sysnumhdr

	printf " * created from%s\n */\n\n", $0 > sysarghdr

	printf " * created from%s\n */\n\n", $0 > rumpcallshdr
	printf "#ifdef _RUMPKERNEL\n" > rumpcallshdr
	printf "#error Interface not supported inside rump kernel\n" > rumpcallshdr
	printf "#endif /* _RUMPKERNEL */\n\n" > rumpcallshdr

	printf "#ifndef _" constprefix "SYSCALL_H_\n" > sysnumhdr
	printf "#define	_" constprefix "SYSCALL_H_\n\n" > sysnumhdr
	printf "#ifndef _" constprefix "SYSCALLARGS_H_\n" > sysarghdr
	printf "#define	_" constprefix "SYSCALLARGS_H_\n\n" > sysarghdr
	# Write max number of system call arguments to both headers
	printf("#define\t%sMAXSYSARGS\t%d\n\n", constprefix, maxsysargs) \
		> sysnumhdr
	printf("#define\t%sMAXSYSARGS\t%d\n\n", constprefix, maxsysargs) \
		> sysarghdr
	printf "#undef\tsyscallarg\n" > sysarghdr
	printf "#define\tsyscallarg(x)\t\t\t\t\t\t\t\\\n" > sysarghdr
	printf "\tunion {\t\t\t\t\t\t\t\t\\\n" > sysarghdr
	printf "\t\t%s pad;\t\t\t\t\t\t\\\n", registertype > sysarghdr
	printf "\t\tstruct { x datum; } le;\t\t\t\t\t\\\n" > sysarghdr
	printf "\t\tstruct { /* LINTED zero array dimension */\t\t\\\n" \
		> sysarghdr
	printf "\t\t\tint8_t pad[  /* CONSTCOND */\t\t\t\\\n" > sysarghdr
	printf "\t\t\t\t(sizeof (%s) < sizeof (x))\t\\\n", \
		registertype > sysarghdr
	printf "\t\t\t\t? 0\t\t\t\t\t\\\n" > sysarghdr
	printf "\t\t\t\t: sizeof (%s) - sizeof (x)];\t\\\n", \
		registertype > sysarghdr
	printf "\t\t\tx datum;\t\t\t\t\t\\\n" > sysarghdr
	printf "\t\t} be;\t\t\t\t\t\t\t\\\n" > sysarghdr
	printf "\t}\n" > sysarghdr
	printf("\n#undef check_syscall_args\n") >sysarghdr
	printf("#define check_syscall_args(call) \\\n" \
		"\ttypedef char call##_check_args" \
		    "[sizeof (struct call##_args) \\\n" \
		"\t\t<= %sMAXSYSARGS * sizeof (%s) ? 1 : -1];\n", \
		constprefix, registertype) >sysarghdr
	next
}
NF == 0 || $1 ~ /^;/ {
	next
}
$0 ~ /^%%$/ {
	intable = 1
	next
}
$1 ~ /^#[ 	]*include/ {
	print > sysdcl
	print > sysnames
	next
}
$1 ~ /^#/ && !intable {
	print > sysdcl
	print > sysnames
	next
}
$1 ~ /^#/ && intable {
	if ($1 ~ /^#[ 	]*if/) {
		savedepth++
		savesyscall[savedepth] = syscall
	}
	if ($1 ~ /^#[ 	]*else/) {
		if (savedepth <= 0) {
			printf("%s: line %d: unbalanced #else\n", \
			    infile, NR)
			exit 1
		}
		syscall = savesyscall[savedepth]
	}
	if ($1 ~ /^#[       ]*endif/) {
		if (savedepth <= 0) {
			printf("%s: line %d: unbalanced #endif\n", \
			    infile, NR)
			exit 1
		}
		savedepth--
	}
	print > sysent
	print > sysarghdr
	print > sysnumhdr
	print > sysprotos
	print > sysnamesbottom

	# XXX: technically we do not want to have conditionals in rump,
	# but it is easier to just let the cpp handle them than try to
	# figure out what we want here in this script
	print > rumpsysent
	next
}
syscall != $1 {
	printf "%s: line %d: syscall number out of sync at %d\n", \
	   infile, NR, syscall
	printf "line is:\n"
	print
	exit 1
}
function parserr(was, wanted) {
	printf "%s: line %d: unexpected %s (expected <%s>)\n", \
	    infile, NR, was, wanted
	printf "line is:\n"
	print
	exit 1
}
function parseline() {
	f=3			# toss number and type
	if ($2 == "INDIR")
		sycall_flags="SYCALL_INDIRECT"
	else
		sycall_flags="0"
	if ($NF != "}") {
		funcalias=$NF
		end=NF-1
	} else {
		funcalias=""
		end=NF
	}
	if ($f == "INDIR") {		# allow for "NOARG INDIR"
		sycall_flags = "SYCALL_INDIRECT | " sycall_flags
		f++
	}
	if ($f == "MODULAR") {		# registered at runtime
		modular = 1
		f++
	} else {
		modular =  0;
	}
	if ($f == "RUMP") {
		rumpable = 1
		f++
	} else {
		rumpable = 0
	}
	if ($f ~ /^[a-z0-9_]*$/) {	# allow syscall alias
		funcalias=$f
		f++
	}
	if ($f != "{")
		parserr($f, "{")
	f++
	if ($end != "}")
		parserr($end, "}")
	end--
	if ($end != ";")
		parserr($end, ";")
	end--
	if ($end != ")")
		parserr($end, ")")
	end--

	returntype = oldf = "";
	do {
		if (returntype != "" && oldf != "*")
			returntype = returntype" ";
		returntype = returntype$f;
		oldf = $f;
		f++
	} while ($f != "|" && f < (end-1))
	if (f == (end - 1)) {
		parserr($f, "function argument definition (maybe \"|\"?)");
	}
	f++

	fprefix=$f
	f++
	if ($f != "|") {
		parserr($f, "function compat delimiter (maybe \"|\"?)");
	}
	f++

	fcompat=""
	if ($f != "|") {
		fcompat=$f
		f++
	}

	if ($f != "|") {
		parserr($f, "function name delimiter (maybe \"|\"?)");
	}
	f++
	fbase=$f

	funcstdname=fprefix "_" fbase
	if (fcompat != "") {
		funcname=fprefix "___" fbase "" fcompat
		wantrename=1
	} else {
		funcname=funcstdname
		wantrename=0
	}

	if (funcalias == "") {
		funcalias=funcname
		sub(/^([^_]+_)*sys_/, "", funcalias)
	}
	f++

	if ($f != "(")
		parserr($f, "(")
	f++

	argc=0;
	argalign=0;
	if (f == end) {
		if ($f != "void")
			parserr($f, "argument definition")
		isvarargs = 0;
		varargc = 0;
		argtype[0]="void";
		return
	}

	# some system calls (open() and fcntl()) can accept a variable
	# number of arguments.  If syscalls accept a variable number of
	# arguments, they must still have arguments specified for
	# the remaining argument "positions," because of the way the
	# kernel system call argument handling works.
	#
	# Indirect system calls, e.g. syscall(), are exceptions to this
	# rule, since they are handled entirely by machine-dependent code
	# and do not need argument structures built.

	isvarargs = 0;
	while (f <= end) {
		if ($f == "...") {
			f++;
			isvarargs = 1;
			varargc = argc;
			continue;
		}
		argc++
		argtype[argc]=""
		oldf=""
		while (f < end && $(f+1) != ",") {
			if (argtype[argc] != "" && oldf != "*")
				argtype[argc] = argtype[argc]" ";
			argtype[argc] = argtype[argc]$f;
			oldf = $f;
			f++
		}
		if (argtype[argc] == "")
			parserr($f, "argument definition")
		if (argtype[argc] == "off_t") {
			if ((argalign % 2) != 0 && sysalign &&
			    funcname != "sys_posix_fadvise") # XXX for now
				parserr($f, "a padding argument")
		} else {
			argalign++;
		}
		argname[argc]=$f;
		f += 2;			# skip name, and any comma
	}
	# must see another argument after varargs notice.
	if (isvarargs) {
		if (argc == varargc)
			parserr($f, "argument definition")
	} else
		varargc = argc;
}

function printproto(wrap) {
	printf("/* syscall: \"%s%s\" ret: \"%s\" args:", wrap, funcalias,
	    returntype) > sysnumhdr
	for (i = 1; i <= varargc; i++)
		printf(" \"%s\"", argtype[i]) > sysnumhdr
	if (isvarargs)
		printf(" \"...\"") > sysnumhdr
	printf(" */\n") > sysnumhdr
	printf("#define\t%s%s%s\t%d\n\n", constprefix, wrap, funcalias,
	    syscall) > sysnumhdr

	# rumpalooza
	if (!rumpable)
		return

	printf("%s rump_%s(", returntype, funcstdname) > rumpcallshdr
	for (i = 1; i < argc; i++)
		printf("%s, ", argtype[i]) > rumpcallshdr
	printf("%s)", argtype[argc]) > rumpcallshdr
	if (wantrename)
		printf(" __RENAME(rump_%s)", funcname) > rumpcallshdr
	printf(";\n") > rumpcallshdr
}

function putent(type, compatwrap) {
	# output syscall declaration for switch table.
	if (compatwrap == "")
		compatwrap_ = ""
	else
		compatwrap_ = compatwrap "_"
	if (argc == 0)
		arg_type = "void";
	else {
		arg_type = "struct " compatwrap_ funcname "_args";
	}
	proto = "int\t" compatwrap_ funcname "(struct lwp *, const " \
	    arg_type " *, register_t *);\n"
	if (sysmap[proto] != 1) {
		sysmap[proto] = 1;
		print proto > sysprotos;
	}

	# output syscall switch entry
	printf("\t{ ") > sysent
	if (argc == 0) {
		printf("0, 0, ") > sysent
	} else {
		printf("ns(struct %s%s_args), ", compatwrap_, funcname) > sysent
	}
	if (modular) 
		wfn = "(sy_call_t *)sys_nomodule";
	else if (compatwrap == "")
		wfn = "(sy_call_t *)" funcname;
	else
		wfn = "(sy_call_t *)" compatwrap "(" funcname ")";
	printf("%s,\n\t    %s },", sycall_flags, wfn) > sysent
	for (i = 0; i < (33 - length(wfn)) / 8; i++)
		printf("\t") > sysent
	printf("/* %d = %s%s */\n", syscall, compatwrap_, funcalias) > sysent

	# output syscall name for names table
	printf("\t/* %3d */\t\"%s%s\",\n", syscall, compatwrap_, funcalias) \
	    > sysnamesbottom

	# output syscall number of header, if appropriate
	if (type == "STD" || type == "NOARGS" || type == "INDIR") {
		# output a prototype, to be used to generate lint stubs in
		# libc.
		printproto("")
	} else if (type == "COMPAT") {
		# Just define the syscall number with a comment.  These
		# may be used by compatibility stubs in libc.
		printproto(compatwrap_)
	}

	# output syscall argument structure, if it has arguments
	if (argc != 0) {
		printf("\nstruct %s%s_args", compatwrap_, funcname) > sysarghdr
		if (type != "NOARGS") {
			print " {" >sysarghdr;
			for (i = 1; i <= argc; i++)
				printf("\tsyscallarg(%s) %s;\n", argtype[i],
				    argname[i]) > sysarghdr
			printf "}" >sysarghdr;
		}
		printf(";\n") > sysarghdr
		if (type != "NOARGS" && type != "INDIR") {
			printf("check_syscall_args(%s%s)\n", compatwrap_,
			    funcname) >sysarghdr
		}
	}

	# output rump marshalling code if necessary
	if (!rumpable) {
		printf("\t{ 0, 0, 0,\n\t    %s },\t\t\t/* %d = unrumped */\n", \
		    "(sy_call_t *)rump_enosys", syscall) > rumpsysent
		return
	}

	# need a local prototype, we export the re-re-named one in .h
	printf("\n%s rump_%s(", returntype, funcname) > rumpcalls
	for (i = 1; i < argc; i++) {
		printf("%s, ", argtype[i]) > rumpcalls
	}
	printf("%s);", argtype[argc]) > rumpcalls

	printf("\n%s\nrump_%s(", returntype, funcname) > rumpcalls
	for (i = 1; i < argc; i++) {
		printf("%s %s, ", argtype[i], argname[i]) > rumpcalls
	}
	printf("%s %s)\n", argtype[argc], argname[argc]) > rumpcalls
	printf("{\n\tregister_t retval = 0;\n\tint error = 0;\n") > rumpcalls

	argarg = "NULL"
	argsize = 0;
	if (argc) {
		argarg = "&arg"
		argsize = "sizeof(arg)"
		printf("\tstruct %s%s_args arg;\n\n", compatwrap_, funcname) \
		    > rumpcalls
		for (i = 1; i <= argc; i++) {
			printf("\tSPARG(&arg, %s) = %s;\n", \
			    argname[i], argname[i]) > rumpcalls
		}
		printf("\n") > rumpcalls
	} else {
		printf("\n") > rumpcalls
	}
	printf("\terror = rump_sysproxy(%s%s, rump_sysproxy_arg,\n\t" \
	    "    (uint8_t *)%s, %s, &retval);\n", constprefix, funcalias, \
	    argarg, argsize) > rumpcalls
	printf("\tif (error) {\n\t\tretval = -1;\n") > rumpcalls
	if (returntype != "void") {
		printf("\t\trumpuser_seterrno(error);\n\t}\n") > rumpcalls
		printf("\treturn retval;\n") > rumpcalls
	} else {
		printf("\t}\n") > rumpcalls
	}
	printf("}\n") > rumpcalls
	printf("__weak_alias(%s,rump_enosys);\n", funcname) > rumpcalls

	# rumpsysent
	printf("\t{ ") > rumpsysent
	if (argc == 0) {
		printf("0, 0, ") > rumpsysent
	} else {
		printf("ns(struct %s%s_args), ", compatwrap_, funcname) > rumpsysent
	}
	printf("0,\n\t    %s },", wfn) > rumpsysent
	for (i = 0; i < (41 - length(wfn)) / 8; i++)
		printf("\t") > rumpsysent
	printf("/* %d = %s%s */\n", syscall, compatwrap_, funcalias) > rumpsysent
}
$2 == "STD" || $2 == "NODEF" || $2 == "NOARGS" || $2 == "INDIR" {
	parseline()
	putent($2, "")
	syscall++
	next
}
$2 == "OBSOL" || $2 == "UNIMPL" || $2 == "EXCL" || $2 == "IGNORED" {
	if ($2 == "OBSOL")
		comment="obsolete"
	else if ($2 == "EXCL")
		comment="excluded"
	else if ($2 == "IGNORED")
		comment="ignored"
	else
		comment="unimplemented"
	for (i = 3; i <= NF; i++)
		comment=comment " " $i

	if ($2 == "IGNORED")
		sys_stub = "(sy_call_t *)nullop";
	else
		sys_stub = sys_nosys;

	printf("\t{ 0, 0, 0,\n\t    %s },\t\t\t/* %d = %s */\n", \
	    sys_stub, syscall, comment) > sysent
	printf("\t{ 0, 0, 0,\n\t    %s },\t\t\t/* %d = %s */\n", \
	    "(sy_call_t *)rump_enosys", syscall, comment) > rumpsysent
	printf("\t/* %3d */\t\"#%d (%s)\",\n", syscall, syscall, comment) \
	    > sysnamesbottom
	if ($2 != "UNIMPL")
		printf("\t\t\t\t/* %d is %s */\n", syscall, comment) > sysnumhdr
	syscall++
	next
}
{
	for (i = 1; i <= ncompat; i++) {
		if ($2 == compat_upper[i]) {
			parseline();
			putent("COMPAT", compat[i])
			syscall++
			next
		}
	}
	printf("%s: line %d: unrecognized keyword %s\n", infile, NR, $2)
	exit 1
}
END {
	maxsyscall = syscall
	if (nsysent) {
		if (syscall > nsysent) {
			printf("%s: line %d: too many syscalls [%d > %d]\n", infile, NR, syscall, nsysent)
			exit 1
		}
		while (syscall < nsysent) {
			printf("\t{ 0, 0, 0,\n\t    %s },\t\t\t/* %d = filler */\n", \
			    sys_nosys, syscall) > sysent
			printf("\t{ 0, 0, 0,\n\t    %s },\t\t\t/* %d = filler */\n", \
			    "(sy_call_t *)rump_enosys", syscall) > rumpsysent
			syscall++
		}
	}
	printf("};\n") > sysent
	printf("};\n") > rumpsysent
	printf("CTASSERT(__arraycount(rump_sysent) == SYS_NSYSENT);\n") > rumpsysent
	printf("};\n") > sysnamesbottom
	printf("#define\t%sMAXSYSCALL\t%d\n", constprefix, maxsyscall) > sysnumhdr
	if (nsysent)
		printf("#define\t%sNSYSENT\t%d\n", constprefix, nsysent) > sysnumhdr
} '

cat $sysprotos >> $sysarghdr
echo "#endif /* _${constprefix}SYSCALL_H_ */" >> $sysnumhdr
echo "#endif /* _${constprefix}SYSCALLARGS_H_ */" >> $sysarghdr
cat $sysdcl $sysent > $syssw
cat $sysnamesbottom >> $sysnames
cat $rumpsysent >> $rumpcalls

#chmod 444 $sysnames $sysnumhdr $syssw
