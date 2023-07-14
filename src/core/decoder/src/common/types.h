/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_TYPES_H_
#define VN_DEC_CORE_TYPES_H_

#include "common/platform.h"

#include <LCEVC/PerseusDecoder.h>

/*------------------------------------------------------------------------------*/

typedef struct Context Context_t;

/*------------------------------------------------------------------------------*/

/*! \brief Various constants used by the quantizer. */
enum QuantConstants
{
    QMinStepWidth = 4,
    QMaxStepWidth = 32767,
    QDefaultChromaSWMultiplier = 64
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
    TransformDDS
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
    QMMCustomLOQ0,
    QMMCustomLOQ1,
    QMMCustomBothUnique
} QuantMatrixMode_t;

const char* quantMatrixModeToString(QuantMatrixMode_t mode);

enum BlockConstants
{
    BSTemporal = 32,
};

enum ResidualConstants
{
    RCLayerCountDD = 4,
    RCLayerCountDDS = 16,
    RCQuantMatrixCount = 3,
    RCLayerMaxCount = RCLayerCountDDS,
    RCMaxPlanes = 3
};

typedef enum TemporalSignal
{
    TSInter = 0,
    TSIntra = 1,
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
                                         int32_t* skip, int32_t* offset);

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

int16_t fpS15ToS7(int16_t val);
int16_t fpU16ToS16(uint16_t val, int16_t shift);
uint16_t fpS16ToU16(int32_t val, int16_t shift, int16_t rounding, int16_t signOffset, uint16_t maxValue);

int16_t fpU8ToS8(uint8_t val);
int16_t fpU10ToS10(uint16_t val);
int16_t fpU12ToS12(uint16_t val);
int16_t fpU14ToS14(uint16_t val);

uint8_t fpS8ToU8(int32_t val);
uint16_t fpS10ToU10(int32_t val);
uint16_t fpS12ToU12(int32_t val);
uint16_t fpS14ToU14(int32_t val);

/*! \brief Maps a floating-point value in the range of 0.0 to 1.0 onto an
 *         unsigned 16-bit integer. */
uint16_t f32ToU16(float val);

typedef int16_t (*FixedPointPromotionFunction_t)(uint16_t);
typedef uint16_t (*FixedPointDemotionFunction_t)(int32_t);

FixedPointPromotionFunction_t fixedPointGetPromotionFunction(FixedPoint_t unsignedFP);
FixedPointDemotionFunction_t fixedPointGetDemotionFunction(FixedPoint_t unsignedFP);

/*------------------------------------------------------------------------------*/

uint16_t alignU16(uint16_t value, uint16_t alignment);
uint32_t alignU32(uint32_t value, uint32_t alignment);
int32_t clampS32(int32_t value, int32_t minValue, int32_t maxValue);
uint32_t clampU32(uint32_t value, uint32_t minValue, uint32_t maxValue);
float clampF32(float value, float minValue, float maxValue);
float floorF32(float value);
int32_t minS32(int32_t x, int32_t y);
uint32_t minU32(uint32_t x, uint32_t y);
uint64_t minU64(uint64_t x, uint64_t y);
int32_t maxS32(int32_t x, int32_t y);
uint64_t maxU64(uint64_t x, uint64_t y);
size_t minSize(size_t x, size_t y);
size_t maxSize(size_t x, size_t y);
uint8_t saturateU8(int32_t value);
int16_t saturateS16(int32_t value);
uint16_t saturateUN(int32_t value, uint16_t maxValue);
int32_t divideCeilS32(int32_t numerator, int32_t denominator);

/* Determines the bit index of the first non-zero bit from MSB to LSB.
 *
 * E.g. if value == 1 return 0.
 *      if value == 10 return 3.
 *
 * If the  value is 0 then 0 is returned. */
size_t bitScanReverse(size_t value);

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

/* \brief Helper function to perform a deep copy of a str.
 *
 * This copies str to dst if str is valid, and it's length is >= 1. If either
 * condition fails, then dst is set to NULL, and no error is reported.
 *
 * If dst is assigned, the user takes responsibility for calling free() on the
 * memory.
 *
 * \param str Source string to copy from.
 * \param dst The destination to allocate and copy string into.
 *
 * \return 0 on success, otherwise -1 */
int32_t strcpyDeep(Context_t* ctx, const char* str, const char** dst);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_TYPES_H_ */