#! /bin/sh

# just helping for cross compilation.

MACHINE=evbsh3
MACHINE_ARCH=sh3el
# just in case you forgot to specify this when you built gcc...
CFLAGS="-D__NetBSD__"
export MACHINE MACHINE_ARCH CFLAGS
TARGET=/usr/pkg/cross/bin/shel-netbsdcoff
CPP=`$TARGET-gcc -print-prog-name=cpp`

make AR=$TARGET-ar AS="$TARGET-as -little" CC=$TARGET-gcc LD=$TARGET-ld NM=$TARGET-nm \
	RANLIB=$TARGET-ranlib SIZE=$TARGET-size \
	STRIP=$TARGET-strip OBJCOPY=$TARGET-objcopy \
	CXX=$TARGET-c++ CPP=$CPP $*
