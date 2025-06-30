/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

// Common pipeline types that match the API types.
//
// This file can be included by C or C++. it is expected that LCEVCdec C code will only ever get passed
// pointers to these structures. The creation and lifetime (and thus initialization) will be
// handled by the LCEVCdec C++ pipeline code, or another external C pipeline.
//
// The duplication is to allow:
//  - Decoupling evolution of API and pipeline over time - We can add/remove enums fields on one side
//    and limit effect on other.
//  - Keeping handles inside API
//  - C++ features can be added to these types (default initialised, enum class etc) (Maybe better to just keep as C?)
//
#ifndef VN_LCEVC_PIPELINE_TYPES_H
#define VN_LCEVC_PIPELINE_TYPES_H

#include <LCEVC/common/return_code.h>
//
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// NOLINTBEGIN(modernize-use-using)

// Matches LCEVC_Access
typedef enum LdpAccess
{
    LdpAccessUnknown = 0,
    LdpAccessRead = 1,
    LdpAccessModify = 2,
    LdpAccessWrite = 3,

    LdpAccessCount,
    LdpAccessForceInt32 = 0x7ffffff,
} LdpAccess;

// Matches LCEVC_ColorRange
typedef enum LdpColorRange
{
    LdpColorRangeUnknown = 0,

    LdpColorRangeFull = 1,
    LdpColorRangeLimited = 2,

    LdpColorRangeCount,
    LdpColorRangeForceInt32 = 0x7ffffff,
} LdpColorRange;

// Matches LCEVC_ColorFormat
typedef enum LdpColorFormat
{
    LdpColorFormatUnknown = 0,

    LdpColorFormatI420_8 = 1001,
    LdpColorFormatI420_10_LE = 1002,
    LdpColorFormatI420_12_LE = 1003,
    LdpColorFormatI420_14_LE = 1004,
    LdpColorFormatI420_16_LE = 1005,

    LdpColorFormatI422_8 = 1201,
    LdpColorFormatI422_10_LE = 1202,
    LdpColorFormatI422_12_LE = 1203,
    LdpColorFormatI422_14_LE = 1204,
    LdpColorFormatI422_16_LE = 1205,

    LdpColorFormatI444_8 = 1401,
    LdpColorFormatI444_10_LE = 1402,
    LdpColorFormatI444_12_LE = 1403,
    LdpColorFormatI444_14_LE = 1404,
    LdpColorFormatI444_16_LE = 1405,

    LdpColorFormatNV12_8 = 2001,
    LdpColorFormatNV21_8 = 2002,

    LdpColorFormatRGB_8 = 3001,
    LdpColorFormatBGR_8 = 3002,
    LdpColorFormatRGBA_8 = 3003,
    LdpColorFormatBGRA_8 = 3004,
    LdpColorFormatARGB_8 = 3005,
    LdpColorFormatABGR_8 = 3006,

    LdpColorFormatRGBA_10_2_LE = 4001,

    LdpColorFormatGRAY_8 = 5001,
    LdpColorFormatGRAY_10_LE = 5002,
    LdpColorFormatGRAY_12_LE = 5003,
    LdpColorFormatGRAY_14_LE = 5004,
    LdpColorFormatGRAY_16_LE = 5005,

    LdpColorFormatForceInt32 = 0x7fffffff,
} LdpColorFormat;

// Matches LCEVC_ColorRange
typedef enum LdpColorPrimaries
{
    LdpColorPrimariesReserved0 = 0,
    LdpColorPrimariesBT709 = 1,
    LdpColorPrimariesUnspecified = 2,
    LdpColorPrimariesReserved3 = 3,
    LdpColorPrimariesBT470M = 4,
    LdpColorPrimariesBT470BG = 5,
    LdpColorPrimariesBT601_NTSC = 6,
    LdpColorPrimariesSMPTE240 = 7,
    LdpColorPrimariesGENERIC_FILM = 8,
    LdpColorPrimariesBT2020 = 9,
    LdpColorPrimariesXYZ = 10,
    LdpColorPrimariesSMPTE431 = 11,
    LdpColorPrimariesSMPTE432 = 12,
    LdpColorPrimariesReserved13 = 13,
    LdpColorPrimariesReserved14 = 14,
    LdpColorPrimariesReserved15 = 15,
    LdpColorPrimariesReserved16 = 16,
    LdpColorPrimariesReserved17 = 17,
    LdpColorPrimariesReserved18 = 18,
    LdpColorPrimariesReserved19 = 19,
    LdpColorPrimariesReserved20 = 20,
    LdpColorPrimariesReserved21 = 21,
    LdpColorPrimariesP22 = 22,

    LdpColorPrimariesCount,
    LdpColorPrimariesForceInt32 = 0x7ffffff,

} LdpColorPrimaries;

