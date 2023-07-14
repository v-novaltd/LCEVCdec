// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/types.h"
#include "LCEVC/utility/types_cli11.h"

#include <gtest/gtest.h>
#include <fmt/core.h>
#include <CLI/CLI.hpp>

#include <sstream>
#include <string_view>
#include <array>

using namespace lcevc_dec::utility;

TEST(Convert, ColorFormat)
{
    LCEVC_ColorFormat fmt{LCEVC_ColorFormat_Unknown};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(fmt) {
    case LCEVC_ColorFormat_Unknown:
    case LCEVC_I420_8:
    case LCEVC_I420_10_LE:
    case LCEVC_I420_12_LE:
    case LCEVC_I420_14_LE:
    case LCEVC_I420_16_LE:
    case LCEVC_YUV420_RASTER_8:
    case LCEVC_NV12_8:
    case LCEVC_NV21_8:
    case LCEVC_RGB_8:
    case LCEVC_BGR_8:
    case LCEVC_RGBA_8:
    case LCEVC_BGRA_8:
    case LCEVC_ARGB_8:
    case LCEVC_ABGR_8:
    case LCEVC_RGBA_10_2_LE:
    case LCEVC_GRAY_8:
    case LCEVC_GRAY_10_LE:
    case LCEVC_GRAY_12_LE:
    case LCEVC_GRAY_14_LE:
    case LCEVC_GRAY_16_LE:

    case LCEVC_ColorFormat_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("I420_8", fmt) && (fmt == LCEVC_I420_8));
    EXPECT_TRUE(fromString("I420_10_LE", fmt) && (fmt == LCEVC_I420_10_LE));
    EXPECT_TRUE(fromString("I420_12_LE", fmt) && (fmt == LCEVC_I420_12_LE));
    EXPECT_TRUE(fromString("I420_14_LE", fmt) && (fmt == LCEVC_I420_14_LE));
    EXPECT_TRUE(fromString("I420_16_LE", fmt) && (fmt == LCEVC_I420_16_LE));
    EXPECT_TRUE(fromString("YUV420_RASTER_8", fmt) && (fmt == LCEVC_YUV420_RASTER_8));
    EXPECT_TRUE(fromString("NV12_8", fmt) && (fmt == LCEVC_NV12_8));
    EXPECT_TRUE(fromString("NV21_8", fmt) && (fmt == LCEVC_NV21_8));
    EXPECT_TRUE(fromString("RGB_8", fmt) && (fmt == LCEVC_RGB_8));
    EXPECT_TRUE(fromString("BGR_8", fmt) && (fmt == LCEVC_BGR_8));
    EXPECT_TRUE(fromString("RGBA_8", fmt) && (fmt == LCEVC_RGBA_8));
    EXPECT_TRUE(fromString("BGRA_8", fmt) && (fmt == LCEVC_BGRA_8));
    EXPECT_TRUE(fromString("ARGB_8", fmt) && (fmt == LCEVC_ARGB_8));
    EXPECT_TRUE(fromString("ABGR_8", fmt) && (fmt == LCEVC_ABGR_8));
    EXPECT_TRUE(fromString("RGBA_10_2_LE", fmt) && (fmt == LCEVC_RGBA_10_2_LE));
    EXPECT_TRUE(fromString("GRAY_8", fmt) && (fmt == LCEVC_GRAY_8));
    EXPECT_TRUE(fromString("GRAY_10_LE", fmt) && (fmt == LCEVC_GRAY_10_LE));
    EXPECT_TRUE(fromString("GRAY_12_LE", fmt) && (fmt == LCEVC_GRAY_12_LE));
    EXPECT_TRUE(fromString("GRAY_14_LE", fmt) && (fmt == LCEVC_GRAY_14_LE));
    EXPECT_TRUE(fromString("GRAY_16_LE", fmt) && (fmt == LCEVC_GRAY_16_LE));
    EXPECT_TRUE(fromString("P420", fmt) && (fmt == LCEVC_I420_8));
    EXPECT_TRUE(fromString("NV12", fmt) && (fmt == LCEVC_NV12_8));
    EXPECT_TRUE(fromString("NV21", fmt) && (fmt == LCEVC_NV21_8));
    EXPECT_TRUE(fromString("RGB", fmt) && (fmt == LCEVC_RGB_8));
    EXPECT_TRUE(fromString("BGR", fmt) && (fmt == LCEVC_BGR_8));
    EXPECT_TRUE(fromString("RGBA", fmt) && (fmt == LCEVC_RGBA_8));
    EXPECT_TRUE(fromString("BGRA", fmt) && (fmt == LCEVC_BGRA_8));
    EXPECT_TRUE(fromString("ARGB", fmt) && (fmt == LCEVC_ARGB_8));
    EXPECT_TRUE(fromString("ABGR", fmt) && (fmt == LCEVC_ABGR_8));
    EXPECT_TRUE(fromString("GRAY", fmt) && (fmt == LCEVC_GRAY_8));
    EXPECT_TRUE(fromString("I420_10", fmt) && (fmt == LCEVC_I420_10_LE));
    EXPECT_TRUE(fromString("I420_12", fmt) && (fmt == LCEVC_I420_12_LE));
    EXPECT_TRUE(fromString("I420_14", fmt) && (fmt == LCEVC_I420_14_LE));
    EXPECT_TRUE(fromString("I420_16", fmt) && (fmt == LCEVC_I420_16_LE));
    EXPECT_TRUE(fromString("RGBA_10", fmt) && (fmt == LCEVC_RGBA_10_2_LE));
    EXPECT_TRUE(fromString("GRAY_10", fmt) && (fmt == LCEVC_GRAY_10_LE));
    EXPECT_TRUE(fromString("GRAY_12", fmt) && (fmt == LCEVC_GRAY_12_LE));
    EXPECT_TRUE(fromString("GRAY_14", fmt) && (fmt == LCEVC_GRAY_14_LE));
    EXPECT_TRUE(fromString("GRAY_16", fmt) && (fmt == LCEVC_GRAY_16_LE));
}
TEST(Convert, ColorFormatSynonyms)
{
    LCEVC_ColorFormat fmt{LCEVC_ColorFormat_Unknown};

    EXPECT_TRUE(fromString("yuv420p", fmt) && (fmt == LCEVC_I420_8));
    EXPECT_TRUE(fromString("yuv420p10le", fmt) && (fmt == LCEVC_I420_10_LE));
    EXPECT_TRUE(fromString("yuv420p12le", fmt) && (fmt == LCEVC_I420_12_LE));
    EXPECT_TRUE(fromString("yuv420p14le", fmt) && (fmt == LCEVC_I420_14_LE));
    EXPECT_TRUE(fromString("yuv420p16le", fmt) && (fmt == LCEVC_I420_16_LE));
    EXPECT_TRUE(fromString("rgb24", fmt) && (fmt == LCEVC_RGB_8));
    EXPECT_TRUE(fromString("bgr24", fmt) && (fmt == LCEVC_BGR_8));
    EXPECT_TRUE(fromString("x2rgb10le", fmt) && (fmt == LCEVC_RGBA_10_2_LE));
    EXPECT_TRUE(fromString("gray8", fmt) && (fmt == LCEVC_GRAY_8));
    EXPECT_TRUE(fromString("gray10le", fmt) && (fmt == LCEVC_GRAY_10_LE));
    EXPECT_TRUE(fromString("gray12le", fmt) && (fmt == LCEVC_GRAY_12_LE));
    EXPECT_TRUE(fromString("gray14le", fmt) && (fmt == LCEVC_GRAY_14_LE));
    EXPECT_TRUE(fromString("gray16le", fmt) && (fmt == LCEVC_GRAY_16_LE));

    EXPECT_FALSE(fromString("", fmt));
    EXPECT_FALSE(fromString("badasdasdasdasdasdasdasd", fmt));

    EXPECT_STREQ(toString(LCEVC_I420_8).data(), "I420_8");
    EXPECT_STREQ(toString(LCEVC_I420_10_LE).data(), "I420_10_LE");
    EXPECT_STREQ(toString(LCEVC_I420_12_LE).data(), "I420_12_LE");
    EXPECT_STREQ(toString(LCEVC_I420_14_LE).data(), "I420_14_LE");
    EXPECT_STREQ(toString(LCEVC_I420_16_LE).data(), "I420_16_LE");
    EXPECT_STREQ(toString(LCEVC_YUV420_RASTER_8).data(), "YUV420_RASTER_8");
    EXPECT_STREQ(toString(LCEVC_NV12_8).data(), "NV12_8");
    EXPECT_STREQ(toString(LCEVC_NV21_8).data(), "NV21_8");
    EXPECT_STREQ(toString(LCEVC_RGB_8).data(), "RGB_8");
    EXPECT_STREQ(toString(LCEVC_BGR_8).data(), "BGR_8");
    EXPECT_STREQ(toString(LCEVC_RGBA_8).data(), "RGBA_8");
    EXPECT_STREQ(toString(LCEVC_BGRA_8).data(), "BGRA_8");
    EXPECT_STREQ(toString(LCEVC_ARGB_8).data(), "ARGB_8");
    EXPECT_STREQ(toString(LCEVC_ABGR_8).data(), "ABGR_8");
    EXPECT_STREQ(toString(LCEVC_RGBA_10_2_LE).data(), "RGBA_10_2_LE");
    EXPECT_STREQ(toString(LCEVC_GRAY_8).data(), "GRAY_8");
    EXPECT_STREQ(toString(LCEVC_GRAY_10_LE).data(), "GRAY_10_LE");
    EXPECT_STREQ(toString(LCEVC_GRAY_12_LE).data(), "GRAY_12_LE");
    EXPECT_STREQ(toString(LCEVC_GRAY_14_LE).data(), "GRAY_14_LE");
    EXPECT_STREQ(toString(LCEVC_GRAY_16_LE).data(), "GRAY_16_LE");
}

