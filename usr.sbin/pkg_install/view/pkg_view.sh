#! /bin/sh

# $NetBSD: pkg_view.sh,v 1.1.2.30 2003/08/25 21:55:29 jlam Exp $

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
chmodprog=/bin/chmod
cmpprog=/usr/bin/cmp
cpprog=/bin/cp
envprog=/usr/bin/env
findprog=/usr/bin/find
grepprog=/usr/bin/grep
linkfarmprog=/usr/sbin/linkfarm
lnprog=/bin/ln
mkdirprog=/bin/mkdir
paxprog=/bin/pax
pkginfoprog=/usr/sbin/pkg_info
rmprog=/bin/rm
rmdirprog=/bin/rmdir
sedprog=/usr/bin/sed
touchprog=/usr/bin/touch

usage() {
	echo 'Usage: pkg_view [-n] [-v] [-i ignore] [-k pkg_dbdir_dflt] [-w viewname] [-d stowdir] [-W viewbase] add|check|delete pkgname...'
	exit 1
}

version() {
	$pkginfoprog -V
	exit 0
}

checkpkg() {
	if [ -d "$2/$1" ]; then
		:
	else
		echo "pkg_view: \`$1' doesn't exist in \`$2'" 1>&2
		exit 1
	fi
}

doit=""
stowdir=""
viewbase=${LOCALBASE:-/usr/pkg}
view=${PKG_VIEW:-""}
dflt_ignorefiles=${PLIST_IGNORE_FILES:-"info/dir *[~#] *.OLD *.orig *,v"}
dflt_pkg_dbdir=${PKG_DBDIR:-/var/db/pkg}
ignorefiles=""
verbose=0

while [ $# -gt 0 ]; do
	case "$1" in
	-d)		stowdir=$2; shift ;;
	-d*)		stowdir=`echo $1 | $sedprog -e 's|^-d||'` ;;
	-i)		ignorefiles="$ignorefiles $2"; shift ;;
	-i*)		ignorefiles="$ignorefiles `echo $1 | $sedprog -e 's|^-i||'`" ;;
	-k)		dflt_pkg_dbdir="$2"; shift ;;
	-k*)		dflt_pkg_dbdir=`echo $1 | $sedprog -e 's|^-k||'` ;;
	-n)		doit=":" ;;
	-V)		version ;;
	-v)		verbose=`expr $verbose + 1` ;;
	-W)		viewbase=$2; shift ;;
	-W*)		viewbase=`echo $1 | $sedprog -e 's|^-p||'` ;;
	-w)		view=$2; shift ;;
	--view=*)	view=`echo $1 | $sedprog -e 's|--view=||'` ;;
	--)		shift; break ;;
	*)		break ;;
	esac
	shift
done

if [ $# -lt 1 ]; then
	usage
fi

# echoN will actually give output if ${verbose} > N.
echo1=":"
echo2=":"
if [ ${verbose} -gt 0 ]; then echo1=echo; fi
if [ ${verbose} -gt 1 ]; then echo2=echo; fi

action=""
case "$1" in
add)		action=add ;;
check)		action=check ;;
delete|rm)	action=delete ;;
*)		usage ;;
esac
shift

# XXX Only support the standard view for now.
case "$view" in
"")	;;
*)	echo "pkg_view: only the standard view is supported" 1>&2
	exit 1
	;;
esac

# if standard view, put package info into ${dflt_pkg_dbdir}
# if not standard view, put package info into view's pkgdb
case "$view" in
"")
	pkg_dbdir=${dflt_pkg_dbdir}
	targetdir=${viewbase}
	viewstr="the standard view"
	;;
*)
	pkg_dbdir=${viewbase}/${view}/.pkgdb
	targetdir=${viewbase}/${view}
	viewstr="view \"${view}\""
	;;
esac

# Use stowdir if it's given, else fall back to ${DEPOTBASE} or else
# default to ${viewbase}/packages.
#
depot_pkg_dbdir=${stowdir:-${DEPOTBASE:-${viewbase}/packages}}

