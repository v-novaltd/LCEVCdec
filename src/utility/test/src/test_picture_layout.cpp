// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/picture_layout.h"

#include <gtest/gtest.h>

TEST(PictureLayout, I420_8_1920x1080)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_I420_8, 1920, 1080), LCEVC_Success);
    EXPECT_EQ(pd.colorFormat, LCEVC_I420_8);
    EXPECT_EQ(pd.width, 1920);
    EXPECT_EQ(pd.height, 1080);

    lcevc_dec::utility::PictureLayout pl(pd);

    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.format(), LCEVC_I420_8);
    EXPECT_EQ(pl.width(), 1920);
    EXPECT_EQ(pl.height(), 1080);
    EXPECT_EQ(pl.width(0), 1920);
    EXPECT_EQ(pl.width(1), 960);
    EXPECT_EQ(pl.height(1), 540);
    EXPECT_EQ(pl.height(2), 540);

    EXPECT_EQ(pl.sampleSize(), 1);
    EXPECT_EQ(pl.sampleBits(), 8);

    EXPECT_EQ(pl.planes(), 3);
    EXPECT_EQ(pl.rowSize(0), 1920);
    EXPECT_EQ(pl.rowSize(1), 960);
    EXPECT_EQ(pl.rowSize(2), 960);

    EXPECT_EQ(pl.rowStride(0), 1920);
    EXPECT_EQ(pl.rowStride(1), 960);
    EXPECT_EQ(pl.rowStride(2), 960);

    EXPECT_EQ(pl.planeOffset(0), 0);
    EXPECT_EQ(pl.planeOffset(1), 2073600);
    EXPECT_EQ(pl.planeOffset(2), 2592000);

    EXPECT_EQ(pl.planeSize(0), 2073600);
    EXPECT_EQ(pl.planeSize(1), 518400);
    EXPECT_EQ(pl.planeSize(2), 518400);

    EXPECT_EQ(pl.rowOffset(0, 100), 192000);
    EXPECT_EQ(pl.rowOffset(1, 150), 2217600);
    EXPECT_EQ(pl.rowOffset(2, 150), 2736000);

    EXPECT_EQ(pl.size(), 3110400);
    EXPECT_EQ(pl.makeRawFilename("BASENAME"), "BASENAME_1920x1080_p420.yuv");
}

TEST(PictureLayout, I420_10_LE_512x512)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_I420_10_LE, 512, 256), LCEVC_Success);
    EXPECT_EQ(pd.colorFormat, LCEVC_I420_10_LE);
    EXPECT_EQ(pd.width, 512);
    EXPECT_EQ(pd.height, 256);

    lcevc_dec::utility::PictureLayout pl(pd);

    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.format(), LCEVC_I420_10_LE);
    EXPECT_EQ(pl.width(), 512);
    EXPECT_EQ(pl.height(), 256);
    EXPECT_EQ(pl.width(0), 512);
    EXPECT_EQ(pl.width(1), 256);
    EXPECT_EQ(pl.height(1), 128);
    EXPECT_EQ(pl.height(2), 128);

    EXPECT_EQ(pl.sampleSize(), 2);
    EXPECT_EQ(pl.sampleBits(), 10);

    EXPECT_EQ(pl.planes(), 3);
    EXPECT_EQ(pl.rowSize(0), 1024);
    EXPECT_EQ(pl.rowSize(1), 512);
    EXPECT_EQ(pl.rowSize(2), 512);

    EXPECT_EQ(pl.rowStride(0), 1024);
    EXPECT_EQ(pl.rowStride(1), 512);
    EXPECT_EQ(pl.rowStride(2), 512);

    EXPECT_EQ(pl.planeOffset(0), 0);
    EXPECT_EQ(pl.planeOffset(1), 262144);
    EXPECT_EQ(pl.planeOffset(2), 327680);

    EXPECT_EQ(pl.planeSize(0), 262144);
    EXPECT_EQ(pl.planeSize(1), 65536);
    EXPECT_EQ(pl.planeSize(2), 65536);

    EXPECT_EQ(pl.rowOffset(0, 100), 102400);
    EXPECT_EQ(pl.rowOffset(1, 150), 338944);
    EXPECT_EQ(pl.rowOffset(2, 150), 404480);

    EXPECT_EQ(pl.size(), 393216);
    EXPECT_EQ(pl.makeRawFilename("BASENAME"), "BASENAME_512x256_10bit_p420.yuv");
}