TEST(Convert, ReturnCode)
{
    LCEVC_ReturnCode rc{LCEVC_Error};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(rc) {
    case LCEVC_Success:
    case LCEVC_Again:
    case LCEVC_NotFound:
    case LCEVC_Error:
    case LCEVC_Uninitialized:
    case LCEVC_Initialized:
    case LCEVC_InvalidParam:
    case LCEVC_NotSupported:
    case LCEVC_Flushed:
    case LCEVC_Timeout:

    case LCEVC_ReturnCode_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("Success", rc) && (rc == LCEVC_Success));
    EXPECT_TRUE(fromString("Again", rc) && (rc == LCEVC_Again));
    EXPECT_TRUE(fromString("NotFound", rc) && (rc == LCEVC_NotFound));
    EXPECT_TRUE(fromString("Error", rc) && (rc == LCEVC_Error));
    EXPECT_TRUE(fromString("Uninitialized", rc) && (rc == LCEVC_Uninitialized));
    EXPECT_TRUE(fromString("Initialized", rc) && (rc == LCEVC_Initialized));
    EXPECT_TRUE(fromString("InvalidParam", rc) && (rc == LCEVC_InvalidParam));
    EXPECT_TRUE(fromString("NotSupported", rc) && (rc == LCEVC_NotSupported));
    EXPECT_TRUE(fromString("Flushed", rc) && (rc == LCEVC_Flushed));
    EXPECT_TRUE(fromString("Timeout", rc) && (rc == LCEVC_Timeout));

    EXPECT_FALSE(fromString("xyzzy", rc));

    EXPECT_STREQ(toString(LCEVC_Success).data(), "Success");
    EXPECT_STREQ(toString(LCEVC_Again).data(), "Again");
    EXPECT_STREQ(toString(LCEVC_NotFound).data(), "NotFound");
    EXPECT_STREQ(toString(LCEVC_Error).data(), "Error");
    EXPECT_STREQ(toString(LCEVC_Uninitialized).data(), "Uninitialized");
    EXPECT_STREQ(toString(LCEVC_Initialized).data(), "Initialized");
    EXPECT_STREQ(toString(LCEVC_InvalidParam).data(), "InvalidParam");
    EXPECT_STREQ(toString(LCEVC_NotSupported).data(), "NotSupported");
    EXPECT_STREQ(toString(LCEVC_Flushed).data(), "Flushed");
    EXPECT_STREQ(toString(LCEVC_Timeout).data(), "Timeout");

    EXPECT_EQ(toString(LCEVC_ReturnCode_ForceInt32).data(), nullptr);
}

