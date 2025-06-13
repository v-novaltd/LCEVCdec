/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#ifndef VN_DEC_CORE_TYPES_H_
#define VN_DEC_CORE_TYPES_H_

#include "common/platform.h"

#include <assert.h>
#include <LCEVC/PerseusDecoder.h>
#include <math.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

typedef struct Memory* Memory_t;

/*------------------------------------------------------------------------------*/

/*! \brief Various constants used by the quantizer. */
enum QuantConstants
{
    QMinStepWidth = 1,
    QMaxStepWidth = 32767
};

/*! \brief Chroma subsampling the LCEVC stream signals.
 *
 *  \note This is not used to determine if enhancement is present on Chroma planes
 *        just the type of subsampling for scaling and image ops. */
typedef enum Chroma
{
    CTMonochrome = 0, /**< No subsampling. */
    CT420,            /**< 4:2:0 subsampling */
    CT422,            /**< 4:2:2 subsampling */
    CT444,            /**< 4:4:4 subsampling */
    CTCount           /**< Number of subsampling types. */
} Chroma_t;

/*! \brief Returns a string representation of a supplied `Chroma_t` value. */
const char* chromaToString(Chroma_t type);

/*! \brief Converts the internal chroma type to the public API equivalent. */
perseus_colourspace chromaToAPI(Chroma_t type);

/*! \brief Determines the horizontal shift needed to convert the luma plane resolution
 *         to the chroma plane resolution given the type. */
int32_t chromaShiftWidth(Chroma_t type);

/*! \brief Determines the vertical shift needed to convert the luma plane resolution
 *         to the chroma plane resolution given the type. */
int32_t chromaShiftHeight(Chroma_t type);

typedef enum BitDepth
{
    Depth8 = 0,
    Depth10,
    Depth12,
    Depth14,
    DepthCount
} BitDepth_t;

const char* bitdepthToString(BitDepth_t type);

typedef enum PictureType
{
    PTFrame,
    PTField,
} PictureType_t;

const char* pictureTypeToString(PictureType_t type);
perseus_picture_type pictureTypeToAPI(PictureType_t type);

typedef enum FieldType
{
    FTTop,
    FTBottom
} FieldType_t;

const char* fieldTypeToString(FieldType_t type);

typedef enum UpscaleType
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
} UpscaleType_t;

const char* upscaleTypeToString(UpscaleType_t type);
perseus_upsample upscaleTypeToAPI(UpscaleType_t upscale);
UpscaleType_t upscaleTypeFromAPI(perseus_upsample upsample);

typedef enum DitherType
{
    DTNone,
    DTUniform
} DitherType_t;

const char* ditherTypeToString(DitherType_t type);
perseus_dither_type ditherTypeToAPI(DitherType_t type);

typedef enum PlanesType
{
    PlanesY,
    PlanesYUV
} PlanesType_t;

const char* planesTypeToString(PlanesType_t type);
int32_t planesTypePlaneCount(PlanesType_t type);

typedef enum TransformType
{
    TransformDD,
    TransformDDS,

    TransformCount
} TransformType_t;

const char* transformTypeToString(TransformType_t type);
int32_t transformTypeLayerCount(TransformType_t type);
TransformType_t transformTypeFromLayerCount(int32_t count);
int32_t transformTypeDimensions(TransformType_t type);

typedef enum LOQIndex
{
    LOQ0 = 0,
    LOQ1 = 1,
    LOQ2 = 2,
    LOQMaxCount,            /* This is the maximum number of LOQs accounting for scaling modes */
    LOQEnhancedCount = LOQ2 /* This is the number of processed LOQs with enhancement */
} LOQIndex_t;

const char* loqIndexToString(LOQIndex_t loq);
LOQIndex_t loqIndexFromAPI(perseus_loq_index loq);

/*! \brief Flags used to state the available "acceleration" features of the CPU
 *         (i.e. SIMD). */
typedef enum CPUAccelerationFlag
{
    CAFSSE = (1 << 0),  /**!< CPU supports SSE4.2 */
    CAFAVX2 = (1 << 1), /**!< CPU supports AVX2 */
    CAFNEON = (1 << 2), /**!< CPU supports NEON */

    CAFNone = 0x0,               /**!< Special flag for no CPU features. */
    CAFAll = (int32_t)0xFFFFFFFF /**!< Special flag for any CPU feature.  */
} CPUAccelerationFlag_t;

typedef uint32_t CPUAccelerationFeatures_t;

bool accelerationFeatureEnabled(CPUAccelerationFeatures_t features, CPUAccelerationFlag_t flag);

