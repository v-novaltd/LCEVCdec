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

#include "common/types.h"

#include "common/memory.h"
#include "context.h"

#include <assert.h>
#include <math.h>

#if VN_OS(WINDOWS)
#include <intrin.h>
#endif

/*------------------------------------------------------------------------------*/

const char* chromaToString(Chroma_t type)
{
    switch (type) {
        case CTMonochrome: return "monochrome";
        case CT420: return "yuv420p";
        case CT422: return "yuv422p";
        case CT444: return "yuv444p";
        case CTCount: break;
    }

    return "Unknown";
}

perseus_colourspace chromaToAPI(Chroma_t type)
{
    switch (type) {
        case CTMonochrome: return PSS_CSP_MONOCHROME;
        case CT420: return PSS_CSP_YUV420P;
        case CT422: return PSS_CSP_YUV422P;
        case CT444: return PSS_CSP_YUV444P;
        case CTCount: break;
    }
    return PSS_CSP_UNSUPPORTED;
}

int32_t chromaShiftWidth(Chroma_t type)
{
    switch (type) {
        case CT420:
        case CT422: return 1;
        case CT444:
        case CTMonochrome:
        case CTCount: break;
    }
    return 0;
}

int32_t chromaShiftHeight(Chroma_t type)
{
    switch (type) {
        case CT420: return 1;
        case CT422:
        case CT444:
        case CTMonochrome:
        case CTCount: break;
    }
    return 0;
}

const char* bitdepthToString(BitDepth_t type)
{
    switch (type) {
        case Depth8: return "8-bit";
        case Depth10: return "10-bit";
        case Depth12: return "12-bit";
        case Depth14: return "14-bit";
        case DepthCount: break;
    }

    return "Unknown";
}

const char* pictureTypeToString(PictureType_t type)
{
    switch (type) {
        case PTFrame: return "frame";
        case PTField: return "field";
    }

    return "Unknown";
}

perseus_picture_type pictureTypeToAPI(PictureType_t type)
{
    switch (type) {
        case PTFrame: return PSS_FRAME;
        case PTField: return PSS_FIELD;
    }

    assert(false);
    return PSS_FRAME;
}

const char* fieldTypeToString(FieldType_t type)
{
    switch (type) {
        case FTTop: return "top";
        case FTBottom: return "bottom";
    }

    return "Unknown";
}

const char* upscaleTypeToString(UpscaleType_t type)
{
    switch (type) {
        case USNearest: return "nearest";
        case USLinear: return "linear";
        case USCubic: return "cubic";
        case USModifiedCubic: return "modifiedcubic";
        case USAdaptiveCubic: return "adaptivecubic";
        case USReserved1: return "reserved1";
        case USReserved2: return "reserved2";
        case USUnspecified: return "unspecified";
        case USCubicPrediction: return "cubicprediction";
        case USMishus: return "mishus";
        case USLanczos: return "lanczos";
    }

    return "Unknown";
}

perseus_upsample upscaleTypeToAPI(UpscaleType_t upscale)
{
    switch (upscale) {
        case USNearest: return PSS_UPS_NEAREST;
        case USLinear: return PSS_UPS_BILINEAR;
        case USCubic: return PSS_UPS_BICUBIC;
        case USAdaptiveCubic: return PSS_UPS_ADAPTIVECUBIC;
        case USCubicPrediction: return PSS_UPS_BICUBICPREDICTION;
        case USMishus: return PSS_UPS_MISHUS;
        case USModifiedCubic: return PSS_UPS_MODIFIEDCUBIC;
        case USLanczos: return PSS_UPS_LANCZOS;
        default:;
    }

    return PSS_UPS_DEFAULT;
}

UpscaleType_t upscaleTypeFromAPI(perseus_upsample upsample)
{
    switch (upsample) {
        case PSS_UPS_DEFAULT: return USMishus;
        case PSS_UPS_NEAREST: return USNearest;
        case PSS_UPS_BILINEAR: return USLinear;
        case PSS_UPS_BICUBIC: return USCubic;
        case PSS_UPS_ADAPTIVECUBIC: return USAdaptiveCubic;
        case PSS_UPS_BICUBICPREDICTION: return USCubicPrediction;
        case PSS_UPS_MISHUS: return USMishus;
        case PSS_UPS_LANCZOS: return USLanczos;
        case PSS_UPS_MODIFIEDCUBIC: return USModifiedCubic;
        default:;
    }

    return USNearest;
}

const char* ditherTypeToString(DitherType_t type)
{
    switch (type) {
        case DTNone: return "none";
        case DTUniform: return "uniform";
    }

    return "Unknown";
}

