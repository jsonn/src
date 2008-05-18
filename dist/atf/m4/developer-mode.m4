dnl
dnl Automated Testing Framework (atf)
dnl
dnl Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
dnl CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
dnl INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
dnl IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
dnl DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
dnl GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
dnl INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
dnl IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
dnl OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
dnl IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl

dnl -----------------------------------------------------------------------
dnl Set up the developer mode
dnl -----------------------------------------------------------------------

AC_DEFUN([ATF_DEVELOPER_MODE], [
    AC_ARG_ENABLE(developer,
                  AS_HELP_STRING(--enable-developer,
                                 [enable developer features]),,
                  [case ${PACKAGE_VERSION} in
                   0.*|*99*|*alpha*|*beta*)
                       enable_developer=yes
                       ;;
                   *)
                       enable_developer=no
                       ;;
                   esac])

    if test ${enable_developer} = yes; then
        try_c_cxx_flags="-g \
                         -Wall \
                         -Wcast-qual \
                         -Werror \
                         -Wextra \
                         -Wno-sign-compare \
                         -Wno-unused-parameter \
                         -Wpointer-arith \
                         -Wredundant-decls \
                         -Wreturn-type \
                         -Wshadow \
                         -Wswitch \
                         -Wwrite-strings"

        try_c_flags="-Wmissing-prototypes \
                     -Wno-traditional \
                     -Wstrict-prototypes"

        try_cxx_flags="-Wabi \
                       -Wctor-dtor-privacy \
                       -Wno-deprecated \
                       -Wno-non-template-friend \
                       -Wno-pmf-conversions \
                       -Wnon-virtual-dtor \
                       -Woverloaded-virtual \
                       -Wreorder \
                       -Wsign-promo \
                       -Wsynth"

        #
        # The following flags should also be enabled but cannot be.  Reasons
        # given below.
        #
        # -Wold-style-cast: Raises errors when using TIOCGWINSZ, at least under
        #                   Mac OS X.  This is due to the way _IOR is defined.
        #
    else
        try_c_cxx_flags="-DNDEBUG"
    fi
    try_c_cxx_flags="${try_c_cxx_flags} -D_FORTIFY_SOURCE=2"

    AC_LANG_PUSH(C)
    valid=""
    for f in ${try_c_cxx_flags} ${try_c_flags}
    do
        AC_MSG_CHECKING(whether ${CC} supports ${f})
        saved_cflags="${CFLAGS}"
        CFLAGS="${CFLAGS} ${f}"
        AC_LINK_IFELSE([int main(void) { return 0; }],
                       AC_MSG_RESULT(yes)
                       valid="${valid} ${f}",
                       AC_MSG_RESULT(no))
        CFLAGS="${saved_cflags}"
    done
    CFLAGS="${CFLAGS} ${valid}"
    AC_LANG_POP()

    AC_LANG_PUSH(C++)
    valid=""
    for f in ${try_c_cxx_flags} ${try_cxx_flags}
    do
        AC_MSG_CHECKING(whether ${CXX} supports ${f})
        saved_cxxflags="${CXXFLAGS}"
        CXXFLAGS="${CXXFLAGS} ${f}"
        AC_LINK_IFELSE([int main(void) { return 0; }],
                       AC_MSG_RESULT(yes)
                       valid="${valid} ${f}",
                       AC_MSG_RESULT(no))
        CXXFLAGS="${saved_cxxflags}"
    done
    CXXFLAGS="${CXXFLAGS} ${valid}"
    AC_LANG_POP()
])