TEST(Convert, ColorRange)
{
    LCEVC_ColorRange cr{LCEVC_ColorRange_Unknown};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(cr) {
    case LCEVC_ColorRange_Unknown:
    case LCEVC_ColorRange_Full:
    case LCEVC_ColorRange_Limited:

    case LCEVC_ColorRange_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("Full", cr) && (cr == LCEVC_ColorRange_Full));
    EXPECT_TRUE(fromString("Limited", cr) && (cr == LCEVC_ColorRange_Limited));

    EXPECT_STREQ(toString(LCEVC_ColorRange_Full).data(), "Full");
    EXPECT_STREQ(toString(LCEVC_ColorRange_Limited).data(), "Limited");
}

TEST(Convert, ColorStandard)
{
    LCEVC_ColorStandard cs{LCEVC_ColorStandard_Unknown};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(cs) {
    case LCEVC_ColorStandard_Unknown:
    case LCEVC_ColorStandard_BT709:
    case LCEVC_ColorStandard_BT601_PAL:
    case LCEVC_ColorStandard_BT601_NTSC:
    case LCEVC_ColorStandard_BT2020:

    case LCEVC_ColorStandard_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("BT709", cs) && (cs == LCEVC_ColorStandard_BT709));
    EXPECT_TRUE(fromString("BT601_NTSC", cs) && (cs == LCEVC_ColorStandard_BT601_NTSC));
    EXPECT_TRUE(fromString("BT601_PAL", cs) && (cs == LCEVC_ColorStandard_BT601_PAL));
    EXPECT_TRUE(fromString("BT2020", cs) && (cs == LCEVC_ColorStandard_BT2020));

    EXPECT_STREQ(toString(LCEVC_ColorStandard_BT709).data(), "BT709");
    EXPECT_STREQ(toString(LCEVC_ColorStandard_BT601_NTSC).data(), "BT601_NTSC");
    EXPECT_STREQ(toString(LCEVC_ColorStandard_BT601_PAL).data(), "BT601_PAL");
    EXPECT_STREQ(toString(LCEVC_ColorStandard_BT2020).data(), "BT2020");
}

