/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <LCEVC/common/cpp_tools.h>

TEST(CppTools, Concat)
{
#define FirstSecond 42
    int x = VNConcat(First, Second);
    EXPECT_EQ(x, 42);
}

TEST(CppTools, NumArgs)
{
    EXPECT_EQ(VNNumArgs(a), 1);
    EXPECT_EQ(VNNumArgs(a, b), 2);
    EXPECT_EQ(VNNumArgs(a, b, c), 3);
    EXPECT_EQ(VNNumArgs(a, b, c, d), 4);
    EXPECT_EQ(VNNumArgs(a, b, c, d, e), 5);
    EXPECT_EQ(VNNumArgs(a, b, c, d, e, f), 6);
    EXPECT_EQ(VNNumArgs(a, b, c, d, e, f, g), 7);
    EXPECT_EQ(VNNumArgs(a, b, c, d, e, f, g, h), 8);
    EXPECT_EQ(VNNumArgs(a, b, c, d, e, f, g, h, i), 9);
    EXPECT_EQ(VNNumArgs(a, b, c, d, e, f, g, h, i, j), 10);
}

#define OP(idx, arg) "[" #arg "]"

TEST(CppTools, ForEach)
{
    const std::string s1 = VNForEach(OP, A);
    EXPECT_EQ(s1, "[A]");

    const std::string s2 = VNForEach(OP, A, B);
    EXPECT_EQ(s2, "[A][B]");

    const std::string s3 = VNForEach(OP, A, B, C);
    EXPECT_EQ(s3, "[A][B][C]");

    const std::string s4 = VNForEach(OP, A, B, C, D);
    EXPECT_EQ(s4, "[A][B][C][D]");

    const std::string s5 = VNForEach(OP, A, B, C, D, E);
    EXPECT_EQ(s5, "[A][B][C][D][E]");

    const std::string s6 = VNForEach(OP, A, B, C, D, E, F);
    EXPECT_EQ(s6, "[A][B][C][D][E][F]");

    const std::string s7 = VNForEach(OP, A, B, C, D, E, F, G);
    EXPECT_EQ(s7, "[A][B][C][D][E][F][G]");

    const std::string s8 = VNForEach(OP, A, B, C, D, E, F, G, H);
    EXPECT_EQ(s8, "[A][B][C][D][E][F][G][H]");

    const std::string s9 = VNForEach(OP, A, B, C, D, E, F, G, H, I);
    EXPECT_EQ(s9, "[A][B][C][D][E][F][G][H][I]");

    const std::string s10 = VNForEach(OP, A, B, C, D, E, F, G, H, I, J);
    EXPECT_EQ(s10, "[A][B][C][D][E][F][G][H][I][J]");
}

#define OP(idx, arg) "[" #arg "]"
#define SEP() ","

TEST(CppTools, ForEachSeperated)
{
    const std::string s1 = VNForEachSeperated(OP, SEP, A);
    EXPECT_EQ(s1, "[A]");

    const std::string s2 = VNForEachSeperated(OP, SEP, A, B);
    EXPECT_EQ(s2, "[A],[B]");

    const std::string s3 = VNForEachSeperated(OP, SEP, A, B, C);
    EXPECT_EQ(s3, "[A],[B],[C]");

    const std::string s4 = VNForEachSeperated(OP, SEP, A, B, C, D);
    EXPECT_EQ(s4, "[A],[B],[C],[D]");

    const std::string s5 = VNForEachSeperated(OP, SEP, A, B, C, D, E);
    EXPECT_EQ(s5, "[A],[B],[C],[D],[E]");

    const std::string s6 = VNForEachSeperated(OP, SEP, A, B, C, D, E, F);
    EXPECT_EQ(s6, "[A],[B],[C],[D],[E],[F]");

    const std::string s7 = VNForEachSeperated(OP, SEP, A, B, C, D, E, F, G);
    EXPECT_EQ(s7, "[A],[B],[C],[D],[E],[F],[G]");

    const std::string s8 = VNForEachSeperated(OP, SEP, A, B, C, D, E, F, G, H);
    EXPECT_EQ(s8, "[A],[B],[C],[D],[E],[F],[G],[H]");

    const std::string s9 = VNForEachSeperated(OP, SEP, A, B, C, D, E, F, G, H, I);
    EXPECT_EQ(s9, "[A],[B],[C],[D],[E],[F],[G],[H],[I]");

    const std::string s10 = VNForEachSeperated(OP, SEP, A, B, C, D, E, F, G, H, I, J);
    EXPECT_EQ(s10, "[A],[B],[C],[D],[E],[F],[G],[H],[I],[J]");

    //    VNForEachSeperated(op, ...)
}

#define OP1(idx, arg) arg
#define SEP1() ,

TEST(CppTools, ForEachSeperatedComma)
{
    constexpr int a[] = {VNForEachSeperated(OP1, SEP1, 10, 20)};
    EXPECT_EQ(sizeof(a) / sizeof(a[0]), 2);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 20);
}
