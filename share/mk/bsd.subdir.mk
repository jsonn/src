#	$NetBSD: bsd.subdir.mk,v 1.28.2.2 1997/11/13 09:20:52 thorpej Exp $
#	@(#)bsd.subdir.mk	8.1 (Berkeley) 6/8/93

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.include <bsd.own.mk>
.MAIN:		all
.endif

.for dir in ${SUBDIR}
.if exists(${dir}.${MACHINE})
__REALSUBDIR+=${dir}.${MACHINE}
.else
__REALSUBDIR+=${dir}
.endif
.endfor

.for dir in ${__REALSUBDIR}
.for targ in ${TARGETS}
.PHONY: ${targ}-${dir}
${targ}-${dir}: .MAKE
	@echo "${targ} ===> ${_THISDIR_}${dir}"
	@cd ${.CURDIR}/${dir}; \
	${MAKE} "_THISDIR_=${_THISDIR_}${dir}/" ${targ}
subdir-${targ}: ${targ}-${dir}
${targ}: subdir-${targ}
.endfor

# Backward-compatibility with the old rules.  If this went away,
# 'xlint' could become 'lint', 'xinstall' could become 'install', etc.
${dir}: all-${dir}
.endfor

# Make sure all of the standard targets are defined, even if they do nothing.
${TARGETS}:
