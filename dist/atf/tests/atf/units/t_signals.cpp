//
// Automated Testing Framework (atf)
//
// Copyright (c) 2008 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All advertising materials mentioning features or use of this
//    software must display the following acknowledgement:
//        This product includes software developed by the NetBSD
//        Foundation, Inc. and its contributors.
// 4. Neither the name of The NetBSD Foundation nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

extern "C" {
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>

#include "atf/exceptions.hpp"
#include "atf/macros.hpp"
#include "atf/signals.hpp"

namespace sigusr1 {
    static bool happened = false;

    static
    void
    handler(int signo)
    {
        happened = true;
    }

    static
    void
    program(void)
    {
        struct sigaction sa;
        sa.sa_handler = handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (::sigaction(SIGUSR1, &sa, NULL) == -1)
            throw atf::system_error("sigusr1::program",
                                    "sigaction(2) failed", errno);
    }
} // namespace sigusr1

namespace sigusr1_2 {
    static bool happened = false;

    static
    void
    handler(int signo)
    {
        happened = true;
    }
} // namespace sigusr1_2

// ------------------------------------------------------------------------
// Tests for the "signal_holder" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(signal_holder_preserve);
ATF_TEST_CASE_HEAD(signal_holder_preserve)
{
    set("descr", "Tests that signal_holder preserves the original signal "
                 "handler and restores it upon destruction");
}
ATF_TEST_CASE_BODY(signal_holder_preserve)
{
    using atf::signals::signal_holder;

    sigusr1::program();

    sigusr1::happened = false;
    ::kill(::getpid(), SIGUSR1);
    ATF_CHECK(sigusr1::happened);

    {
        signal_holder hld(SIGUSR1);
        ::kill(::getpid(), SIGUSR1);
    }

    sigusr1::happened = false;
    ::kill(::getpid(), SIGUSR1);
    ATF_CHECK(sigusr1::happened);
}

ATF_TEST_CASE(signal_holder_destructor);
ATF_TEST_CASE_HEAD(signal_holder_destructor)
{
    set("descr", "Tests that signal_holder processes a pending signal "
                 "upon destruction");
}
ATF_TEST_CASE_BODY(signal_holder_destructor)
{
    using atf::signals::signal_holder;

    sigusr1::program();

    sigusr1::happened = false;
    ::kill(::getpid(), SIGUSR1);
    ATF_CHECK(sigusr1::happened);

    {
        signal_holder hld(SIGUSR1);

        sigusr1::happened = false;
        ::kill(::getpid(), SIGUSR1);
        ATF_CHECK(!sigusr1::happened);
    }
    ATF_CHECK(sigusr1::happened);
}

ATF_TEST_CASE(signal_holder_process);
ATF_TEST_CASE_HEAD(signal_holder_process)
{
    set("descr", "Tests that signal_holder's process method works to "
                 "process a delayed signal explicitly");
}
ATF_TEST_CASE_BODY(signal_holder_process)
{
    using atf::signals::signal_holder;

    sigusr1::program();

    sigusr1::happened = false;
    ::kill(::getpid(), SIGUSR1);
    ATF_CHECK(sigusr1::happened);

    {
        signal_holder hld(SIGUSR1);

        sigusr1::happened = false;
        ::kill(::getpid(), SIGUSR1);
        ATF_CHECK(!sigusr1::happened);

        hld.process();
        ATF_CHECK(sigusr1::happened);

        sigusr1::happened = false;
    }
    ATF_CHECK(!sigusr1::happened);
}

// ------------------------------------------------------------------------
// Tests for the "signal_programmer" class.
// ------------------------------------------------------------------------

ATF_TEST_CASE(signal_programmer_program);
ATF_TEST_CASE_HEAD(signal_programmer_program)
{
    set("descr", "Tests that signal_programmer correctly installs a "
                 "handler");
}
ATF_TEST_CASE_BODY(signal_programmer_program)
{
    using atf::signals::signal_programmer;

    signal_programmer sp(SIGUSR1, sigusr1_2::handler);

    sigusr1_2::happened = false;
    ::kill(::getpid(), SIGUSR1);
    ATF_CHECK(sigusr1_2::happened);
}

ATF_TEST_CASE(signal_programmer_preserve);
ATF_TEST_CASE_HEAD(signal_programmer_preserve)
{
    set("descr", "Tests that signal_programmer uninstalls the handler "
                 "during destruction");
}
ATF_TEST_CASE_BODY(signal_programmer_preserve)
{
    using atf::signals::signal_programmer;

    sigusr1::program();
    sigusr1::happened = false;

    {
        signal_programmer sp(SIGUSR1, sigusr1_2::handler);

        sigusr1_2::happened = false;
        ::kill(::getpid(), SIGUSR1);
        ATF_CHECK(sigusr1_2::happened);
    }

    ATF_CHECK(!sigusr1::happened);
    ::kill(::getpid(), SIGUSR1);
    ATF_CHECK(sigusr1::happened);
}

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add the tests for the "signal_holder" class.
    ATF_ADD_TEST_CASE(tcs, signal_holder_preserve);
    ATF_ADD_TEST_CASE(tcs, signal_holder_destructor);
    ATF_ADD_TEST_CASE(tcs, signal_holder_process);

    // Add the tests for the "signal_programmer" class.
    ATF_ADD_TEST_CASE(tcs, signal_programmer_program);
    ATF_ADD_TEST_CASE(tcs, signal_programmer_preserve);
}