typedef enum QuantMatrixMode
{
    QMMUsePrevious = 0,
    QMMUseDefault,
    QMMCustomBoth,
    QMMCustomLOQ0, /* LOQ1 uses previous in this case */
    QMMCustomLOQ1, /* LOQ0 uses previous in this case */
    QMMCustomBothUnique
} QuantMatrixMode_t;

const char* quantMatrixModeToString(QuantMatrixMode_t mode);

/* Block sizes. A block (if temporal is on) is 32x32 pixels, and 32 = 1 << 5. */
enum BlockConstants
{
    BSTemporal = 32,
    BSTemporalShift = 5,
};

enum ResidualConstants
{
    RCLayerCountDD = 4,
    RCLayerCountDDS = 16,
    RCQuantMatrixCount = 3,
    RCLayerMaxCount = RCLayerCountDDS,
    RCMaxPlanes = 3
};

enum MetaCmdBufferConstants
{
    MaxCmdBufferEntryPoints = 16
};

typedef enum TemporalSignal
{
    TSInter = 0, /* i.e. add */
    TSIntra = 1, /* i.e. set */
    TSCount
} TemporalSignal_t;

typedef enum ScalingMode
{
    Scale0D,
    Scale1D,
    Scale2D
} ScalingMode_t;

const char* scalingModeToString(ScalingMode_t mode);
perseus_scaling_mode scalingModeToAPI(ScalingMode_t mode);

typedef enum TileDimensions
{
    TDTNone = 0,
    TDT512x256,
    TDT1024x512,
    TDTCustom
} TileDimensions_t;

const char* tileDimensionsToString(TileDimensions_t type);
int32_t tileDimensionsFromType(TileDimensions_t type, uint16_t* width, uint16_t* height);

typedef enum TileCompressionSizePerTile
{
    TCSPTTNone = 0,
    TCSPTTPrefix,
    TCSPTTPrefixOnDiff
} TileCompressionSizePerTile_t;

typedef enum UserDataMode
{
    UDMNone = 0,
    UDMWith2Bits,
    UDMWith6Bits
} UserDataMode_t;

enum UserDataConstants
{
    UDCLayerIndexDD = 1,
    UDCLayerIndexDDS = 5,
    UDCShift2 = 2,
    UDCShift6 = 6
};

typedef struct UserDataConfig
{
    bool enabled;
    uint32_t layerIndex;
    int16_t shift;
} UserDataConfig_t;

const char* userDataModeToString(UserDataMode_t mode);

typedef enum SharpenType
{
    STDisabled = 0,
    STInLoop = 1,
    STOutOfLoop = 2,
} SharpenType_t;

const char* sharpenTypeToString(SharpenType_t type);
perseus_s_mode sharpenTypeToAPI(SharpenType_t type);

typedef enum DequantOffsetMode
{
    DQMDefault = 0,
    DQMConstOffset
} DequantOffsetMode_t;

const char* dequantOffsetModeToString(DequantOffsetMode_t mode);

typedef enum NALType
{
    NTError = 0,
    NTNonIDR = 28,
    NTIDR = 29,
} NALType_t;

const char* nalTypeToString(NALType_t type);

/*------------------------------------------------------------------------------*/

/*! \brief Rounds upwards a value to a multiple of.
 *  \param value     value to round up.
 *  \param multiple  multiple to round up to
 *  \return The value rounded up to the multiple. */
int32_t roundupToMultiple(int32_t value, int32_t multiple);

/*! \brief Rounds upwards a fraction to a multiple of
 *  \param numerator    numerator of the fraction
 *  \param denominator  denominator of the fraction
 *  \param multiple  multiple to round up to
 *  \return The fraction value rounded up to the multiple. */
int32_t roundupFractionToMultiple(int32_t numerator, int32_t denominator, int32_t multiple);

/*------------------------------------------------------------------------------*/

BitDepth_t bitdepthFromAPI(perseus_bitdepth external);
perseus_bitdepth bitdepthToAPI(BitDepth_t type);

/*------------------------------------------------------------------------------*/

/* Mirrors the public API.  */
typedef enum Interleaving
{
    ILNone, /**< Surface is planar */
    ILYUYV, /**< Surface is YUV422 of YUYV */
    ILNV12, /**< Surface is YUV420 of UV */
    ILUYVY, /**< Surface is YUV422 of UYVY */
    ILRGB,  /**< Surface is interleaved RGB channels */
    ILRGBA, /**< Surface is interleaved RGBA channels */
    ILCount
} Interleaving_t;

uint32_t interleavingGetChannelCount(Interleaving_t interleaving);

int32_t interleavingGetChannelSkipOffset(Interleaving_t interleaving, uint32_t channelIdx,
                                         uint32_t* skip, uint32_t* offset);

