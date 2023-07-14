/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_APPLY_CMDBUFFER_COMMON_H_
#define VN_DEC_CORE_APPLY_CMDBUFFER_COMMON_H_

#include "common/platform.h"

/*------------------------------------------------------------------------------*/

#define VN_APPLY_CMDBUFFER_PIXEL_LOOP_BEGIN(Pixel_t, transformSize)                          \
    const int16_t* coords = args->coords;                                                    \
    const int16_t* residuals = args->residuals;                                              \
    VN_UNUSED(residuals);                                                                    \
    Pixel_t* surfaceData = (Pixel_t*)surfaceGetLine(args->surface, 0);                       \
    const size_t stride = surfaceGetStrideInPixels(args->surface);                           \
    for (int32_t i = 0; i < args->count; ++i) {                                              \
        const int16_t x = *coords++;                                                         \
        const int16_t y = *coords++;                                                         \
        Pixel_t* pixels = surfaceData + (y * stride) + x;                                    \
        const int32_t pixelCount = minS32(transformSize, (int32_t)args->surface->width - x); \
        const int32_t rowCount = minS32(transformSize, (int32_t)args->surface->height - y);

#define VN_APPLY_CMDBUFFER_PIXEL_LOOP_END() }

/*------------------------------------------------------------------------------*/

typedef struct Highlight Highlight_t;
typedef struct Surface Surface_t;

/*------------------------------------------------------------------------------*/

typedef struct ApplyCmdBufferArgs
{
    const Surface_t* surface;
    const int16_t* coords;
    const int16_t* residuals;
    int32_t count;
    const Highlight_t* highlight;
} ApplyCmdBufferArgs_t;

typedef void (*ApplyCmdBufferFunction_t)(const ApplyCmdBufferArgs_t* args);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_APPLY_CMDBUFFER_COMMON_H_ */