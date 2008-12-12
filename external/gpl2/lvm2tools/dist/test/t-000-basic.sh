# Copyright (C) 2007 Red Hat, Inc. All rights reserved.
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v.2.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

. ./test-utils.sh

lvm version

v=$abs_top_srcdir/tools/version.h
sed -n "/#define LVM_VERSION ./s///p" "$v" | sed "s/ .*//" > expected

lvm pvmove --version|sed -n "1s/.*: *\([0-9][^ ]*\) .*/\1/p" > actual

# ensure they are the same
diff -u actual expected

# Need mdadm for some pvcreate tests
# verify mdadm is installed and in path (needed for pvcreate tests) ... is it?
which mdadm
