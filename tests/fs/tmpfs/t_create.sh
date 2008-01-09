# $NetBSD: t_create.sh,v 1.1.4.2 2008/01/09 01:59:13 matt Exp $
#
# Copyright (c) 2005, 2006, 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

#
# Verifies that the create operation works.
#

atf_test_case create
create_head() {
	atf_set "descr" "Verifies that files can be created"
	atf_set "require.user" "root"
}
create_body() {
	test_mount

	atf_check 'test -f a' 1 null null
	atf_check 'touch a' 0 null null
	atf_check 'test -f a' 0 null null

	test_unmount
}

atf_test_case attrs
attrs_head() {
	atf_set "descr" "Verifies that a new file gets the correct" \
	                "attributes"
	atf_set "require.config" "unprivileged-user"
	atf_set "require.user" "root"
}
attrs_body() {
	# Allow the unprivileged user to access the work directory.
	chmod 711 .

	test_mount

	umask 022
	atf_check 'test -f a' 1 null null
	atf_check 'touch a' 0 null null
	atf_check 'test -f a' 0 null null

	eval $(stat -s . | sed -e 's|st_|dst_|g')
	eval $(stat -s a)
	test ${st_flags} -eq 0 || atf_fail "Incorrect flags"
	test ${st_size} -eq 0 || atf_fail "Incorrect size"
	test ${st_uid} -eq $(id -u) || atf_fail "Incorrect uid"
	test ${st_gid} -eq ${dst_gid} || atf_fail "Incorrect gid"
	test ${st_mode} = 0100644 || atf_fail "Incorrect mode"

	user=$(atf_config_get unprivileged-user)

	atf_check 'mkdir b c' 0 null null

	atf_check "chown ${user}:0 b" 0 null null
	eval $(stat -s b)
	[ ${st_uid} -eq $(id -u ${user}) ] || atf_fail "Incorrect owner"
	[ ${st_gid} -eq 0 ] || atf_fail "Incorrect group"

	atf_check "chown ${user}:100 c" 0 null null
	eval $(stat -s c)
	[ ${st_uid} -eq $(id -u ${user}) ] || atf_fail "Incorrect owner"
	[ ${st_gid} -eq 100 ] || atf_fail "Incorrect group"

	su ${user} -c 'touch b/a'
	eval $(stat -s b/a)
	[ ${st_uid} -eq $(id -u ${user}) ] || atf_fail "Incorrect owner"
	[ ${st_gid} -eq 0 ] || atf_fail "Incorrect group"

	su ${user} -c 'touch c/a'
	eval $(stat -s c/a)
	[ ${st_uid} -eq $(id -u ${user}) ] || atf_fail "Incorrect owner"
	[ ${st_gid} -eq 100 ] || atf_fail "Incorrect group"

	test_unmount
}

atf_test_case kqueue
kqueue_head() {
	atf_set "descr" "Verifies that creating a file raises the correct" \
	                "kqueue events"
	atf_set "require.user" "root"
}
kqueue_body() {
	test_mount

	atf_check 'mkdir dir' 0 null null
	echo 'touch dir/a' | kqueue_monitor 1 dir
	kqueue_check dir NOTE_WRITE

	test_unmount
}

atf_init_test_cases() {
	. $(atf_get_srcdir)/../h_funcs.subr
	. $(atf_get_srcdir)/h_funcs.subr

	atf_add_test_case create
	atf_add_test_case attrs
	atf_add_test_case kqueue
}