Interleaving_t interleavingFromAPI(perseus_interleaving interleaving);
const char* interleavingToString(Interleaving_t interleaving);

/*------------------------------------------------------------------------------*/

typedef enum FixedPoint
{
    FPU8 = 0, /**< U8.0  (uint8_t) */
    FPU10,    /**< U10.0 (uint16_t) */
    FPU12,    /**< U12.0 (uint16_t) */
    FPU14,    /**< U14.0 (uint16_t) */
    FPS8,     /**< S8.7  (int16_t) */
    FPS10,    /**< S10.5 (int16_t) */
    FPS12,    /**< S12.3 (int16_t) */
    FPS14,    /**< S14.1 (int16_t) */
    FPCount,
    FPUnsignedCount = 4
} FixedPoint_t;

FixedPoint_t fixedPointFromBitdepth(BitDepth_t depth);
uint32_t fixedPointByteSize(FixedPoint_t type);
FixedPoint_t fixedPointLowPrecision(FixedPoint_t type);
FixedPoint_t fixedPointHighPrecision(FixedPoint_t type);
bool fixedPointIsSigned(FixedPoint_t type);
const char* fixedPointToString(FixedPoint_t type);
bool fixedPointIsValid(FixedPoint_t type);

BitDepth_t bitdepthFromFixedPoint(FixedPoint_t type);

/* This function is used help convert upsample kernel coeffs for 8bit pipeline. 64 is 2^6. */
static inline int16_t fpS15ToS7(int16_t val) { return (int16_t)((val + 64) >> 7); }

static inline int16_t fpU16ToS16(uint16_t val, int16_t shift)
{
    int16_t res = (int16_t)(val << shift);
    res -= 0x4000;
    return res;
}

static inline uint16_t fpS16ToU16(int32_t val, int16_t shift, int16_t rounding, int16_t signOffset,
                                  uint16_t maxValue)
{
    int16_t res = (int16_t)(((val + rounding) >> shift) + signOffset);

    if (res > (int16_t)maxValue) {
        return maxValue;
    }

    if (res < 0x00) {
        return 0x00;
    }

    return (uint16_t)res;
}

static inline int16_t fpU8ToS8(uint8_t val) { return fpU16ToS16(val, 7); }
static inline int16_t fpU10ToS10(uint16_t val) { return fpU16ToS16(val, 5); }
static inline int16_t fpU12ToS12(uint16_t val) { return fpU16ToS16(val, 3); }
static inline int16_t fpU14ToS14(uint16_t val) { return fpU16ToS16(val, 1); }

static inline uint8_t fpS8ToU8(int32_t val)
{
    return (uint8_t)fpS16ToU16(val, 7, 0x40, 0x80, 0xFF);
}
static inline uint16_t fpS10ToU10(int32_t val) { return fpS16ToU16(val, 5, 0x10, 0x200, 0x3FF); }
static inline uint16_t fpS12ToU12(int32_t val) { return fpS16ToU16(val, 3, 0x4, 0x800, 0xFFF); }
static inline uint16_t fpS14ToU14(int32_t val) { return fpS16ToU16(val, 1, 0x1, 0x2000, 0x3FFF); }

/*! \brief Maps a floating-point value in the range of 0.0 to 1.0 onto an
 *         unsigned 16-bit integer. */
uint16_t f32ToU16(float val);

typedef int16_t (*FixedPointPromotionFunction_t)(uint16_t);
typedef uint16_t (*FixedPointDemotionFunction_t)(int32_t);

FixedPointPromotionFunction_t fixedPointGetPromotionFunction(FixedPoint_t unsignedFP);
FixedPointDemotionFunction_t fixedPointGetDemotionFunction(FixedPoint_t unsignedFP);

/*------------------------------------------------------------------------------*/

static inline uint16_t alignU16(uint16_t value, uint16_t alignment)
{
    if (alignment == 0) {
        return value;
    }

    return (value + (alignment - 1)) & ~(alignment - 1);
}

static inline uint32_t alignU32(uint32_t value, uint32_t alignment)
{
    if (alignment == 0) {
        return value;
    }

    return (value + (alignment - 1)) & ~(alignment - 1);
}

static inline uint16_t clampU16(uint16_t value, uint16_t minValue, uint16_t maxValue)
{
    return (value < minValue) ? minValue : (value > maxValue) ? maxValue : value;
}

static inline uint32_t clampU32(uint32_t value, uint32_t minValue, uint32_t maxValue)
{
    return (value < minValue) ? minValue : (value > maxValue) ? maxValue : value;
}

