#!/bin/bash
#
# This script configures, builds and packs the binary package for
# the Cygwin net distribution version of OpenSSL
#

# Uncomment when debugging
#set -x

CONFIG_OPTIONS="--prefix=/usr shared no-idea no-rc5 no-mdc2"
INSTALL_PREFIX=/tmp/install

VERSION=
SUBVERSION=$1

function cleanup()
{
  rm -rf ${INSTALL_PREFIX}/etc
  rm -rf ${INSTALL_PREFIX}/usr
}

function get_openssl_version()
{
  eval `grep '^VERSION=' Makefile.ssl`
  if [ -z "${VERSION}" ]
  then
    echo "Error: Couldn't retrieve OpenSSL version from Makefile.ssl."
    echo "       Check value of variable VERSION in Makefile.ssl."
    exit 1
  fi
}

function base_install()
{
  mkdir -p ${INSTALL_PREFIX}
  cleanup
  make install INSTALL_PREFIX="${INSTALL_PREFIX}"
}

function doc_install()
{
  DOC_DIR=${INSTALL_PREFIX}/usr/doc/openssl

  mkdir -p ${DOC_DIR}
  cp CHANGES CHANGES.SSLeay INSTALL LICENSE NEWS README ${DOC_DIR}

  create_cygwin_readme
}

function create_cygwin_readme()
{
  README_DIR=${INSTALL_PREFIX}/usr/doc/Cygwin
  README_FILE=${README_DIR}/openssl-${VERSION}.README

  mkdir -p ${README_DIR}
  cat > ${README_FILE} <<- EOF
	The Cygwin version has been built using the following configure:

	  ./config ${CONFIG_OPTIONS}

	The IDEA, RC5 and MDC2 algorithms are disabled due to patent and/or
	licensing issues.
	EOF
}

function create_profile_files()
{
  PROFILE_DIR=${INSTALL_PREFIX}/etc/profile.d

  mkdir -p $PROFILE_DIR
  cat > ${PROFILE_DIR}/openssl.sh <<- "EOF"
	export MANPATH="${MANPATH}:/usr/ssl/man"
	EOF
  cat > ${PROFILE_DIR}/openssl.csh <<- "EOF"
	if ( $?MANPATH ) then
	  setenv MANPATH "${MANPATH}:/usr/ssl/man"
	else
	  setenv MANPATH ":/usr/ssl/man"
	endif
	EOF
}

if [ -z "${SUBVERSION}" ]
then
  echo "Usage: $0 subversion"
  exit 1
fi

if [ ! -f config ]
then
  echo "You must start this script in the OpenSSL toplevel source dir."
  exit 1
fi

./config ${CONFIG_OPTIONS}

get_openssl_version

make || exit 1

base_install

doc_install

create_cygwin_readme

create_profile_files

cd ${INSTALL_PREFIX}
strip usr/bin/*.exe usr/bin/*.dll

# Runtime package
find etc usr/bin usr/doc usr/ssl/certs usr/ssl/man/man[157] usr/ssl/misc \
     usr/ssl/openssl.cnf usr/ssl/private -empty -o \! -type d |
tar cjfT openssl-${VERSION}-${SUBVERSION}.tar.bz2 -
# Development package
find usr/include usr/lib usr/ssl/man/man3 -empty -o \! -type d |
tar cjfT openssl-devel-${VERSION}-${SUBVERSION}.tar.bz2 -

ls -l openssl-${VERSION}-${SUBVERSION}.tar.bz2
ls -l openssl-devel-${VERSION}-${SUBVERSION}.tar.bz2

cleanup

exit 0
