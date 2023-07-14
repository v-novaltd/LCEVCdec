/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "common/cmdbuffer.h"
#include "common/memory.h"
#include "context.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/apply_cmdbuffer_common.h"
#include "surface/surface.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/
/* Inter */
/*------------------------------------------------------------------------------*/

static void interDD_U8(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint8_t, 2)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS8ToU8(fpU8ToS8(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 2;
        pixels += stride;
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDD_U10(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint16_t, 2)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS10ToU10(fpU10ToS10(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 2;
        pixels += stride;
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDD_U12(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint16_t, 2)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS12ToU12(fpU12ToS12(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 2;
        pixels += stride;
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDD_U14(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint16_t, 2)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS14ToU14(fpU14ToS14(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 2;
        pixels += stride;
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDDS_U8(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint8_t, 4)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS8ToU8(fpU8ToS8(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 4;
        pixels += stride;
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDDS_U10(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint16_t, 4)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS10ToU10(fpU10ToS10(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 4;
        pixels += stride;
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDDS_U12(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint16_t, 4)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS12ToU12(fpU12ToS12(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 4;
        pixels += stride;
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDDS_U14(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(uint16_t, 4)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = fpS14ToU14(fpU14ToS14(pixels[pixel]) + loadResiduals[pixel]);
        }
        loadResiduals += 4;
        pixels += stride;
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDD_S16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = saturateS16(pixels[pixel] + loadResiduals[pixel]);
        }
        loadResiduals += 2;
        pixels += stride;
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void interDDS_S16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 4)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {
            pixels[pixel] = saturateS16(pixels[pixel] + loadResiduals[pixel]);
        }
        loadResiduals += 4;
        pixels += stride;
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

/*------------------------------------------------------------------------------*/
/* Intra */
/*------------------------------------------------------------------------------*/

static void intraDD(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 2)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        memoryCopy(pixels, loadResiduals, sizeof(int16_t) * pixelCount);
        loadResiduals += 2;
        pixels += stride;
    }
    residuals += 4;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

static void intraDDS(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(int16_t, 4)

    const int16_t* loadResiduals = residuals;
    for (int32_t row = 0; row < rowCount; ++row) {
        memoryCopy(pixels, loadResiduals, sizeof(int16_t) * pixelCount);
        loadResiduals += 4;
        pixels += stride;
    }
    residuals += 16;

    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()
}

/*------------------------------------------------------------------------------*/
/* Tile Clear */
/*------------------------------------------------------------------------------*/

static void tileClear_N8(const ApplyCmdBufferArgs_t* args)
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

        for (int32_t blockY = y; blockY < yMax; ++blockY) {
            memorySet(pixels, 0, clearBytes);
            pixels += stride;
        }
    }
}

static void tileClear_N16(const ApplyCmdBufferArgs_t* args)
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

        for (int32_t blockY = y; blockY < yMax; ++blockY) {
            memorySet(pixels, 0, clearBytes);
            pixels += stride;
        }
    }
}

/*------------------------------------------------------------------------------*/
/* Highlight */
/*------------------------------------------------------------------------------*/

#define VN_APPLY_CMDBUFFER_HIGHLIGHT(Pixel_t, transformSize, highlightArg) \
    const Pixel_t highlightValue = (Pixel_t)args->highlight->highlightArg; \
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(Pixel_t, transformSize)            \
    for (int32_t row = 0; row < rowCount; ++row) {                         \
        for (int32_t pixel = 0; pixel < pixelCount; ++pixel) {             \
            pixels[pixel] = highlightValue;                                \
        }                                                                  \
        pixels += stride;                                                  \
    }                                                                      \
    VN_APPLY_CMDBUFFER_PIXEL_LOOP_END()

static void highlightDD_U8(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint8_t, 2, valUnsigned);
}

static void highlightDD_U16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint16_t, 2, valUnsigned);
}

static void highlightDD_S16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 2, valSigned);
}

static void highlightDDS_U8(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint8_t, 4, valUnsigned);
}

static void highlightDDS_U16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(uint16_t, 4, valUnsigned);
}

static void highlightDDS_S16(const ApplyCmdBufferArgs_t* args)
{
    VN_APPLY_CMDBUFFER_HIGHLIGHT(int16_t, 4, valSigned);
}

/*------------------------------------------------------------------------------*/
/* Module entry */
/*------------------------------------------------------------------------------*/

static const ApplyCmdBufferFunction_t kInterTable[2][FPCount] = {
    {
        &interDD_U8,  /* FP_U8 */
        &interDD_U10, /* FP_U10 */
        &interDD_U12, /* FP_U12 */
        &interDD_U14, /* FP_U14 */
        &interDD_S16, /* FP_S8 */
        &interDD_S16, /* FP_S10 */
        &interDD_S16, /* FP_S12 */
        &interDD_S16, /* FP_S14 */
    },
    {
        &interDDS_U8,  /* FP_U8 */
        &interDDS_U10, /* FP_U10 */
        &interDDS_U12, /* FP_U12 */
        &interDDS_U14, /* FP_U14 */
        &interDDS_S16, /* FP_S8 */
        &interDDS_S16, /* FP_S10 */
        &interDDS_S16, /* FP_S12 */
        &interDDS_S16, /* FP_S14 */
    },
};

static const ApplyCmdBufferFunction_t kIntraTable[2] = {&intraDD, &intraDDS};

static const ApplyCmdBufferFunction_t kHighlightTable[2][FPCount] = {
    {
        &highlightDD_U8,  /* FP_U8 */
        &highlightDD_U16, /* FP_U10 */
        &highlightDD_U16, /* FP_U12 */
        &highlightDD_U16, /* FP_U14 */
        &highlightDD_S16, /* FP_S8 */
        &highlightDD_S16, /* FP_S10 */
        &highlightDD_S16, /* FP_S12 */
        &highlightDD_S16, /* FP_S14 */
    },
    {
        &highlightDDS_U8,  /* FP_U8 */
        &highlightDDS_U16, /* FP_U10 */
        &highlightDDS_U16, /* FP_U12 */
        &highlightDDS_U16, /* FP_U14 */
        &highlightDDS_S16, /* FP_S8 */
        &highlightDDS_S16, /* FP_S10 */
        &highlightDDS_S16, /* FP_S12 */
        &highlightDDS_S16, /* FP_S14 */
    },
};

static const ApplyCmdBufferFunction_t kClearTable[FPCount] = {
    &tileClear_N8,  /* FP_U8 */
    &tileClear_N16, /* FP_U10 */
    &tileClear_N16, /* FP_U12 */
    &tileClear_N16, /* FP_U14 */
    &tileClear_N16, /* FP_S8 */
    &tileClear_N16, /* FP_S10 */
    &tileClear_N16, /* FP_S12 */
    &tileClear_N16, /* FP_S14 */
};

ApplyCmdBufferFunction_t applyCmdBufferGetFunctionScalar(ApplyCmdBufferMode_t mode,
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

    if (mode == ACBM_Highlight) {
        return kHighlightTable[transform][surfaceFP];
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/