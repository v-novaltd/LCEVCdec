/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "common/cmdbuffer.h"
#include "common/memory.h"
#include "common/neon.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_cmdbuffer_common.h"
#include "surface/surface.h"

#include <assert.h>

#if VN_CORE_FEATURE(NEON)

/*------------------------------------------------------------------------------*/

static inline int16x4_t loadPixelsDD(const int16_t* src)
{
    int16x4_t res = vld1_dup_s16(src);
    res = vld1_lane_s16(src + 1, res, 1);
    return res;
}

static inline uint8x8_t loadPixelsDD_U8(const uint8_t* src)
{
    uint8x8_t res = vld1_dup_u8(src);
    res = vld1_lane_u8(src + 1, res, 1);
    return res;
}

static inline uint8x8_t loadPixelsDDS_U8(const uint8_t* src)
{
    uint8x8_t res = vld1_dup_u8(src);
    res = vld1_lane_u8(src + 1, res, 1);
    res = vld1_lane_u8(src + 2, res, 2);
    res = vld1_lane_u8(src + 3, res, 3);
    return res;
}

static inline void storePixelsDD(int16_t* dst, int16x4_t data)
{
    vst1_lane_s16(dst, data, 0);
    vst1_lane_s16(dst + 1, data, 1);
}

static inline void storePixelsDD_U8(uint8_t* dst, int16x4_t data)
{
    const uint8x8_t res = vqmovun_s16(vcombine_s16(data, data));
    vst1_lane_u8(dst, res, 0);
    vst1_lane_u8(dst + 1, res, 1);
}

static inline void storePixelsDDS_U8(uint8_t* dst, int16x4_t data)
{
    const uint8x8_t res = vqmovun_s16(vcombine_s16(data, data));
    vst1_lane_u8(dst, res, 0);
    vst1_lane_u8(dst + 1, res, 1);
    vst1_lane_u8(dst + 2, res, 2);
    vst1_lane_u8(dst + 3, res, 3);
}

static inline int16x4_t loadResidualsDD(const int16_t* src) { return vld1_s16(src); }

static inline int16x4x4_t loadResidualsDDS(const int16_t* src)
{
    int16x4x4_t res;
    res.val[0] = vld1_s16(src);
    res.val[1] = vld1_s16(src + 4);
    res.val[2] = vld1_s16(src + 8);
    res.val[3] = vld1_s16(src + 12);
    return res;
}

/*------------------------------------------------------------------------------*/
/* Inter Application */
/*------------------------------------------------------------------------------*/

