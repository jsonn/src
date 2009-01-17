#!/bin/sh

# Generates various defines needed for using rump on non-NetBSD systems.
# Run this occasionally (yes, it's a slightly suboptimal kludge, but
# better than nothing).

echo Generating rumpdefs.h
rm -f rumpdefs.h
exec > rumpdefs.h

printf '/*	$NetBSD: makerumpdefs.sh,v 1.3.4.2 2009/01/17 13:29:35 mjf Exp $	*/\n\n'
printf '/*\n *\tAUTOMATICALLY GENERATED.  DO NOT EDIT.\n */\n\n'
printf '#ifndef _RUMP_RUMPDEFS_H_\n'
printf '#define _RUMP_RUMPDEFS_H_\n\n'
printf '#include <rump/rump_namei.h>\n'

fromvers () {
	echo
	sed -n '1{s/\$//gp;q;}' $1
}

fromvers ../../../sys/fcntl.h
sed -n '/#define	O_[A-Z]*	*0x/s/O_/RUMP_O_/gp' \
    < ../../../sys/fcntl.h

fromvers ../../../sys/vnode.h
printf '#ifndef __VTYPE_DEFINED\n#define __VTYPE_DEFINED\n'
sed -n '/enum vtype.*{/p' < ../../../sys/vnode.h
printf '#endif /* __VTYPE_DEFINED */\n'

fromvers ../../../sys/errno.h
printf '#ifndef EJUSTRETURN\n'
sed -n '/EJUSTRETURN/p'	< ../../../sys/errno.h
printf '#endif /* EJUSTRETURN */\n'

fromvers ../../../sys/lock.h
sed -n '/#define.*LK_[A-Z]/s/LK_/RUMP_LK_/gp' <../../../sys/lock.h	\
    | sed 's,/\*.*$,,'

printf '\n#endif /* _RUMP_RUMPDEFS_H_ */\n'
