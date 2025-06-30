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

#define VNComponentCurrent LdcComponentBufferManager

#include "core_interface.h"
//
#include <LCEVC/common/log.h>
#include <LCEVC/legacy/PerseusDecoder.h>

namespace lcevc_dec::decoder {

// Private helper functions for coreFormatToLCEVCPictureDesc
//
namespace {
    LdpColorRange getColorRangeFromStream(uint32_t vuiFlags)
    {
        return vuiFlags & PSS_VUIF_VIDEO_SIGNAL_FULL_RANGE_FLAG ? LdpColorRangeFull : LdpColorRangeLimited;
    }

    LdpColorPrimaries getColorPrimariesFromStream(uint8_t vuiColorPrimaries)
    {
        // Enum values strictly match the ITU-T/ISO VUI constants so we can safely cast
        return static_cast<LdpColorPrimaries>(vuiColorPrimaries);
    }

    LdpTransferCharacteristics getTransferCharacteristicsFromStream(uint8_t vuiTransferCharacteristics)
    {
        switch (vuiTransferCharacteristics) {
            // From ITU-T Series H Supplement 18: Signalling, backward compatibility and display
            // adaptation for HDR/WCG video coding. Note that linear transfer is not an option.
            case 1:
            case 6:
            case 14:
            case 15: return LdpTransferCharacteristicsBT709;
            case 16: return LdpTransferCharacteristicsPQ;
            case 18: return LdpTransferCharacteristicsHLG;
            default: return LdpTransferCharacteristicsUnspecified;
        }
    }

    void getHdrStaticInfoFromStream(LdpHDRStaticInfo& dest, const lcevc_hdr_info& hdrInfo)
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
                       "uint16_t variable");
            dest.maxDisplayMasteringLuminance = UINT16_MAX;
        } else {
            dest.maxDisplayMasteringLuminance = static_cast<uint16_t>(max);
        }

        if (hdrInfo.mastering_display.min_display_mastering_luminance > UINT16_MAX) {
            VNLogError("min_display_mastering_luminance value is too big to be stored in a "
                       "uint16_t variable");
            dest.minDisplayMasteringLuminance = UINT16_MAX;
        } else {
            dest.minDisplayMasteringLuminance =
                static_cast<uint16_t>(hdrInfo.mastering_display.min_display_mastering_luminance);
        }

        dest.maxContentLightLevel = hdrInfo.content_light_level.max_content_light_level;
        dest.maxFrameAverageLightLevel = hdrInfo.content_light_level.max_pic_average_light_level;
    }

    LdpAspectRatio getSampleAspectRatioFromStream(const lcevc_vui_info& vuiInfo)
    {
        // clang-format off
    // From ITU-T H.273 | ISO/IEC 23091-2:2019, 8.6, ITU-T H.264 & H.265 Table E-1
    // aspect_ratio_idc ->                            0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
    static const uint16_t idcSarNumerators[17]   = {  1,  1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64,160,  4,  3,  2 };
    static const uint16_t idcSarDenominators[17] = {  1,  1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99,  3,  2,  1 };
        // clang-format on
        LdpAspectRatio sar = {1, 1};

        if (const uint8_t idc = vuiInfo.aspect_ratio_idc; idc < 17) {
            sar = {idcSarNumerators[idc], idcSarDenominators[idc]};
        } else if (vuiInfo.aspect_ratio_idc == 255) {
            sar = {vuiInfo.sar_width, vuiInfo.sar_height};
        } else {
            VNLogError("LCEVC VUI aspect_ratio_idc %u in unallowed reserved range 17..254, "
                       "overriding with 1:1",
                       idc);
        }
        return sar;
    }
} // namespace

