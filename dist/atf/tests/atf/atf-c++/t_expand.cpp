//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
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

#include <cstring>

#include "atf-c++/expand.hpp"
#include "atf-c++/macros.hpp"

// XXX Many of the tests here are duplicated in atf-c/t_expand.  Should
// find a way to easily share them, or maybe remove the ones here.

// ------------------------------------------------------------------------
// Test cases for the free functions.
// ------------------------------------------------------------------------

ATF_TEST_CASE(is_glob);
ATF_TEST_CASE_HEAD(is_glob)
{
    set_md_var("descr", "Tests the is_glob function.");
}
ATF_TEST_CASE_BODY(is_glob)
{
    using atf::expand::is_glob;

    ATF_CHECK(!is_glob(""));
    ATF_CHECK(!is_glob("a"));
    ATF_CHECK(!is_glob("foo"));

    ATF_CHECK( is_glob("*"));
    ATF_CHECK( is_glob("a*"));
    ATF_CHECK( is_glob("*a"));
    ATF_CHECK( is_glob("a*b"));

    ATF_CHECK( is_glob("?"));
    ATF_CHECK( is_glob("a?"));
    ATF_CHECK( is_glob("?a"));
    ATF_CHECK( is_glob("a?b"));
}

ATF_TEST_CASE(matches_glob_plain);
ATF_TEST_CASE_HEAD(matches_glob_plain)
{
    set_md_var("descr", "Tests the matches_glob function by using plain "
               "text strings as patterns only.");
}
ATF_TEST_CASE_BODY(matches_glob_plain)
{
    using atf::expand::matches_glob;

    ATF_CHECK( matches_glob("", ""));
    ATF_CHECK(!matches_glob("a", ""));
    ATF_CHECK(!matches_glob("", "a"));

    ATF_CHECK( matches_glob("ab", "ab"));
    ATF_CHECK(!matches_glob("abc", "ab"));
    ATF_CHECK(!matches_glob("ab", "abc"));
}

ATF_TEST_CASE(matches_glob_star);
ATF_TEST_CASE_HEAD(matches_glob_star)
{
    set_md_var("descr", "Tests the matches_glob function by using the '*' "
               "meta-character as part of the pattern.");
}
ATF_TEST_CASE_BODY(matches_glob_star)
{
    using atf::expand::matches_glob;

    ATF_CHECK( matches_glob("*", ""));
    ATF_CHECK( matches_glob("*", "a"));
    ATF_CHECK( matches_glob("*", "ab"));

    ATF_CHECK(!matches_glob("a*", ""));
    ATF_CHECK( matches_glob("a*", "a"));
    ATF_CHECK( matches_glob("a*", "aa"));
    ATF_CHECK( matches_glob("a*", "ab"));
    ATF_CHECK( matches_glob("a*", "abc"));
    ATF_CHECK(!matches_glob("a*", "ba"));

    ATF_CHECK( matches_glob("*a", "a"));
    ATF_CHECK( matches_glob("*a", "ba"));
    ATF_CHECK(!matches_glob("*a", "bc"));
    ATF_CHECK(!matches_glob("*a", "bac"));

    ATF_CHECK( matches_glob("*ab", "ab"));
    ATF_CHECK( matches_glob("*ab", "aab"));
    ATF_CHECK( matches_glob("*ab", "aaab"));
    ATF_CHECK( matches_glob("*ab", "bab"));
    ATF_CHECK(!matches_glob("*ab", "bcb"));
    ATF_CHECK(!matches_glob("*ab", "bacb"));

    ATF_CHECK( matches_glob("a*b", "ab"));
    ATF_CHECK( matches_glob("a*b", "acb"));
    ATF_CHECK( matches_glob("a*b", "acdeb"));
    ATF_CHECK(!matches_glob("a*b", "acdebz"));
    ATF_CHECK(!matches_glob("a*b", "zacdeb"));
}

ATF_TEST_CASE(matches_glob_question);
ATF_TEST_CASE_HEAD(matches_glob_question)
{
    set_md_var("descr", "Tests the matches_glob function by using the '?' "
               "meta-character as part of the pattern.");
}
ATF_TEST_CASE_BODY(matches_glob_question)
{
    using atf::expand::matches_glob;

    ATF_CHECK(!matches_glob("?", ""));
    ATF_CHECK( matches_glob("?", "a"));
    ATF_CHECK(!matches_glob("?", "ab"));

    ATF_CHECK( matches_glob("?", "b"));
    ATF_CHECK( matches_glob("?", "c"));

    ATF_CHECK( matches_glob("a?", "ab"));
    ATF_CHECK( matches_glob("a?", "ac"));
    ATF_CHECK(!matches_glob("a?", "ca"));

    ATF_CHECK( matches_glob("???", "abc"));
    ATF_CHECK( matches_glob("???", "def"));
    ATF_CHECK(!matches_glob("???", "a"));
    ATF_CHECK(!matches_glob("???", "ab"));
    ATF_CHECK(!matches_glob("???", "abcd"));
}

