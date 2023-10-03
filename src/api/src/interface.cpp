/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "interface.h"

#include "accel_context.h"
#include "uLog.h"

#include <LCEVC/PerseusDecoder.h>
#include <LCEVC/lcevc_dec.h>

#include <cstring>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

using namespace lcevc_dec::api_utility;

static_assert(sizeof(HDRStaticInfo) == sizeof(LCEVC_HDRStaticInfo),
              "Please keep HDRStaticInfo up to date with LCEVC_HDRStaticInfo. If this struct has "
              "changed, you may need to update its equals function and getHdrStaticInfoFromStream");
static_assert(sizeof(DecodeInformation) == sizeof(LCEVC_DecodeInformation),
              "Please keep DecodeInformation up to date with LCEVC_DecodeInformation.");
static_assert(sizeof(PictureBufferDesc) == sizeof(LCEVC_PictureBufferDesc),
              "Please keep PictureBufferDesc up to date with LCEVC_PictureBufferDesc. If this "
              "struct has changed, you will need to update its equals function, as well as "
              "functions which convert to and from LCEVC_PictureBufferDesc");
static_assert(sizeof(LCEVC_PictureDesc) == 76,
              "If LCEVC_PictureDesc has changed, please update the equals operator, and update the "
              "size being checked here to the new size.");
static_assert(sizeof(PicturePlaneDesc) == sizeof(LCEVC_PicturePlaneDesc),
              "Please keep PicturePlaneDesc up to date with LCEVC_PicturePlaneDesc. If this "
              "struct has changed, you will need to update its equals function, as well as "
              "functions which convert to and from LCEVC_PictureDesc");

bool equals(const LCEVC_HDRStaticInfo& lhs, const LCEVC_HDRStaticInfo& rhs)
{
    // Note: this is ONLY safe because HDRStaticInfo is trivially-copyable (i.e. no pointers), and
    // has no padding (i.e. every member is strictly the same size).
    return (std::memcmp(&lhs, &rhs, sizeof(LCEVC_HDRStaticInfo)) == 0);
}

ColorRange fromLCEVCColorRange(int32_t lcevcColorRange)
{
    auto safeLcevcColorRange = static_cast<LCEVC_ColorRange>(lcevcColorRange);

    switch (safeLcevcColorRange) {
        case LCEVC_ColorRange_Full: return ColorRange::Full;
        case LCEVC_ColorRange_Limited: return ColorRange::Limited;
        case LCEVC_ColorRange_Unknown:
        case LCEVC_ColorRange_ForceInt32: break;
    }
    return ColorRange::Unknown;
}

int32_t toLCEVCColorRange(ColorRange colorRange)
{
    switch (colorRange) {
        case ColorRange::Full: return LCEVC_ColorRange_Full;
        case ColorRange::Limited: return LCEVC_ColorRange_Limited;
        case ColorRange::Unknown: break;
    }
    return LCEVC_ColorRange_Unknown;
}

Colorspace::Enum fromLCEVCColorPrimaries(int32_t lcevcColorPrimaries)
{
    auto safeLcevcColorPrimaries = static_cast<LCEVC_ColorPrimaries>(lcevcColorPrimaries);

    switch (safeLcevcColorPrimaries) {
        case LCEVC_ColorPrimaries_BT709: return Colorspace::YCbCr_BT709;

        // These aren't identical but apparently we don't distinguish them
        case LCEVC_ColorPrimaries_BT470_BG:
        case LCEVC_ColorPrimaries_BT2020: return Colorspace::YCbCr_BT2020;

        // These are the same based on comments in lcevc_dec.h
        case LCEVC_ColorPrimaries_SMPTE240:
        case LCEVC_ColorPrimaries_BT601_NTSC: return Colorspace::YCbCr_BT601;

        // Every other format has no corresponding internal format.
        case LCEVC_ColorPrimaries_Reserved_0:
        case LCEVC_ColorPrimaries_Unspecified:
        case LCEVC_ColorPrimaries_Reserved_3:
        case LCEVC_ColorPrimaries_BT470_M:
        case LCEVC_ColorPrimaries_GENERIC_FILM:
        case LCEVC_ColorPrimaries_XYZ:
        case LCEVC_ColorPrimaries_SMPTE431:
        case LCEVC_ColorPrimaries_SMPTE432:
        case LCEVC_ColorPrimaries_Reserved_13:
        case LCEVC_ColorPrimaries_Reserved_14:
        case LCEVC_ColorPrimaries_Reserved_15:
        case LCEVC_ColorPrimaries_Reserved_16:
        case LCEVC_ColorPrimaries_Reserved_17:
        case LCEVC_ColorPrimaries_Reserved_18:
        case LCEVC_ColorPrimaries_Reserved_19:
        case LCEVC_ColorPrimaries_Reserved_20:
        case LCEVC_ColorPrimaries_Reserved_21:
        case LCEVC_ColorPrimaries_P22:
        case LCEVC_ColorPrimaries_ForceInt32: break;
    }
    return Colorspace::Invalid;
}

