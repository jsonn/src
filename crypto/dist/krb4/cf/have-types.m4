dnl
dnl $Id: have-types.m4,v 1.1.1.1.4.2 2000/06/16 18:46:11 thorpej Exp $
dnl

AC_DEFUN(AC_HAVE_TYPES, [
for i in $1; do
        AC_HAVE_TYPE($i)
done
: << END
changequote(`,')dnl
@@@funcs="$funcs $1"@@@
changequote([,])dnl
END
])