bool coreFormatToLdpPictureDesc(const perseus_decoder_stream& coreFormat, LdpPictureDesc& picDescOut)
{
    picDescOut.width = coreFormat.global_config.width;
    picDescOut.height = coreFormat.global_config.height;

    if (coreFormat.conformance_window.enabled) {
        picDescOut.cropBottom = coreFormat.conformance_window.planes[0].bottom;
        picDescOut.cropLeft = coreFormat.conformance_window.planes[0].left;
        picDescOut.cropRight = coreFormat.conformance_window.planes[0].right;
        picDescOut.cropTop = coreFormat.conformance_window.planes[0].top;
    }

    uint8_t bitdepth = 0;
    if (!fromCoreBitdepth(coreFormat.global_config.bitdepths[0], bitdepth)) {
        VNLogError("Invalid bitdepth in core stream: %d",
                   static_cast<int32_t>(coreFormat.global_config.bitdepths[0]));
        return false;
    }

    if (picDescOut.colorFormat == LdpColorFormatNV12_8 &&
        coreFormat.global_config.colourspace == PSS_CSP_YUV420P) {
        // Special case to preserve NV12 on the output as the core stores the interleaving data in
        // perseus_image which is not accessible like the perseus_decoder_stream
        picDescOut.colorFormat = LdpColorFormatNV12_8;
        return true;
    }
    switch (coreFormat.global_config.colourspace) {
        case PSS_CSP_YUV420P: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LdpColorFormatI420_8; break;
                case 10: picDescOut.colorFormat = LdpColorFormatI420_10_LE; break;
                case 12: picDescOut.colorFormat = LdpColorFormatI420_12_LE; break;
                case 14: picDescOut.colorFormat = LdpColorFormatI420_14_LE; break;
                case 16: picDescOut.colorFormat = LdpColorFormatI420_16_LE; break;
                default: assert(0);
            }
            break;
        }

        case PSS_CSP_YUV422P: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LdpColorFormatI422_8; break;
                case 10: picDescOut.colorFormat = LdpColorFormatI422_10_LE; break;
                case 12: picDescOut.colorFormat = LdpColorFormatI422_12_LE; break;
                case 14: picDescOut.colorFormat = LdpColorFormatI422_14_LE; break;
                case 16: picDescOut.colorFormat = LdpColorFormatI422_16_LE; break;
                default: assert(0);
            }
            break;
        }

        case PSS_CSP_YUV444P: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LdpColorFormatI444_8; break;
                case 10: picDescOut.colorFormat = LdpColorFormatI444_10_LE; break;
                case 12: picDescOut.colorFormat = LdpColorFormatI444_12_LE; break;
                case 14: picDescOut.colorFormat = LdpColorFormatI444_14_LE; break;
                case 16: picDescOut.colorFormat = LdpColorFormatI444_16_LE; break;
                default: assert(0);
            }
            break;
        }

        case PSS_CSP_MONOCHROME: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LdpColorFormatGRAY_8; break;
                case 10: picDescOut.colorFormat = LdpColorFormatGRAY_10_LE; break;
                case 12: picDescOut.colorFormat = LdpColorFormatGRAY_12_LE; break;
                case 14: picDescOut.colorFormat = LdpColorFormatGRAY_14_LE; break;
                case 16: picDescOut.colorFormat = LdpColorFormatGRAY_16_LE; break;
                default: assert(0);
            }
            break;
        }

        default: {
            VNLogError("Core decoder using a format (%u) not available in decoder API. Possibly "
                       "invalid format?",
                       static_cast<uint32_t>(coreFormat.global_config.colourspace));
            picDescOut.colorFormat = LdpColorFormatUnknown;
            break;
        }
    }

    picDescOut.colorRange = getColorRangeFromStream(coreFormat.vui_info.flags);
    picDescOut.colorPrimaries = getColorPrimariesFromStream(coreFormat.vui_info.colour_primaries);
    picDescOut.transferCharacteristics =
        getTransferCharacteristicsFromStream(coreFormat.vui_info.transfer_characteristics);
    getHdrStaticInfoFromStream(picDescOut.hdrStaticInfo, coreFormat.hdr_info);

    const LdpAspectRatio newSar = getSampleAspectRatioFromStream(coreFormat.vui_info);
    picDescOut.sampleAspectRatioDen = newSar.denominator;
    picDescOut.sampleAspectRatioNum = newSar.numerator;

    return true;
}

