#	$NetBSD: bsd.man.mk,v 1.93.2.1 2005/05/07 14:55:20 riz Exp $
#	@(#)bsd.man.mk	8.1 (Berkeley) 6/8/93

.include <bsd.init.mk>

##### Basic targets
.PHONY:		catinstall maninstall catpages manpages
realinstall:	${MANINSTALL}

##### Default values
.if ${USETOOLS} == "yes"
TMACDEPDIR?=	${TOOLDIR}/share/groff/tmac
.else
TMACDEPDIR?=	/usr/share/tmac
.endif

HTMLDIR?=	${DESTDIR}/usr/share/man
CATDEPS?=	${TMACDEPDIR}/andoc.tmac \
		${TMACDEPDIR}/doc.tmac \
		${TMACDEPDIR}/mdoc/doc-common \
		${TMACDEPDIR}/mdoc/doc-ditroff \
		${TMACDEPDIR}/mdoc/doc-nroff \
		${TMACDEPDIR}/mdoc/doc-syms
HTMLDEPS?=	${TMACDEPDIR}/doc2html.tmac
MANTARGET?=	cat

MAN?=
MLINKS?=
_MNUMBERS=	1 2 3 4 5 6 7 8 9
.SUFFIXES:	${_MNUMBERS:@N@.$N@}

.if ${MKMANZ} == "no"
MANCOMPRESS?=
MANSUFFIX?=
.else
MANCOMPRESS?=	gzip -cf
MANSUFFIX?=	.gz
.endif

# make MANCOMPRESS a filter, so it can be inserted on an as-needed basis
.if !empty(MANCOMPRESS)
MANCOMPRESS:=	| ${MANCOMPRESS}
.endif

__installpage: .USE
	@cmp -s ${.ALLSRC} ${.TARGET} > /dev/null 2>&1 || \
	    (${_MKSHMSG_INSTALL} ${.TARGET}; \
	     ${_MKSHECHO} "${INSTALL_FILE} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		${SYSPKGDOCTAG} ${.ALLSRC} ${.TARGET}" && \
	     ${INSTALL_FILE} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		${SYSPKGDOCTAG} ${.ALLSRC} ${.TARGET})

##### Build and install rules (source form pages)

.if ${MKMAN} != "no"
maninstall:	manlinks
manpages::	# ensure target exists
MANPAGES=	${MAN:C/.$/&${MANSUFFIX}/}

realall:	${MANPAGES}
.if !empty(MANSUFFIX)
.NOPATH:	${MANPAGES}
.SUFFIXES:	${_MNUMBERS:@N@.$N${MANSUFFIX}@}

${_MNUMBERS:@N@.$N.$N${MANSUFFIX}@}:			# build rule
	cat ${.IMPSRC} ${MANCOMPRESS} > ${.TARGET}.tmp && mv ${.TARGET}.tmp ${.TARGET}
.endif # !empty(MANSUFFIX)