int32_t toLCEVCColorPrimaries(Colorspace::Enum colorspace)
{
    // Our colorspace utility doesn't distinguish PAL and NTSC so default to PAL. Auto and Invalid
    // default to unknown, and sRGB outputs as BT709 (since sRGB uses the same primaries and white
    // point).
    switch (colorspace) {
        case Colorspace::sRGB:
        case Colorspace::YCbCr_BT709: return LCEVC_ColorPrimaries_BT709;
        case Colorspace::YCbCr_BT601: return LCEVC_ColorPrimaries_BT470_BG;
        case Colorspace::YCbCr_BT2020: return LCEVC_ColorPrimaries_BT2020;
        case Colorspace::Auto:
        case Colorspace::Invalid: break;
    }
    return LCEVC_ColorPrimaries_Unspecified;
}

MatrixCoefficients fromLCEVCMatrixCoefficients(int32_t lcevcMatrixCoefficients)
{
    auto safeLcevcMatrixCoefficients = static_cast<LCEVC_MatrixCoefficients>(lcevcMatrixCoefficients);
    switch (safeLcevcMatrixCoefficients) {
        case LCEVC_MatrixCoefficients_IDENTITY: return MatrixCoefficients::Identity;
        case LCEVC_MatrixCoefficients_BT709: return MatrixCoefficients::BT709;
        case LCEVC_MatrixCoefficients_Unspecified: return MatrixCoefficients::Unspecified;
        case LCEVC_MatrixCoefficients_Reserved_3: return MatrixCoefficients::Reserved3;
        case LCEVC_MatrixCoefficients_USFCC: return MatrixCoefficients::USFCC;
        case LCEVC_MatrixCoefficients_BT470_BG: return MatrixCoefficients::BT470BG;
        case LCEVC_MatrixCoefficients_BT601_NTSC: return MatrixCoefficients::BT601NTSC;
        case LCEVC_MatrixCoefficients_SMPTE240: return MatrixCoefficients::SMPTE240;
        case LCEVC_MatrixCoefficients_YCGCO: return MatrixCoefficients::YCgCo;
        case LCEVC_MatrixCoefficients_BT2020_NCL: return MatrixCoefficients::BT2020NCL;
        case LCEVC_MatrixCoefficients_BT2020_CL: return MatrixCoefficients::BT2020CL;
        case LCEVC_MatrixCoefficients_SMPTE2085: return MatrixCoefficients::SMPTE2085;
        case LCEVC_MatrixCoefficients_CHROMATICITY_NCL: return MatrixCoefficients::ChromaticityNCL;
        case LCEVC_MatrixCoefficients_CHROMATICITY_CL: return MatrixCoefficients::ChromaticityCL;
        case LCEVC_MatrixCoefficients_ICTCP: return MatrixCoefficients::ICTCP;
        default: break;
    }
    return MatrixCoefficients::Unknown;
}