static void interDD_U8_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    const int16x4_t shiftDown = vdup_n_s16(-7);
    const int16x4_t usToSOffset = vdup_n_s16(16384);
    const int16x4_t roundingOffsetV = vdup_n_s16(0x40);
    const int16x4_t signOffsetV = vdup_n_s16(0x80);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint8_t, 2)

    if (pixelCount == 2) {
        int16x4_t data = loadResidualsDD(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            const uint8x8_t pelsU8 = loadPixelsDD_U8(pixels);

            /* val <<= shift */
            int16x4_t pels = vget_low_s16(vreinterpretq_s16_u16(vshll_n_u8(pelsU8, 7)));

            /* val -= 0x4000 */
            pels = vsub_s16(pels, usToSOffset);

            /* val += src */
            pels = vqadd_s16(pels, data);

            /* val += rounding */
            pels = vqadd_s16(pels, roundingOffsetV);

            /* val >>= 5 */ // @todo(bob): This can be replaced with rounding shift and above removed?
            pels = vshl_s16(pels, shiftDown);

            /* val += sign offset */
            pels = vadd_s16(pels, signOffsetV);

            /* Clamp to unsigned range and store */
            storePixelsDD_U8(pixels, pels);

            /* Move down next 2 elements for storage. */
            data = vext_s16(data, data, 2);
            pixels += stride;
        }
    } else {
        const int16_t* loadResiduals = residuals;
        for (int32_t row = 0; row < rowCount; ++row) {
            for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
                int32_t pel = fpU8ToS8(pixels[pixel]);
                pel += loadResiduals[pixel];
                pixels[pixel] = fpS8ToU8(pel);
            }
            loadResiduals += 2;
            pixels += stride;
        }
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static inline void interDD_UN_NEON(const ApplyCmdBufferArgs_t* args, int32_t shift,
                                   int16_t roundingOffset, int16_t signOffset, int16_t resultMax)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    FixedPointPromotionFunction_t uToS = fixedPointGetPromotionFunction(args->surface->type);
    FixedPointDemotionFunction_t sToU = fixedPointGetDemotionFunction(args->surface->type);

    const int16x4_t shiftUp = vdup_n_s16(shift);
    const int16x4_t shiftDown = vdup_n_s16(-shift);
    const int16x4_t usToSOffset = vdup_n_s16(16384);
    const int16x4_t roundingOffsetV = vdup_n_s16(roundingOffset);
    const int16x4_t signOffsetV = vdup_n_s16(signOffset);
    const int16x4_t minV = vdup_n_s16(0);
    const int16x4_t maxV = vdup_n_s16(resultMax);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    if (pixelCount == 2) {
        int16x4_t data = loadResidualsDD(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            /* Load as int16_t, source data is maximally unsigned 14-bit so will fit. */
            int16x4_t pels = loadPixelsDD(pixels);

            /* val <<= shift */
            pels = vshl_s16(pels, shiftUp);

            /* val -= 0x4000 */
            pels = vsub_s16(pels, usToSOffset);

            /* val += src */
            pels = vqadd_s16(pels, data);

            /* val += rounding */
            pels = vqadd_s16(pels, roundingOffsetV);

            /* val >>= 5 */ // @todo(bob): This can be replaced with rounding shift and above removed?
            pels = vshl_s16(pels, shiftDown);

            /* val += sign offset */
            pels = vadd_s16(pels, signOffsetV);

            /* Clamp to unsigned range */
            pels = vmax_s16(vmin_s16(pels, maxV), minV);

            /* Store */
            storePixelsDD(pixels, pels);

            /* Move down next 2 elements for storage. */
            data = vext_s16(data, data, 2);
            pixels += stride;
        }
    } else {
        const int16_t* loadResiduals = residuals;
        for (int32_t row = 0; row < rowCount; ++row) {
            for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
                int32_t pel = uToS((uint16_t)pixels[pixel]);
                pel += loadResiduals[pixel];
                pixels[pixel] = (int16_t)sToU(pel);
            }
            loadResiduals += 2;
            pixels += stride;
        }
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDD_U10_NEON(const ApplyCmdBufferArgs_t* args)
{
    interDD_UN_NEON(args, 5, 16, 512, 1023);
}

static void interDD_U12_NEON(const ApplyCmdBufferArgs_t* args)
{
    interDD_UN_NEON(args, 3, 4, 2048, 4095);
}

static void interDD_U14_NEON(const ApplyCmdBufferArgs_t* args)
{
    interDD_UN_NEON(args, 1, 1, 8192, 16383);
}