while [ $# -gt 0 ]; do
	case $action in
	add)
		checkpkg $1 ${depot_pkg_dbdir}
		if [ -f ${pkg_dbdir}/$1/+DEPOT ]; then
			echo "pkg_view: \`$1' already exists in $viewstr" 1>&2
			exit 1
		fi
		$echo1 "Adding $1 to ${targetdir}."
		if [ -f ${depot_pkg_dbdir}/$1/+BUILD_INFO ]; then
			ignore=`$grepprog "^_PLIST_IGNORE_FILES=" ${depot_pkg_dbdir}/$1/+BUILD_INFO | $sedprog -e 's|^_PLIST_IGNORE_FILES=[ 	]*||'`
		fi
		case "$ignore" in
		"")	ignore="${dflt_ignorefiles}" ;;
		esac
		dbs=`(cd ${depot_pkg_dbdir}/$1; echo +*)`
		ignore="${ignore} ${ignorefiles} $dbs"
		$doit $envprog PLIST_IGNORE_FILES="${ignore}" $linkfarmprog --target=${targetdir} --dir=${depot_pkg_dbdir} $1
		$doit $mkdirprog -p ${depot_pkg_dbdir}/$1
		temp=${depot_pkg_dbdir}/$1/+VIEWS.$$
		$doit $touchprog ${depot_pkg_dbdir}/$1/+VIEWS
		$doit $cpprog ${depot_pkg_dbdir}/$1/+VIEWS ${temp}
		case "$doit" in
		"")	($grepprog -v '^'${pkg_dbdir}'$' ${temp} || true; echo ${pkg_dbdir}) > ${depot_pkg_dbdir}/$1/+VIEWS ;;
		esac
		$doit $rmprog ${temp}
		$doit $mkdirprog -p ${pkg_dbdir}/$1
		#
		# Copy all of the metadata files except for +VIEWS,
		# which is only for the depoted package, and
		# +REQUIRED_BY, which is irrelevant for a package in
		# a view.
		#
		case "$doit" in
		"")	(cd ${depot_pkg_dbdir}/$1; $paxprog -rwpe '-s|\./\+VIEWS$||' '-s|\./\+REQUIRED_BY$||' ./+* ${pkg_dbdir}/$1)
			$sedprog -e 's|'${depot_pkg_dbdir}/$1'|'${targetdir}'|g' < ${depot_pkg_dbdir}/$1/+CONTENTS > ${pkg_dbdir}/$1/+CONTENTS
			echo "${depot_pkg_dbdir}/$1" > ${pkg_dbdir}/$1/+DEPOT
			;;
		esac
		if [ -f ${pkg_dbdir}/$1/+INSTALL ]; then
			$doit $chmodprog +x ${pkg_dbdir}/$1/+INSTALL
			$doit $envprog -i PKG_PREFIX=${targetdir} ${pkg_dbdir}/$1/+INSTALL $1 VIEW-INSTALL
			exit $?
		fi
		;;
	check)
		checkpkg $1 ${depot_pkg_dbdir}
		$echo1 "Checking $1 in ${targetdir}."
		$doit $linkfarmprog -c --target=${targetdir} --dir=${depot_pkg_dbdir} $1
		exit $?
		;;
	delete)
		checkpkg $1 ${depot_pkg_dbdir}
		if [ -f ${pkg_dbdir}/$1/+DEPOT ]; then
			:
		else
			echo "pkg_view: \`$1' doesn't exist in $viewstr" 1>&2
			exit 1
		fi
		$echo1 "Deleting $1 from ${targetdir}."
		if [ -f ${pkg_dbdir}/$1/+REQUIRED_BY ]; then
			if $cmpprog -s ${pkg_dbdir}/$1/+REQUIRED_BY /dev/null; then
				: # not really required by another pkg
			else
				(echo "pkg_view: \`$1' is required by other packages:"
				$sedprog -e 's|^|	|' ${pkg_dbdir}/$1/+REQUIRED_BY) 1>&2
				exit 1
			fi
		fi
		if [ -f ${pkg_dbdir}/$1/+DEINSTALL ]; then
			$doit $chmodprog +x ${pkg_dbdir}/$1/+DEINSTALL
			$doit envprog -i PKG_PREFIX=${targetdir} ${pkg_dbdir}/$1/+DEINSTALL $1 VIEW-DEINSTALL
			ec=$?
			if [ $ec != 0 ]; then
				echo "pkg_view: de-install script returned an error." 1>&2
				exit $ec
			fi
		fi
		if [ -f ${depot_pkg_dbdir}/$1/+BUILD_INFO ]; then
			ignore=`$grepprog "^_PLIST_IGNORE_FILES=" ${depot_pkg_dbdir}/$1/+BUILD_INFO | $sedprog -e 's|^_PLIST_IGNORE_FILES=[ 	]*||'`
		fi
		case "$ignore" in
		"")	ignore="${dflt_ignorefiles}" ;;
		esac
		dbs=`(cd ${depot_pkg_dbdir}/$1; echo +*)`
		ignore="${ignore} ${ignorefiles} $dbs"
		$doit env PLIST_IGNORE_FILES="${ignore}" $linkfarmprog -D --target=${targetdir} --dir=${depot_pkg_dbdir} $1
		temp=${depot_pkg_dbdir}/$1/+VIEWS.$$
		$doit $cpprog ${depot_pkg_dbdir}/$1/+VIEWS ${temp}
		case "$doit" in
		"")	($grepprog -v '^'${pkg_dbdir}'$' ${temp} || true) > ${depot_pkg_dbdir}/$1/+VIEWS ;;
		esac
		$doit $rmprog ${temp}
		$doit $rmprog -rf ${pkg_dbdir}/$1
		;;
	esac
	shift
done

exit 0
