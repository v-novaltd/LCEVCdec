/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#include "LCEVC/utility/configure.h"
#include "lcevc_config.h"
#include "mock_api.h"

#include <gtest/gtest.h>

#include <sstream>

// Windows linker does not like __declspec(dllimport) for statically linked symbols, so don't
// test this on Windows until we have proper API logging, or use linker .def file rather than
// attributes to specify imports.
//
#if !VN_OS(WINDOWS)

using namespace lcevc_dec::utility;

TEST(Configure, All)
{
    LCEVC_DecoderHandle dh{1554};
    std::ostringstream os;

    mockApiSetStream(&os);
    EXPECT_EQ(configureDecoderFromJson(dh, ""), LCEVC_Error);
    EXPECT_EQ(os.str(), "");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{}"), LCEVC_Success);
    EXPECT_EQ(os.str(), "");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"bbbb\":true}"), LCEVC_Success);
    EXPECT_EQ(os.str(), "LCEVC_ConfigureDecoderBool(1554, \"bbbb\", true)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"ccccc\":false}"), LCEVC_Success);
    EXPECT_EQ(os.str(), "LCEVC_ConfigureDecoderBool(1554, \"ccccc\", false)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"foo\":42}"), LCEVC_Success);
    EXPECT_EQ(os.str(), "LCEVC_ConfigureDecoderInt(1554, \"foo\", 42)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"bar\":34.0}"), LCEVC_Success);
    EXPECT_EQ(os.str(), "LCEVC_ConfigureDecoderFloat(1554, \"bar\", 34)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"bletch\":137.2}"), LCEVC_Success);
    EXPECT_EQ(os.str(), "LCEVC_ConfigureDecoderFloat(1554, \"bletch\", 137.2)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"burble\":\"string\"}"), LCEVC_Success);
    EXPECT_EQ(os.str(), "LCEVC_ConfigureDecoderString(1554, \"burble\", \"string\")\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"ba\":[true,false,true,false]}"), LCEVC_Success);
    EXPECT_EQ(os.str(),
              "LCEVC_ConfigureDecoderBoolArray(1554, \"ba\", true, false, true, false)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"ia\":[102, 3132, 2147483647, 0, -23890472, -1]}"),
              LCEVC_Success);
    EXPECT_EQ(
        os.str(),
        "LCEVC_ConfigureDecoderIntArray(1554, \"ia\", 102, 3132, 2147483647, 0, -23890472, -1)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"farr\":[34.10, -20.1, 34.76, 48.1, 22.0, -100.1]}"),
              LCEVC_Success);
    EXPECT_EQ(
        os.str(),
        "LCEVC_ConfigureDecoderFloatArray(1554, \"farr\", 34.1, -20.1, 34.76, 48.1, 22, -100.1)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(
                  dh, "{\"strs\":[\"alpha\", \"bravo\", \"charlie\", \"delta\", \"foxtrot\"]}"),
              LCEVC_Success);
    EXPECT_EQ(os.str(), "LCEVC_ConfigureDecoderStringArray(1554, \"strs\", alpha, bravo, charlie, "
                        "delta, foxtrot)\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(dh, "{\"xx\":1234345, "
                                           " \"b1\":true, "
                                           " \"yyy\":324.0, "
                                           " \"b2\":false, "
                                           " \"yadayayd\":42.2, "
                                           " \"nnnn\":\"aaaaaaaaaaa\\\"aaaaaaaaaaa\"}"),
              LCEVC_Success);
    EXPECT_EQ(os.str(),
              "LCEVC_ConfigureDecoderInt(1554, \"xx\", 1234345)\n"
              "LCEVC_ConfigureDecoderBool(1554, \"b1\", true)\n"
              "LCEVC_ConfigureDecoderFloat(1554, \"yyy\", 324)\n"
              "LCEVC_ConfigureDecoderBool(1554, \"b2\", false)\n"
              "LCEVC_ConfigureDecoderFloat(1554, \"yadayayd\", 42.2)\n"
              "LCEVC_ConfigureDecoderString(1554, \"nnnn\", \"aaaaaaaaaaa\"aaaaaaaaaaa\")\n");
    os.str("");

    EXPECT_EQ(configureDecoderFromJson(
                  dh, "{\"xx\":1234345, "
                      " \"barray\":[true], "
                      " \"b1\":true, "
                      " \"barray2\":[true,false,false,true], "
                      " \"yyy\":324.0, "
                      " \"b2\":false, "
                      " \"ints\":[10,9,8,7,6,5,4,3,3,2,1], "
                      " \"yadayayd\":42.2, "
                      " \"floats\":[10.0,11.1, 9087.3, 78786.2,89.0,0.0,3.14], "
                      " \"nnnn\":\"aaaaaaaaaaaaaaaaaaaaaa\","
                      " \"names\":[\"tango\", \"whisky\", \"uniform\", \"november\", \"tango\"] "
                      "}"),
              LCEVC_Success);
    EXPECT_EQ(
        os.str(),
        "LCEVC_ConfigureDecoderInt(1554, \"xx\", 1234345)\n"
        "LCEVC_ConfigureDecoderBoolArray(1554, \"barray\", true)\n"
        "LCEVC_ConfigureDecoderBool(1554, \"b1\", true)\n"
        "LCEVC_ConfigureDecoderBoolArray(1554, \"barray2\", true, false, false, true)\n"
        "LCEVC_ConfigureDecoderFloat(1554, \"yyy\", 324)\n"
        "LCEVC_ConfigureDecoderBool(1554, \"b2\", false)\n"
        "LCEVC_ConfigureDecoderIntArray(1554, \"ints\", 10, 9, 8, 7, 6, 5, 4, 3, 3, 2, 1)\n"
        "LCEVC_ConfigureDecoderFloat(1554, \"yadayayd\", 42.2)\n"
        "LCEVC_ConfigureDecoderFloatArray(1554, \"floats\", 10, 11.1, 9087.3, 78786.2, 89, 0, "
        "3.14)\n"
        "LCEVC_ConfigureDecoderString(1554, \"nnnn\", \"aaaaaaaaaaaaaaaaaaaaaa\")\n"
        "LCEVC_ConfigureDecoderStringArray(1554, \"names\", tango, whisky, uniform, november, "
        "tango)\n");
    os.str("");
}

#endif