static void interDD_S16_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    if (pixelCount == 2) {
        int16x4_t data = loadResidualsDD(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            const int16x4_t pels = loadPixelsDD(pixels);

            storePixelsDD(pixels, vqadd_s16(pels, data));

            /* Move down next 2 elements. */
            data = vext_s16(data, data, 2);
            pixels += stride;
        }
    } else {
        const int16_t* loadResiduals = residuals;
        for (int32_t row = 0; row < rowCount; ++row) {
            for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
                pixels[pixel] = saturateS16(pixels[pixel] + loadResiduals[pixel]);
            }
            loadResiduals += 2;
            pixels += stride;
        }
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static inline void interDDS_U8_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    const int16x4_t shiftDown = vdup_n_s16(-7);
    const int16x4_t usToSOffset = vdup_n_s16(16384);
    const int16x4_t roundingOffsetV = vdup_n_s16(0x40);
    const int16x4_t signOffsetV = vdup_n_s16(0x80);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint8_t, 4)

    if (pixelCount == 4) {
        const int16x4x4_t data = loadResidualsDDS(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            const uint8x8_t pelsU8 = loadPixelsDDS_U8(pixels);

            /* val <<= shift */
            int16x4_t pels = vget_low_s16(vreinterpretq_s16_u16(vshll_n_u8(pelsU8, 7)));

            /* val -= 0x4000 */
            pels = vsub_s16(pels, usToSOffset);

            /* val += src */
            pels = vqadd_s16(pels, data.val[row]);

            /* val += rounding */
            pels = vqadd_s16(pels, roundingOffsetV);

            /* val >>= 5 */ // @todo(bob): This can be replaced with rounding shift and above removed?
            pels = vshl_s16(pels, shiftDown);

            /* val += sign offset */
            pels = vadd_s16(pels, signOffsetV);

            /* Clamp to unsigned range and store */
            storePixelsDDS_U8(pixels, pels);

            pixels += stride;
        }
    } else {
        const int16_t* loadResiduals = residuals;
        for (int32_t row = 0; row < rowCount; ++row) {
            for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
                int32_t pel = fpU8ToS8(pixels[pixel]);
                pel += loadResiduals[pixel];
                pixels[pixel] = fpS8ToU8(pel);
            }
            loadResiduals += 4;
            pixels += stride;
        }
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static inline void interDDS_UN_NEON(const ApplyCmdBufferArgs_t* args, int32_t shift,
                                    int16_t roundingOffset, int16_t signOffset, int16_t resultMax)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    FixedPointPromotionFunction_t uToS = fixedPointGetPromotionFunction(args->surface->type);
    FixedPointDemotionFunction_t sToU = fixedPointGetDemotionFunction(args->surface->type);

    const int16x4_t shiftUp = vdup_n_s16(shift);
    const int16x4_t shiftDown = vdup_n_s16(-shift);
    const int16x4_t usToSOffset = vdup_n_s16(16384);
    const int16x4_t roundingOffsetV = vdup_n_s16(roundingOffset);
    const int16x4_t signOffsetV = vdup_n_s16(signOffset);
    const int16x4_t minV = vdup_n_s16(0);
    const int16x4_t maxV = vdup_n_s16(resultMax);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint16_t, 4)

    if (pixelCount == 4) {
        const int16x4x4_t data = loadResidualsDDS(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            /* Load as int16_t, source data is maximally unsigned 14-bit so will fit. */
            int16x4_t pels = vld1_s16((const int16_t*)pixels);

            /* val <<= shift */
            pels = vshl_s16(pels, shiftUp);

            /* val -= 0x4000 */
            pels = vsub_s16(pels, usToSOffset);

            /* val += src */
            pels = vqadd_s16(pels, data.val[row]);

            /* val += rounding */
            pels = vqadd_s16(pels, roundingOffsetV);

            /* val >>= 5 */ // @todo(bob): This can be replaced with rounding shift and above removed?
            pels = vshl_s16(pels, shiftDown);

            /* val += sign offset */
            pels = vadd_s16(pels, signOffsetV);

            /* Clamp to unsigned range */
            pels = vmax_s16(vmin_s16(pels, maxV), minV);

            /* Store */
            vst1_s16((int16_t*)pixels, pels);
            pixels += stride;
        }
    } else {
        const int16_t* loadResiduals = residuals;
        for (int32_t row = 0; row < rowCount; ++row) {
            for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
                int32_t pel = uToS(pixels[pixel]);
                pel += loadResiduals[pixel];
                pixels[pixel] = sToU(pel);
            }
            loadResiduals += 4;
            pixels += stride;
        }
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDDS_U10_NEON(const ApplyCmdBufferArgs_t* args)
{
    interDDS_UN_NEON(args, 5, 16, 512, 1023);
}