TEST(PictureLayout, NV12_720x576)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_NV12_8, 720, 576), LCEVC_Success);
    EXPECT_EQ(pd.colorFormat, LCEVC_NV12_8);
    EXPECT_EQ(pd.width, 720);
    EXPECT_EQ(pd.height, 576);

    lcevc_dec::utility::PictureLayout pl(pd);

    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.format(), LCEVC_NV12_8);
    EXPECT_EQ(pl.width(), 720);
    EXPECT_EQ(pl.height(), 576);
    EXPECT_EQ(pl.width(0), 720);
    EXPECT_EQ(pl.width(1), 360);
    EXPECT_EQ(pl.height(1), 288);
    EXPECT_EQ(pl.height(2), 288);

    EXPECT_EQ(pl.sampleSize(), 1);
    EXPECT_EQ(pl.sampleBits(), 8);

    EXPECT_EQ(pl.planes(), 3);
    EXPECT_EQ(pl.rowSize(0), 720);
    EXPECT_EQ(pl.rowSize(1), 720);
    EXPECT_EQ(pl.rowSize(2), 720);

    EXPECT_EQ(pl.rowStride(0), 720);
    EXPECT_EQ(pl.rowStride(1), 720);
    EXPECT_EQ(pl.rowStride(2), 720);

    EXPECT_EQ(pl.planeOffset(0), 0);
    EXPECT_EQ(pl.planeOffset(1), 414720);
    EXPECT_EQ(pl.planeOffset(2), 414721);

    EXPECT_EQ(pl.planeSize(0), 414720);
    EXPECT_EQ(pl.planeSize(1), 207360);
    EXPECT_EQ(pl.planeSize(2), 207360);

    EXPECT_EQ(pl.rowOffset(0, 100), 72000);
    EXPECT_EQ(pl.rowOffset(1, 350), 666720);
    EXPECT_EQ(pl.rowOffset(2, 350), 666721);

    EXPECT_EQ(pl.size(), 622080);
}

TEST(PictureLayout, NV21_720x576)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_NV21_8, 720, 576), LCEVC_Success);
    EXPECT_EQ(pd.colorFormat, LCEVC_NV21_8);
    EXPECT_EQ(pd.width, 720);
    EXPECT_EQ(pd.height, 576);

    lcevc_dec::utility::PictureLayout pl(pd);
    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.planeOffset(0), 0);
    EXPECT_EQ(pl.planeOffset(1), 414721);
    EXPECT_EQ(pl.planeOffset(2), 414720);

    EXPECT_EQ(pl.planeSize(0), 414720);
    EXPECT_EQ(pl.planeSize(1), 207360);
    EXPECT_EQ(pl.planeSize(2), 207360);

    EXPECT_EQ(pl.rowOffset(0, 100), 72000);
    EXPECT_EQ(pl.rowOffset(1, 350), 666721);
    EXPECT_EQ(pl.rowOffset(2, 350), 666720);

    EXPECT_EQ(pl.size(), 622080);
    EXPECT_EQ(pl.makeRawFilename("xyzzy"), "xyzzy_720x576.nv21");
}

TEST(PictureLayout, RGB_8_3840x2160)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_RGB_8, 3840, 2160), LCEVC_Success);
    EXPECT_EQ(pd.colorFormat, LCEVC_RGB_8);
    EXPECT_EQ(pd.width, 3840);
    EXPECT_EQ(pd.height, 2160);

    lcevc_dec::utility::PictureLayout pl(pd);
    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.planeOffset(0), 0);
    EXPECT_EQ(pl.planeOffset(1), 1);
    EXPECT_EQ(pl.planeOffset(2), 2);

    EXPECT_EQ(pl.planeSize(0), 24883200);
    EXPECT_EQ(pl.planeSize(1), 24883200);
    EXPECT_EQ(pl.planeSize(2), 24883200);

    EXPECT_EQ(pl.rowOffset(0, 100), 1152000);
    EXPECT_EQ(pl.rowOffset(1, 350), 4032001);
    EXPECT_EQ(pl.rowOffset(2, 350), 4032002);

    EXPECT_EQ(pl.size(), 24883200);
    EXPECT_EQ(pl.makeRawFilename("testing"), "testing_3840x2160.rgb");
}

TEST(PictureLayout, RGBA_8_3840x2160)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_RGBA_8, 3840, 2160), LCEVC_Success);
    EXPECT_EQ(pd.colorFormat, LCEVC_RGBA_8);
    EXPECT_EQ(pd.width, 3840);
    EXPECT_EQ(pd.height, 2160);

    lcevc_dec::utility::PictureLayout pl(pd);
    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.planeOffset(0), 0);
    EXPECT_EQ(pl.planeOffset(1), 1);
    EXPECT_EQ(pl.planeOffset(2), 2);

    EXPECT_EQ(pl.planeSize(0), 33177600);
    EXPECT_EQ(pl.planeSize(1), 33177600);
    EXPECT_EQ(pl.planeSize(2), 33177600);

    EXPECT_EQ(pl.rowOffset(0, 100), 1536000);
    EXPECT_EQ(pl.rowOffset(1, 350), 5376001);
    EXPECT_EQ(pl.rowOffset(2, 350), 5376002);

    EXPECT_EQ(pl.size(), 33177600);
    EXPECT_EQ(pl.makeRawFilename("testing"), "testing_3840x2160.rgba");
}