int32_t toLCEVCMatrixCoefficients(MatrixCoefficients matrixCoefficients)
{
    switch (matrixCoefficients) {
        case MatrixCoefficients::Identity: return LCEVC_MatrixCoefficients_IDENTITY;
        case MatrixCoefficients::BT709: return LCEVC_MatrixCoefficients_BT709;
        case MatrixCoefficients::Unspecified: return LCEVC_MatrixCoefficients_Unspecified;
        case MatrixCoefficients::Reserved3: return LCEVC_MatrixCoefficients_Reserved_3;
        case MatrixCoefficients::USFCC: return LCEVC_MatrixCoefficients_USFCC;
        case MatrixCoefficients::BT470BG: return LCEVC_MatrixCoefficients_BT470_BG;
        case MatrixCoefficients::BT601NTSC: return LCEVC_MatrixCoefficients_BT601_NTSC;
        case MatrixCoefficients::SMPTE240: return LCEVC_MatrixCoefficients_SMPTE240;
        case MatrixCoefficients::YCgCo: return LCEVC_MatrixCoefficients_YCGCO;
        case MatrixCoefficients::BT2020NCL: return LCEVC_MatrixCoefficients_BT2020_NCL;
        case MatrixCoefficients::BT2020CL: return LCEVC_MatrixCoefficients_BT2020_CL;
        case MatrixCoefficients::SMPTE2085: return LCEVC_MatrixCoefficients_SMPTE2085;
        case MatrixCoefficients::ChromaticityNCL: return LCEVC_MatrixCoefficients_CHROMATICITY_NCL;
        case MatrixCoefficients::ChromaticityCL: return LCEVC_MatrixCoefficients_CHROMATICITY_CL;
        case MatrixCoefficients::ICTCP: return LCEVC_MatrixCoefficients_ICTCP;

        case MatrixCoefficients::Unknown: break;
    }
    // Note: this isn't exactly one-to-one. Unknown and Unspecified BOTH produce "unspecified" as
    // output, but technically unknown is distinct (it might just mean WE don't know it).
    return LCEVC_MatrixCoefficients_Unspecified;
}

TransferCharacteristics fromLCEVCTransferCharacteristics(int32_t lcevcTransferCharacteristics)
{
    auto safeLcevcTransferCharacteristics =
        static_cast<LCEVC_TransferCharacteristics>(lcevcTransferCharacteristics);

    switch (safeLcevcTransferCharacteristics) {
        case LCEVC_TransferCharacteristics_Reserved_0: return TransferCharacteristics::Reserved0;
        case LCEVC_TransferCharacteristics_BT709: return TransferCharacteristics::BT709;
        case LCEVC_TransferCharacteristics_Unspecified: return TransferCharacteristics::Unspecified;
        case LCEVC_TransferCharacteristics_Reserved_3: return TransferCharacteristics::Reserved3;
        case LCEVC_TransferCharacteristics_GAMMA22: return TransferCharacteristics::Gamma22;
        case LCEVC_TransferCharacteristics_GAMMA28: return TransferCharacteristics::Gamma28;
        case LCEVC_TransferCharacteristics_BT601: return TransferCharacteristics::BT601;
        case LCEVC_TransferCharacteristics_SMPTE240: return TransferCharacteristics::SMPTE240;
        case LCEVC_TransferCharacteristics_LINEAR: return TransferCharacteristics::Linear;
        case LCEVC_TransferCharacteristics_LOG100: return TransferCharacteristics::Log100;
        case LCEVC_TransferCharacteristics_LOG100_SQRT10:
            return TransferCharacteristics::Log100Sqrt10;
        case LCEVC_TransferCharacteristics_IEC61966: return TransferCharacteristics::IEC61966;
        case LCEVC_TransferCharacteristics_BT1361: return TransferCharacteristics::BT1361;
        case LCEVC_TransferCharacteristics_SRGB_SYCC: return TransferCharacteristics::SRGBsYCC;
        case LCEVC_TransferCharacteristics_BT2020_10BIT:
            return TransferCharacteristics::BT202010Bit;
        case LCEVC_TransferCharacteristics_BT2020_12BIT:
            return TransferCharacteristics::BT202012Bit;
        case LCEVC_TransferCharacteristics_PQ: return TransferCharacteristics::PQ;
        case LCEVC_TransferCharacteristics_SMPTE428: return TransferCharacteristics::SMPTE428;
        case LCEVC_TransferCharacteristics_HLG: return TransferCharacteristics::HLG;

        case LCEVC_TransferCharacteristics_ForceInt32: break;
    }
    return TransferCharacteristics::Unknown;
}