.for F in ${MANPAGES:S/${MANSUFFIX}$//:O:u}
_F:=		${DESTDIR}${MANDIR}/man${F:T:E}${MANSUBDIR}/${F}${MANSUFFIX}

.if ${MKUPDATE} == "no"
${_F}!		${F}${MANSUFFIX} __installpage		# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}!		.MADE					# no build at install
.endif
.else
${_F}:		${F}${MANSUFFIX} __installpage		# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}:		.MADE					# no build at install
.endif
.endif

manpages::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

manlinks: .PHONY manpages				# symlink install
.if !empty(MLINKS)
	@set ${MLINKS}; \
	while test $$# -ge 2; do \
		name=$$1; shift; \
		dir=${DESTDIR}${MANDIR}/man$${name##*.}; \
		l=$${dir}${MANSUBDIR}/$${name}${MANSUFFIX}; \
		name=$$1; shift; \
		dir=${DESTDIR}${MANDIR}/man$${name##*.}; \
		t=$${dir}${MANSUBDIR}/$${name}${MANSUFFIX}; \
		if test $$l -nt $$t -o ! -f $$t; then \
			${_MKSHMSG_INSTALL} $$t; \
			${_MKSHECHO} ${INSTALL_LINK} ${SYSPKGDOCTAG} $$l $$t; \
			${INSTALL_LINK} ${SYSPKGDOCTAG} $$l $$t; \
		fi; \
	done
.endif
.endif # ${MKMAN} != "no"

##### Build and install rules (plaintext pages)

.if (${MKCATPAGES} != "no") && (${MKMAN} != "no")
catinstall:	catlinks
catpages::	# ensure target exists
CATPAGES=	${MAN:C/\.([1-9])$/.cat\1${MANSUFFIX}/}

realall:	${CATPAGES}
.NOPATH:	${CATPAGES}
.SUFFIXES:	${_MNUMBERS:@N@.cat$N${MANSUFFIX}@}
.MADE:	${CATDEPS}
.MADE:	${HTMLDEPS}

${_MNUMBERS:@N@.$N.cat$N${MANSUFFIX}@}: ${CATDEPS}	# build rule
	${_MKTARGET_FORMAT}
.if defined(USETBL)
	${TOOL_TBL} ${.IMPSRC} | ${TOOL_ROFF_ASCII} -mandoc ${MANCOMPRESS} \
	    > ${.TARGET}.tmp && mv ${.TARGET}.tmp ${.TARGET}
.else
	${TOOL_ROFF_ASCII} -mandoc ${.IMPSRC} ${MANCOMPRESS} \
	    > ${.TARGET}.tmp && mv ${.TARGET}.tmp ${.TARGET}
.endif

.for F in ${CATPAGES:S/${MANSUFFIX}$//:O:u}
_F:=		${DESTDIR}${MANDIR}/${F:T:E}${MANSUBDIR}/${F:R}.0${MANSUFFIX}

.if ${MKUPDATE} == "no"
${_F}!		${F}${MANSUFFIX} __installpage		# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}!		.MADE					# no build at install
.endif
.else
${_F}:		${F}${MANSUFFIX} __installpage		# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}:		.MADE					# no build at install
.endif
.endif

catpages::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

catlinks: .PHONY catpages				# symlink install
.if !empty(MLINKS)
	@set ${MLINKS}; \
	while test $$# -ge 2; do \
		name=$$1; shift; \
		dir=${DESTDIR}${MANDIR}/cat$${name##*.}; \
		l=$${dir}${MANSUBDIR}/$${name%.*}.0${MANSUFFIX}; \
		name=$$1; shift; \
		dir=${DESTDIR}${MANDIR}/cat$${name##*.}; \
		t=$${dir}${MANSUBDIR}/$${name%.*}.0${MANSUFFIX}; \
		if test $$l -nt $$t -o ! -f $$t; then \
			${_MKSHMSG_INSTALL} $$t; \
			${_MKSHECHO} ${INSTALL_LINK} ${SYSPKGDOCTAG} $$l $$t; \
			${INSTALL_LINK} ${SYSPKGDOCTAG} $$l $$t; \
		fi; \
	done
.endif
.endif # (${MKCATPAGES} != "no") && (${MKMAN} != "no")

##### Build and install rules (HTML pages)

.if ${MKHTML} != "no"					# {
installhtml:	.PHONY htmlpages
htmlpages::	# ensure target exists
HTMLPAGES=	${MAN:C/\.([1-9])$/.html\1/}

html:		.PHONY ${HTMLPAGES}
.NOPATH:	${HTMLPAGES}
.SUFFIXES:	${_MNUMBERS:@N@.html$N@}

${_MNUMBERS:@N@.$N.html$N@}: ${HTMLDEPS}			# build rule
	${_MKTARGET_FORMAT}
	${TOOL_ROFF_HTML} ${.IMPSRC} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

.for F in ${HTMLPAGES:O:u}
# construct installed path
_F:=		${HTMLDIR}/${F:T:E}${MANSUBDIR}/${F:R:S-/index$-/x&-}.html

.if ${MKUPDATE} == "no"
${_F}!		${F} __installpage			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}!		.MADE					# no build at install
.endif
.else
${_F}:		${F} __installpage			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}:		.MADE					# no build at install
.endif
.endif

htmlpages::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

cleanhtml: .PHONY
	rm -f ${HTMLPAGES}
.endif							# }

##### Clean rules
.undef _F

cleandir: cleanman
.if !empty(CLEANFILES)
	rm -f ${CLEANFILES}
.endif

cleanman: .PHONY
.if !empty(MAN) && (${MKMAN} != "no")
.if (${MKCATPAGES} != "no")
	rm -f ${CATPAGES}
.endif
.if !empty(MANSUFFIX)
	rm -f ${MANPAGES} ${CATPAGES:S/${MANSUFFIX}$//}
.endif
.endif
# (XXX ${CATPAGES:S...} cleans up old .catN files where .catN.gz now used)

##### Pull in related .mk logic
.include <bsd.obj.mk>
.include <bsd.files.mk>
.include <bsd.sys.mk>

${TARGETS} catinstall maninstall: # ensure existence
