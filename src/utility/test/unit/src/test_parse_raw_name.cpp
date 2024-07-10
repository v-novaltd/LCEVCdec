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

#include "parse_raw_name.h"

#include <gtest/gtest.h>
#include <LCEVC/lcevc_dec.h>

using namespace lcevc_dec::utility;

static bool checkDesc(const LCEVC_PictureDesc& desc, LCEVC_ColorFormat colorFormat, uint32_t width,
                      uint32_t height)
{
    return desc.width == width && desc.height == height && desc.colorFormat == colorFormat;
}

TEST(ParseRawName, Test)
{
    // To summarise: "rgb[a]" in any order is fine, as is just "y". If not specified, yuv is
    // assumed to be 420
    float rate = 0.0f;

    EXPECT_TRUE(checkDesc(parseRawName("", rate), LCEVC_ColorFormat_Unknown, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("rgba", rate), LCEVC_RGBA_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("rgbaa", rate), LCEVC_ColorFormat_Unknown, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("RGB", rate), LCEVC_RGB_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo-Y", rate), LCEVC_GRAY_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("xxx_AbgR", rate), LCEVC_ABGR_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("c:\\foo\\xxx.ARGB", rate), LCEVC_ARGB_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("/foo/f____/xxx.BGR", rate), LCEVC_BGR_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("______________________________,,,,,,,,,,,,,,,,,,,,,,,,,,,,."
                                       "........................rgb",
                                       rate),
                          LCEVC_RGB_8, 0, 0) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("\\|<>\"\'@+_-)(*&^%$!#?/~][{}=-.rgb", rate), LCEVC_RGB_8, 0, 0) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("test_1920x1080.yuv", rate), LCEVC_I420_8, 1920, 1080) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("640x480.yuv", rate), LCEVC_I420_8, 640, 480) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("****_!.yuv", rate), LCEVC_I420_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo_420p_10bit.yuv", rate), LCEVC_I420_10_LE, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo_p420_100x120_10bit.yuv", rate), LCEVC_I420_10_LE, 100, 120) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo_420p_10bpp_450x300.yuv", rate), LCEVC_I420_10_LE, 450, 300) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo_70000x10_420p_12bpp.yuv", rate), LCEVC_I420_12_LE, 70000, 10) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo_420p_14bits.yuv", rate), LCEVC_I420_14_LE, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo_420_16bits.yuv", rate), LCEVC_I420_16_LE, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("$$$$$$_8bpp.p420", rate), LCEVC_I420_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo_10bpp.nv12", rate), LCEVC_ColorFormat_Unknown, 0, 0) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bar_800x600_8bpp.nv12", rate), LCEVC_NV12_8, 800, 600) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bar_1800x1600_8bpp.nv21", rate), LCEVC_NV21_8, 1800, 1600) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bar_8000x6000.nv12", rate), LCEVC_NV12_8, 8000, 6000) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bar_6x8.nv21", rate), LCEVC_NV21_8, 6, 8) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bletch.y", rate), LCEVC_GRAY_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bletch_8bit_1000x2000.y", rate), LCEVC_GRAY_8, 1000, 2000) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bletch_12bit_1x1.y", rate), LCEVC_GRAY_12_LE, 1, 1) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bletch_1920x1080_14bpp.y", rate), LCEVC_GRAY_14_LE, 1920, 1080) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bletch_16bit.y", rate), LCEVC_GRAY_16_LE, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("bletch_13bit.y", rate), LCEVC_ColorFormat_Unknown, 0, 0) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("xyzzy.rgb", rate), LCEVC_RGB_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("xyzzy_720x576_8bit.rgb", rate), LCEVC_RGB_8, 720, 576) &&
                rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("xyzzy_8bpp_90x50_.rgb", rate), LCEVC_RGB_8, 90, 50) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("xyzzy_8bit.rgb", rate), LCEVC_RGB_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("8bit.bgr", rate), LCEVC_BGR_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo.bgra", rate), LCEVC_BGRA_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo.abgr", rate), LCEVC_ABGR_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo.argb", rate), LCEVC_ARGB_8, 0, 0) && rate == 0.0f);
    EXPECT_TRUE(checkDesc(parseRawName("foo.rgba", rate), LCEVC_RGBA_8, 0, 0) && rate == 0.0f);
}