int32_t toLCEVCTransferCharacteristics(TransferCharacteristics transferCharacteristics)
{
    switch (transferCharacteristics) {
        case TransferCharacteristics::Reserved0: return LCEVC_TransferCharacteristics_Reserved_0;
        case TransferCharacteristics::BT709: return LCEVC_TransferCharacteristics_BT709;
        case TransferCharacteristics::Unspecified: return LCEVC_TransferCharacteristics_Unspecified;
        case TransferCharacteristics::Reserved3: return LCEVC_TransferCharacteristics_Reserved_3;
        case TransferCharacteristics::Gamma22: return LCEVC_TransferCharacteristics_GAMMA22;
        case TransferCharacteristics::Gamma28: return LCEVC_TransferCharacteristics_GAMMA28;
        case TransferCharacteristics::BT601: return LCEVC_TransferCharacteristics_BT601;
        case TransferCharacteristics::SMPTE240: return LCEVC_TransferCharacteristics_SMPTE240;
        case TransferCharacteristics::Linear: return LCEVC_TransferCharacteristics_LINEAR;
        case TransferCharacteristics::Log100: return LCEVC_TransferCharacteristics_LOG100;
        case TransferCharacteristics::Log100Sqrt10:
            return LCEVC_TransferCharacteristics_LOG100_SQRT10;
        case TransferCharacteristics::IEC61966: return LCEVC_TransferCharacteristics_IEC61966;
        case TransferCharacteristics::BT1361: return LCEVC_TransferCharacteristics_BT1361;
        case TransferCharacteristics::SRGBsYCC: return LCEVC_TransferCharacteristics_SRGB_SYCC;
        case TransferCharacteristics::BT202010Bit:
            return LCEVC_TransferCharacteristics_BT2020_10BIT;
        case TransferCharacteristics::BT202012Bit:
            return LCEVC_TransferCharacteristics_BT2020_12BIT;
        case TransferCharacteristics::PQ: return LCEVC_TransferCharacteristics_PQ;
        case TransferCharacteristics::SMPTE428: return LCEVC_TransferCharacteristics_SMPTE428;
        case TransferCharacteristics::HLG: return LCEVC_TransferCharacteristics_HLG;

        case TransferCharacteristics::Unknown: break;
    }
    // Note: this isn't exactly one-to-one. Unknown and Unspecified BOTH produce "unspecified" as
    // output, but technically unknown is distinct (it might just mean WE don't know it).
    return LCEVC_TransferCharacteristics_Unspecified;
}

PictureFormat::Enum fromLCEVCDescColorFormat(int32_t descColorFormat)
{
    auto safeDescColorFormat = static_cast<LCEVC_ColorFormat>(descColorFormat);

    switch (safeDescColorFormat) {
        case LCEVC_NV12_8:
        case LCEVC_NV21_8:
        case LCEVC_I420_8: return PictureFormat::YUV8Planar420;
        case LCEVC_I420_10_LE: return PictureFormat::YUV10Planar420;
        case LCEVC_I420_12_LE: return PictureFormat::YUV12Planar420;
        case LCEVC_I420_14_LE: return PictureFormat::YUV14Planar420;
        case LCEVC_YUV420_RASTER_8: return PictureFormat::YUV8Raster420;

        case LCEVC_RGB_8: return PictureFormat::RGB24;
        case LCEVC_BGR_8: return PictureFormat::BGR24;
        case LCEVC_RGBA_8: return PictureFormat::RGBA32;
        case LCEVC_BGRA_8: return PictureFormat::BGRA32;
        case LCEVC_ARGB_8: return PictureFormat::ARGB32;
        case LCEVC_ABGR_8: return PictureFormat::ABGR32;
        case LCEVC_RGBA_10_2_LE: return PictureFormat::RGB10A2;

        case LCEVC_GRAY_8: return PictureFormat::Y8Planar;
        case LCEVC_GRAY_10_LE: return PictureFormat::Y10Planar;
        case LCEVC_GRAY_12_LE: return PictureFormat::Y12Planar;
        case LCEVC_GRAY_14_LE: return PictureFormat::Y14Planar;

        // These are not currently supported, so fallthrough to invalid cases
        case LCEVC_GRAY_16_LE: // format = PictureFormat::Y16Planar; break;
        case LCEVC_I420_16_LE: // format = PictureFormat::YUV16Planar420; break;

        // These will never be supported types
        case LCEVC_ColorFormat_Unknown:
        case LCEVC_ColorFormat_ForceInt32: break;
    }

    VNLogError("Invalid color format provided: %d.\n", descColorFormat);
    return PictureFormat::Invalid;
}

PictureInterleaving::Enum fromLCEVCDescInterleaving(int32_t descColorFormat)
{
    auto safeDescColorFormat = static_cast<LCEVC_ColorFormat>(descColorFormat);

    switch (safeDescColorFormat) {
        case LCEVC_NV12_8: return PictureInterleaving::NV12;

        case LCEVC_I420_8:
        case LCEVC_I420_10_LE:
        case LCEVC_I420_12_LE:
        case LCEVC_I420_14_LE:
        case LCEVC_I420_16_LE:
        case LCEVC_YUV420_RASTER_8:
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
        case LCEVC_GRAY_16_LE: return PictureInterleaving::None;

        // Not currently supported, so fallthrough to invalid cases:
        case LCEVC_NV21_8: // return PictureInterleaving::NV21;

        // Not ever going to be supported:
        case LCEVC_ColorFormat_Unknown:
        case LCEVC_ColorFormat_ForceInt32: break;
    }

    VNLogError("Cannot deduce interleaving from color format: %d.\n", descColorFormat);
    return PictureInterleaving::Invalid;
}