perseus_dither_type ditherTypeToAPI(DitherType_t type)
{
    switch (type) {
        case DTNone: return PSS_DITHER_NONE;
        case DTUniform: return PSS_DITHER_UNIFORM;
    }

    assert(false);
    return PSS_DITHER_NONE;
}

const char* planesTypeToString(PlanesType_t type)
{
    switch (type) {
        case PlanesY: return "y";
        case PlanesYUV: return "yuv";
    }

    return "Unknown";
}

int32_t planesTypePlaneCount(PlanesType_t type) { return (type == PlanesY) ? 1 : 3; }

const char* transformTypeToString(TransformType_t type)
{
    switch (type) {
        case TransformDD: return "DD";
        case TransformDDS: return "DDS";
        case TransformCount: break;
    }

    return "Unknown";
}

int32_t transformTypeLayerCount(TransformType_t type) { return (type == TransformDD) ? 4 : 16; }

TransformType_t transformTypeFromLayerCount(int32_t count)
{
    if (count == 16) {
        return TransformDDS;
    }

    assert(count == 4);
    return TransformDD;
}

int32_t transformTypeDimensions(TransformType_t type) { return (type == TransformDD) ? 2 : 4; }

const char* loqIndexToString(LOQIndex_t loq)
{
    switch (loq) {
        case LOQ0: return "LOQ-0";
        case LOQ1: return "LOQ-1";
        case LOQ2: return "LOQ-2";
        default:;
    }

    return "Unknown";
}

LOQIndex_t loqIndexFromAPI(perseus_loq_index loq)
{
    switch (loq) {
        case PSS_LOQ_0: return LOQ0;
        case PSS_LOQ_1: return LOQ1;
        case PSS_LOQ_2: return LOQ2;
    }

    assert(false);
    return LOQ0;
}

bool accelerationFeatureEnabled(CPUAccelerationFeatures_t features, CPUAccelerationFlag_t flag)
{
    return (features & flag) == (uint32_t)flag;
}

const char* quantMatrixModeToString(QuantMatrixMode_t mode)
{
    switch (mode) {
        case QMMUsePrevious: return "use_previous";
        case QMMUseDefault: return "use_default";
        case QMMCustomBoth: return "custom_both_same";
        case QMMCustomLOQ0: return "custom_loq0";
        case QMMCustomLOQ1: return "custom_loq1";
        case QMMCustomBothUnique: return "custom_both_unique";
    }

    return "Unknown";
}

const char* scalingModeToString(ScalingMode_t mode)
{
    switch (mode) {
        case Scale0D: return "0D";
        case Scale1D: return "1D";
        case Scale2D: return "2D";
    }

    return "Unknown";
}

perseus_scaling_mode scalingModeToAPI(ScalingMode_t mode)
{
    switch (mode) {
        case Scale0D: return PSS_SCALE_0D;
        case Scale1D: return PSS_SCALE_1D;
        case Scale2D: return PSS_SCALE_2D;
    }

    assert(false);
    return PSS_SCALE_2D;
}

const char* tileDimensionsToString(TileDimensions_t type)
{
    switch (type) {
        case TDTNone: return "none";
        case TDT512x256: return "512x256";
        case TDT1024x512: return "1024x512";
        case TDTCustom: return "custom";
    }

    return "Unknown";
}

int32_t tileDimensionsFromType(TileDimensions_t type, uint16_t* width, uint16_t* height)
{
    assert(width);
    assert(height);

    switch (type) {
        case TDT512x256:
            *width = 512;
            *height = 256;
            break;
        case TDT1024x512:
            *width = 1024;
            *height = 512;
            break;
        case TDTNone:
        case TDTCustom: return -1;
    }

    return 0;
}

const char* userDataModeToString(UserDataMode_t mode)
{
    switch (mode) {
        case UDMNone: return "none";
        case UDMWith2Bits: return "2-bits";
        case UDMWith6Bits: return "6-bits";
    }

    return "Unknown";
}

const char* sharpenTypeToString(SharpenType_t type)
{
    switch (type) {
        case STDisabled: return "disabled";
        case STInLoop: return "in_loop";
        case STOutOfLoop: return "out_of_loop";
    }

    return "Unknown";
}

perseus_s_mode sharpenTypeToAPI(SharpenType_t type)
{
    switch (type) {
        case STDisabled: return PSS_S_MODE_DISABLED;
        case STInLoop: return PSS_S_MODE_IN_LOOP;
        case STOutOfLoop: return PSS_S_MODE_OUT_OF_LOOP;
    }

    assert(false);
    return PSS_S_MODE_DISABLED;
}

const char* dequantOffsetModeToString(DequantOffsetMode_t mode)
{
    switch (mode) {
        case DQMDefault: return "default";
        case DQMConstOffset: return "const_offset";
    }

    return "Unknown";
}

