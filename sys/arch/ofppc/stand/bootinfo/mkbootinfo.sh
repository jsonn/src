#!/bin/sh
# $NetBSD: mkbootinfo.sh,v 1.2.12.2 2008/03/23 02:04:15 matt Exp $

OS=$1
BPART=$2
OUT=$3
BITMAP=$4

if [ -z "$OS" ]; then
	OS="NetBSD/ofppc"
fi

if [ -z "$BPART" ]; then
	BPART=3
fi

if [ -z "$OUT" ]; then
	OUT="bootinfo.txt"
fi

if [ -z "$BITMAP" ]; then
	BITMAP="/usr/mdec/netbsd.chrp"
fi

echo "<chrp-boot>" > $OUT
echo "<description>${OS}</description>" >>$OUT
echo "<os-name>${OS}</os-name>" >>$OUT
echo "<boot-script>boot &device;:${BPART}</boot-script>" >>$OUT
if [ -f "${BITMAP}" ]; then
	/bin/cat ${BITMAP} >>$OUT
fi
echo "</chrp-boot>" >>$OUT