int32_t toLCEVCDescColorFormat(PictureFormat::Enum format, PictureInterleaving::Enum interleaving)
{
    switch (format) {
        case PictureFormat::YUV8Planar420: {
            switch (interleaving) {
                case PictureInterleaving::None: return LCEVC_I420_8;
                case PictureInterleaving::NV12: return LCEVC_NV12_8;
                case PictureInterleaving::Invalid: return LCEVC_ColorFormat_Unknown;
            }
        }

        case PictureFormat::YUV10Planar420: return LCEVC_I420_10_LE;
        case PictureFormat::YUV12Planar420: return LCEVC_I420_12_LE;
        case PictureFormat::YUV14Planar420: return LCEVC_I420_14_LE;
        case PictureFormat::YUV16Planar420: return LCEVC_I420_16_LE;

        case PictureFormat::YUV8Raster420: return LCEVC_YUV420_RASTER_8;

        case PictureFormat::Y8Planar: return LCEVC_GRAY_8;
        case PictureFormat::Y10Planar: return LCEVC_GRAY_10_LE;
        case PictureFormat::Y12Planar: return LCEVC_GRAY_12_LE;
        case PictureFormat::Y14Planar: return LCEVC_GRAY_14_LE;
        case PictureFormat::Y16Planar: return LCEVC_GRAY_16_LE;

        case PictureFormat::RGB24: return LCEVC_RGB_8;
        case PictureFormat::BGR24: return LCEVC_BGR_8;
        case PictureFormat::RGBA32: return LCEVC_RGBA_8;
        case PictureFormat::BGRA32: return LCEVC_BGRA_8;
        case PictureFormat::ABGR32: return LCEVC_ABGR_8;
        case PictureFormat::ARGB32: return LCEVC_ARGB_8;

        case PictureFormat::RGB10A2: return LCEVC_RGBA_10_2_LE;

        case PictureFormat::YUV8Planar422:
        case PictureFormat::YUV8Planar444:
        case PictureFormat::YUV10Planar422:
        case PictureFormat::YUV10Planar444:
        case PictureFormat::YUV12Planar422:
        case PictureFormat::YUV12Planar444:
        case PictureFormat::YUV14Planar422:
        case PictureFormat::YUV14Planar444:
        case PictureFormat::YUV16Planar422:
        case PictureFormat::YUV16Planar444:
        case PictureFormat::RAW8:
        case PictureFormat::RAW16:
        case PictureFormat::RAW16f:
        case PictureFormat::RAW32f:
        case PictureFormat::RGBA64:
        case PictureFormat::Invalid: break;
    }
    return LCEVC_ColorFormat_Unknown;
}

uint32_t bitdepthFromLCEVCDescColorFormat(int32_t descColorFormat)
{
    auto safeDescColorFormat = static_cast<LCEVC_ColorFormat>(descColorFormat);

    switch (safeDescColorFormat) {
        case LCEVC_I420_8:
        case LCEVC_YUV420_RASTER_8:
        case LCEVC_NV12_8:
        case LCEVC_NV21_8:
        case LCEVC_RGB_8:
        case LCEVC_BGR_8:
        case LCEVC_RGBA_8:
        case LCEVC_BGRA_8:
        case LCEVC_ARGB_8:
        case LCEVC_ABGR_8:
        case LCEVC_GRAY_8: return 8;

        case LCEVC_I420_10_LE:
        case LCEVC_RGBA_10_2_LE:
        case LCEVC_GRAY_10_LE: return 10;

        case LCEVC_I420_12_LE:
        case LCEVC_GRAY_12_LE: return 12;

        case LCEVC_I420_14_LE:
        case LCEVC_GRAY_14_LE: return 14;

        case LCEVC_I420_16_LE:
        case LCEVC_GRAY_16_LE: return 16;

        case LCEVC_ColorFormat_Unknown:
        case LCEVC_ColorFormat_ForceInt32: break;
    }
    return 0;
}

