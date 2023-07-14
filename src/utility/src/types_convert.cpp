// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/types_convert.h"

#include "lcevc_config.h"

#include <array>
#include <cstring>
#include <string_view>
#include <utility>

namespace lcevc_dec::utility {

#if VN_COMPILER(MSVC)
#define strcasecmp _stricmp
#endif

// Use static array and templated from/to linear functions for enum conversion
//  - Avoids any static constructors
//  - Allows synonyms
//  - Case insensitive
//
template <typename E, size_t N>
inline std::string_view enumToString(const std::pair<E, const char*> (&arr)[N], E enm)
{
    for (auto i = 0u; i < N; ++i) {
        if (arr[i].first == enm) {
            return arr[i].second;
        }
    }
    return {};
}

template <typename E, size_t N>
inline bool enumFromString(const std::pair<E, const char*> (&arr)[N], std::string_view str, E defaultValue, E& out)
{
    for (auto i = 0u; i < N; ++i) {
        if (!strcasecmp(arr[i].second, str.data())) {
            out = arr[i].first;
            return true;
        }
    }

    out = defaultValue;
    return false;
}

//// LCEVC_ColorFormat
//
static const std::pair<LCEVC_ColorFormat, const char*> kColorFormatTable[]{
    {LCEVC_I420_8, "I420_8"},
    {LCEVC_I420_10_LE, "I420_10_LE"},
    {LCEVC_I420_12_LE, "I420_12_LE"},
    {LCEVC_I420_14_LE, "I420_14_LE"},
    {LCEVC_I420_16_LE, "I420_16_LE"},
    {LCEVC_YUV420_RASTER_8, "YUV420_RASTER_8"},
    {LCEVC_NV12_8, "NV12_8"},
    {LCEVC_NV21_8, "NV21_8"},
    {LCEVC_RGB_8, "RGB_8"},
    {LCEVC_BGR_8, "BGR_8"},
    {LCEVC_RGBA_8, "RGBA_8"},
    {LCEVC_BGRA_8, "BGRA_8"},
    {LCEVC_ARGB_8, "ARGB_8"},
    {LCEVC_ABGR_8, "ABGR_8"},
    {LCEVC_RGBA_10_2_LE, "RGBA_10_2_LE"},
    {LCEVC_GRAY_8, "GRAY_8"},
    {LCEVC_GRAY_10_LE, "GRAY_10_LE"},
    {LCEVC_GRAY_12_LE, "GRAY_12_LE"},
    {LCEVC_GRAY_14_LE, "GRAY_14_LE"},
    {LCEVC_GRAY_16_LE, "GRAY_16_LE"},

    // Synonyms for 8 bit
    {LCEVC_I420_8, "P420"},
    {LCEVC_NV12_8, "NV12"},
    {LCEVC_NV21_8, "NV21"},
    {LCEVC_RGB_8, "RGB"},
    {LCEVC_BGR_8, "BGR"},
    {LCEVC_RGBA_8, "RGBA"},
    {LCEVC_BGRA_8, "BGRA"},
    {LCEVC_ARGB_8, "ARGB"},
    {LCEVC_ABGR_8, "ABGR"},
    {LCEVC_GRAY_8, "GRAY"},

    // Synonyms for little endian
    {LCEVC_I420_10_LE, "I420_10"},
    {LCEVC_I420_12_LE, "I420_12"},
    {LCEVC_I420_14_LE, "I420_14"},
    {LCEVC_I420_16_LE, "I420_16"},
    {LCEVC_RGBA_10_2_LE, "RGBA_10"},
    {LCEVC_GRAY_10_LE, "GRAY_10"},
    {LCEVC_GRAY_12_LE, "GRAY_12"},
    {LCEVC_GRAY_14_LE, "GRAY_14"},
    {LCEVC_GRAY_16_LE, "GRAY_16"},

    // Other common synony,s
    {LCEVC_I420_8, "I420"},
    {LCEVC_I420_8, "P420"},
    {LCEVC_I420_8, "P420_8"},
    {LCEVC_I420_10_LE, "P420_10"},
    {LCEVC_I420_12_LE, "P420_12"},
    {LCEVC_I420_14_LE, "P420_14"},
    {LCEVC_I420_16_LE, "P420_16"},

    // Synonyms for libav pixel formats
    {LCEVC_I420_8, "yuv420p"},
    {LCEVC_I420_10_LE, "yuv420p10le"},
    {LCEVC_I420_12_LE, "yuv420p12le"},
    {LCEVC_I420_14_LE, "yuv420p14le"},
    {LCEVC_I420_16_LE, "yuv420p16le"},
    {LCEVC_RGB_8, "rgb24"},
    {LCEVC_BGR_8, "bgr24"},
    {LCEVC_RGBA_10_2_LE, "x2rgb10le"},
    {LCEVC_GRAY_8, "gray8"},
    {LCEVC_GRAY_10_LE, "gray10le"},
    {LCEVC_GRAY_12_LE, "gray12le"},
    {LCEVC_GRAY_14_LE, "gray14le"},
    {LCEVC_GRAY_16_LE, "gray16le"},

    {LCEVC_ColorFormat_Unknown, "unknown"},
};

std::string_view toString(LCEVC_ColorFormat colorFormat) { return enumToString(kColorFormatTable, colorFormat); }

bool fromString(std::string_view str, LCEVC_ColorFormat& out)
{
    return enumFromString(kColorFormatTable, str, LCEVC_ColorFormat_Unknown, out);
}

//// LCEVC_ReturnCode
//
static const std::pair<LCEVC_ReturnCode, const char*> kReturnCodeTable[]{
    {LCEVC_Success, "Success"},
    {LCEVC_Again, "Again"},
    {LCEVC_NotFound, "NotFound"},
    {LCEVC_Error, "Error"},
    {LCEVC_Uninitialized, "Uninitialized"},
    {LCEVC_Initialized, "Initialized"},
    {LCEVC_InvalidParam, "InvalidParam"},
    {LCEVC_NotSupported, "NotSupported"},
    {LCEVC_Flushed, "Flushed"},
    {LCEVC_Timeout, "Timeout"},
};

std::string_view toString(LCEVC_ReturnCode returnCode)
{
    return enumToString(kReturnCodeTable, returnCode);
}

bool fromString(std::string_view str, LCEVC_ReturnCode& out)
{
    return enumFromString(kReturnCodeTable, str, LCEVC_Success, out);
}

//// LCEVC_ColorRange
//
static const std::pair<LCEVC_ColorRange, const char*> kColorRangeTable[]{
    {LCEVC_ColorRange_Full, "Full"},
    {LCEVC_ColorRange_Limited, "Limited"},
};

std::string_view toString(LCEVC_ColorRange colorRange)
{
    return enumToString(kColorRangeTable, colorRange);
}

bool fromString(std::string_view str, LCEVC_ColorRange& out)
{
    return enumFromString(kColorRangeTable, str, LCEVC_ColorRange_Unknown, out);
}

//// LCEVC_ColorStandard
//
static const std::pair<LCEVC_ColorStandard, const char*> kColorStandardTable[]{
    {LCEVC_ColorStandard_BT709, "BT709"},
    {LCEVC_ColorStandard_BT601_NTSC, "BT601_NTSC"},
    {LCEVC_ColorStandard_BT601_PAL, "BT601_PAL"},
    {LCEVC_ColorStandard_BT2020, "BT2020"},
};

std::string_view toString(LCEVC_ColorStandard colorStandard)
{
    return enumToString(kColorStandardTable, colorStandard);
}

bool fromString(std::string_view str, LCEVC_ColorStandard& out)
{
    return enumFromString(kColorStandardTable, str, LCEVC_ColorStandard_Unknown, out);
}

//// LCEVC_ColorTransfer
//
static const std::pair<LCEVC_ColorTransfer, const char*> kColorTransferTable[]{
    {LCEVC_ColorTransfer_Linear, "Linear"},
    {LCEVC_ColorTransfer_SDR, "SDR"},
    {LCEVC_ColorTransfer_ST2084, "ST2084"},
    {LCEVC_ColorTransfer_HLG, "HLG"},
};

std::string_view toString(LCEVC_ColorTransfer colorTransfer)
{
    return enumToString(kColorTransferTable, colorTransfer);
}

bool fromString(std::string_view str, LCEVC_ColorTransfer& out)
{
    return enumFromString(kColorTransferTable, str, LCEVC_ColorTransfer_Unknown, out);
}

//// LCEVC_PictureFlag
//
static const std::pair<LCEVC_PictureFlag, const char*> kPictureFlagTable[]{
    {LCEVC_PictureFlag_IDR, "IDR"},
    {LCEVC_PictureFlag_Interlaced, "Interlaced"},
};

std::string_view toString(LCEVC_PictureFlag pictureFlag)
{
    return enumToString(kPictureFlagTable, pictureFlag);
}

bool fromString(std::string_view str, LCEVC_PictureFlag& out)
{
    return enumFromString(kPictureFlagTable, str, LCEVC_PictureFlag_Unknown, out);
}

//// LCEVC_Access
//
static const std::pair<LCEVC_Access, const char*> kAccessTable[]{
    {LCEVC_Access_Read, "Read"},
    {LCEVC_Access_Modify, "Modify"},
    {LCEVC_Access_Write, "Write"},
};

std::string_view toString(LCEVC_Access access) { return enumToString(kAccessTable, access); }

bool fromString(std::string_view str, LCEVC_Access& out)
{
    return enumFromString(kAccessTable, str, LCEVC_Access_Unknown, out);
}

//// LCEVC_Event
//
static const std::pair<LCEVC_Event, const char*> kEventTable[]{
    {LCEVC_Log, "Log"},
    {LCEVC_Exit, "Exit"},
    {LCEVC_CanSendBase, "CanSendBase"},
    {LCEVC_CanSendEnhancement, "CanSendEnhancement"},
    {LCEVC_CanSendPicture, "CanSendPicture"},
    {LCEVC_CanReceive, "CanReceive"},
    {LCEVC_BasePictureDone, "BasePictureDone"},
    {LCEVC_OutputPictureDone, "OutputPictureDone"},
};

std::string_view toString(LCEVC_Event event) { return enumToString(kEventTable, event); }

bool fromString(std::string_view str, LCEVC_Event& out)
{
    return enumFromString(kEventTable, str, LCEVC_EventCount, out);
}

} // namespace lcevc_dec::utility
