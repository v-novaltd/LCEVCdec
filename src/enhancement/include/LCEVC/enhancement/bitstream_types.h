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

#ifndef VN_LCEVC_ENHANCEMENT_BITSTREAM_TYPES_H
#define VN_LCEVC_ENHANCEMENT_BITSTREAM_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

/*! \brief upscale kernel */
typedef struct LdeKernel
{
    int16_t coeffs[2][8]; /**< upscale kernels of length 'len', ordered with forward kernel first. */
    uint8_t length;       /**< length (taps) of upscale kernels */
    bool approximatedPA; /**< true if predicted-average computation has been pre-baked into this kernel */
} LdeKernel;

typedef struct LdeCrop
{
    uint16_t left;   /**< Number of pixels from the left edge to crop for a plane. */
    uint16_t right;  /**< Number of pixels from the right edge to crop for a plane. */
    uint16_t top;    /**< Number of pixels from the top edge to crop for a plane. */
    uint16_t bottom; /**< Number of pixels from the bottom edge to crop for a plane. */
} LdeCrop;

/*! \brief Various constants used by the quantizer. */
enum LdeQuantConstants
{
    QMinStepWidth = 1,
    QMaxStepWidth = 32767
};

/*! \brief Chroma subsampling of the LCEVC stream
 *
 *  \note This is not used to determine if enhancement is present on Chroma planes
 *        just the type of subsampling for scaling and image operations. */
typedef enum LdeChroma
{
    CTMonochrome = 0, /**< No subsampling. */
    CT420,            /**< 4:2:0 subsampling */
    CT422,            /**< 4:2:2 subsampling */
    CT444,            /**< 4:4:4 subsampling */
    CTCount           /**< Number of subsampling types. */
} LdeChroma;

/*! \brief Previous revisions of the LCEVC MPEG-5 Part 2 standard require small changes to parse,
 *         internal version numbers are given to the revisions here to allow backwards compatibility
 */
typedef enum LdeBitstreamVersion
{
    BitstreamVersionInitial = 0,
    BitstreamVersionNewCodeLengths = 1,
    BitstreamVersionAlignWithSpec = 2,

    BitstreamVersionCurrent = BitstreamVersionAlignWithSpec,
    BitstreamVersionUnspecified = BitstreamVersionCurrent + 1
} LdeBitstreamVersion;

typedef struct LdeDeblock
{
    uint32_t corner; /**< The corner coefficient to use. */
    uint32_t side;   /**< The side coefficient to use */
} LdeDeblock;

typedef enum LdeBitDepth
{
    Depth8 = 0,
    Depth10,
    Depth12,
    Depth14,
    DepthCount
} LdeBitDepth;

typedef enum LdePictureType
{
    PTFrame,
    PTField,
} LdePictureType;

typedef enum LdeFieldType
{
    FTTop,
    FTBottom
} LdeFieldType;

typedef enum LdeUpscaleType
{
    USNearest,
    USLinear,
    USCubic,
    USModifiedCubic,

    USAdaptiveCubic,
    USReserved1,
    USReserved2,
    USUnspecified,

    /* These are non-standard. */
    USLanczos,
    USCubicPrediction,
    USMishus,
} LdeUpscaleType;

typedef enum LdeDitherType
{
    DTNone,
    DTUniform
} LdeDitherType;

typedef enum LdePlanesType
{
    PlanesY,
    PlanesYUV
} LdePlanesType;

typedef enum LdeTransformType
{
    TransformDD,
    TransformDDS,

    TransformCount
} LdeTransformType;

typedef enum LdeLOQIndex
{
    LOQ0 = 0,
    LOQ1 = 1,
    LOQ2 = 2,
    LOQMaxCount = 3,        /* This is the maximum number of LOQs accounting for scaling modes */
    LOQEnhancedCount = LOQ2 /* This is the number of processed LOQs with enhancement */
} LdeLOQIndex;

typedef enum LdeQuantMatrixMode
{
    QMMUsePrevious = 0,
    QMMUseDefault,
    QMMCustomBoth,
    QMMCustomLOQ0, /* LOQ1 uses previous in this case */
    QMMCustomLOQ1, /* LOQ0 uses previous in this case */
    QMMCustomBothUnique
} LdeQuantMatrixMode;

/* Block sizes. A block (if temporal is on) is 32x32 pixels, and 32 = 1 << 5. */
enum LdeBlockConstants
{
    BSTemporal = 32,
    BSTemporalShift = 5,
};

enum LdeResidualConstants
{
    RCLayerCountDD = 4,
    RCLayerCountDDS = 16,
    RCQuantMatrixCount = 3,
    RCLayerMaxCount = RCLayerCountDDS,
    RCMaxPlanes = 3
};

typedef struct LdeQuantMatrix
{
    uint8_t values[LOQEnhancedCount][RCLayerCountDDS];
    bool set;
} LdeQuantMatrix;

typedef enum LdeScalingMode
{
    Scale0D,
    Scale1D,
    Scale2D
} LdeScalingMode;

typedef enum LdeTileDimensions
{
    TDTNone = 0,
    TDT512x256,
    TDT1024x512,
    TDTCustom
} LdeTileDimensions;

typedef enum LdeTileCompressionSizePerTile
{
    TCSPTTNone = 0,
    TCSPTTPrefix,
    TCSPTTPrefixOnDiff
} LdeTileCompressionSizePerTile;

typedef enum LdeUserDataMode
{
    UDMNone = 0,
    UDMWith2Bits,
    UDMWith6Bits
} LdeUserDataMode;

enum LdeUserDataConstants
{
    UDCLayerIndexDD = 1,
    UDCLayerIndexDDS = 5,
    UDCShift2 = 2,
    UDCShift6 = 6
};

typedef struct LdeUserDataConfig
{
    bool enabled;
    uint32_t layerIndex;
    int16_t shift;
} LdeUserDataConfig;

typedef enum LdeSharpenType
{
    STDisabled = 0,
    STInLoop = 1,
    STOutOfLoop = 2,
} LdeSharpenType;

typedef enum LdeDequantOffsetMode
{
    DQMDefault = 0,
    DQMConstOffset
} LdeDequantOffsetMode;

typedef enum LdeNALType
{
    NTError = 0,
    NTNonIDR = 28,
    NTIDR = 29,
} LdeNALType;

#endif // VN_LCEVC_ENHANCEMENT_BITSTREAM_TYPES_H
