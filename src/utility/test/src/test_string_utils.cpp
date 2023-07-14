// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "string_utils.h"

#include <gtest/gtest.h>

#include <assert.h>
#include <stdio.h>

TEST(StringUtils, Lowercase)
{
    EXPECT_EQ(lcevc_dec::utility::lowercase(""), "");
    EXPECT_EQ(lcevc_dec::utility::lowercase("ABC"), "abc");
    EXPECT_EQ(lcevc_dec::utility::lowercase("xyz"), "xyz");
    EXPECT_EQ(lcevc_dec::utility::lowercase("abcABCabc"), "abcabcabc");
    EXPECT_EQ(lcevc_dec::utility::lowercase("abcdefghijklmnopqrstuvwxyz"),
              "abcdefghijklmnopqrstuvwxyz");
    EXPECT_EQ(lcevc_dec::utility::lowercase("ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
              "abcdefghijklmnopqrstuvwxyz");
    EXPECT_EQ(lcevc_dec::utility::lowercase("!$$%^FOO&*()_+BAR@~;:<bletch>,>/?{Test}[]"),
              "!$$%^foo&*()_+bar@~;:<bletch>,>/?{test}[]");
    EXPECT_EQ(lcevc_dec::utility::lowercase("\"XYZ\""), "\"xyz\"");
}

TEST(StringUtils, Uppercase)
{
    EXPECT_EQ(lcevc_dec::utility::uppercase(""), "");
    EXPECT_EQ(lcevc_dec::utility::uppercase("ABC"), "ABC");
    EXPECT_EQ(lcevc_dec::utility::uppercase("xyz"), "XYZ");
    EXPECT_EQ(lcevc_dec::utility::uppercase("ABCabcABC"), "ABCABCABC");
    EXPECT_EQ(lcevc_dec::utility::uppercase("ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    EXPECT_EQ(lcevc_dec::utility::uppercase("abcdefghijklmnopqrstuvwxyz"),
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    EXPECT_EQ(lcevc_dec::utility::uppercase("!$$%^foo&*()_+bar@~;:<BLETCH>,>/?{tEST}[]"),
              "!$$%^FOO&*()_+BAR@~;:<BLETCH>,>/?{TEST}[]");
    EXPECT_EQ(lcevc_dec::utility::uppercase("\"xyz\""), "\"XYZ\"");
}

std::vector<std::string> strings(std::vector<std::string> strs) { return strs; }

TEST(StringUtils, Split)
{
    EXPECT_EQ(lcevc_dec::utility::split("a,b", ","), strings({"a", "b"}));
    EXPECT_EQ(lcevc_dec::utility::split("", ","), strings({}));
    EXPECT_EQ(lcevc_dec::utility::split(".", ",_."), strings({"", ""}));
    EXPECT_EQ(lcevc_dec::utility::split("______", ",_."), strings({"", ""}));
    EXPECT_EQ(lcevc_dec::utility::split("a.b", ",_."), strings({"a", "b"}));
    EXPECT_EQ(lcevc_dec::utility::split("alpha.bravo,charlie", ",_."),
              strings({"alpha", "bravo", "charlie"}));
    EXPECT_EQ(lcevc_dec::utility::split("alpha_bravo,charlie.delta", ",_."),
              strings({"alpha", "bravo", "charlie", "delta"}));
    EXPECT_EQ(lcevc_dec::utility::split("alpha_.,bravo,_charlie.....delta", ",_."),
              strings({"alpha", "bravo", "charlie", "delta"}));
    EXPECT_EQ(lcevc_dec::utility::split(",alpha_.,bravo,_charlie.delta,foxtrot", ",_."),
              strings({"", "alpha", "bravo", "charlie", "delta", "foxtrot"}));
    EXPECT_EQ(lcevc_dec::utility::split("alpha_.,bravo,_charlie.....delta_", ",_."),
              strings({"alpha", "bravo", "charlie", "delta", ""}));
    EXPECT_EQ(lcevc_dec::utility::split(",alpha_.,bravo,_charlie.....delta_", ",_."),
              strings({"", "alpha", "bravo", "charlie", "delta", ""}));
}