// These are all private helper functions for coreFormatToLCEVCPictureDesc

LCEVC_ColorRange getColorRangeFromStream(uint32_t vuiFlags)
{
    return vuiFlags & PSS_VUIF_VIDEO_SIGNAL_FULL_RANGE_FLAG ? LCEVC_ColorRange_Full
                                                            : LCEVC_ColorRange_Limited;
}

LCEVC_ColorPrimaries getColorPrimariesFromStream(uint8_t vuiColorPrimaries)
{
    // Enum values strictly match the ITU-T/ISO VUI constants so we can safely cast
    return static_cast<LCEVC_ColorPrimaries>(vuiColorPrimaries);
}

LCEVC_TransferCharacteristics getTransferCharacteristicsFromStream(uint8_t vuiTransferCharacteristics)
{
    switch (vuiTransferCharacteristics) {
        // From ITU-T Series H Supplement 18: Signalling, backward compatibility and display
        // adaptation for HDR/WCG video coding. Note that linear transfer is not an option.
        case 1:
        case 6:
        case 14:
        case 15: return LCEVC_TransferCharacteristics_BT709;
        case 16: return LCEVC_TransferCharacteristics_PQ;
        case 18: return LCEVC_TransferCharacteristics_HLG;
    }
    return LCEVC_TransferCharacteristics_Unspecified;
}

void getHdrStaticInfoFromStream(LCEVC_HDRStaticInfo& dest, const lcevc_hdr_info& hdrInfo)
{
    dest.displayPrimariesX0 = hdrInfo.mastering_display.display_primaries_x[0];
    dest.displayPrimariesY0 = hdrInfo.mastering_display.display_primaries_y[0];
    dest.displayPrimariesX1 = hdrInfo.mastering_display.display_primaries_x[1];
    dest.displayPrimariesY1 = hdrInfo.mastering_display.display_primaries_y[1];
    dest.displayPrimariesX2 = hdrInfo.mastering_display.display_primaries_x[2];
    dest.displayPrimariesY2 = hdrInfo.mastering_display.display_primaries_y[2];
    dest.whitePointX = hdrInfo.mastering_display.white_point_x;
    dest.whitePointY = hdrInfo.mastering_display.white_point_y;

    auto max = static_cast<float>(hdrInfo.mastering_display.max_display_mastering_luminance);
    // convert to LCEVC API's units
    max /= 10000.f;
    if (max > UINT16_MAX) {
        VNLogError("max_display_mastering_luminance value is too big to be stored in a "
                   "uint16_t variable\n");
        dest.maxDisplayMasteringLuminance = UINT16_MAX;
    } else {
        dest.maxDisplayMasteringLuminance = static_cast<uint16_t>(max);
    }

    if (hdrInfo.mastering_display.min_display_mastering_luminance > UINT16_MAX) {
        VNLogError("min_display_mastering_luminance value is too big to be stored in a "
                   "uint16_t variable\n");
        dest.minDisplayMasteringLuminance = UINT16_MAX;
    } else {
        dest.minDisplayMasteringLuminance =
            static_cast<uint16_t>(hdrInfo.mastering_display.min_display_mastering_luminance);
    }

    dest.maxContentLightLevel = hdrInfo.content_light_level.max_content_light_level;
    dest.maxFrameAverageLightLevel = hdrInfo.content_light_level.max_pic_average_light_level;
}

AspectRatio getSampleAspectRatioFromStream(const lcevc_vui_info& vuiInfo)
{
    // clang-format off
    // From ITU-T H.273 | ISO/IEC 23091-2:2019, 8.6, ITU-T H.264 & H.265 Table E-1
    // aspect_ratio_idc ->                            0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
    static const uint16_t idcSarNumerators[17]   = {  1,  1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64,160,  4,  3,  2 };
    static const uint16_t idcSarDenominators[17] = {  1,  1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99,  3,  2,  1 };
    // clang-format on
    AspectRatio sar = {1, 1};
    uint8_t idc = vuiInfo.aspect_ratio_idc;

    if (idc < 17) {
        sar = {idcSarNumerators[idc], idcSarDenominators[idc]};
    } else if (vuiInfo.aspect_ratio_idc == 255) {
        sar = {vuiInfo.sar_width, vuiInfo.sar_height};
    } else {
        VNLogError("LCEVC VUI aspect_ratio_idc %u in unallowed reserved range 17..254, "
                   "overriding with 1:1\n",
                   idc);
    }
    return sar;
}

