#
# Automated Testing Framework (atf)
#
# Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
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
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

atf_test_case exitcode_0_0
exitcode_0_0_head()
{
    atf_set "descr" "Runs a program that returns true and expects true"
}
exitcode_0_0_body()
{
    atf_check 'true' 0 null null
}

atf_test_case exitcode_0_1
exitcode_0_1_head()
{
    atf_set "descr" "Runs a program that returns true and expects false"
}
exitcode_0_1_body()
{
    atf_check 'true' 1 null null
}

atf_test_case exitcode_1_0
exitcode_1_0_head()
{
    atf_set "descr" "Runs a program that returns false and expects true"
}
exitcode_1_0_body()
{
    atf_check 'false' 0 null null
}

atf_test_case exitcode_1_1
exitcode_1_1_head()
{
    atf_set "descr" "Runs a program that returns false and expects false"
}
exitcode_1_1_body()
{
    atf_check 'false' 1 null null
}

atf_test_case stdout_expout_pass
stdout_expout_pass_head()
{
    atf_set "descr" "Runs a program with stdout set to expout and passes"
}
stdout_expout_pass_body()
{
    echo foo >expout
    atf_check 'echo foo' 0 expout null
}

atf_test_case stdout_expout_fail
stdout_expout_fail_head()
{
    atf_set "descr" "Runs a program with stdout set to expout and fails"
}
stdout_expout_fail_body()
{
    echo foo >expout
    atf_check 'echo bar' 0 expout null
}

atf_test_case stdout_ignore_empty
stdout_ignore_empty_head()
{
    atf_set "descr" "Runs a program with stdout set to ignore and" \
                    "writes nothing"
}
stdout_ignore_empty_body()
{
    atf_check 'true' 0 ignore null
}

atf_test_case stdout_ignore_sth
stdout_ignore_sth_head()
{
    atf_set "descr" "Runs a program with stdout set to ignore and" \
                    "writes something"
}
stdout_ignore_sth_body()
{
    atf_check 'echo foo' 0 ignore null
}

atf_test_case stdout_null_empty
stdout_null_empty_head()
{
    atf_set "descr" "Runs a program with stdout set to null and" \
                    "writes nothing"
}
stdout_null_empty_body()
{
    atf_check 'true' 0 null null
}

atf_test_case stdout_null_sth
stdout_null_sth_head()
{
    atf_set "descr" "Runs a program with stdout set to null and" \
                    "writes something"
}
stdout_null_sth_body()
{
    atf_check 'echo foo' 0 null null
}

atf_test_case stdout_stdout_written
stdout_stdout_written_head()
{
    atf_set "descr" "Runs a program with stdout set to stdout and" \
                    "writes something"
}
stdout_stdout_written_body()
{
    atf_check 'echo foo' 0 stdout null
    echo foo >aux
    cmp -s stdout aux || atf_fail "Test failed"
}

atf_test_case stdout_stdout_noclobber
stdout_stdout_noclobber_head()
{
    atf_set "descr" "Runs a program multiple times with different stdout" \
                    "modes and ensures that they do not overwrite an old" \
                    "stdout file"
}
stdout_stdout_noclobber_body()
{
    echo bar >stdout

    echo foo >expout
    atf_check 'echo foo' 0 expout null
    atf_check 'echo foo' 0 ignore null
    atf_check 'true' 0 null null

    echo bar >aux
    cmp -s stdout aux || atf_fail "Test failed"

    echo "foo bar" >stdout
    atf_check "cut -d ' ' -f 1 <stdout" 0 stdout null
    echo foo >expout
    atf_check "cat stdout" 0 expout null
}

atf_test_case stderr_experr_pass
stderr_experr_pass_head()
{
    atf_set "descr" "Runs a program with stderr set to experr and passes"
}
stderr_experr_pass_body()
{
    echo foo >experr
    atf_check 'echo foo 1>&2' 0 null experr
}

atf_test_case stderr_experr_fail
stderr_experr_fail_head()
{
    atf_set "descr" "Runs a program with stderr set to experr and fails"
}
stderr_experr_fail_body()
{
    echo foo >experr
    atf_check 'echo bar 1>&2' 0 null experr
}

atf_test_case stderr_ignore_empty
stderr_ignore_empty_head()
{
    atf_set "descr" "Runs a program with stderr set to ignore and" \
                    "writes nothing"
}
stderr_ignore_empty_body()
{
    atf_check 'true 1>&2' 0 null ignore
}

atf_test_case stderr_ignore_sth
stderr_ignore_sth_head()
{
    atf_set "descr" "Runs a program with stderr set to ignore and" \
                    "writes something"
}
stderr_ignore_sth_body()
{
    atf_check 'echo foo 1>&2' 0 null ignore
}

atf_test_case stderr_null_empty
stderr_null_empty_head()
{
    atf_set "descr" "Runs a program with stderr set to null and" \
                    "writes nothing"
}
stderr_null_empty_body()
{
    atf_check 'true 1>&2' 0 null null
}

atf_test_case stderr_null_sth
stderr_null_sth_head()
{
    atf_set "descr" "Runs a program with stderr set to null and" \
                    "writes something"
}
stderr_null_sth_body()
{
    atf_check 'echo foo 1>&2' 0 null null
}

atf_test_case stderr_stderr_written
stderr_stderr_written_head()
{
    atf_set "descr" "Runs a program with stderr set to stderr and" \
                    "writes something"
}
stderr_stderr_written_body()
{
    atf_check 'echo foo 1>&2' 0 null stderr
    echo foo >aux
    cmp -s stderr aux || atf_fail "Test failed"
}

atf_test_case stderr_stderr_noclobber
stderr_stderr_noclobber_head()
{
    atf_set "descr" "Runs a program multiple times with different stderr" \
                    "modes and ensures that they do not overwrite an old" \
                    "stderr file"
}
stderr_stderr_noclobber_body()
{
    echo bar >stderr

    echo foo >experr
    atf_check 'echo foo 1>&2' 0 null experr
    atf_check 'echo foo 1>&2' 0 null ignore
    atf_check 'true 1>&2' 0 null null

    echo bar >aux
    cmp -s stderr aux || atf_fail "Test failed"

    echo "foo bar" >stderr
    atf_check "cut -d ' ' -f 1 <stderr 1>&2" 0 null stderr
    echo foo >experr
    atf_check "cat stderr 1>&2" 0 null experr
}

atf_init_test_cases()
{
    atf_add_test_case exitcode_0_0
    atf_add_test_case exitcode_0_1
    atf_add_test_case exitcode_1_0
    atf_add_test_case exitcode_1_1

    atf_add_test_case stdout_expout_pass
    atf_add_test_case stdout_expout_fail
    atf_add_test_case stdout_ignore_empty
    atf_add_test_case stdout_ignore_sth
    atf_add_test_case stdout_null_empty
    atf_add_test_case stdout_null_sth
    atf_add_test_case stdout_stdout_written
    atf_add_test_case stdout_stdout_noclobber

    atf_add_test_case stderr_experr_pass
    atf_add_test_case stderr_experr_fail
    atf_add_test_case stderr_ignore_empty
    atf_add_test_case stderr_ignore_sth
    atf_add_test_case stderr_null_empty
    atf_add_test_case stderr_null_sth
    atf_add_test_case stderr_stderr_written
    atf_add_test_case stderr_stderr_noclobber
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