static void interDDS_U12_NEON(const ApplyCmdBufferArgs_t* args)
{
    interDDS_UN_NEON(args, 3, 4, 2048, 4095);
}

static void interDDS_U14_NEON(const ApplyCmdBufferArgs_t* args)
{
    interDDS_UN_NEON(args, 1, 1, 8192, 16383);
}

static void interDDS_S16_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 4)

    if (pixelCount == 4) {
        const int16x4x4_t data = loadResidualsDDS(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            const int16x4_t pels = vld1_s16(pixels);
            vst1_s16(pixels, vqadd_s16(pels, data.val[row]));
            pixels += stride;
        }
    } else {
        const int16_t* loadResiduals = residuals;
        for (int32_t row = 0; row < rowCount; ++row) {
            for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
                pixels[pixel] = saturateS16(pixels[pixel] + loadResiduals[pixel]);
            }
            loadResiduals += 4;
            pixels += stride;
        }
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

/*------------------------------------------------------------------------------*/
/* Intra Application */
/*------------------------------------------------------------------------------*/

static void intraDD_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    if (pixelCount == 2) {
        int16x4_t data = loadResidualsDD(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            storePixelsDD(pixels, data);

            /* Move down next 2 elements for storage. */
            data = vext_s16(data, data, 2);
            pixels += stride;
        }

        residuals += 4;
    } else {
        for (int32_t row = 0; row < rowCount; ++row) {
            memoryCopy(pixels, residuals, sizeof(int16_t) * pixelCount);
            residuals += 2;
            pixels += stride;
        }
    }

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void intraDDS_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 4)

    if (pixelCount == 4) {
        int16x4x4_t data = loadResidualsDDS(residuals);

        for (int32_t row = 0; row < rowCount; ++row) {
            vst1_s16(pixels, data.val[row]);
            pixels += stride;
        }
    } else {
        const int16_t* loadResiduals = residuals;
        for (int32_t row = 0; row < rowCount; ++row) {
            for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
                pixels[pixel] = saturateS16(pixels[pixel] + loadResiduals[pixel]);
            }
            loadResiduals += 4;
            pixels += stride;
        }
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

/*------------------------------------------------------------------------------*/
/* Tile Clear */
/*------------------------------------------------------------------------------*/

static void tileClear_N8_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(args->residuals == NULL);

    static const int32_t kBlockSize = 32;
    const int16_t* coords = args->coords;

    uint8_t* surfaceData = surfaceGetLine(args->surface, 0);
    const size_t stride = surfaceGetStrideInPixels(args->surface);
    const uint8x16_t zero = vdupq_n_u8(0);

    for (int32_t i = 0; i < args->count; ++i) {
        const int16_t x = *coords++;
        const int16_t y = *coords++;

        const int32_t yMax = minS32(y + kBlockSize, (int32_t)args->surface->height);
        const int32_t clearPixels = minS32(kBlockSize, (int32_t)args->surface->width - x);
        const size_t clearBytes = clearPixels * sizeof(uint8_t);

        uint8_t* pixels = surfaceData + (y * stride) + x;

        if (clearPixels == kBlockSize) {
            for (int32_t blockY = y; blockY < yMax; blockY++) {
                vst1q_u8(pixels, zero);
                vst1q_u8(pixels + 16, zero);
                pixels += stride;
            }
        } else {
            for (int32_t blockY = y; blockY < yMax; blockY++) {
                memorySet(pixels, 0, clearBytes);
                pixels += stride;
            }
        }
    }
}