Margins getConformanceWindowCropFromStream(const lcevc_conformance_window& window)
{
    Margins conformanceWindowCrop = {0};

    if (window.enabled) {
        conformanceWindowCrop.left = window.planes[0].left;
        conformanceWindowCrop.top = window.planes[0].top;
        conformanceWindowCrop.right = window.planes[0].right;
        conformanceWindowCrop.bottom = window.planes[0].bottom;
    }
    return conformanceWindowCrop;
}

ChromaSamplingType::Enum fromCoreChromaSubsamplingType(int32_t coreChromaSubsample)
{
    auto safeCoreChromaSubsample = static_cast<perseus_colourspace>(coreChromaSubsample);

    switch (safeCoreChromaSubsample) {
        case PSS_CSP_YUV420P: return ChromaSamplingType::Chroma420;
        case PSS_CSP_YUV422P: return ChromaSamplingType::Chroma422;
        case PSS_CSP_YUV444P: return ChromaSamplingType::Chroma444;
        case PSS_CSP_MONOCHROME: return ChromaSamplingType::Monochrome;
        case PSS_CSP_UNSUPPORTED:
        case PSS_CSP_LAST: break;
    }
    return ChromaSamplingType::Invalid;
}

bool equals(const LCEVC_PictureDesc& lhs, const LCEVC_PictureDesc& rhs)
{
    return (lhs.width == rhs.width) && (lhs.height == rhs.height) &&
           (lhs.colorFormat == rhs.colorFormat) && (lhs.colorRange == rhs.colorRange) &&
           (lhs.colorPrimaries == rhs.colorPrimaries) &&
           (lhs.matrixCoefficients == rhs.matrixCoefficients) &&
           (lhs.transferCharacteristics == rhs.transferCharacteristics) &&
           equals(lhs.hdrStaticInfo, rhs.hdrStaticInfo) &&
           (lhs.sampleAspectRatioDen == rhs.sampleAspectRatioDen) &&
           (lhs.sampleAspectRatioNum == rhs.sampleAspectRatioNum) &&
           (lhs.cropBottom == rhs.cropBottom) && (lhs.cropLeft == rhs.cropLeft) &&
           (lhs.cropRight == rhs.cropRight) && (lhs.cropTop == rhs.cropTop);
}

bool coreFormatToLCEVCPictureDesc(const perseus_decoder_stream& coreFormat, LCEVC_PictureDesc& picDescOut)
{
    picDescOut.width = coreFormat.global_config.width;
    picDescOut.height = coreFormat.global_config.height;

    // ColorFormat is a hassle:
    {
        uint8_t bitdepth = 0;
        if (!fromCoreBitdepth(coreFormat.global_config.bitdepths[0], bitdepth)) {
            VNLogError("Invalid bitdepth in core stream: %d\n", coreFormat.global_config.bitdepths[0]);
            return false;
        }
        const BitDepthType::Enum utilBitdepth = BitDepthType::FromValue(bitdepth);
        const ChromaSamplingType::Enum utilChromaSubsampling =
            fromCoreChromaSubsamplingType(coreFormat.global_config.colourspace);
        const PictureFormat::Enum utilFormat =
            PictureFormat::FromBitDepthChroma(utilBitdepth, utilChromaSubsampling);

        // Preserve whatever interleaving we originally had, if possible.
        picDescOut.colorFormat = static_cast<LCEVC_ColorFormat>(
            toLCEVCDescColorFormat(utilFormat, fromLCEVCDescInterleaving(picDescOut.colorFormat)));
    }

    picDescOut.colorRange = getColorRangeFromStream(coreFormat.vui_info.flags);
    picDescOut.colorPrimaries = getColorPrimariesFromStream(coreFormat.vui_info.colour_primaries);
    picDescOut.transferCharacteristics =
        getTransferCharacteristicsFromStream(coreFormat.vui_info.transfer_characteristics);
    getHdrStaticInfoFromStream(picDescOut.hdrStaticInfo, coreFormat.hdr_info);

    const AspectRatio newSar = getSampleAspectRatioFromStream(coreFormat.vui_info);
    picDescOut.sampleAspectRatioDen = newSar.denominator;
    picDescOut.sampleAspectRatioNum = newSar.numerator;

    Margins newCrop = getConformanceWindowCropFromStream(coreFormat.conformance_window);
    picDescOut.cropBottom = newCrop.bottom;
    picDescOut.cropLeft = newCrop.left;
    picDescOut.cropRight = newCrop.right;
    picDescOut.cropTop = newCrop.top;

    return true;
}

