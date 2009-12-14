#	$NetBSD: bsd.o32.mk,v 1.1.2.2 2009/12/14 06:21:11 mrg Exp $

LD+=		-m elf32_mipso32
MLIBDIR=	o32

#.include "${NETBSDSRCDIR}/compat/Makefile.m32"
COPTS+=			-mabi=32 -march=mips3
CPUFLAGS+=		-mabi=32 -march=mips3
LDADD+=			-mabi=32 -march=mips3
LDFLAGS+=		-mabi=32 -march=mips3
MKDEPFLAGS+=		-mabi=32 -march=mips3

.include "${NETBSDSRCDIR}/compat/Makefile.compat"
