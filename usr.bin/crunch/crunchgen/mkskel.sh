#!/bin/sh

#	$NetBSD: mkskel.sh,v 1.2.56.1 2009/05/13 19:19:47 jym Exp $

# idea and sed lines taken straight from flex

cat <<!EOF
/* File created via mkskel.sh */

const char *crunched_skel[] = {
!EOF

sed 's/\\/&&/g' $* | sed 's/"/\\"/g' | sed 's/.*/  "&",/'

cat <<!EOF
  0
};
!EOF