TEST(PictureLayout, GRAY_16_640x480)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_GRAY_16_LE, 640, 480), LCEVC_Success);
    EXPECT_EQ(pd.colorFormat, LCEVC_GRAY_16_LE);
    EXPECT_EQ(pd.width, 640);
    EXPECT_EQ(pd.height, 480);

    lcevc_dec::utility::PictureLayout pl(pd);
    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.planeOffset(0), 0);

    EXPECT_EQ(pl.planeSize(0), 614400);

    EXPECT_EQ(pl.rowOffset(0, 100), 128000);

    EXPECT_EQ(pl.size(), 614400);
    EXPECT_EQ(pl.makeRawFilename("base"), "base_640x480_16bit.y");
}

TEST(PictureLayout, IsValid1)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_I420_8, 721, 576), LCEVC_Success);

    lcevc_dec::utility::PictureLayout pl(pd);
    EXPECT_FALSE(pl.isValid());
}
TEST(PictureLayout, IsValid2)
{
    LCEVC_PictureDesc pd{};
    EXPECT_EQ(LCEVC_DefaultPictureDesc(&pd, LCEVC_I420_8, 720, 577), LCEVC_Success);

    lcevc_dec::utility::PictureLayout pl(pd);
    EXPECT_FALSE(pl.isValid());
}

TEST(PictureLayout, IsCompatible_YUV)
{
    lcevc_dec::utility::PictureLayout pl1(LCEVC_I420_8, 720, 576);
    EXPECT_TRUE(pl1.isValid());

    lcevc_dec::utility::PictureLayout pl2(LCEVC_I420_8, 720, 576);
    EXPECT_TRUE(pl2.isValid());
    EXPECT_TRUE(pl1.isCompatible(pl2));
    EXPECT_TRUE(pl2.isCompatible(pl1));

    lcevc_dec::utility::PictureLayout pl3(LCEVC_I420_8, 730, 576);
    EXPECT_TRUE(pl3.isValid());
    EXPECT_FALSE(pl3.isCompatible(pl1));

    lcevc_dec::utility::PictureLayout pl4(LCEVC_I420_8, 730, 578);
    EXPECT_TRUE(pl4.isValid());
    EXPECT_FALSE(pl4.isCompatible(pl1));

    lcevc_dec::utility::PictureLayout pl5(LCEVC_NV12_8, 720, 576);
    EXPECT_TRUE(pl5.isValid());
    EXPECT_TRUE(pl5.isCompatible(pl1));

    lcevc_dec::utility::PictureLayout pl6(LCEVC_NV21_8, 720, 576);
    EXPECT_TRUE(pl6.isValid());
    EXPECT_TRUE(pl6.isCompatible(pl5));

    lcevc_dec::utility::PictureLayout pl7(LCEVC_I420_10_LE, 720, 576);
    EXPECT_TRUE(pl7.isValid());
    EXPECT_FALSE(pl7.isCompatible(pl1));

    lcevc_dec::utility::PictureLayout pl8(LCEVC_I420_12_LE, 720, 576);
    EXPECT_TRUE(pl8.isValid());
    EXPECT_FALSE(pl8.isCompatible(pl7));
}

TEST(PictureLayout, IsCompatible_RGBA)
{
    lcevc_dec::utility::PictureLayout pl1(LCEVC_RGBA_8, 1024, 768);
    EXPECT_TRUE(pl1.isValid());

    lcevc_dec::utility::PictureLayout pl2(LCEVC_BGRA_8, 1024, 768);
    EXPECT_TRUE(pl2.isValid());
    EXPECT_TRUE(pl1.isCompatible(pl2));
    EXPECT_TRUE(pl2.isCompatible(pl1));

    lcevc_dec::utility::PictureLayout pl3(LCEVC_ABGR_8, 1024, 768);
    EXPECT_TRUE(pl3.isValid());
    EXPECT_TRUE(pl1.isCompatible(pl3));

    lcevc_dec::utility::PictureLayout pl4(LCEVC_ARGB_8, 1024, 768);
    EXPECT_TRUE(pl4.isValid());
    EXPECT_TRUE(pl1.isCompatible(pl4));
}