TEST(Convert, ColorTransfer)
{
    LCEVC_ColorTransfer ct{LCEVC_ColorTransfer_Unknown};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(ct) {
    case LCEVC_ColorTransfer_Unknown:
    case LCEVC_ColorTransfer_SDR:
    case LCEVC_ColorTransfer_Linear:
    case LCEVC_ColorTransfer_ST2084:
    case LCEVC_ColorTransfer_HLG:
    case LCEVC_ColorTransfer_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("Linear", ct) && (ct == LCEVC_ColorTransfer_Linear));
    EXPECT_TRUE(fromString("SDR", ct) && (ct == LCEVC_ColorTransfer_SDR));
    EXPECT_TRUE(fromString("ST2084", ct) && (ct == LCEVC_ColorTransfer_ST2084));
    EXPECT_TRUE(fromString("HLG", ct) && (ct == LCEVC_ColorTransfer_HLG));

    EXPECT_STREQ(toString(LCEVC_ColorTransfer_Linear).data(), "Linear");
    EXPECT_STREQ(toString(LCEVC_ColorTransfer_SDR).data(), "SDR");
    EXPECT_STREQ(toString(LCEVC_ColorTransfer_ST2084).data(), "ST2084");
    EXPECT_STREQ(toString(LCEVC_ColorTransfer_HLG).data(), "HLG");
}

