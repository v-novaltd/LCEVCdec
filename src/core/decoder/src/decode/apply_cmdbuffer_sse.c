/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "common/cmdbuffer.h"
#include "common/memory.h"
#include "common/sse.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_cmdbuffer_common.h"
#include "surface/surface.h"

#include <assert.h>

#if VN_CORE_FEATURE(SSE)

/*------------------------------------------------------------------------------*/

static inline void loadResidualsDD(const int16_t* data, __m128i dst[2])
{
    dst[0] = _mm_loadl_epi64((const __m128i*)data);
    dst[1] = _mm_bsrli_si128(dst[0], 4);
}

static inline void loadResidualsDDS(const int16_t* data, __m128i dst[4])
{
    dst[0] = _mm_loadu_si128((const __m128i*)data);
    dst[2] = _mm_loadu_si128((const __m128i*)(data + 8));

    dst[1] = _mm_bsrli_si128(dst[0], 8);
    dst[3] = _mm_bsrli_si128(dst[2], 8);
}

/*------------------------------------------------------------------------------*/
/* Inter Application */
/*------------------------------------------------------------------------------*/

static void interDD_U8_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    static const int32_t kShift = 7;
    const __m128i usToSOffset = _mm_set1_epi16(0x4000);
    const __m128i fractOffset = _mm_set1_epi16(0x40);
    const __m128i signOffset = _mm_set1_epi16(0x80);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint8_t, 2)

    if (pixelCount == 2) {
        __m128i data[2];
        loadResidualsDD(residuals, data);

        for (int32_t row = 0; row < rowCount; ++row) {
            __m128i pels = _mm_cvtepu8_epi16(_mm_loadu_si16((const __m128i*)pixels));

            /* val <<= shift */
            pels = _mm_slli_epi16(pels, kShift);

            /* val -= 0x4000 */
            pels = _mm_sub_epi16(pels, usToSOffset);

            /* val += src */
            pels = _mm_adds_epi16(pels, data[row]);

            /* val += rounding */
            pels = _mm_adds_epi16(pels, fractOffset);

            /* val >>= shift */
            pels = _mm_srai_epi16(pels, kShift);

            /* val += sign offset */
            pels = _mm_add_epi16(pels, signOffset);

            /* Clamp to unsigned range and store */
            _mm_storeu_si16((__m128i*)pixels, _mm_packus_epi16(pels, pels));

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

static inline void interDD_UN_SSE(const ApplyCmdBufferArgs_t* args, int32_t shift,
                                  int16_t roundingOffset, int16_t signOffset, int16_t resultMax)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    FixedPointPromotionFunction_t uToS = fixedPointGetPromotionFunction(args->surface->type);
    FixedPointDemotionFunction_t sToU = fixedPointGetDemotionFunction(args->surface->type);

    const __m128i usToSOffset = _mm_set1_epi16(16384);
    const __m128i roundingOffsetV = _mm_set1_epi16(roundingOffset);
    const __m128i signOffsetV = _mm_set1_epi16(signOffset);
    const __m128i minV = _mm_set1_epi16(0);
    const __m128i maxV = _mm_set1_epi16(resultMax);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    if (pixelCount == 2) {
        __m128i data[2];
        loadResidualsDD(residuals, data);

        for (int32_t row = 0; row < rowCount; ++row) {
            /* Load as int16_t, source data is maximally unsigned 14-bit so will fit. */
            __m128i pels = _mm_loadu_si32((const __m128i*)pixels);

            /* val <<= shift */
            pels = _mm_slli_epi16(pels, shift);

            /* val -= 0x4000 */
            pels = _mm_sub_epi16(pels, usToSOffset);

            /* val += src */
            pels = _mm_adds_epi16(pels, data[row]);

            /* val += rounding */
            pels = _mm_adds_epi16(pels, roundingOffsetV);

            /* val >>= shift */
            pels = _mm_srai_epi16(pels, shift);

            /* val += sign offset */
            pels = _mm_add_epi16(pels, signOffsetV);

            /* Clamp to unsigned range */
            pels = _mm_max_epi16(_mm_min_epi16(pels, maxV), minV);

            /* Store */
            _mm_storeu_si32((__m128i*)pixels, pels);
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

static void interDD_U10_SSE(const ApplyCmdBufferArgs_t* args)
{
    interDD_UN_SSE(args, 5, 16, 512, 1023);
}

static void interDD_U12_SSE(const ApplyCmdBufferArgs_t* args)
{
    interDD_UN_SSE(args, 3, 4, 2048, 4095);
}

static void interDD_U14_SSE(const ApplyCmdBufferArgs_t* args)
{
    interDD_UN_SSE(args, 1, 1, 8192, 16383);
}

static void interDD_S16_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    __m128i data[2];

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    if (pixelCount == 2) {
        loadResidualsDD(residuals, data);
        for (int32_t row = 0; row < rowCount; ++row) {
            const __m128i pels = _mm_loadu_si32((const __m128i*)pixels);
            _mm_storeu_si32((__m128i*)pixels, _mm_adds_epi16(pels, data[row]));
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

static void interDDS_U8_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    static const int32_t kShift = 7;
    const __m128i usToSOffset = _mm_set1_epi16(0x4000);
    const __m128i fractOffset = _mm_set1_epi16(0x40);
    const __m128i signOffset = _mm_set1_epi16(0x80);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint8_t, 4)

    if (pixelCount == 4) {
        __m128i data[4];
        loadResidualsDDS(residuals, data);

        for (int32_t row = 0; row < rowCount; ++row) {
            __m128i pels = _mm_cvtepu8_epi16(_mm_loadu_si32((const __m128i*)pixels));

            /* val <<= shift */
            pels = _mm_slli_epi16(pels, kShift);

            /* val -= 0x4000 */
            pels = _mm_sub_epi16(pels, usToSOffset);

            /* val += src */
            pels = _mm_adds_epi16(pels, data[row]);

            /* val += rounding */
            pels = _mm_adds_epi16(pels, fractOffset);

            /* val >>= shift */
            pels = _mm_srai_epi16(pels, kShift);

            /* val += sign offset */
            pels = _mm_add_epi16(pels, signOffset);

            /* Clamp to unsigned range and store */
            _mm_storeu_si32((__m128i*)pixels, _mm_packus_epi16(pels, pels));

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

static inline void interDDS_UN_SSE(const ApplyCmdBufferArgs_t* args, int32_t shift,
                                   int16_t roundingOffset, int16_t signOffset, int16_t resultMax)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    FixedPointPromotionFunction_t uToS = fixedPointGetPromotionFunction(args->surface->type);
    FixedPointDemotionFunction_t sToU = fixedPointGetDemotionFunction(args->surface->type);

    const __m128i usToSOffset = _mm_set1_epi16(16384);
    const __m128i roundingOffsetV = _mm_set1_epi16(roundingOffset);
    const __m128i signOffsetV = _mm_set1_epi16(signOffset);
    const __m128i minV = _mm_set1_epi16(0);
    const __m128i maxV = _mm_set1_epi16(resultMax);

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 4)

    if (pixelCount == 4) {
        __m128i data[4];
        loadResidualsDDS(residuals, data);

        for (int32_t row = 0; row < rowCount; ++row) {
            /* Load as int16_t, source data is maximally unsigned 14-bit so will fit. */
            __m128i pels = _mm_loadl_epi64((const __m128i*)pixels);

            /* val <<= shift */
            pels = _mm_slli_epi16(pels, shift);

            /* val -= 0x4000 */
            pels = _mm_sub_epi16(pels, usToSOffset);

            /* val += src */
            pels = _mm_adds_epi16(pels, data[row]);

            /* val += rounding */
            pels = _mm_adds_epi16(pels, roundingOffsetV);

            /* val >>= shift */
            pels = _mm_srai_epi16(pels, shift);

            /* val += sign offset */
            pels = _mm_add_epi16(pels, signOffsetV);

            /* Clamp to unsigned range */
            pels = _mm_max_epi16(_mm_min_epi16(pels, maxV), minV);

            /* Store */
            _mm_storel_epi64((__m128i*)pixels, pels);
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
            loadResiduals += 4;
            pixels += stride;
        }
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDDS_U10_SSE(const ApplyCmdBufferArgs_t* args)
{
    interDDS_UN_SSE(args, 5, 16, 512, 1023);
}

static void interDDS_U12_SSE(const ApplyCmdBufferArgs_t* args)
{
    interDDS_UN_SSE(args, 3, 4, 2048, 4095);
}

static void interDDS_U14_SSE(const ApplyCmdBufferArgs_t* args)
{
    interDDS_UN_SSE(args, 1, 1, 8192, 16383);
}

static void interDDS_S16_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    __m128i data[4];

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 4)

    if (pixelCount == 4) {
        loadResidualsDDS(residuals, data);

        for (int32_t row = 0; row < rowCount; ++row) {
            const __m128i pels = _mm_loadl_epi64((const __m128i*)pixels);
            _mm_storel_epi64((__m128i*)pixels, _mm_adds_epi16(pels, data[row]));
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

static void intraDD_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    __m128i data[2];

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    if (pixelCount == 2) {
        loadResidualsDD(residuals, data);
        for (int32_t row = 0; row < rowCount; ++row) {
            _mm_storeu_si32((__m128i*)pixels, data[row]);
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

static void intraDDS_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    __m128i data[4];

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 4)

    if (pixelCount == 4) {
        loadResidualsDDS(residuals, data);
        for (int32_t row = 0; row < rowCount; ++row) {
            _mm_storel_epi64((__m128i*)pixels, data[row]);
            pixels += stride;
        }

        residuals += 16;
    } else {
        for (int32_t row = 0; row < rowCount; ++row) {
            memoryCopy(pixels, residuals, sizeof(int16_t) * pixelCount);
            residuals += 4;
            pixels += stride;
        }
    }

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

/*------------------------------------------------------------------------------*/
/* Tile Clear */
/*------------------------------------------------------------------------------*/

static void tileClear_N8_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(args->residuals == NULL);

    static const int32_t kBlockSize = 32;
    const int16_t* coords = args->coords;

    uint8_t* surfaceData = surfaceGetLine(args->surface, 0);
    const size_t stride = surfaceGetStrideInPixels(args->surface);

    for (int32_t i = 0; i < args->count; ++i) {
        const int16_t x = *coords++;
        const int16_t y = *coords++;

        const int32_t yMax = minS32(y + kBlockSize, (int32_t)args->surface->height);
        const int32_t clearPixels = minS32(kBlockSize, (int32_t)args->surface->width - x);
        const size_t clearBytes = clearPixels * sizeof(uint8_t);

        uint8_t* pixels = surfaceData + (y * stride) + x;

        if (clearPixels == kBlockSize) {
            for (int32_t blockY = y; blockY < yMax; blockY++) {
                _mm_storeu_si128((__m128i*)pixels, _mm_setzero_si128());
                _mm_storeu_si128((__m128i*)(pixels + 16), _mm_setzero_si128());
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

static void tileClear_N16_SSE(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(args->residuals == NULL);

    static const int32_t kBlockSize = 32;
    const int16_t* coords = args->coords;

    int16_t* surfaceData = (int16_t*)surfaceGetLine(args->surface, 0);
    const size_t stride = surfaceGetStrideInPixels(args->surface);

    for (int32_t i = 0; i < args->count; ++i) {
        const int16_t x = *coords++;
        const int16_t y = *coords++;

        const int32_t yMax = minS32(y + kBlockSize, (int32_t)args->surface->height);
        const int32_t clearPixels = minS32(kBlockSize, (int32_t)args->surface->width - x);
        const size_t clearBytes = clearPixels * sizeof(int16_t);

        int16_t* pixels = surfaceData + (y * stride) + x;

        if (clearPixels == kBlockSize) {
            for (int32_t blockY = y; blockY < yMax; blockY++) {
                _mm_storeu_si128((__m128i*)pixels, _mm_setzero_si128());
                _mm_storeu_si128((__m128i*)(pixels + 8), _mm_setzero_si128());
                _mm_storeu_si128((__m128i*)(pixels + 16), _mm_setzero_si128());
                _mm_storeu_si128((__m128i*)(pixels + 24), _mm_setzero_si128());
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

    /* DD **/
    {
        &interDD_U8_SSE,  /* FP_U8 */
        &interDD_U10_SSE, /* FP_U10 */
        &interDD_U12_SSE, /* FP_U12 */
        &interDD_U14_SSE, /* FP_U14 */
        &interDD_S16_SSE, /* FP_S8 */
        &interDD_S16_SSE, /* FP_S10 */
        &interDD_S16_SSE, /* FP_S12 */
        &interDD_S16_SSE, /* FP_S14 */
    },

    /* DDS */
    {
        &interDDS_U8_SSE,  /* FP_U8 */
        &interDDS_U10_SSE, /* FP_U10 */
        &interDDS_U12_SSE, /* FP_U12 */
        &interDDS_U14_SSE, /* FP_U14 */
        &interDDS_S16_SSE, /* FP_S8 */
        &interDDS_S16_SSE, /* FP_S10 */
        &interDDS_S16_SSE, /* FP_S12 */
        &interDDS_S16_SSE, /* FP_S14 */
    },
};

static const ApplyCmdBufferFunction_t kIntraTable[2] = {&intraDD_SSE, &intraDDS_SSE};

static const ApplyCmdBufferFunction_t kClearTable[FPCount] = {
    &tileClear_N8_SSE,  /* FP_U8 */
    &tileClear_N16_SSE, /* FP_U10 */
    &tileClear_N16_SSE, /* FP_U12 */
    &tileClear_N16_SSE, /* FP_U14 */
    &tileClear_N16_SSE, /* FP_S8 */
    &tileClear_N16_SSE, /* FP_S10 */
    &tileClear_N16_SSE, /* FP_S12 */
    &tileClear_N16_SSE, /* FP_S14 */
};

ApplyCmdBufferFunction_t applyCmdBufferGetFunctionSSE(ApplyCmdBufferMode_t mode,
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

/*------------------------------------------------------------------------------*/

#else

ApplyCmdBufferFunction_t applyCmdBufferGetFunctionSSE(ApplyCmdBufferMode_t mode,
                                                      FixedPoint_t surfaceFP, TransformType_t transform)
{
    VN_UNUSED(mode);
    VN_UNUSED(surfaceFP);
    VN_UNUSED(transform);
    return NULL;
}

#endif
