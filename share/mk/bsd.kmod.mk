#	$NetBSD: bsd.kmod.mk,v 1.22.2.1 1997/11/04 21:51:30 thorpej Exp $

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.include <bsd.own.mk>
.include <bsd.obj.mk>
.MAIN:		all
.endif

.PHONY:		cleankmod kmodinstall load unload
realinstall:	kmodinstall
clean cleandir:	cleankmod

S?=		/sys
KERN=		$S/kern

CFLAGS+=	${COPTS} -D_KERNEL -D_LKM -I. -I${.CURDIR} -I$S -I$S/arch

DPSRCS+=	${SRCS:M*.l:.l=.c} ${SRCS:M*.y:.y=.c}
CLEANFILES+=	${DPSRCS}

OBJS+=		${SRCS:N*.h:N*.sh:R:S/$/.o/g}

.if !defined(PROG)
PROG=	${KMOD}.o
.endif

${PROG}: ${DPSRCS} ${OBJS} ${DPADD}
	${LD} -r ${LDFLAGS} -o tmp.o ${OBJS}
	mv tmp.o ${.TARGET}

.if	!defined(MAN)
MAN=	${KMOD}.4
.endif

all: machine-links ${PROG}

.PHONY:	machine-links
beforedepend: machine-links
machine-links:
	-rm -f machine && \
	    ln -s $S/arch/${MACHINE}/include machine
	-rm -f ${MACHINE_ARCH} && \
	    ln -s $S/arch/${MACHINE_ARCH}/include ${MACHINE_ARCH}
CLEANFILES+=machine ${MACHINE_ARCH}

cleankmod:
	rm -f a.out [Ee]rrs mklog core *.core \
		${PROG} ${OBJS} ${LOBJS} ${CLEANFILES}

#
# define various install targets
#
.if !target(kmodinstall)
kmodinstall:: ${DESTDIR}${KMODDIR}/${PROG}
.if !defined(UPDATE)
.PHONY: ${DESTDIR}${KMODDIR}/${PROG}
.endif
.if !defined(BUILD)
${DESTDIR}${KMODDIR}/${PROG}: .MADE
.endif

.PRECIOUS: ${DESTDIR}${KMODDIR}/${PROG}
${DESTDIR}${KMODDIR}/${PROG}: ${PROG}
	${INSTALL} ${COPY} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
		${.ALLSRC} ${.TARGET}
.endif

lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	${LINT} ${LINTFLAGS} ${LDFLAGS:M-L*} ${LOBJS} ${LDADD}
.endif

.if !target(load)
load:	${PROG}
	/sbin/modload -o ${KMOD} -e${KMOD}_lkmentry ${PROG}
.endif

.if !target(unload)
unload: ${PROG}
	/sbin/modunload -n ${KMOD}
.endif

.include <bsd.man.mk>
.include <bsd.links.mk>
.include <bsd.dep.mk>
.include <bsd.sys.mk>