bool equals(const LCEVC_PictureBufferDesc& lhs, const LCEVC_PictureBufferDesc& rhs)
{
    return (lhs.data == rhs.data) && (lhs.byteSize == rhs.byteSize) &&
           (lhs.accelBuffer.hdl == rhs.accelBuffer.hdl) && (lhs.access == rhs.access);
}

void fromLCEVCPictureBufferDesc(const LCEVC_PictureBufferDesc& lcevcPictureBufferDesc,
                                PictureBufferDesc& pictureBufferDescOut)
{
    pictureBufferDescOut.accelBuffer = lcevcPictureBufferDesc.accelBuffer.hdl;
    pictureBufferDescOut.data = lcevcPictureBufferDesc.data;
    pictureBufferDescOut.byteSize = lcevcPictureBufferDesc.byteSize;
    pictureBufferDescOut.access = lcevcPictureBufferDesc.access;
}
void toLCEVCPictureBufferDesc(const PictureBufferDesc& pictureBufferDesc,
                              LCEVC_PictureBufferDesc& lcevcPictureBufferDescOut)
{
    lcevcPictureBufferDescOut.accelBuffer.hdl = pictureBufferDesc.accelBuffer.handle;
    lcevcPictureBufferDescOut.data = pictureBufferDesc.data;
    lcevcPictureBufferDescOut.byteSize = pictureBufferDesc.byteSize;
    lcevcPictureBufferDescOut.access = static_cast<LCEVC_Access>(pictureBufferDesc.access);
}

bool equals(const LCEVC_PicturePlaneDesc& lhs, const LCEVC_PicturePlaneDesc& rhs)
{
    return (lhs.firstSample == rhs.firstSample) && (lhs.rowByteStride == rhs.rowByteStride);
}

void fromLCEVCPicturePlaneDesc(const LCEVC_PicturePlaneDesc& lcevcPicturePlaneDesc,
                               PicturePlaneDesc& picturePlaneDescOut)
{
    picturePlaneDescOut.firstSample = lcevcPicturePlaneDesc.firstSample;
    picturePlaneDescOut.rowByteStride = lcevcPicturePlaneDesc.rowByteStride;
}

void toLCEVCPicturePlaneDesc(const PicturePlaneDesc& picturePlaneDesc,
                             LCEVC_PicturePlaneDesc& lcevcPicturePlaneDescOut)
{
    lcevcPicturePlaneDescOut.firstSample = picturePlaneDesc.firstSample;
    lcevcPicturePlaneDescOut.rowByteStride = picturePlaneDesc.rowByteStride;
}

bool toCoreInterleaving(const lcevc_dec::api_utility::PictureFormat::Enum& format,
                        const lcevc_dec::api_utility::PictureInterleaving::Enum& interleaving,
                        int32_t& interleavingOut)
{
    if (PictureFormat::IsRGB(format)) {
        switch (format) {
            case PictureFormat::RGB24: interleavingOut = PSS_ILV_RGB; return true;
            case PictureFormat::RGBA32: interleavingOut = PSS_ILV_RGBA; return true;

            default: VNLogError("invalid RGB format %d:%d\n", format, interleaving); return false;
        }
    } else if (PictureFormat::IsYUV(format)) {
        interleavingOut = (interleaving == PictureInterleaving::NV12 ? PSS_ILV_NV12 : PSS_ILV_NONE);
        return true;
    } else if (PictureFormat::IsMonochrome(format)) {
        interleavingOut = PSS_ILV_NONE;
        return true;
    }
    VNLogError("invalid format %d:%d\n", format, interleaving);
    return false;
}

bool toCoreBitdepth(uint8_t val, int32_t& out)
{
    switch (val) {
        case 14: out = PSS_DEPTH_14; return true;
        case 12: out = PSS_DEPTH_12; return true;
        case 10: out = PSS_DEPTH_10; return true;
        case 8: out = PSS_DEPTH_8; return true;
    }
    return false;
}

bool fromCoreBitdepth(const int32_t& val, uint8_t& out)
{
    switch (val) {
        case PSS_DEPTH_14: out = 14; return true;
        case PSS_DEPTH_12: out = 12; return true;
        case PSS_DEPTH_10: out = 10; return true;
        case PSS_DEPTH_8: out = 8; return true;
    }
    return false;
}

} // namespace lcevc_dec::decoder
