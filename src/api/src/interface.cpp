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

#include "interface.h"

#include "accel_context.h"
#include "log.h"
#include "picture.h"
#include "timestamps.h"

#include <LCEVC/PerseusDecoder.h>

#include <cstring>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

static const LogComponent kComp = LogComponent::Interface;

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

// ------------------------------------------------------------------------------------------------

bool equals(const LCEVC_HDRStaticInfo& lhs, const LCEVC_HDRStaticInfo& rhs)
{
    // Note: this is ONLY safe because HDRStaticInfo is trivially-copyable (i.e. no pointers), and
    // has no padding (i.e. every member is strictly the same size).
    return (std::memcmp(&lhs, &rhs, sizeof(LCEVC_HDRStaticInfo)) == 0);
}

DecodeInformation::DecodeInformation(const Picture& base, bool lcevcAvailable,
                                     bool shouldPassthrough, bool shouldFail)
    : timestamp(timehandleGetTimestamp(base.getTimehandle()))
    , hasBase(true)
    , hasEnhancement(lcevcAvailable)
    , skipped(false)
    , enhanced(!shouldFail && !shouldPassthrough)
    , baseWidth(base.getWidth())
    , baseHeight(base.getHeight())
    , baseBitdepth(base.getBitdepth())
    , userData(base.getUserData())
{}

uint32_t bitdepthFromLCEVCDescColorFormat(int32_t descColorFormat)
{
    auto safeDescColorFormat = static_cast<LCEVC_ColorFormat>(descColorFormat);

    switch (safeDescColorFormat) {
        case LCEVC_I420_8:
        case LCEVC_I422_8:
        case LCEVC_I444_8:
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
        case LCEVC_I422_10_LE:
        case LCEVC_I444_10_LE:
        case LCEVC_RGBA_10_2_LE:
        case LCEVC_GRAY_10_LE: return 10;

        case LCEVC_I420_12_LE:
        case LCEVC_I422_12_LE:
        case LCEVC_I444_12_LE:
        case LCEVC_GRAY_12_LE: return 12;

        case LCEVC_I420_14_LE:
        case LCEVC_I422_14_LE:
        case LCEVC_I444_14_LE:
        case LCEVC_GRAY_14_LE: return 14;

        case LCEVC_I420_16_LE:
        case LCEVC_I422_16_LE:
        case LCEVC_I444_16_LE:
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

    if (coreFormat.conformance_window.enabled) {
        picDescOut.cropBottom = coreFormat.conformance_window.planes[0].bottom;
        picDescOut.cropLeft = coreFormat.conformance_window.planes[0].left;
        picDescOut.cropRight = coreFormat.conformance_window.planes[0].right;
        picDescOut.cropTop = coreFormat.conformance_window.planes[0].top;
    }

    uint8_t bitdepth = 0;
    if (!fromCoreBitdepth(coreFormat.global_config.bitdepths[0], bitdepth)) {
        VNLogError("Invalid bitdepth in core stream: %d\n", coreFormat.global_config.bitdepths[0]);
        return false;
    } else if (picDescOut.colorFormat == LCEVC_NV12_8 &&
               coreFormat.global_config.colourspace == PSS_CSP_YUV420P) {
        // Special case to preserve NV12 on the output as the core stores the interleaving data in
        // perseus_image which is not accessible like the perseus_decoder_stream
        picDescOut.colorFormat = LCEVC_NV12_8;
        return true;
    }
    switch (coreFormat.global_config.colourspace) {
        case PSS_CSP_YUV420P: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LCEVC_I420_8; break;
                case 10: picDescOut.colorFormat = LCEVC_I420_10_LE; break;
                case 12: picDescOut.colorFormat = LCEVC_I420_12_LE; break;
                case 14: picDescOut.colorFormat = LCEVC_I420_14_LE; break;
                case 16: picDescOut.colorFormat = LCEVC_I420_16_LE; break;
            }
            break;
        }

        case PSS_CSP_YUV422P: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LCEVC_I422_8; break;
                case 10: picDescOut.colorFormat = LCEVC_I422_10_LE; break;
                case 12: picDescOut.colorFormat = LCEVC_I422_12_LE; break;
                case 14: picDescOut.colorFormat = LCEVC_I422_14_LE; break;
                case 16: picDescOut.colorFormat = LCEVC_I422_16_LE; break;
            }
            break;
        }

        case PSS_CSP_YUV444P: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LCEVC_I444_8; break;
                case 10: picDescOut.colorFormat = LCEVC_I444_10_LE; break;
                case 12: picDescOut.colorFormat = LCEVC_I444_12_LE; break;
                case 14: picDescOut.colorFormat = LCEVC_I444_14_LE; break;
                case 16: picDescOut.colorFormat = LCEVC_I444_16_LE; break;
            }
            break;
        }

        case PSS_CSP_MONOCHROME: {
            switch (bitdepth) {
                case 8: picDescOut.colorFormat = LCEVC_GRAY_8; break;
                case 10: picDescOut.colorFormat = LCEVC_GRAY_10_LE; break;
                case 12: picDescOut.colorFormat = LCEVC_GRAY_12_LE; break;
                case 14: picDescOut.colorFormat = LCEVC_GRAY_14_LE; break;
                case 16: picDescOut.colorFormat = LCEVC_GRAY_16_LE; break;
            }
            break;
        }

        case PSS_CSP_UNSUPPORTED:
        case PSS_CSP_LAST:
        default: {
            VNLogError("Core decoder using a format (%u) not available in decoder API. Possibly "
                       "invalid format?\n",
                       coreFormat.global_config.colourspace);
            picDescOut.colorFormat = LCEVC_ColorFormat_Unknown;
            break;
        }
    }

    picDescOut.colorRange = getColorRangeFromStream(coreFormat.vui_info.flags);
    picDescOut.colorPrimaries = getColorPrimariesFromStream(coreFormat.vui_info.colour_primaries);
    picDescOut.transferCharacteristics =
        getTransferCharacteristicsFromStream(coreFormat.vui_info.transfer_characteristics);
    getHdrStaticInfoFromStream(picDescOut.hdrStaticInfo, coreFormat.hdr_info);

    const AspectRatio newSar = getSampleAspectRatioFromStream(coreFormat.vui_info);
    picDescOut.sampleAspectRatioDen = newSar.denominator;
    picDescOut.sampleAspectRatioNum = newSar.numerator;

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

