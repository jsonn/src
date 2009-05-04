#!/bin/sh
# $NetBSD: genwakecode.sh,v 1.2.18.1 2009/05/04 08:12:09 yamt Exp $

AWK=${AWK:=awk}
HEXDUMP=${HEXDUMP:=hexdump}
SED=${SED:=sed}

P='/WAKEUP_/ { printf("#define\t%s%s\t%s\n", $2, length($2) < 16 ? "\t" : "", $1); }'
${AWK} "$P" < acpi_wakecode.bin.map

echo 
echo 'static const unsigned char wakecode[] = {';
${HEXDUMP} -v -e '"\t" 8/1 "0x%02x, " "\n"' < acpi_wakecode.bin | ${SED} 's/0x  /0x00/g'
echo '};'

exit 0