bool toCoreInterleaving(LdpColorFormat format, bool interleaved, int32_t& interleavingOut)
{
    if (!interleaved) {
        interleavingOut = 0;
        return true;
    }
    switch (format) {
        case LdpColorFormatI420_8:
        case LdpColorFormatI420_10_LE:
        case LdpColorFormatI420_12_LE:
        case LdpColorFormatI420_14_LE:
        case LdpColorFormatI420_16_LE:
        case LdpColorFormatI422_8:
        case LdpColorFormatI422_10_LE:
        case LdpColorFormatI422_12_LE:
        case LdpColorFormatI422_14_LE:
        case LdpColorFormatI422_16_LE:
        case LdpColorFormatI444_8:
        case LdpColorFormatI444_10_LE:
        case LdpColorFormatI444_12_LE:
        case LdpColorFormatI444_14_LE:
        case LdpColorFormatI444_16_LE: interleavingOut = 1; return true; // These aren't possible

        case LdpColorFormatNV12_8:
        case LdpColorFormatNV21_8: interleavingOut = 2; return true;

        case LdpColorFormatRGB_8:
        case LdpColorFormatBGR_8: interleavingOut = 4; return true;

        case LdpColorFormatRGBA_8:
        case LdpColorFormatBGRA_8:
        case LdpColorFormatARGB_8:
        case LdpColorFormatABGR_8:
        case LdpColorFormatRGBA_10_2_LE: interleavingOut = 5; return true;
        default:
            VNLogError("Invalid interleaved color format to convert to core %d:%d",
                       static_cast<int32_t>(format), static_cast<int32_t>(interleaved));
            return false;
    }
}

bool toCoreBitdepth(uint8_t val, int32_t& out)
{
    switch (val) {
        case 14: out = PSS_DEPTH_14; return true;
        case 12: out = PSS_DEPTH_12; return true;
        case 10: out = PSS_DEPTH_10; return true;
        case 8: out = PSS_DEPTH_8; return true;
        default: assert(0);
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
        default: assert(0);
    }
    return false;
}

uint32_t bitdepthFromLdpColorFormat(int32_t colorFormat)
{
    switch (const auto safeDescColorFormat = static_cast<LdpColorFormat>(colorFormat); safeDescColorFormat) {
        case LdpColorFormatI420_8:
        case LdpColorFormatI422_8:
        case LdpColorFormatI444_8:
        case LdpColorFormatNV12_8:
        case LdpColorFormatNV21_8:
        case LdpColorFormatRGB_8:
        case LdpColorFormatBGR_8:
        case LdpColorFormatRGBA_8:
        case LdpColorFormatBGRA_8:
        case LdpColorFormatARGB_8:
        case LdpColorFormatABGR_8:
        case LdpColorFormatGRAY_8: return 8;

        case LdpColorFormatI420_10_LE:
        case LdpColorFormatI422_10_LE:
        case LdpColorFormatI444_10_LE:
        case LdpColorFormatRGBA_10_2_LE:
        case LdpColorFormatGRAY_10_LE: return 10;

        case LdpColorFormatI420_12_LE:
        case LdpColorFormatI422_12_LE:
        case LdpColorFormatI444_12_LE:
        case LdpColorFormatGRAY_12_LE: return 12;

        case LdpColorFormatI420_14_LE:
        case LdpColorFormatI422_14_LE:
        case LdpColorFormatI444_14_LE:
        case LdpColorFormatGRAY_14_LE: return 14;

        case LdpColorFormatI420_16_LE:
        case LdpColorFormatI422_16_LE:
        case LdpColorFormatI444_16_LE:
        case LdpColorFormatGRAY_16_LE: return 16;

        default: break;
    }
    return 0;
}

} // namespace lcevc_dec::decoder
