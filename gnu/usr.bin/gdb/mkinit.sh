#!/bin/sh
#
# $NetBSD: mkinit.sh,v 1.1.1.1.6.1 1999/12/27 18:29:19 wrstuden Exp $
#
# Generate the init.c file on stdout.
# Arguments are names of *.o files.

# Run awk on the (edited) output of nm, i.e:
# 00000000 T _initialize_xxxx
# and turn it into a list of function calls.

# Which awk do we use? (awk,gawk,nawk?)
awk=${AWK:-awk}

# $NM may be a name of nm command for cross compilation.  The default
# value here is a default when you invoke this script manually.
nm=${NM:-nm}

# Does the compiler prepend an underscore?
if ($nm version.o |grep -q ' _version')
then
 sedarg='s/ _/ /'
else
 sedarg=
fi
# echo "mkinit.sh: sedarg=$sedarg" >&2

echo '/* Do not modify this file.  */'
echo '/* It is created automatically by the Makefile.  */'
echo 'void initialize_all_files () {'

for f
do
  $nm -p $f
done |
sed -e "$sedarg" |
$awk '
function doit(str) {
  printf("  {extern void %s (); %s ();}\n", str, str);
}
/ T _initialize_/ {
  doit($3);
  next;
}
{ next; }
'

echo '}'

exit 0