ATF_TEST_CASE(expand_glob_base);
ATF_TEST_CASE_HEAD(expand_glob_base)
{
    set_md_var("descr", "Tests the expand_glob function with random "
               "patterns.");
}
ATF_TEST_CASE_BODY(expand_glob_base)
{
    using atf::expand::expand_glob;

    std::vector< std::string > candidates;
    candidates.push_back("foo");
    candidates.push_back("bar");
    candidates.push_back("baz");
    candidates.push_back("foobar");
    candidates.push_back("foobarbaz");
    candidates.push_back("foobarbazfoo");

    std::vector< std::string > exps;

    exps = expand_glob("foo", candidates);
    ATF_CHECK_EQUAL(exps.size(), 1);
    ATF_CHECK(exps[0] == "foo");

    exps = expand_glob("bar", candidates);
    ATF_CHECK_EQUAL(exps.size(), 1);
    ATF_CHECK(exps[0] == "bar");

    exps = expand_glob("foo*", candidates);
    ATF_CHECK_EQUAL(exps.size(), 4);
    ATF_CHECK(exps[0] == "foo");
    ATF_CHECK(exps[1] == "foobar");
    ATF_CHECK(exps[2] == "foobarbaz");
    ATF_CHECK(exps[3] == "foobarbazfoo");

    exps = expand_glob("*foo", candidates);
    ATF_CHECK_EQUAL(exps.size(), 2);
    ATF_CHECK(exps[0] == "foo");
    ATF_CHECK(exps[1] == "foobarbazfoo");

    exps = expand_glob("*foo*", candidates);
    ATF_CHECK_EQUAL(exps.size(), 4);
    ATF_CHECK(exps[0] == "foo");
    ATF_CHECK(exps[1] == "foobar");
    ATF_CHECK(exps[2] == "foobarbaz");
    ATF_CHECK(exps[3] == "foobarbazfoo");

    exps = expand_glob("ba", candidates);
    ATF_CHECK_EQUAL(exps.size(), 0);

    exps = expand_glob("ba*", candidates);
    ATF_CHECK_EQUAL(exps.size(), 2);
    ATF_CHECK(exps[0] == "bar");
    ATF_CHECK(exps[1] == "baz");

    exps = expand_glob("*ba", candidates);
    ATF_CHECK_EQUAL(exps.size(), 0);

    exps = expand_glob("*ba*", candidates);
    ATF_CHECK_EQUAL(exps.size(), 5);
    ATF_CHECK(exps[0] == "bar");
    ATF_CHECK(exps[1] == "baz");
    ATF_CHECK(exps[2] == "foobar");
    ATF_CHECK(exps[3] == "foobarbaz");
    ATF_CHECK(exps[4] == "foobarbazfoo");
}

ATF_TEST_CASE(expand_glob_tps);
ATF_TEST_CASE_HEAD(expand_glob_tps)
{
    set_md_var("descr", "Tests the expand_glob function with patterns that "
               "match typical test program names.  This is just a subcase "
               "of expand_base, but it is nice to make sure that it really "
               "works.");
}
ATF_TEST_CASE_BODY(expand_glob_tps)
{
    using atf::expand::expand_glob;

    std::vector< std::string > candidates;
    candidates.push_back("Atffile");
    candidates.push_back("h_foo");
    candidates.push_back("t_foo");
    candidates.push_back("t_bar");
    candidates.push_back("t_baz");
    candidates.push_back("foo_helper");
    candidates.push_back("foo_test");
    candidates.push_back("bar_test");
    candidates.push_back("baz_test");

    std::vector< std::string > exps;

    exps = expand_glob("t_*", candidates);
    ATF_CHECK_EQUAL(exps.size(), 3);
    ATF_CHECK(exps[0] == "t_foo");
    ATF_CHECK(exps[1] == "t_bar");
    ATF_CHECK(exps[2] == "t_baz");

    exps = expand_glob("*_test", candidates);
    ATF_CHECK_EQUAL(exps.size(), 3);
    ATF_CHECK(exps[0] == "foo_test");
    ATF_CHECK(exps[1] == "bar_test");
    ATF_CHECK(exps[2] == "baz_test");
}

ATF_INIT_TEST_CASES(tcs)
{
    // Add the tests for the free functions.
    ATF_ADD_TEST_CASE(tcs, is_glob);
    ATF_ADD_TEST_CASE(tcs, matches_glob_plain);
    ATF_ADD_TEST_CASE(tcs, matches_glob_star);
    ATF_ADD_TEST_CASE(tcs, matches_glob_question);
    ATF_ADD_TEST_CASE(tcs, expand_glob_base);
    ATF_ADD_TEST_CASE(tcs, expand_glob_tps);
}