static inline int32_t clampS32(int32_t value, int32_t minValue, int32_t maxValue)
{
    return (value < minValue) ? minValue : (value > maxValue) ? maxValue : value;
}

static inline int64_t clampS64(int64_t value, int64_t minValue, int64_t maxValue)
{
    return (value < minValue) ? minValue : (value > maxValue) ? maxValue : value;
}

static inline float clampF32(float value, float minValue, float maxValue)
{
    return (value < minValue) ? minValue : (value > maxValue) ? maxValue : value;
}

static inline float floorF32(float value) { return floorf(value); }

static inline int32_t minS16(int16_t x, int16_t y) { return x < y ? x : y; }

static inline int32_t minS32(int32_t x, int32_t y) { return x < y ? x : y; }

static inline uint8_t minU8(uint8_t x, uint8_t y) { return x < y ? x : y; }

static inline uint16_t minU16(uint16_t x, uint16_t y) { return x < y ? x : y; }

static inline uint32_t minU32(uint32_t x, uint32_t y) { return x < y ? x : y; }

static inline uint64_t minU64(uint64_t x, uint64_t y) { return x < y ? x : y; }

static inline int32_t maxS32(int32_t x, int32_t y) { return x > y ? x : y; }

static inline uint32_t maxU32(uint32_t x, uint32_t y) { return x > y ? x : y; }

static inline uint64_t maxU64(uint64_t x, uint64_t y) { return x > y ? x : y; }

static inline size_t minSize(size_t x, size_t y) { return x < y ? x : y; }

static inline size_t maxSize(size_t x, size_t y) { return x > y ? x : y; }

static inline uint8_t saturateU8(int32_t value)
{
    const int32_t result = clampS32(value, 0, 255);
    return (uint8_t)result;
}

/* S15 saturation is for the END of upscaling (this is the thing that you apply residuals to, so
 * the maximum and minimum values must be one maximum-residual apart). */
static inline int16_t saturateS15(int32_t value)
{
    const int32_t result = minS32(maxS32(value, -16384), 16383);
    return (int16_t)result;
}

/* S16 saturation is for RESIDUALS (and general use demoting int32_ts to int16_ts). */
static inline int16_t saturateS16(int32_t value)
{
    const int32_t result = clampS32(value, -32768, 32767);
    return (int16_t)result;
}

static inline uint16_t saturateUN(int32_t value, uint16_t maxValue)
{
    return (value < 0) ? 0 : (value > maxValue) ? maxValue : (uint16_t)value;
}

static inline int32_t divideCeilS32(int32_t numerator, int32_t denominator)
{
    /* This function is for ceiling a positive division. */
    assert(numerator > 0);
    assert(denominator > 0);

    if (denominator == 0) {
        return 0;
    }

    return (numerator + denominator - 1) / denominator;
}

/* Determines if a 32-bit unsigned value is a power of 2.
 *
 * \param value   Value to check for power of 2.
 *
 * \return true if value is a power of 2, otherwise false.
 */
bool isPow2(uint32_t value);

/* Truncated alignment of a 32-bit unsigned value.
 *
 * For example:
 *     `align_trunc_u32(850, 16) == 848`
 *
 * This function requires that alignment is a power of 2 number and not zero.
 *
 * \param value   The value to align.
 * \param alignment The alignment to align with, must be a power of 2.
 *
 * return The truncated aligned value.
 */
uint32_t alignTruncU32(uint32_t value, uint32_t alignment);

/*------------------------------------------------------------------------------*/

/* \brief Extract bits from the middle of a number.
 *
 * \param startBit The first bit that you want, inclusive
 * \param endBit   The last bit that you want, exclusive
 *
 * \return         The bits in the specified interval, right-aligned. For example,
 *                 extract(0xf0f1f2f3, 8, 20) returns 0x00000f1f
 */
static inline uint32_t extractBits(uint32_t data, uint8_t startBit, uint8_t endBit)
{
    const uint32_t mask = (1 << (endBit - startBit)) - 1;
    return (data >> (32 - endBit)) & mask;
}

/*------------------------------------------------------------------------------*/

/* \brief Helper function to perform a deep copy of a str.
 *
 * This copies str to dst if str is valid, and its length is >= 1. If either
 * condition fails, then dst is set to NULL, and no error is reported.
 *
 * If dst is assigned, the user takes responsibility for calling free() on the
 * memory.
 *
 * \param str Source string to copy from.
 * \param dst The destination to allocate and copy string into.
 *
 * \return 0 on success, otherwise -1 */
int32_t strcpyDeep(Memory_t memory, const char* str, const char** dst);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_TYPES_H_ */
