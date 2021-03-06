#	$NetBSD: Makefile,v 1.31 2017/07/01 12:23:09 jmcneill Exp $

.include <bsd.own.mk>

MODULE=		gdb
GDBDIR=		${.CURDIR}/../../external/gpl3/${EXTERNAL_GDB_SUBDIR}
GNUHOSTDIST=    ${GDBDIR}/dist

FIND_ARGS=	\! \( -type d -name sim -prune \)

CONFIGURE_ARGS=	--target=${MACHINE_GNU_PLATFORM} --disable-nls \
		--program-transform-name="s,^,${MACHINE_GNU_PLATFORM}-,"

MAKE_ARGS=	MACHINE= MAKEINFO=${TOOL_MAKEINFO:Q}

ALL_TARGET=	all-gdb
INSTALL_TARGET=	install-gdb

.include "${GDBDIR}/Makefile.gdb_arch"
#GDB_MACHINE_ARCH=       ${MACHINE_ARCH:C/e?armv[4-7]h?f?/arm/}

.include "${.CURDIR}/../Makefile.gmakehost"

CCADDFLAGS+= --sysroot=${DESTDIR} -B${DESTDIR}/usr/lib/ -I${.OBJDIR}/.native/gcc/include
LDADDFLAGS+= -L${DESTDIR}/lib -L${DESTDIR}/usr/lib
CXXADDFLAGS+= -D__STDC_FORMAT_MACROS
CXXADDFLAGS+= -D__STDC_LIMIT_MACROS
CXXADDFLAGS+= -D__STDC_CONSTANT_MACROS
HOST_CXXFLAGS+= ${CXXADDFLAGS}

NEWCONFIGDIR?=	${.CURDIR}/../..
MKNATIVE?=	${.CURDIR}/mknative-gdb

# Some configure tests require running a test program, which is not
# possible when cross-building.  Provide configure with "cached"
# values in the environment.
MKNATIVE_CONFIGURE_PRESET= \
	ac_cv_prog_cc_cross=yes \
	ac_cv_func_fork_works=yes \
	ac_cv_func_strcoll_works=yes \
	bash_cv_func_ctype_nonascii=yes \
	bash_cv_func_sigsetjmp=present \
	bash_cv_func_strcoll_broken=no \
	bash_cv_must_reinstall_sighandlers=no

CONFIGURE_ENV+=	am_cv_func_iconv=no

# Recent versions of Solaris have ncurses, but they hide the lib in an
# odd directory. Prevent configure from finding the ncurses headers,
# Solaris curses is sufficient.
.if ${BUILD_OSTYPE} == "SunOS"
CONFIGURE_ENV+=	ac_cv_header_ncurses_h=no \
		ac_cv_header_ncurses_ncurses_h=no \
		ac_cv_header_ncurses_term_h=no
.endif

# Disable sim unless it's known to work (configure's default is to
# enable sim if supported).
CONFIGURE_ARGS_SIM=
.if \
    ${MACHINE_CPU} != "powerpc" && \
    ${MACHINE_CPU} != "powerpc64"
CONFIGURE_ARGS_SIM+= --disable-sim
.endif

native-gdb: .native/.configure_done
	@echo 'Extracting GDB configury for a native toolchain.'
	MAKE=${MAKE:Q} ${HOST_SH} ${MKNATIVE} gdb \
		${.OBJDIR}/.native ${NEWCONFIGDIR} ${MACHINE_GNU_PLATFORM} \
		${GDB_MACHINE_ARCH}

.native/.configure_done: ${_GNU_CFGSRC} ${.CURDIR}/Makefile
	mkdir ${.OBJDIR}/.native 2>/dev/null || true
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd ${.OBJDIR}/.native && ${CONFIGURE_ENV:NC*:NLD*} \
			CC_FOR_BUILD=${HOST_CC:Q} \
			CC=${CC:Q}' '${CCADDFLAGS:Q} \
			CXX=${CXX:Q}' '${CCADDFLAGS:Q} \
			CPP=${CPP:Q}' '-isystem' '${DESTDIR}/usr/include \
			CFLAGS= CPPFLAGS= CXXFLAGS=${CXXADDFLAGS:Q} \
			LDFLAGS=${LDADDFLAGS:Q} \
			MSGFMT=${TOOLDIR}/bin/${_TOOL_PREFIX}msgfmt \
			XGETTEXT=${TOOLDIR}/bin/${_TOOL_PREFIX}xgettext \
			LIBS=-lintl \
			${MKNATIVE_CONFIGURE_PRESET} \
			${HOST_SH} ${GNUHOSTDIST}/configure \
			--prefix=/usr \
			--with-separate-debug-dir=/usr/libdata/debug \
			${CONFIGURE_ARGS_SIM} \
			--build=`${GNUHOSTDIST}/config.guess` \
			--host=${MACHINE_GNU_PLATFORM} \
			--target=${MACHINE_GNU_PLATFORM}) && \
		(cd ${.OBJDIR}/.native && \
			/usr/bin/env ${MKNATIVE_CONFIGURE_PRESET} \
				${MAKE} configure-host) && \
		(cd ${.OBJDIR}/.native/bfd && ${MAKE} bfd.h bfdver.h) && \
		(cd ${.OBJDIR}/.native/gdb && ${MAKE} init.c version.c)
	@touch $@

clean: clean.native
clean.native:
	-rm -r -f .native
