#	$NetBSD: Makefile,v 1.2 2017/06/08 17:09:29 skrll Exp $

HOSTLIB=	fdt

.include <bsd.own.mk>

LIBFDT_DIR=	${.CURDIR}/../../sys/external/bsd/libfdt/dist

.include "${LIBFDT_DIR}/Makefile.libfdt"

SRCS=	${LIBFDT_SRCS}

CPPFLAGS+=	-I${.CURDIR}/../compat -I${LIBFDT_DIR}
CPPFLAGS+=	-I${TOOLDIR}/include/nbinclude

.PATH:		${LIBFDT_DIR}

CPPFLAGS+=	-DHAVE_NBTOOL_CONFIG_H=1 -D_FILE_OFFSET_BITS=64

HOST_CPPFLAGS:=	${CPPFLAGS} ${HOST_CPPFLAGS}
CPPFLAGS:=	# empty

.include "${.CURDIR}/../Makefile.nbincludes"
.include <bsd.hostlib.mk>