TEST(Convert, PictureFlag)
{
    LCEVC_PictureFlag pf{LCEVC_PictureFlag_Unknown};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(pf) {
    case LCEVC_PictureFlag_Unknown:
    case LCEVC_PictureFlag_IDR:
    case LCEVC_PictureFlag_Interlaced:

    case LCEVC_PictureFlag_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("IDR", pf) && (pf == LCEVC_PictureFlag_IDR));
    EXPECT_TRUE(fromString("Interlaced", pf) && (pf == LCEVC_PictureFlag_Interlaced));

    EXPECT_STREQ(toString(LCEVC_PictureFlag_IDR).data(), "IDR");
    EXPECT_STREQ(toString(LCEVC_PictureFlag_Interlaced).data(), "Interlaced");
}

TEST(Convert, Access)
{
    LCEVC_Access ac{LCEVC_Access_Unknown};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(ac) {
    case LCEVC_Access_Unknown:
    case LCEVC_Access_Read:
    case LCEVC_Access_Modify:
    case LCEVC_Access_Write:
    case LCEVC_Access_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("Read", ac) && (ac == LCEVC_Access_Read));
    EXPECT_TRUE(fromString("Modify", ac) && (ac == LCEVC_Access_Modify));
    EXPECT_TRUE(fromString("Write", ac) && (ac == LCEVC_Access_Write));

    EXPECT_STREQ(toString(LCEVC_Access_Read).data(), "Read");
    EXPECT_STREQ(toString(LCEVC_Access_Modify).data(), "Modify");
    EXPECT_STREQ(toString(LCEVC_Access_Write).data(), "Write");
}

TEST(Convert, Event)
{
    LCEVC_Event ev{LCEVC_Exit};

    // An error wne enumerations value are not covered - add case, and appropriate tests
    switch(ev) {
    case LCEVC_Log:
    case LCEVC_Exit:
    case LCEVC_CanSendBase:
    case LCEVC_CanSendEnhancement:
    case LCEVC_CanSendPicture:
    case LCEVC_CanReceive:
    case LCEVC_BasePictureDone:
    case LCEVC_OutputPictureDone:

    case LCEVC_EventCount:
    case LCEVC_Event_ForceInt32:
        ;
    }

    EXPECT_TRUE(fromString("Log", ev) && (ev == LCEVC_Log));
    EXPECT_TRUE(fromString("Exit", ev) && (ev == LCEVC_Exit));
    EXPECT_TRUE(fromString("CanSendBase", ev) && (ev == LCEVC_CanSendBase));
    EXPECT_TRUE(fromString("CanSendEnhancement", ev) && (ev == LCEVC_CanSendEnhancement));
    EXPECT_TRUE(fromString("CanSendPicture", ev) && (ev == LCEVC_CanSendPicture));
    EXPECT_TRUE(fromString("CanReceive", ev) && (ev == LCEVC_CanReceive));
    EXPECT_TRUE(fromString("BasePictureDone", ev) && (ev == LCEVC_BasePictureDone));
    EXPECT_TRUE(fromString("OutputPictureDone", ev) && (ev == LCEVC_OutputPictureDone));

    EXPECT_STREQ(toString(LCEVC_Log).data(), "Log");
    EXPECT_STREQ(toString(LCEVC_Exit).data(), "Exit");
    EXPECT_STREQ(toString(LCEVC_CanSendBase).data(), "CanSendBase");
    EXPECT_STREQ(toString(LCEVC_CanSendEnhancement).data(), "CanSendEnhancement");
    EXPECT_STREQ(toString(LCEVC_CanSendPicture).data(), "CanSendPicture");
    EXPECT_STREQ(toString(LCEVC_CanReceive).data(), "CanReceive");
    EXPECT_STREQ(toString(LCEVC_BasePictureDone).data(), "BasePictureDone");
    EXPECT_STREQ(toString(LCEVC_OutputPictureDone).data(), "OutputPictureDone");
}

