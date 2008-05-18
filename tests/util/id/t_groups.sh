# $NetBSD: t_groups.sh,v 1.2.8.1 2008/05/18 12:36:01 yamt Exp $
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
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

run_groups() {
	[ -f ./groups ] || ln -s $(atf_get_srcdir)/h_id ./groups
	./groups "${@}"
}

atf_test_case correct
correct_head() {
	atf_set "descr" "Checks that correct queries work"
}
correct_body() {
	echo "users wheel" >expout
	atf_check "run_groups" 0 expout null
	atf_check "run_groups 100" 0 expout null
	atf_check "run_groups test" 0 expout null

	echo "wheel" >expout
	atf_check "run_groups 0" 0 expout null
	atf_check "run_groups root" 0 expout null
}

atf_test_case syntax
syntax_head() {
	atf_set "descr" "Checks the command's syntax"
}
syntax_body() {
	# Give an invalid flag but which is allowed by id (with which
	# groups shares code) when using the -Gn options.
	atf_check "run_groups -r" 1 null stderr
	atf_check "grep '^usage:' stderr" 0 ignore null
}

atf_init_test_cases()
{
	atf_add_test_case correct
	atf_add_test_case syntax
}
