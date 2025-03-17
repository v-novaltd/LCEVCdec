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

#include "LCEVC/utility/types_convert.h"

#include <LCEVC/api_utility/enum_map.h>
#include <LCEVC/lcevc_dec.h>

#include <string_view>

namespace lcevc_dec::utility {

//// LCEVC_ColorFormat
//
static const EnumMap<LCEVC_ColorFormat> kColorFormatTable{
    {LCEVC_I420_8, "I420_8"},
    {LCEVC_I420_10_LE, "I420_10_LE"},
    {LCEVC_I420_12_LE, "I420_12_LE"},
    {LCEVC_I420_14_LE, "I420_14_LE"},
    {LCEVC_I420_16_LE, "I420_16_LE"},
    {LCEVC_I422_8, "I422_8"},
    {LCEVC_I422_10_LE, "I422_10_LE"},
    {LCEVC_I422_12_LE, "I422_12_LE"},
    {LCEVC_I422_14_LE, "I422_14_LE"},
    {LCEVC_I422_16_LE, "I422_16_LE"},
    {LCEVC_I444_8, "I444_8"},
    {LCEVC_I444_10_LE, "I444_10_LE"},
    {LCEVC_I444_12_LE, "I444_12_LE"},
    {LCEVC_I444_14_LE, "I444_14_LE"},
    {LCEVC_I444_16_LE, "I444_16_LE"},
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

std::string_view toString(LCEVC_ColorFormat colorFormat)
{
    return enumToString(kColorFormatTable, colorFormat);
}

bool fromString(std::string_view str, LCEVC_ColorFormat& out)
{
    return enumFromString(kColorFormatTable, str, LCEVC_ColorFormat_Unknown, out);
}

//// LCEVC_ReturnCode
//
static constexpr EnumMapArr<LCEVC_ReturnCode, 10> kReturnCodeTable{
    LCEVC_Success,       "Success",       LCEVC_Again,        "Again",
    LCEVC_NotFound,      "NotFound",      LCEVC_Error,        "Error",
    LCEVC_Uninitialized, "Uninitialized", LCEVC_Initialized,  "Initialized",
    LCEVC_InvalidParam,  "InvalidParam",  LCEVC_NotSupported, "NotSupported",
    LCEVC_Flushed,       "Flushed",       LCEVC_Timeout,      "Timeout",
};
static_assert(!kReturnCodeTable.isMissingEnums(),
              "kReturnCodeTable is missing a string for a return code. Last time it was updated, "
              "there were 10 return codes");

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
static constexpr EnumMapArr<LCEVC_ColorRange, LCEVC_ColorRange_Count> kColorRangeTable{
    LCEVC_ColorRange_Unknown, "Unknown", LCEVC_ColorRange_Full, "Full",
    LCEVC_ColorRange_Limited, "Limited",
};
static_assert(!kColorRangeTable.isMissingEnums(),
              "kColorRangeTable is missing a string for a color range");

std::string_view toString(LCEVC_ColorRange colorRange)
{
    return enumToString(kColorRangeTable, colorRange);
}

bool fromString(std::string_view str, LCEVC_ColorRange& out)
{
    return enumFromString(kColorRangeTable, str, LCEVC_ColorRange_Unknown, out);
}

//// LCEVC_ColorPrimaries
//
static constexpr EnumMapArr<LCEVC_ColorPrimaries, LCEVC_ColorPrimaries_Count> kColorPrimariesTable{
    LCEVC_ColorPrimaries_Reserved_0,   "Reserved_0",
    LCEVC_ColorPrimaries_BT709,        "BT709",
    LCEVC_ColorPrimaries_Unspecified,  "Unspecified",
    LCEVC_ColorPrimaries_Reserved_3,   "Reserved_3",
    LCEVC_ColorPrimaries_BT470_M,      "BT470_M",
    LCEVC_ColorPrimaries_BT470_BG,     "BT470_BG",
    LCEVC_ColorPrimaries_BT601_NTSC,   "BT601_NTSC",
    LCEVC_ColorPrimaries_SMPTE240,     "SMPTE240",
    LCEVC_ColorPrimaries_GENERIC_FILM, "Generic film",
    LCEVC_ColorPrimaries_BT2020,       "BT2020",
    LCEVC_ColorPrimaries_XYZ,          "SMPTE ST 428-1 (XYZ)",
    LCEVC_ColorPrimaries_SMPTE431,     "SMPTE RP 431-2",
    LCEVC_ColorPrimaries_SMPTE432,     "SMPTE EG 432-1",
    LCEVC_ColorPrimaries_Reserved_13,  "Reserved_13",
    LCEVC_ColorPrimaries_Reserved_14,  "Reserved_14",
    LCEVC_ColorPrimaries_Reserved_15,  "Reserved_15",
    LCEVC_ColorPrimaries_Reserved_16,  "Reserved_16",
    LCEVC_ColorPrimaries_Reserved_17,  "Reserved_17",
    LCEVC_ColorPrimaries_Reserved_18,  "Reserved_18",
    LCEVC_ColorPrimaries_Reserved_19,  "Reserved_19",
    LCEVC_ColorPrimaries_Reserved_20,  "Reserved_20",
    LCEVC_ColorPrimaries_Reserved_21,  "Reserved_21",
    LCEVC_ColorPrimaries_P22,          "P22 (none)",
};
static_assert(!kColorPrimariesTable.isMissingEnums(),
              "kColorPrimariesTable is missing a string for a color primary.");

std::string_view toString(LCEVC_ColorPrimaries colorPrimaries)
{
    return enumToString(kColorPrimariesTable, colorPrimaries);
}

bool fromString(std::string_view str, LCEVC_ColorPrimaries& out)
{
    return enumFromString(kColorPrimariesTable, str, LCEVC_ColorPrimaries_Unspecified, out);
}

//// LCEVC_TransferCharacteristics
//
static constexpr EnumMapArr<LCEVC_TransferCharacteristics, LCEVC_TransferCharacteristics_Count> kTransferCharacteristicsTable{
    LCEVC_TransferCharacteristics_Reserved_0,
    "Reserved_0",
    LCEVC_TransferCharacteristics_BT709,
    "BT709",
    LCEVC_TransferCharacteristics_Unspecified,
    "Unspecified",
    LCEVC_TransferCharacteristics_Reserved_3,
    "Reserved_3",
    LCEVC_TransferCharacteristics_GAMMA22,
    "GAMMA22",
    LCEVC_TransferCharacteristics_GAMMA28,
    "GAMMA28",
    LCEVC_TransferCharacteristics_BT601,
    "BT601",
    LCEVC_TransferCharacteristics_SMPTE240,
    "SMPTE240",
    LCEVC_TransferCharacteristics_LINEAR,
    "LINEAR",
    LCEVC_TransferCharacteristics_LOG100,
    "LOG100",
    LCEVC_TransferCharacteristics_LOG100_SQRT10,
    "LOG100_SQRT10",
    LCEVC_TransferCharacteristics_IEC61966,
    "IEC61966",
    LCEVC_TransferCharacteristics_BT1361,
    "BT1361",
    LCEVC_TransferCharacteristics_SRGB_SYCC,
    "SRGB_SYCC",
    LCEVC_TransferCharacteristics_BT2020_10BIT,
    "BT2020_10BIT",
    LCEVC_TransferCharacteristics_BT2020_12BIT,
    "BT2020_12BIT",
    LCEVC_TransferCharacteristics_PQ,
    "PQ",
    LCEVC_TransferCharacteristics_SMPTE428,
    "SMPTE428",
    LCEVC_TransferCharacteristics_HLG,
    "HLG",
};
static_assert(!kTransferCharacteristicsTable.isMissingEnums(),
              "kTransferCharacteristicsTable is missing a string for a characteristics enum.");

std::string_view toString(LCEVC_TransferCharacteristics transferCharacteristics)
{
    return enumToString(kTransferCharacteristicsTable, transferCharacteristics);
}

bool fromString(std::string_view str, LCEVC_TransferCharacteristics& out)
{
    return enumFromString(kTransferCharacteristicsTable, str,
                          LCEVC_TransferCharacteristics_Unspecified, out);
}

//// LCEVC_PictureFlag
//
static constexpr EnumMapArr<LCEVC_PictureFlag, LCEVC_PictureFlag_Count> kPictureFlagTable{
    LCEVC_PictureFlag_Unknown,    "Unknown",    LCEVC_PictureFlag_IDR, "IDR",
    LCEVC_PictureFlag_Interlaced, "Interlaced",
};
static_assert(!kPictureFlagTable.isMissingEnums(),
              "kPictureFlagTable is missing a string for a picture flag.");

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
static constexpr EnumMapArr<LCEVC_Access, LCEVC_Access_Count> kAccessTable{
    LCEVC_Access_Unknown, "Unknown", LCEVC_Access_Read,  "Read",
    LCEVC_Access_Modify,  "Modify",  LCEVC_Access_Write, "Write",
};
static_assert(!kAccessTable.isMissingEnums(),
              "kAccessTable is missing a string for an access type.");

std::string_view toString(LCEVC_Access access) { return enumToString(kAccessTable, access); }

bool fromString(std::string_view str, LCEVC_Access& out)
{
    return enumFromString(kAccessTable, str, LCEVC_Access_Unknown, out);
}

//// LCEVC_Event
//
static constexpr EnumMapArr<LCEVC_Event, LCEVC_EventCount> kEventTable{
    LCEVC_Log,
    "Log",
    LCEVC_Exit,
    "Exit",
    LCEVC_CanSendBase,
    "CanSendBase",
    LCEVC_CanSendEnhancement,
    "CanSendEnhancement",
    LCEVC_CanSendPicture,
    "CanSendPicture",
    LCEVC_CanReceive,
    "CanReceive",
    LCEVC_BasePictureDone,
    "BasePictureDone",
    LCEVC_OutputPictureDone,
    "OutputPictureDone",
};
static_assert(!kEventTable.isMissingEnums(), "kEventTable is missing a string for an event type.");

std::string_view toString(LCEVC_Event event) { return enumToString(kEventTable, event); }

bool fromString(std::string_view str, LCEVC_Event& out)
{
    return enumFromString(kEventTable, str, LCEVC_EventCount, out);
}

} // namespace lcevc_dec::utility