// Matches LCEVC_TransferCharacteristics
typedef enum LdpTransferCharacteristics
{
    LdpTransferCharacteristicsReserved0 = 0,
    LdpTransferCharacteristicsBT709 = 1,
    LdpTransferCharacteristicsUnspecified = 2,
    LdpTransferCharacteristicsReserved3 = 3,
    LdpTransferCharacteristicsGAMMA22 = 4,
    LdpTransferCharacteristicsGAMMA28 = 5,
    LdpTransferCharacteristicsBT601 = 6,
    LdpTransferCharacteristicsSMPTE240 = 7,
    LdpTransferCharacteristicsLINEAR = 8,
    LdpTransferCharacteristicsLOG100 = 9,
    LdpTransferCharacteristicsLOG100_SQRT10 = 10,
    LdpTransferCharacteristicsIEC61966 = 11,
    LdpTransferCharacteristicsBT1361 = 12,
    LdpTransferCharacteristicsSRGBSYCC = 13,
    LdpTransferCharacteristicsBT2020_10BIT = 14,
    LdpTransferCharacteristicsBT2020_12BIT = 15,
    LdpTransferCharacteristicsPQ = 16,
    LdpTransferCharacteristicsSMPTE428 = 17,
    LdpTransferCharacteristicsHLG = 18,

    LdpTransferCharacteristicsCount,
    LdpTransferCharacteristicsForceInt32 = 0x7ffffff,
} LdpTransferCharacteristics;

// Matches LCEVC_MatrixCoefficients
typedef enum LdpMatrixCoefficients
{
    LdpMatrixCoefficientsIDENTITY = 0,
    LdpMatrixCoefficientsBT709 = 1,
    LdpMatrixCoefficientsUnspecified = 2,
    LdpMatrixCoefficientsReserved3 = 3,
    LdpMatrixCoefficientsUSFCC = 4,
    LdpMatrixCoefficientsBT470_BG = 5,
    LdpMatrixCoefficientsBT601_NTSC = 6,
    LdpMatrixCoefficientsSMPTE240 = 7,
    LdpMatrixCoefficientsYCGCO = 8,
    LdpMatrixCoefficientsBT2020_NCL = 9,
    LdpMatrixCoefficientsBT2020_CL = 10,
    LdpMatrixCoefficientsSMPTE2085 = 11,
    LdpMatrixCoefficientsCHROMATICITY_NCL = 12,
    LdpMatrixCoefficientsCHROMATICITY_CL = 13,
    LdpMatrixCoefficientsICTCP = 14,

    LdpMatrixCoefficientsCount,
    LdpMatrixCoefficientsForceInt32 = 0x7ffffff,
} LdpMatrixCoefficients;

typedef enum LdpFixedPoint
{
    LdpFPU8 = 0, /**< U8.0  (uint8_t) */
    LdpFPU10,    /**< U10.0 (uint16_t) */
    LdpFPU12,    /**< U12.0 (uint16_t) */
    LdpFPU14,    /**< U14.0 (uint16_t) */
    LdpFPS8,     /**< S8.7  (int16_t) */
    LdpFPS10,    /**< S10.5 (int16_t) */
    LdpFPS12,    /**< S12.3 (int16_t) */
    LdpFPS14,    /**< S14.1 (int16_t) */
    LdpFPCount,
    LdpFPUnsignedCount = 4

} LdpFixedPoint;

typedef struct LdpAccelBuffer LdpAccelBuffer;