static void tileClear_N16_NEON(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(args->residuals == NULL);

    static const int32_t kBlockSize = 32;
    const int16_t* coords = args->coords;

    int16_t* surfaceData = (int16_t*)surfaceGetLine(args->surface, 0);
    const size_t stride = surfaceGetStrideInPixels(args->surface);
    const int16x8_t zero = vdupq_n_s16(0);

    for (int32_t i = 0; i < args->count; ++i) {
        const int16_t x = *coords++;
        const int16_t y = *coords++;

        const int32_t yMax = minS32(y + kBlockSize, (int32_t)args->surface->height);
        const int32_t clearPixels = minS32(kBlockSize, (int32_t)args->surface->width - x);
        const size_t clearBytes = clearPixels * sizeof(int16_t);

        int16_t* pixels = surfaceData + (y * stride) + x;

        if (clearPixels == kBlockSize) {
            for (int32_t blockY = y; blockY < yMax; blockY++) {
                vst1q_s16(pixels, zero);
                vst1q_s16(pixels + 8, zero);
                vst1q_s16(pixels + 16, zero);
                vst1q_s16(pixels + 24, zero);
                pixels += stride;
            }
        } else {
            for (int32_t blockY = y; blockY < yMax; blockY++) {
                memorySet(pixels, 0, clearBytes);
                pixels += stride;
            }
        }
    }
}

/*------------------------------------------------------------------------------*/
/* Module entry */
/*------------------------------------------------------------------------------*/

static const ApplyCmdBufferFunction_t kInterTable[2][FPCount] = {

    /* DD */
    {
        &interDD_U8_NEON,  /* FP_U8 */
        &interDD_U10_NEON, /* FP_U10 */
        &interDD_U12_NEON, /* FP_U12 */
        &interDD_U14_NEON, /* FP_U14 */
        &interDD_S16_NEON, /* FP_S8 */
        &interDD_S16_NEON, /* FP_S10 */
        &interDD_S16_NEON, /* FP_S12 */
        &interDD_S16_NEON, /* FP_S14 */
    },

    /* DDS */
    {
        &interDDS_U8_NEON,  /* FP_U8 */
        &interDDS_U10_NEON, /* FP_U10 */
        &interDDS_U12_NEON, /* FP_U12 */
        &interDDS_U14_NEON, /* FP_U14 */
        &interDDS_S16_NEON, /* FP_S8 */
        &interDDS_S16_NEON, /* FP_S10 */
        &interDDS_S16_NEON, /* FP_S12 */
        &interDDS_S16_NEON, /* FP_S14 */
    },
};

static const ApplyCmdBufferFunction_t kIntraTable[2] = {&intraDD_NEON, &intraDDS_NEON};

static const ApplyCmdBufferFunction_t kClearTable[FPCount] = {
    &tileClear_N8_NEON,  /* FP_U8 */
    &tileClear_N16_NEON, /* FP_U10 */
    &tileClear_N16_NEON, /* FP_U12 */
    &tileClear_N16_NEON, /* FP_U14 */
    &tileClear_N16_NEON, /* FP_S8 */
    &tileClear_N16_NEON, /* FP_S10 */
    &tileClear_N16_NEON, /* FP_S12 */
    &tileClear_N16_NEON, /* FP_S14 */
};

ApplyCmdBufferFunction_t applyCmdBufferGetFunctionNEON(ApplyCmdBufferMode_t mode,
                                                       FixedPoint_t surfaceFP, TransformType_t transform)
{
    if (mode == ACBM_Inter) {
        return kInterTable[transform][surfaceFP];
    }

    if (mode == ACBM_Intra) {
        assert(fixedPointIsSigned(surfaceFP));
        return kIntraTable[transform];
    }

    if (mode == ACBM_Tiles) {
        return kClearTable[surfaceFP];
    }

    return NULL;
}

#else

ApplyCmdBufferFunction_t applyCmdBufferGetFunctionNEON(ApplyCmdBufferMode_t mode,
                                                       FixedPoint_t surfaceFP, TransformType_t transform)
{
    VN_UNUSED(mode);
    VN_UNUSED(surfaceFP);
    VN_UNUSED(transform);
    return NULL;
}

#endif