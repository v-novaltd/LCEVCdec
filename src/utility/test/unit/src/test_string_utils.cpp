/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "LCEVC/utility/string_utils.h"

#include <assert.h>
#include <gtest/gtest.h>
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
