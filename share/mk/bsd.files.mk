#	$NetBSD: bsd.files.mk,v 1.8.4.1 1999/08/10 00:43:35 mcr Exp $

# This file can be included multiple times.  It clears the definition of
# FILES at the end so that this is possible.

.PHONY:		filesinstall
realinstall:	filesinstall

.if defined(FILES) && !empty(FILES)
FILESDIR?=${BINDIR}
FILESOWN?=${BINOWN}
FILESGRP?=${BINGRP}
FILESMODE?=${NONBINMODE}
.for F in ${FILES}
FILESDIR_${F}?=${FILESDIR}
FILESOWN_${F}?=${FILESOWN}
FILESGRP_${F}?=${FILESGRP}
FILESMODE_${F}?=${FILESMODE}
.if defined(FILESNAME)
FILESNAME_${F} ?= ${FILESNAME}
.else
FILESNAME_${F} ?= ${F:T}
.endif
filesinstall:: ${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}
.if !defined(UPDATE)
.PHONY: ${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}
.endif
.if !defined(BUILD)
${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}: .MADE
.endif

.PRECIOUS: ${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}
${DESTDIR}${FILESDIR_${F}}/${FILESNAME_${F}}: ${F}
.if ${MORTALINSTALL} != "no"
	${INSTALL} ${PRESERVE} ${COPY} \
		-m ${FILESMODE_${F}} ${.ALLSRC} ${.TARGET}
.else
	${INSTALL} ${PRESERVE} ${COPY} -o ${FILESOWN_${F}} -g ${FILESGRP_${F}} \
		-m ${FILESMODE_${F}} ${.ALLSRC} ${.TARGET}
.endif
.endfor
.endif

.if !target(filesinstall)
filesinstall::
.endif

FILES:=
