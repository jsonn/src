#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.1.4.2 2008/10/19 22:39:50 haad Exp $
#
# Extract the new tarball and rename the libarchive-X.Y.Z directory
# to dist.  Run this script and check for additional files and
# directories to prune, only relevant content is included.

set -e

mkdir tmp
cd tmp
../dist/configure
mv config.h ../include/config_netbsd.h
cd ..
rm -rf tmp

cd dist

rm -rf config.aux contrib doc examples windows
rm INSTALL Makefile.am Makefile.in README aclocal.m4 config.h.in
rm configure configure.ac version
rm tar/getdate.c
