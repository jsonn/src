#! /bin/sh

# $NetBSD: pkg_view.sh,v 1.1.2.4 2003/07/14 11:58:13 jlam Exp $

#
# Copyright (c) 2001 Alistair G. Crooks.  All rights reserved.
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
#	This product includes software developed by Alistair G. Crooks.
# 4. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior written
#    permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# set -x

# set up program definitions
cpprog=/bin/cp
findprog=/usr/bin/find
grepprog=/usr/bin/grep
lnprog=/bin/ln
mkdirprog=/bin/mkdir
paxprog=/bin/pax
rmprog=/bin/rm
rmdirprog=/bin/rmdir
sedprog=/usr/bin/sed
touchprog=/usr/bin/touch

usage() {
	echo 'Usage: pkg_view [-i ignore] [-v viewname] [-p prefix] add|check|delete pkgname...'
	exit 1
}

prefix=${PREFIX:-/usr/pkg}
view=${PKG_VIEW:-""}
ignorefiles=${PLIST_IGNORE_FILES:-info/dir}
dflt_pkg_dbdir=${PKG_DBDIR:-/var/db/pkg}

while [ $# -gt 1 ]; do
	case "$1" in
	-i)		ignorefiles="$ignorefiles $2"; shift ;;
	-i*)		ignorefiles="$ignorefiles `echo $1 | $sedprog -e 's|^-i||'`" ;;
	-p)		prefix=$2; shift ;;
	-p*)		prefix=`echo $1 | $sedprog -e 's|^-p||'` ;;
	-v)		view=$2; shift ;;
	--view=*)	view=`echo $1 | $sedprog -e 's|--view=||'` ;;
	--)		shift; break ;;
	*)		break ;;
	esac
	shift
done

if [ $# -lt 1 ]; then
	usage
fi

action=""
case "$1" in
add)		action=add ;;
check)		action=check ;;
delete|rm)	action=delete ;;
*)		usage ;;
esac
shift

depot_pkg_dbdir=${prefix}/packages

# XXX Only support the standard view.
view=""

# if standard view, put package info into ${dflt_pkg_dbdir}
# if not standard view, put package info into view's pkgdb
case "$view" in
"")	pkg_dbdir=${dflt_pkg_dbdir} ;;
*)	pkg_dbdir=${prefix}/${view}/.pkgdb ;;
esac

while [ $# -gt 0 ]; do
	case $action in
	add)
		if [ -f ${pkg_dbdir}/$1/+CONTENTS ]; then
			echo "Package $1 already exists in view \"${view}\""
		else
			dbs=`(cd ${depot_pkg_dbdir}/$1; echo +*)`
			env PLIST_IGNORE_FILES="${PLIST_IGNORE_FILES} $dbs" linkfarm --target=${prefix}/${view} --dir=${depot_pkg_dbdir} $1
			$mkdirprog -p ${depot_pkg_dbdir}/$1
			temp=${depot_pkg_dbdir}/$1/+VIEWS.$$
			$touchprog ${depot_pkg_dbdir}/$1/+VIEWS
			$cpprog ${depot_pkg_dbdir}/$1/+VIEWS ${temp}
			($grepprog -v "'"'^'${pkg_dbdir}'$'"'" ${temp} || true; echo ${pkg_dbdir}) > ${depot_pkg_dbdir}/$1/+VIEWS
			$rmprog ${temp}
			$mkdirprog -p ${pkg_dbdir}/$1
			(cd ${depot_pkg_dbdir}/$1; $paxprog -rwpe '-s|\./\+VIEWS$||' ./+* ${pkg_dbdir}/$1)
			$sedprog -e 's|'${depot_pkg_dbdir}/$1'|'${prefix}/${view}'|g' < ${depot_pkg_dbdir}/$1/+CONTENTS > ${pkg_dbdir}/$1/+CONTENTS
		fi
		;;
	check)
		linkfarm -c --target=${prefix}/${view} --dir=${depot_pkg_dbdir} $1
		exit $?
		;;
	delete)
		if [ ! -f ${pkg_dbdir}/$1/+CONTENTS ]; then
			echo "Package $1 does not exist in view \"${view}\""
		else
			linkfarm -D --target=${prefix}/${view} --dir=${depot_pkg_dbdir} $1
			temp=${depot_pkg_dbdir}/$1/+VIEWS.$$
			$cpprog ${depot_pkg_dbdir}/$1/+VIEWS ${temp}
			($grepprog -v "'"'^'${pkg_dbdir}'$'"'" ${temp} || true) > ${depot_pkg_dbdir}/$1/+VIEWS
			$rmprog ${temp}
			$rmprog -rf ${pkg_dbdir}/$1
		fi
		;;
	esac
	shift
done

exit 0