const char* nalTypeToString(NALType_t type)
{
    switch (type) {
        case NTError: return "error";
        case NTIDR: return "IDR";
        case NTNonIDR: return "NonIDR";
    }

    return "unknown";
}

int32_t roundupToMultiple(int32_t value, int32_t multiple)
{
    if (value % multiple) {
        return value + (multiple - (value % multiple));
    }

    return value;
}

int32_t roundupFractionToMultiple(int32_t numerator, int32_t denominator, int32_t multiple)
{
    /* calculate 'numerator' (n) divided by 'denominator' (d) rounded up to the nearest multiple of 'multiple' (m)
       => n/d + r = x * m, where 0 <= r < m
       => n/(d*m) <= x < n/(d*m) + 1 */
    const int32_t dm = denominator * multiple;
    const int32_t quantized = numerator / dm;
    const int32_t result = (numerator % dm) ? quantized + 1 : quantized;
    return result * multiple;
}

/*------------------------------------------------------------------------------*/

BitDepth_t bitdepthFromAPI(perseus_bitdepth external)
{
    switch (external) {
        case PSS_DEPTH_8: return Depth8;
        case PSS_DEPTH_10: return Depth10;
        case PSS_DEPTH_12: return Depth12;
        case PSS_DEPTH_14: return Depth14;
        default: break;
    }

    return DepthCount;
}

perseus_bitdepth bitdepthToAPI(BitDepth_t type)
{
    switch (type) {
        case Depth8: return PSS_DEPTH_8;
        case Depth10: return PSS_DEPTH_10;
        case Depth12: return PSS_DEPTH_12;
        case Depth14: return PSS_DEPTH_14;
        default: break;
    }

    return PSS_DEPTH_8;
}

/*------------------------------------------------------------------------------*/

typedef struct InterleavingInfo
{
    uint32_t channelCount;
    uint32_t componentCount;
    uint32_t offset[4]; /* Initial offset mapping  */
    uint32_t skip[4];
} InterleavingInfo_t;

enum InterleavingConstants
{
    ICUnused = 0
};

/* clang-format off */
static const InterleavingInfo_t kInterleavingInfos[ILCount] = {
	{   /* None (planar) */
		1, 1,
		{0, ICUnused, ICUnused, ICUnused},
		{1, ICUnused, ICUnused, ICUnused}
	},
	{   /* YUYV */
		2, 3,
		{0, 1, 3, ICUnused},
		{2, 4, 4, ICUnused}
	},
	{   /* NV12 */
		2, 2,
		{0, 1, ICUnused, ICUnused},
		{2, 2, ICUnused, ICUnused}
	},
	{   /* UYVY*/
		2, 3,
		{1, 0, 2, ICUnused},
		{2, 4, 4, ICUnused}
	},
	{   /* RGB */
		3, 3,
		{0, 1, 2, ICUnused},
		{3, 3, 3, ICUnused}
	},
	{   /* RGBA */
		4, 4,
		{0, 1, 2, 3},
		{4, 4, 4, 4}
	}
};
/* clang-format on */

uint32_t interleavingGetChannelCount(Interleaving_t interleaving)
{
    assert(interleaving >= 0 && interleaving < ILCount);
    return kInterleavingInfos[interleaving].channelCount;
}

int32_t interleavingGetChannelSkipOffset(Interleaving_t interleaving, uint32_t channelIdx,
                                         uint32_t* skip, uint32_t* offset)
{
    /* channelIdx maps to one of the following forms:
     *
     *  [Y, U, V]    = [0, 1, 2]
     *  [R, G, B]    = [0, 1, 2]
     *  [R, G, B, A] = [0, 1, 2, 3]
     *  [U, V]       = [0, 1]
     *
     * Each interleaving types info maps from the input index to the appropriate
     * initial offset for that channel based upon the above forms. */
    if (!skip || !offset) {
        return -1;
    }

    if ((uint32_t)interleaving >= ILCount) {
        return -1;
    }

    const InterleavingInfo_t* info = &kInterleavingInfos[interleaving];

    if (channelIdx >= info->componentCount) {
        return -1;
    }

    *skip = info->skip[channelIdx];
    *offset = info->offset[channelIdx];

    return 0;
}

Interleaving_t interleavingFromAPI(perseus_interleaving interleaving)
{
    switch (interleaving) {
        case PSS_ILV_NONE: return ILNone;
        case PSS_ILV_YUYV: return ILYUYV;
        case PSS_ILV_NV12: return ILNV12;
        case PSS_ILV_UYVY: return ILUYVY;
        case PSS_ILV_RGB: return ILRGB;
        case PSS_ILV_RGBA: return ILRGBA;
        default: break;
    }

    return ILCount;
}