TEST(Convert, fmt)
{
    LCEVC_ColorFormat fmt = LCEVC_I420_8;

    EXPECT_STREQ(fmt::format("Formatted {}", fmt).c_str(), "Formatted ColorFormat_I420_8");
    EXPECT_STREQ(fmt::format("Formatted {:>32}", fmt).c_str(), "Formatted               ColorFormat_I420_8");
    EXPECT_STREQ(fmt::format("{:<20} X", fmt).c_str(), "ColorFormat_I420_8   X");

    LCEVC_ReturnCode rc{LCEVC_Error};
    EXPECT_STREQ(fmt::format("Formatted {:*^30}", rc).c_str(), "Formatted *******ReturnCode_Error*******");

    LCEVC_ColorRange cr{LCEVC_ColorRange_Full};
    EXPECT_STREQ(fmt::format("Formatted {}", cr).c_str(), "Formatted ColorRange_Full");

    LCEVC_ColorStandard cs{LCEVC_ColorStandard_BT601_NTSC};
    EXPECT_STREQ(fmt::format("Formatted {}", cs).c_str(), "Formatted ColorStandard_BT601_NTSC");

    LCEVC_ColorTransfer ct{LCEVC_ColorTransfer_HLG};
    EXPECT_STREQ(fmt::format("Formatted {}", ct).c_str(), "Formatted ColorTransfer_HLG");

    LCEVC_PictureFlag pf{LCEVC_PictureFlag_IDR};
    EXPECT_STREQ(fmt::format("Formatted {}", pf).c_str(), "Formatted PictureFlag_IDR");

    LCEVC_Access ac{LCEVC_Access_Modify};
    EXPECT_STREQ(fmt::format("Formatted {}", ac).c_str(), "Formatted Access_Modify");

    LCEVC_Event ev{LCEVC_CanReceive};
    EXPECT_STREQ(fmt::format("Formatted {}", ev).c_str(), "Formatted Event_CanReceive");

}

TEST(Convert, CLI11)
{
    LCEVC_ColorFormat cf{LCEVC_ColorFormat_Unknown};
    LCEVC_ReturnCode rc{LCEVC_Success};
    LCEVC_ColorRange cr{LCEVC_ColorRange_Unknown};
    LCEVC_ColorStandard cs{LCEVC_ColorStandard_Unknown};
    LCEVC_ColorTransfer ct{LCEVC_ColorTransfer_Unknown};
    LCEVC_PictureFlag pf{LCEVC_PictureFlag_IDR};
    LCEVC_Access ac{LCEVC_Access_Modify};
    LCEVC_Event ev{LCEVC_CanReceive};

    CLI::App app{"LCEVC_DEC C++ Type Converting"};
    app.add_option("--colorFormat", cf, "Color Format")->default_val(LCEVC_ColorFormat_Unknown);
    app.add_option("--colorRange", cr, "Color Range")->default_val(LCEVC_ColorRange_Unknown);
    app.add_option("--colorStandard", cs, "Color Standard")->default_val(LCEVC_ColorStandard_Unknown);
    app.add_option("--colorTransfer", ct, "Color Transfer")->default_val(LCEVC_ColorTransfer_Unknown);
    app.add_option("--returnCode", rc, "Return Code");
    app.add_option("--pictureFlag", pf, "Picture Flag");
    app.add_option("--access", ac, "Access");
    app.add_option("--event", ev, "Event");

    static std::array argv = {
        "",
        "--colorFormat=I420_8",
        "--colorRange=full",
        "--colorStandard=bt2020",
        "--colorTransfer=ST2084",
        "--returnCode=Error",
        "--pictureFlag=Idr",
        "--access=Read",
        "--event=LOG",
    };

    app.parse(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(cf, LCEVC_I420_8);
    EXPECT_EQ(rc, LCEVC_Error);
    EXPECT_EQ(cr, LCEVC_ColorRange_Full);
    EXPECT_EQ(cs, LCEVC_ColorStandard_BT2020);
    EXPECT_EQ(ct, LCEVC_ColorTransfer_ST2084);
    EXPECT_EQ(pf, LCEVC_PictureFlag_IDR);
    EXPECT_EQ(ac, LCEVC_Access_Read);
    EXPECT_EQ(ev, LCEVC_Log);
}