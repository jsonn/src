#	$NetBSD: bsd.dep.mk,v 1.27.4.1 1999/12/27 18:31:11 wrstuden Exp $

.PHONY:		cleandepend
cleandir distclean: cleandepend

MKDEP?=		mkdep

# some of the rules involve .h sources, so remove them from mkdep line
depend: beforedepend
.if defined(SRCS)
depend: .depend
	@true # hack to prevent "make depend" from using implicit rules
.NOPATH: .depend
.depend: ${SRCS} ${DPSRCS}
	@rm -f .depend
	@files="${.ALLSRC:M*.s} ${.ALLSRC:M*.S}"; \
	if [ "$$files" != " " ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${AFLAGS:M-[ID]*:Q} ${CPPFLAGS:Q} -traditional-cpp ${AINC:Q} \
	    $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${AFLAGS:M-[ID]*} ${CPPFLAGS} -traditional-cpp ${AINC} $$files; \
	fi
	@files="${.ALLSRC:M*.c}"; \
	if [ "$$files" != "" ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CFLAGS:M-[ID]*:Q} ${CPPFLAGS:Q} $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
	@files="${.ALLSRC:M*.m}"; \
	if [ "$$files" != "" ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${OBJCFLAGS:M-[ID]*:Q} ${CPPFLAGS:Q} $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${OBJCFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
	@files="${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cxx}"; \
	if [ "$$files" != "  " ]; then \
	  echo ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CXXFLAGS:M-[ID]*:Q} ${CPPFLAGS:Q} $$files; \
	  ${MKDEP} -a ${MKDEPFLAGS} \
	    ${CXXFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
cleandepend:
	rm -f .depend ${.CURDIR}/tags ${CLEANDEPEND}
.else
cleandepend:
.endif
depend: afterdepend

beforedepend:
afterdepend:

.if !target(tags)
.if defined(SRCS)
tags: ${SRCS}
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:N*.h} | \
	    sed "s;\${.CURDIR}/;;" > tags
.else
tags:
.endif
.endif