const char* interleavingToString(Interleaving_t interleaving)
{
    switch (interleaving) {
        case ILNone: return "none";
        case ILYUYV: return "yuyv";
        case ILNV12: return "nv12";
        case ILUYVY: return "uyvy";
        case ILRGB: return "rgb";
        case ILRGBA: return "rgba";
        case ILCount: break;
    }

    return "unknown";
}

/*------------------------------------------------------------------------------*/

FixedPoint_t fixedPointFromBitdepth(BitDepth_t depth)
{
    switch (depth) {
        case Depth8: return FPU8;
        case Depth10: return FPU10;
        case Depth12: return FPU12;
        case Depth14: return FPU14;
        default: break;
    }

    assert(false);
    return FPCount;
}

uint32_t fixedPointByteSize(FixedPoint_t type)
{
    assert(fixedPointIsValid(type));

    switch (type) {
        case FPU8: return sizeof(uint8_t);
        case FPU10:
        case FPU12:
        case FPU14:
        case FPS8:
        case FPS10:
        case FPS12:
        case FPS14: return sizeof(int16_t);
        default: break;
    }

    return 0;
}

FixedPoint_t fixedPointLowPrecision(FixedPoint_t type)
{
    switch (type) {
        case FPS8: return FPU8;
        case FPS10: return FPU10;
        case FPS12: return FPU12;
        case FPS14: return FPU14;
        default: break;
    }

    return type;
}

FixedPoint_t fixedPointHighPrecision(FixedPoint_t type)
{
    switch (type) {
        case FPU8: return FPS8;
        case FPU10: return FPS10;
        case FPU12: return FPS12;
        case FPU14: return FPS14;
        default: break;
    }
    return type;
}

bool fixedPointIsSigned(FixedPoint_t type)
{
    assert(fixedPointIsValid(type));

    switch (type) {
        case FPU8:
        case FPU10:
        case FPU12:
        case FPU14: return false;
        case FPS8:
        case FPS10:
        case FPS12:
        case FPS14: return true;
        default: break;
    }

    return false;
}

const char* fixedPointToString(FixedPoint_t type)
{
    switch (type) {
        case FPU8: return "U8";
        case FPU10: return "U10";
        case FPU12: return "U12";
        case FPU14: return "U14";
        case FPS8: return "S8_7";
        case FPS10: return "S10_5";
        case FPS12: return "S12_3";
        case FPS14: return "S14_1";
        default: break;
    }

    assert(false);
    return "unknown";
}

bool fixedPointIsValid(FixedPoint_t type) { return (uint32_t)type < FPCount; }

BitDepth_t bitdepthFromFixedPoint(FixedPoint_t type)
{
    assert(fixedPointIsValid(type));

    switch (type) {
        case FPU8:
        case FPS8: return Depth8;
        case FPU10:
        case FPS10: return Depth10;
        case FPU12:
        case FPS12: return Depth12;
        case FPU14:
        case FPS14: return Depth14;
        default: break;
    }
    return Depth8;
}

uint16_t f32ToU16(float val)
{
    assert(val >= 0.0f && val <= 1.0f);
    return (uint16_t)(val * ((1 << 16) - 1));
}

FixedPointPromotionFunction_t fixedPointGetPromotionFunction(FixedPoint_t unsignedFP)
{
    switch (unsignedFP) {
        case FPU10: return &fpU10ToS10;
        case FPU12: return &fpU12ToS12;
        case FPU14: return &fpU14ToS14;
        default:;
    }

    return NULL;
}

FixedPointDemotionFunction_t fixedPointGetDemotionFunction(FixedPoint_t unsignedFP)
{
    switch (unsignedFP) {
        case FPU10: return &fpS10ToU10;
        case FPU12: return &fpS12ToU12;
        case FPU14: return &fpS14ToU14;
        default:;
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/

bool isPow2(uint32_t value) { return (value & (value - 1)) == 0; }

uint32_t alignTruncU32(uint32_t value, uint32_t alignment)
{
    assert(alignment > 0);
    assert(isPow2(alignment));
    return value & ~(alignment - 1);
}

int32_t strcpyDeep(Memory_t memory, const char* str, const char** dst)
{
    const size_t strLen = str ? strlen(str) : 0;
    *dst = NULL;

    if (!str || strLen == 0) {
        return 0;
    }

    char* newStr = VN_MALLOC_T_ARR(memory, char, strLen + 1);

    if (!newStr) {
        return -1;
    }

    /* NOLINTNEXTLINE */
    *dst = strcpy(newStr, str);
    return 0;
}

/*------------------------------------------------------------------------------*/