bool toCoreInterleaving(const LCEVC_ColorFormat format, bool interleaved, int32_t& interleavingOut)
{
    if (!interleaved) {
        interleavingOut = 0;
        return true;
    }
    switch (format) {
        case LCEVC_I420_8:
        case LCEVC_I420_10_LE:
        case LCEVC_I420_12_LE:
        case LCEVC_I420_14_LE:
        case LCEVC_I420_16_LE:
        case LCEVC_I422_8:
        case LCEVC_I422_10_LE:
        case LCEVC_I422_12_LE:
        case LCEVC_I422_14_LE:
        case LCEVC_I422_16_LE:
        case LCEVC_I444_8:
        case LCEVC_I444_10_LE:
        case LCEVC_I444_12_LE:
        case LCEVC_I444_14_LE:
        case LCEVC_I444_16_LE: interleavingOut = 1; return true; // These aren't possible

        case LCEVC_NV12_8:
        case LCEVC_NV21_8: interleavingOut = 2; return true;

        case LCEVC_RGB_8:
        case LCEVC_BGR_8: interleavingOut = 4; return true;

        case LCEVC_RGBA_8:
        case LCEVC_BGRA_8:
        case LCEVC_ARGB_8:
        case LCEVC_ABGR_8:
        case LCEVC_RGBA_10_2_LE: interleavingOut = 5; return true;
        default:
            VNLogError("Invalid interleaved LCEVC_ColorFormat to convert to core %d:%d\n", format,
                       interleaved);
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