// Matches LCEVC_DecodeInformation
typedef struct LdpDecodeInformation
{
    uint64_t timestamp;
    bool hasBase;
    bool hasEnhancement;
    bool skipped;
    bool enhanced;

    uint32_t baseWidth;
    uint32_t baseHeight;
    uint8_t baseBitdepth;

    void* userData;
} LdpDecodeInformation;

// Stores sample_aspect_ratio_num and sample_aspect_ratio_den
typedef struct LdpAspectRatio
{
    uint32_t numerator;
    uint32_t denominator;
} LdpAspectRatio;

// Matches LCEVC_PictureBufferDesc
typedef struct LdpPictureBufferDesc
{
    uint8_t* data;
    uint32_t byteSize;
    LdpAccelBuffer* accelBuffer;
    LdpAccess access;
} LdpPictureBufferDesc;

// Replicates LCEVC_PicturePlaneDesc
typedef struct LdpPicturePlaneDesc
{
    uint8_t* firstSample;
    uint32_t rowByteStride;
} LdpPicturePlaneDesc;

// Replicates LCEVC_HDRStaticInfo
typedef struct LdpHDRStaticInfo
{
    uint16_t displayPrimariesX0;
    uint16_t displayPrimariesY0;
    uint16_t displayPrimariesX1;
    uint16_t displayPrimariesY1;
    uint16_t displayPrimariesX2;
    uint16_t displayPrimariesY2;
    uint16_t whitePointX;
    uint16_t whitePointY;
    uint16_t maxDisplayMasteringLuminance;
    uint16_t minDisplayMasteringLuminance;
    uint16_t maxContentLightLevel;
    uint16_t maxFrameAverageLightLevel;
} LdpHDRStaticInfo;

// Matches LCEVC_PictureDesc
typedef struct LdpPictureDesc
{
    uint32_t width;
    uint32_t height;

    LdpColorFormat colorFormat;
    LdpColorRange colorRange;
    LdpColorPrimaries colorPrimaries;
    LdpMatrixCoefficients matrixCoefficients;
    LdpTransferCharacteristics transferCharacteristics;
    LdpHDRStaticInfo hdrStaticInfo;

    uint32_t sampleAspectRatioNum;
    uint32_t sampleAspectRatioDen;

    uint32_t cropTop;
    uint32_t cropBottom;
    uint32_t cropLeft;
    uint32_t cropRight;
} LdpPictureDesc;

//
//
LdcReturnCode ldpDefaultPictureDesc(LdpPictureDesc* pictureDesc, LdpColorFormat format,
                                    uint32_t width, uint32_t height);

// Use for layouts
//
typedef enum LdpColorSpace
{
    LdpColorSpaceYUV = 0,
    LdpColorSpaceRGB = 1,
    LdpColorSpaceGreyscale = 2,

    LdpColorSpaceCount,
    LdpColorSpaceForceInt32 = 0x7ffffff,
} LdpColorSpace;

#ifdef __cplusplus
}
#endif

// C++ helpers
//
#ifdef __cplusplus
bool operator==(const LdpHDRStaticInfo& lhs, const LdpHDRStaticInfo& rhs);
bool operator==(const LdpPictureDesc& lhs, const LdpPictureDesc& rhs);
bool operator==(const LdpPicturePlaneDesc& lhs, const LdpPicturePlaneDesc& rhs);
bool operator==(const LdpPictureBufferDesc& lhs, const LdpPictureBufferDesc& rhs);

static inline bool operator!=(const LdpHDRStaticInfo& lhs, const LdpHDRStaticInfo& rhs)
{
    return !(lhs == rhs);
};
static inline bool operator!=(const LdpPictureDesc& lhs, const LdpPictureDesc& rhs)
{
    return !(lhs == rhs);
};
static inline bool operator!=(const LdpPicturePlaneDesc& lhs, const LdpPicturePlaneDesc& rhs)
{
    return !(lhs == rhs);
};
static inline bool operator!=(const LdpPictureBufferDesc& lhs, const LdpPictureBufferDesc& rhs)
{
    return !(lhs == rhs);
};
#endif

    // NOLINTEND(modernize-use-using)

#endif // VN_LCEVC_PIPELINE_TYPES_H
