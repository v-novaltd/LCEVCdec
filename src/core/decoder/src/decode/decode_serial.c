/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
/** \file apply_residual.c
 * Residual application functions
 */

#include "decode/decode_serial.h"

#include "common/memory.h"
#include "common/profiler.h"
#include "context.h"
#include "decode/decode_common.h"
#include "decode/dequant.h"
#include "decode/entropy.h"
#include "decode/transform.h"
#include "decode/transform_unit.h"
#include "surface/blit.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/*------------------------------------------------------------------------------*/

enum
{
    AC_MaxResidualParallel = 3,
};

/*------------------------------------------------------------------------------*/

typedef struct ResidualArgs
{
    Surface_t* dst;
    int32_t skip;
    int32_t offset;
    Highlight_t* highlight;
} ResidualArgs_t;

/*------------------------------------------------------------------------------*/

/* Add inverse to U8.0 buffer and write back. */
static void addResidualsDD_U8(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint8_t* pel = dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[4];

    assert(dst->type == FPU8);

    /* Load pel and convert to S8_7 */
    pelFP[0] = fpU8ToS8(pel[offset]);
    pelFP[1] = fpU8ToS8(pel[offset + skip]);
    pelFP[2] = fpU8ToS8(pel[offset + stride]);
    pelFP[3] = fpU8ToS8(pel[offset + skip + stride]);

    /* Apply & write back */
    pel[offset] = fpS8ToU8(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS8ToU8(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS8ToU8(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS8ToU8(pelFP[3] + residuals[3]);
}

/* Add inverse to U10.0 buffer and write back. */
static void addResidualsDD_U10(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[4];

    assert(dst->type == FPU10);

    /* Load pel and convert to S10_5 */
    pelFP[0] = fpU10ToS10(pel[offset]);
    pelFP[1] = fpU10ToS10(pel[offset + skip]);
    pelFP[2] = fpU10ToS10(pel[offset + stride]);
    pelFP[3] = fpU10ToS10(pel[offset + skip + stride]);

    /* Apply & write back */
    pel[offset] = fpS10ToU10(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS10ToU10(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS10ToU10(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS10ToU10(pelFP[3] + residuals[3]);
}

/* Add inverse to U12.0 buffer and write back. */
static void addResidualsDD_U12(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)args->dst;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[4];

    assert(dst->type == FPU12);

    /* Load pel and convert to S12_3 */
    pelFP[0] = fpU12ToS12(pel[offset]);
    pelFP[1] = fpU12ToS12(pel[offset + skip]);
    pelFP[2] = fpU12ToS12(pel[offset + stride]);
    pelFP[3] = fpU12ToS12(pel[offset + skip + stride]);

    /* Apply & write back */
    pel[offset] = fpS12ToU12(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS12ToU12(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS12ToU12(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS12ToU12(pelFP[3] + residuals[3]);
}

/* Add inverse to U14.0 buffer and write back. */
static void addResidualsDD_U14(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)args->dst;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[4];

    assert(dst->type == FPU14);

    /* Load pel and convert to S14_1 */
    pelFP[0] = fpU14ToS14(pel[offset]);
    pelFP[1] = fpU14ToS14(pel[offset + skip]);
    pelFP[2] = fpU14ToS14(pel[offset + stride]);
    pelFP[3] = fpU14ToS14(pel[offset + skip + stride]);

    /* Apply & write back */
    pel[offset] = fpS14ToU14(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS14ToU14(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS14ToU14(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS14ToU14(pelFP[3] + residuals[3]);
}

/* Add inverse to S8.7/S10.5/S12.3/S14.1 buffer and write back. */
static void addResidualsDD_S16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    int16_t* pel = (int16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Add inverse to S8.7/S10.5/S12.3/S14.1 buffer. */
    pel[offset] = saturateS16(pel[offset] + residuals[0]);
    pel[offset + skip] = saturateS16(pel[offset + skip] + residuals[1]);
    pel[offset + stride] = saturateS16(pel[offset + stride] + residuals[2]);
    pel[offset + skip + stride] = saturateS16(pel[offset + skip + stride] + residuals[3]);
}

/* Write inverse to S8.7/S10.5/S12.3/S14.1 buffer. */
static void writeResidualsDD_S16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    int16_t* pel = (int16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Write out. */
    pel[offset] = residuals[0];
    pel[offset + skip] = residuals[1];
    pel[offset + stride] = residuals[2];
    pel[offset + skip + stride] = residuals[3];
}

/* Write highlight colour to U8.0 buffer. */
static void writeHighlightDD_U8(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    VN_UNUSED(residuals);

    Surface_t* dst = args->dst;
    uint8_t* pel = dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    const uint8_t highlight = (uint8_t)args->highlight->valUnsigned;

    assert(dst->type == FPU8);

    pel[offset] = highlight;
    pel[offset + skip] = highlight;
    pel[offset + stride] = highlight;
    pel[offset + skip + stride] = highlight;
}

/* Write highlight colour to U10.0/U12.0/U14.0 buffer. */
static void writeHighlightDD_U16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    VN_UNUSED(residuals);

    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    const uint16_t highlight = args->highlight->valUnsigned;

    assert((dst->type == FPU10) || (dst->type == FPU12) || (dst->type == FPU14));

    pel[offset] = highlight;
    pel[offset + skip] = highlight;
    pel[offset + stride] = highlight;
    pel[offset + skip + stride] = highlight;
}

/* Write highlight colour to S8.7/S10.5/S12.3/S14.1 buffer. */
static void writeHighlightDD_S16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    VN_UNUSED(residuals);

    Surface_t* dst = args->dst;
    int16_t* pel = (int16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    const int16_t highlight = args->highlight->valSigned;

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    pel[offset] = highlight;
    pel[offset + skip] = highlight;
    pel[offset + stride] = highlight;
    pel[offset + skip + stride] = highlight;
}

/*------------------------------------------------------------------------------*/

/* Add inverse to U8.0 buffer and write back. */
static void addResidualsDDS_U8(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint8_t* pel = (uint8_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[16];

    assert(dst->type == FPU8);

    /* Load pel and convert to S8.7 */
    pelFP[0] = fpU8ToS8(pel[offset]);
    pelFP[1] = fpU8ToS8(pel[offset + skip]);
    pelFP[2] = fpU8ToS8(pel[offset + stride]);
    pelFP[3] = fpU8ToS8(pel[offset + skip + stride]);
    pelFP[4] = fpU8ToS8(pel[offset + 2 * skip]);
    pelFP[5] = fpU8ToS8(pel[offset + 3 * skip]);
    pelFP[6] = fpU8ToS8(pel[offset + 2 * skip + stride]);
    pelFP[7] = fpU8ToS8(pel[offset + 3 * skip + stride]);
    pelFP[8] = fpU8ToS8(pel[offset + 2 * stride]);
    pelFP[9] = fpU8ToS8(pel[offset + skip + 2 * stride]);
    pelFP[10] = fpU8ToS8(pel[offset + 3 * stride]);
    pelFP[11] = fpU8ToS8(pel[offset + skip + 3 * stride]);
    pelFP[12] = fpU8ToS8(pel[offset + 2 * skip + 2 * stride]);
    pelFP[13] = fpU8ToS8(pel[offset + 3 * skip + 2 * stride]);
    pelFP[14] = fpU8ToS8(pel[offset + 2 * skip + 3 * stride]);
    pelFP[15] = fpU8ToS8(pel[offset + 3 * skip + 3 * stride]);

    /* Apply & write back */
    pel[offset] = fpS8ToU8(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS8ToU8(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS8ToU8(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS8ToU8(pelFP[3] + residuals[3]);
    pel[offset + 2 * skip] = fpS8ToU8(pelFP[4] + residuals[4]);
    pel[offset + 3 * skip] = fpS8ToU8(pelFP[5] + residuals[5]);
    pel[offset + 2 * skip + stride] = fpS8ToU8(pelFP[6] + residuals[6]);
    pel[offset + 3 * skip + stride] = fpS8ToU8(pelFP[7] + residuals[7]);
    pel[offset + 2 * stride] = fpS8ToU8(pelFP[8] + residuals[8]);
    pel[offset + skip + 2 * stride] = fpS8ToU8(pelFP[9] + residuals[9]);
    pel[offset + 3 * stride] = fpS8ToU8(pelFP[10] + residuals[10]);
    pel[offset + skip + 3 * stride] = fpS8ToU8(pelFP[11] + residuals[11]);
    pel[offset + 2 * skip + 2 * stride] = fpS8ToU8(pelFP[12] + residuals[12]);
    pel[offset + 3 * skip + 2 * stride] = fpS8ToU8(pelFP[13] + residuals[13]);
    pel[offset + 2 * skip + 3 * stride] = fpS8ToU8(pelFP[14] + residuals[14]);
    pel[offset + 3 * skip + 3 * stride] = fpS8ToU8(pelFP[15] + residuals[15]);
}

/* Add inverse to U10.0 buffer and write back. */
static void addResidualsDDS_U10(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[16];

    assert(dst->type == FPU10);

    /* Load pel and convert to S10.5 */
    pelFP[0] = fpU10ToS10(pel[offset]);
    pelFP[1] = fpU10ToS10(pel[offset + skip]);
    pelFP[2] = fpU10ToS10(pel[offset + stride]);
    pelFP[3] = fpU10ToS10(pel[offset + skip + stride]);
    pelFP[4] = fpU10ToS10(pel[offset + 2 * skip]);
    pelFP[5] = fpU10ToS10(pel[offset + 3 * skip]);
    pelFP[6] = fpU10ToS10(pel[offset + 2 * skip + stride]);
    pelFP[7] = fpU10ToS10(pel[offset + 3 * skip + stride]);
    pelFP[8] = fpU10ToS10(pel[offset + 2 * stride]);
    pelFP[9] = fpU10ToS10(pel[offset + skip + 2 * stride]);
    pelFP[10] = fpU10ToS10(pel[offset + 3 * stride]);
    pelFP[11] = fpU10ToS10(pel[offset + skip + 3 * stride]);
    pelFP[12] = fpU10ToS10(pel[offset + 2 * skip + 2 * stride]);
    pelFP[13] = fpU10ToS10(pel[offset + 3 * skip + 2 * stride]);
    pelFP[14] = fpU10ToS10(pel[offset + 2 * skip + 3 * stride]);
    pelFP[15] = fpU10ToS10(pel[offset + 3 * skip + 3 * stride]);

    /* Apply & write back */
    pel[offset] = fpS10ToU10(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS10ToU10(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS10ToU10(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS10ToU10(pelFP[3] + residuals[3]);
    pel[offset + 2 * skip] = fpS10ToU10(pelFP[4] + residuals[4]);
    pel[offset + 3 * skip] = fpS10ToU10(pelFP[5] + residuals[5]);
    pel[offset + 2 * skip + stride] = fpS10ToU10(pelFP[6] + residuals[6]);
    pel[offset + 3 * skip + stride] = fpS10ToU10(pelFP[7] + residuals[7]);
    pel[offset + 2 * stride] = fpS10ToU10(pelFP[8] + residuals[8]);
    pel[offset + skip + 2 * stride] = fpS10ToU10(pelFP[9] + residuals[9]);
    pel[offset + 3 * stride] = fpS10ToU10(pelFP[10] + residuals[10]);
    pel[offset + skip + 3 * stride] = fpS10ToU10(pelFP[11] + residuals[11]);
    pel[offset + 2 * skip + 2 * stride] = fpS10ToU10(pelFP[12] + residuals[12]);
    pel[offset + 3 * skip + 2 * stride] = fpS10ToU10(pelFP[13] + residuals[13]);
    pel[offset + 2 * skip + 3 * stride] = fpS10ToU10(pelFP[14] + residuals[14]);
    pel[offset + 3 * skip + 3 * stride] = fpS10ToU10(pelFP[15] + residuals[15]);
}

/* Add inverse to U12.0 buffer and write back. */
static void addResidualsDDS_U12(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[16];

    assert(dst->type == FPU12);

    /* Load pel and convert to S12.3 */
    pelFP[0] = fpU12ToS12(pel[offset]);
    pelFP[1] = fpU12ToS12(pel[offset + skip]);
    pelFP[2] = fpU12ToS12(pel[offset + stride]);
    pelFP[3] = fpU12ToS12(pel[offset + skip + stride]);
    pelFP[4] = fpU12ToS12(pel[offset + 2 * skip]);
    pelFP[5] = fpU12ToS12(pel[offset + 3 * skip]);
    pelFP[6] = fpU12ToS12(pel[offset + 2 * skip + stride]);
    pelFP[7] = fpU12ToS12(pel[offset + 3 * skip + stride]);
    pelFP[8] = fpU12ToS12(pel[offset + 2 * stride]);
    pelFP[9] = fpU12ToS12(pel[offset + skip + 2 * stride]);
    pelFP[10] = fpU12ToS12(pel[offset + 3 * stride]);
    pelFP[11] = fpU12ToS12(pel[offset + skip + 3 * stride]);
    pelFP[12] = fpU12ToS12(pel[offset + 2 * skip + 2 * stride]);
    pelFP[13] = fpU12ToS12(pel[offset + 3 * skip + 2 * stride]);
    pelFP[14] = fpU12ToS12(pel[offset + 2 * skip + 3 * stride]);
    pelFP[15] = fpU12ToS12(pel[offset + 3 * skip + 3 * stride]);

    /* Apply & write back */
    pel[offset] = fpS12ToU12(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS12ToU12(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS12ToU12(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS12ToU12(pelFP[3] + residuals[3]);
    pel[offset + 2 * skip] = fpS12ToU12(pelFP[4] + residuals[4]);
    pel[offset + 3 * skip] = fpS12ToU12(pelFP[5] + residuals[5]);
    pel[offset + 2 * skip + stride] = fpS12ToU12(pelFP[6] + residuals[6]);
    pel[offset + 3 * skip + stride] = fpS12ToU12(pelFP[7] + residuals[7]);
    pel[offset + 2 * stride] = fpS12ToU12(pelFP[8] + residuals[8]);
    pel[offset + skip + 2 * stride] = fpS12ToU12(pelFP[9] + residuals[9]);
    pel[offset + 3 * stride] = fpS12ToU12(pelFP[10] + residuals[10]);
    pel[offset + skip + 3 * stride] = fpS12ToU12(pelFP[11] + residuals[11]);
    pel[offset + 2 * skip + 2 * stride] = fpS12ToU12(pelFP[12] + residuals[12]);
    pel[offset + 3 * skip + 2 * stride] = fpS12ToU12(pelFP[13] + residuals[13]);
    pel[offset + 2 * skip + 3 * stride] = fpS12ToU12(pelFP[14] + residuals[14]);
    pel[offset + 3 * skip + 3 * stride] = fpS12ToU12(pelFP[15] + residuals[15]);
}

/* Add inverse to U14.0 buffer and write back. */
static void addResidualsDDS_U14(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    int32_t pelFP[16];

    assert(dst->type == FPU14);

    /* Load pel and convert to S14.1 */
    pelFP[0] = fpU14ToS14(pel[offset]);
    pelFP[1] = fpU14ToS14(pel[offset + skip]);
    pelFP[2] = fpU14ToS14(pel[offset + stride]);
    pelFP[3] = fpU14ToS14(pel[offset + skip + stride]);
    pelFP[4] = fpU14ToS14(pel[offset + 2 * skip]);
    pelFP[5] = fpU14ToS14(pel[offset + 3 * skip]);
    pelFP[6] = fpU14ToS14(pel[offset + 2 * skip + stride]);
    pelFP[7] = fpU14ToS14(pel[offset + 3 * skip + stride]);
    pelFP[8] = fpU14ToS14(pel[offset + 2 * stride]);
    pelFP[9] = fpU14ToS14(pel[offset + skip + 2 * stride]);
    pelFP[10] = fpU14ToS14(pel[offset + 3 * stride]);
    pelFP[11] = fpU14ToS14(pel[offset + skip + 3 * stride]);
    pelFP[12] = fpU14ToS14(pel[offset + 2 * skip + 2 * stride]);
    pelFP[13] = fpU14ToS14(pel[offset + 3 * skip + 2 * stride]);
    pelFP[14] = fpU14ToS14(pel[offset + 2 * skip + 3 * stride]);
    pelFP[15] = fpU14ToS14(pel[offset + 3 * skip + 3 * stride]);

    /* Apply & write back */
    pel[offset] = fpS14ToU14(pelFP[0] + residuals[0]);
    pel[offset + skip] = fpS14ToU14(pelFP[1] + residuals[1]);
    pel[offset + stride] = fpS14ToU14(pelFP[2] + residuals[2]);
    pel[offset + skip + stride] = fpS14ToU14(pelFP[3] + residuals[3]);
    pel[offset + 2 * skip] = fpS14ToU14(pelFP[4] + residuals[4]);
    pel[offset + 3 * skip] = fpS14ToU14(pelFP[5] + residuals[5]);
    pel[offset + 2 * skip + stride] = fpS14ToU14(pelFP[6] + residuals[6]);
    pel[offset + 3 * skip + stride] = fpS14ToU14(pelFP[7] + residuals[7]);
    pel[offset + 2 * stride] = fpS14ToU14(pelFP[8] + residuals[8]);
    pel[offset + skip + 2 * stride] = fpS14ToU14(pelFP[9] + residuals[9]);
    pel[offset + 3 * stride] = fpS14ToU14(pelFP[10] + residuals[10]);
    pel[offset + skip + 3 * stride] = fpS14ToU14(pelFP[11] + residuals[11]);
    pel[offset + 2 * skip + 2 * stride] = fpS14ToU14(pelFP[12] + residuals[12]);
    pel[offset + 3 * skip + 2 * stride] = fpS14ToU14(pelFP[13] + residuals[13]);
    pel[offset + 2 * skip + 3 * stride] = fpS14ToU14(pelFP[14] + residuals[14]);
    pel[offset + 3 * skip + 3 * stride] = fpS14ToU14(pelFP[15] + residuals[15]);
}

/* Add inverse to S8.7/S10.5/S12.3/S14.1 buffer and write back. */
static void addResidualsDDS_S16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    int16_t* pel = (int16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    const int32_t min = -32768;
    const int32_t max = 32767;

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Add inverse to S8.7/S10.4/S12.3/S14.1 buffer. */
    /* clang-format off */
	pel[offset]                         = (int16_t)clampS32((pel[offset] + residuals[0]), min, max);
	pel[offset + skip]                  = (int16_t)clampS32((pel[offset + skip] + residuals[1]), min, max);
	pel[offset + stride]                = (int16_t)clampS32((pel[offset + stride] + residuals[2]), min, max);
	pel[offset + skip + stride]         = (int16_t)clampS32((pel[offset + skip + stride] + residuals[3]), min, max);
	pel[offset + 2 * skip]              = (int16_t)clampS32((pel[offset + 2 * skip] + residuals[4]), min, max);
	pel[offset + 3 * skip]              = (int16_t)clampS32((pel[offset + 3 * skip] + residuals[5]), min, max);
	pel[offset + 2 * skip + stride]     = (int16_t)clampS32((pel[offset + 2 * skip + stride] + residuals[6]), min, max);
	pel[offset + 3 * skip + stride]     = (int16_t)clampS32((pel[offset + 3 * skip + stride] + residuals[7]), min, max);
	pel[offset + 2 * stride]            = (int16_t)clampS32((pel[offset + 2 * stride] + residuals[8]), min, max);
	pel[offset + skip + 2 * stride]     = (int16_t)clampS32((pel[offset + skip + 2 * stride] + residuals[9]), min, max);
	pel[offset + 3 * stride]            = (int16_t)clampS32((pel[offset + 3 * stride] + residuals[10]), min, max);
	pel[offset + skip + 3 * stride]     = (int16_t)clampS32((pel[offset + skip + 3 * stride] + residuals[11]), min, max);
	pel[offset + 2 * skip + 2 * stride] = (int16_t)clampS32((pel[offset + 2 * skip + 2 * stride] + residuals[12]), min, max);
	pel[offset + 3 * skip + 2 * stride] = (int16_t)clampS32((pel[offset + 3 * skip + 2 * stride] + residuals[13]), min, max);
	pel[offset + 2 * skip + 3 * stride] = (int16_t)clampS32((pel[offset + 2 * skip + 3 * stride] + residuals[14]), min, max);
	pel[offset + 3 * skip + 3 * stride] = (int16_t)clampS32((pel[offset + 3 * skip + 3 * stride] + residuals[15]), min, max);
    /* clang-format on */
}

/* Write inverse to S8.7/S10.5/S12.3/S14.1 buffer. */
static void writeResidualsDDS_S16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    Surface_t* dst = args->dst;
    int16_t* pel = (int16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Write out. */
    pel[offset] = residuals[0];
    pel[offset + skip] = residuals[1];
    pel[offset + stride] = residuals[2];
    pel[offset + skip + stride] = residuals[3];
    pel[offset + 2 * skip] = residuals[4];
    pel[offset + 3 * skip] = residuals[5];
    pel[offset + 2 * skip + stride] = residuals[6];
    pel[offset + 3 * skip + stride] = residuals[7];
    pel[offset + 2 * stride] = residuals[8];
    pel[offset + skip + 2 * stride] = residuals[9];
    pel[offset + 3 * stride] = residuals[10];
    pel[offset + skip + 3 * stride] = residuals[11];
    pel[offset + 2 * skip + 2 * stride] = residuals[12];
    pel[offset + 3 * skip + 2 * stride] = residuals[13];
    pel[offset + 2 * skip + 3 * stride] = residuals[14];
    pel[offset + 3 * skip + 3 * stride] = residuals[15];
}

/* Write highlight colour to U8.0 buffer. */
static void writeHighlightDDS_U8(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    VN_UNUSED(residuals);

    Surface_t* dst = args->dst;
    uint8_t* pel = dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    const uint8_t highlight = (uint8_t)args->highlight->valUnsigned;

    pel[offset] = highlight;
    pel[offset + skip] = highlight;
    pel[offset + stride] = highlight;
    pel[offset + skip + stride] = highlight;
    pel[offset + 2 * skip] = highlight;
    pel[offset + 3 * skip] = highlight;
    pel[offset + 2 * skip + stride] = highlight;
    pel[offset + 3 * skip + stride] = highlight;
    pel[offset + 2 * stride] = highlight;
    pel[offset + skip + 2 * stride] = highlight;
    pel[offset + 3 * stride] = highlight;
    pel[offset + skip + 3 * stride] = highlight;
    pel[offset + 2 * skip + 2 * stride] = highlight;
    pel[offset + 3 * skip + 2 * stride] = highlight;
    pel[offset + 2 * skip + 3 * stride] = highlight;
    pel[offset + 3 * skip + 3 * stride] = highlight;
}

/* Write highlight colour to U10.0/U12.0/U14.0 buffer. */
static void writeHighlightDDS_U16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    VN_UNUSED(residuals);

    Surface_t* dst = args->dst;
    uint16_t* pel = (uint16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    const uint16_t highlight = args->highlight->valUnsigned;

    pel[offset] = highlight;
    pel[offset + skip] = highlight;
    pel[offset + stride] = highlight;
    pel[offset + skip + stride] = highlight;
    pel[offset + 2 * skip] = highlight;
    pel[offset + 3 * skip] = highlight;
    pel[offset + 2 * skip + stride] = highlight;
    pel[offset + 3 * skip + stride] = highlight;
    pel[offset + 2 * stride] = highlight;
    pel[offset + skip + 2 * stride] = highlight;
    pel[offset + 3 * stride] = highlight;
    pel[offset + skip + 3 * stride] = highlight;
    pel[offset + 2 * skip + 2 * stride] = highlight;
    pel[offset + 3 * skip + 2 * stride] = highlight;
    pel[offset + 2 * skip + 3 * stride] = highlight;
    pel[offset + 3 * skip + 3 * stride] = highlight;
}

/* Write highlight colour to S8.7/S10.5/S12.3/S14.1 buffer. */
static void writeHighlightDDS_S16(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals)
{
    VN_UNUSED(residuals);

    Surface_t* dst = args->dst;
    int16_t* pel = (int16_t*)dst->data;
    const int32_t stride = (int32_t)dst->stride;
    const int32_t skip = args->skip;
    const int32_t offset = args->offset + (x * skip) + (y * stride);
    const int16_t highlight = args->highlight->valSigned;

    pel[offset] = highlight;
    pel[offset + skip] = highlight;
    pel[offset + stride] = highlight;
    pel[offset + skip + stride] = highlight;
    pel[offset + 2 * skip] = highlight;
    pel[offset + 3 * skip] = highlight;
    pel[offset + 2 * skip + stride] = highlight;
    pel[offset + 3 * skip + stride] = highlight;
    pel[offset + 2 * stride] = highlight;
    pel[offset + skip + 2 * stride] = highlight;
    pel[offset + 3 * stride] = highlight;
    pel[offset + skip + 3 * stride] = highlight;
    pel[offset + 2 * skip + 2 * stride] = highlight;
    pel[offset + 3 * skip + 2 * stride] = highlight;
    pel[offset + 2 * skip + 3 * stride] = highlight;
    pel[offset + 3 * skip + 3 * stride] = highlight;
}

/*------------------------------------------------------------------------------*/

typedef void (*ResidualFunction_t)(ResidualArgs_t* args, int32_t x, int32_t y, const int16_t* residuals);

typedef struct ApplyResidualFunctions
{
    ResidualFunction_t addResiduals;
    ResidualFunction_t writeResiduals;
    ResidualFunction_t writeHighlight;
} ResidualFunctions_t;

/* clang-format off */
static const ResidualFunctions_t kResidualFunctionTable[FPCount][2] = {
	{{addResidualsDD_U8,  NULL,                 writeHighlightDD_U8},  {addResidualsDDS_U8,  NULL,                  writeHighlightDDS_U8}},  /* U8 */
	{{addResidualsDD_U10, NULL,                 writeHighlightDD_U16}, {addResidualsDDS_U10, NULL,                  writeHighlightDDS_U16}}, /* U10 */
	{{addResidualsDD_U12, NULL,                 writeHighlightDD_U16}, {addResidualsDDS_U12, NULL,                  writeHighlightDDS_U16}}, /* U12 */
	{{addResidualsDD_U14, NULL,                 writeHighlightDD_U16}, {addResidualsDDS_U14, NULL,                  writeHighlightDDS_U16}}, /* U14 */
	{{addResidualsDD_S16, writeResidualsDD_S16, writeHighlightDD_S16}, {addResidualsDDS_S16, writeResidualsDDS_S16, writeHighlightDDS_S16}}, /* S8.7 */
	{{addResidualsDD_S16, writeResidualsDD_S16, writeHighlightDD_S16}, {addResidualsDDS_S16, writeResidualsDDS_S16, writeHighlightDDS_S16}}, /* S10.5 */
	{{addResidualsDD_S16, writeResidualsDD_S16, writeHighlightDD_S16}, {addResidualsDDS_S16, writeResidualsDDS_S16, writeHighlightDDS_S16}}, /* S12.3 */
	{{addResidualsDD_S16, writeResidualsDD_S16, writeResidualsDD_S16}, {addResidualsDDS_S16, writeResidualsDDS_S16, writeResidualsDDS_S16}}, /* S14.1 */
};
/* clang-format on */

typedef enum ResidualMode
{
    RM_Add = 0,
    RM_Write,
    RM_Highlight,
} ResidualMode_t;

static inline ResidualFunction_t getResidualFunction(ResidualMode_t mode, bool bDDS, FixedPoint_t fpType)
{
    const ResidualFunctions_t* fns = &kResidualFunctionTable[fpType][bDDS];

    switch (mode) {
        case RM_Add: return fns->addResiduals;
        case RM_Write: return fns->writeResiduals;
        case RM_Highlight: return fns->writeHighlight;
        default: break;
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/

typedef struct ConvertArgs
{
    Surface_t* src;
    int32_t srcSkip;
    int32_t srcOffset;
    Surface_t* dst;
    int32_t dstSkip;
    int32_t dstOffset;
} ConvertArgs_t;

/* Converts S87 value in a source buffer to an S8 representation in the dest buffer for a DD transform */
static void convertDD_S87_S8(ConvertArgs_t* args, int32_t x, int32_t y)
{
    Surface_t* src = args->src;
    Surface_t* dst = args->dst;
    uint8_t* dstPels = (uint8_t*)dst->data;
    const int16_t* srcPels = (const int16_t*)src->data;
    const int32_t dstStride = (int32_t)dst->stride;
    const int32_t srcStride = (int32_t)src->stride;
    const int32_t dstSkip = args->dstSkip;
    const int32_t srcSkip = args->srcSkip;
    const uint32_t dstOffset = args->dstOffset + (x * dstSkip) + (y * dstStride);
    const uint32_t srcOffset = args->srcOffset + (x * srcSkip) + (y * srcStride);

    assert(src->type == FPS8);

    /* clang-format off */
	dstPels[dstOffset]                       = (uint8_t)(srcPels[srcOffset] >> 8);
	dstPels[dstOffset + dstSkip]             = (uint8_t)(srcPels[srcOffset + srcSkip] >> 8);
	dstPels[dstOffset + dstStride]           = (uint8_t)(srcPels[srcOffset + srcStride] >> 8);
	dstPels[dstOffset + dstSkip + dstStride] = (uint8_t)(srcPels[srcOffset + srcSkip + srcStride] >> 8);
    /* clang-format on */
}

/* Converts S87 value in a source buffer to an S8 representation in the dest buffer for a DDS transform */
static void convertDDS_S87_S8(ConvertArgs_t* args, int32_t x, int32_t y)
{
    Surface_t* src = args->src;
    Surface_t* dst = args->dst;
    uint8_t* dstPels = (uint8_t*)dst->data;
    const int16_t* srcPels = (const int16_t*)src->data;
    const int32_t dstStride = (int32_t)dst->stride;
    const int32_t srcStride = (int32_t)src->stride;
    const int32_t dstSkip = args->dstSkip;
    const int32_t srcSkip = args->srcSkip;
    const int32_t dstOffset = args->dstOffset + (x * dstSkip) + (y * dstStride);
    const int32_t srcOffset = args->srcOffset + (x * srcSkip) + (y * srcStride);

    assert(src->type == FPS8);

    /* clang-format off */
	dstPels[dstOffset]                               = (uint8_t)(srcPels[srcOffset] >> 8);
	dstPels[dstOffset + dstSkip]                     = (uint8_t)(srcPels[srcOffset + srcSkip] >> 8);
	dstPels[dstOffset + dstStride]                   = (uint8_t)(srcPels[srcOffset + srcStride] >> 8);
	dstPels[dstOffset + dstSkip + dstStride]         = (uint8_t)(srcPels[srcOffset + srcSkip + srcStride] >> 8);
	dstPels[dstOffset + 2 * dstSkip]                 = (uint8_t)(srcPels[srcOffset + 2 * srcSkip] >> 8);
	dstPels[dstOffset + 3 * dstSkip]                 = (uint8_t)(srcPels[srcOffset + 3 * srcSkip] >> 8);
	dstPels[dstOffset + 2 * dstSkip + dstStride]     = (uint8_t)(srcPels[srcOffset + 2 * srcSkip + srcStride] >> 8);
	dstPels[dstOffset + 3 * dstSkip + dstStride]     = (uint8_t)(srcPels[srcOffset + 3 * srcSkip + srcStride] >> 8);
	dstPels[dstOffset + 2 * dstStride]               = (uint8_t)(srcPels[srcOffset + 2 * srcStride] >> 8);
	dstPels[dstOffset + dstSkip + 2 * dstStride]     = (uint8_t)(srcPels[srcOffset + srcSkip + 2 * srcStride] >> 8);
	dstPels[dstOffset + 3 * dstStride]               = (uint8_t)(srcPels[srcOffset + 3 * srcStride] >> 8);
	dstPels[dstOffset + dstSkip + 3 * dstStride]     = (uint8_t)(srcPels[srcOffset + srcSkip + 3 * srcStride] >> 8);
	dstPels[dstOffset + 2 * dstSkip + 2 * dstStride] = (uint8_t)(srcPels[srcOffset + 2 * srcSkip + 2 * srcStride] >> 8);
	dstPels[dstOffset + 3 * dstSkip + 2 * dstStride] = (uint8_t)(srcPels[srcOffset + 3 * srcSkip + 2 * srcStride] >> 8);
	dstPels[dstOffset + 2 * dstSkip + 3 * dstStride] = (uint8_t)(srcPels[srcOffset + 2 * srcSkip + 3 * srcStride] >> 8);
	dstPels[dstOffset + 3 * dstSkip + 3 * dstStride] = (uint8_t)(srcPels[srcOffset + 3 * srcSkip + 3 * srcStride] >> 8);
    /* clang-format on */
}

/*------------------------------------------------------------------------------*/

typedef void (*ConvertFunction_t)(ConvertArgs_t* args, int32_t x, int32_t y);

static const ConvertFunction_t kConvertTable[2] = {convertDD_S87_S8, convertDDS_S87_S8};

static inline ConvertFunction_t getConvertFunction(bool bDDS) { return kConvertTable[bDDS]; }

/* ----------------------------------------------------------------------------*/

static void clearPatch(Surface_t* dst, uint32_t x, uint32_t y, uint32_t elementSize,
                       uint32_t patchWidth, uint32_t patchHeight)
{
    assert(dst);

    const uint32_t width = dst->width;
    const uint32_t height = dst->height;
    const uint32_t stride = dst->stride;
    const uint32_t byteCount = minU32(patchWidth, width - x) * elementSize;
    const uint32_t y_max = minU32(y + patchHeight, height);
    const uint32_t step = stride * elementSize;

    uint8_t* pels = &dst->data[(y * stride * elementSize) + (x * elementSize)];

    assert(x < width);

    for (; y < y_max; y++) {
        memset(pels, 0, byteCount);
        pels += step;
    }
}

/* ----------------------------------------------------------------------------*/

/*! \brief Helper function for remapping the current DDS residual layout to a scanline
 *         ordering to simplify the usage of cmdbuffers.
 *
 *  \note  This function is fully intended to be removed and is an intermediate
 *         solution as the effort to change the residual memory representation is
 *         significant. */
static inline void cmdbufferAppendDDS(CmdBuffer_t* cmdbuffer, int16_t x, int16_t y, const int16_t* values)
{
    const int16_t tmp[16] = {values[0],  values[1],  values[4],  values[5], values[2],  values[3],
                             values[6],  values[7],  values[8],  values[9], values[12], values[13],
                             values[10], values[11], values[14], values[15]};

    cmdBufferAppend(cmdbuffer, x, y, tmp);
}

/* ----------------------------------------------------------------------------*/

int32_t prepareLayerDecoders(Context_t* ctx, TileState_t* tile,
                             EntropyDecoder_t residualDecoders[RCLayerCountDDS],
                             EntropyDecoder_t* temporalDecoder, int32_t layerCount)
{
    int32_t res;

    if (tile->chunks) {
        for (int32_t layerIdx = 0; layerIdx < layerCount; ++layerIdx) {
            VN_CHECK(entropyInitialise(ctx, &residualDecoders[layerIdx], &tile->chunks[layerIdx], EDTDefault));
        }
    }

    if (tile->temporalChunk) {
        VN_CHECK(entropyInitialise(ctx, temporalDecoder, tile->temporalChunk, EDTTemporal));
    }

    return 0;
}

void releaseLayerDecoders(EntropyDecoder_t residualDecoders[RCLayerCountDDS], EntropyDecoder_t* temporalDecoder)
{
    for (int32_t layerIdx = 0; layerIdx < RCLayerCountDDS; ++layerIdx) {
        entropyRelease(&residualDecoders[layerIdx]);
    }

    entropyRelease(temporalDecoder);
}

/* ----------------------------------------------------------------------------*/

typedef struct CacheTileData
{
    TileState_t* tiles;
    int32_t tileCount;
} CacheTileData_t;

typedef struct DecodeSerial
{
    Memory_t memory;
    CacheTileData_t tiles[AC_MaxResidualParallel];
    bool generateCmdBuffers;
    CmdBuffer_t* cmdBufferIntra[LOQEnhancedCount]; /**< Intra command buffer for both enhanced LOQ. */
    CmdBuffer_t* cmdBufferInter;                   /**< Inter command buffer for LOQ-0. */
    CmdBuffer_t* cmdBufferClear;                   /**< Clear tile command buffer for LOQ-0. */
}* DecodeSerial_t;

static int32_t tilesCheckAlloc(Context_t* ctx, int32_t planeIndex, int32_t tileCount)
{
    /* Validate args. */
    if (planeIndex < 0 || planeIndex > AC_MaxResidualParallel) {
        return -1;
    }

    if (tileCount < 0) {
        return -1;
    }

    CacheTileData_t* tileData = &ctx->decodeSerial->tiles[planeIndex];

    if (tileData->tileCount != tileCount) {
        TileState_t* newPtr = VN_REALLOC_T_ARR(ctx->memory, tileData->tiles, TileState_t, tileCount);

        if (!newPtr) {
            return -1;
        }

        tileData->tiles = newPtr;
        tileData->tileCount = tileCount;

        memset(tileData->tiles, 0, tileCount * sizeof(TileState_t));
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

typedef struct ApplyResidualJobData
{
    Context_t* ctx;
    uint32_t plane;
    LOQIndex_t loq;
    const Dequant_t* dequant;
    FieldType_t fieldType;
    bool temporal;
    Surface_t* dst;
    uint32_t dstChannel;
    TileState_t* tiles;
    int32_t tileCount;
} ApplyResidualJobData_t;

/*------------------------------------------------------------------------------*/

static int32_t applyResidualJob(void* jobData)
{
    ApplyResidualJobData_t* applyData = (ApplyResidualJobData_t*)jobData;
    Context_t* ctx = applyData->ctx;
    DeserialisedData_t* data = &ctx->deserialised;
    int32_t res = 0;

    /* general */
    const LOQIndex_t loq = applyData->loq;
    const Dequant_t* dequant = applyData->dequant;
    const uint8_t numLayers = data->numLayers;
    const bool dds = data->transform == TransformDDS;
    const ScalingMode_t scaling = (LOQ0 == loq) ? data->scalingModes[LOQ0] : Scale2D;
    PlaneSurfaces_t* plane = &ctx->planes[applyData->plane];

    /* user data */
    const UserDataConfig_t* userData = &data->userData;

    /* temporal */
    Surface_t* temporalSurface = NULL;
    uint8_t* temporalBlockSignal = NULL;

    /* cmdbuffer */
    const uint8_t generateCmdBuffers = ctx->generateCmdBuffers;
    CmdBuffer_t* cmdBufIntra = ctx->decodeSerial->cmdBufferIntra[loq];
    CmdBuffer_t* cmdBufInter = (loq == LOQ0) ? ctx->decodeSerial->cmdBufferInter : NULL;
    CmdBuffer_t* cmdBufClear = (loq == LOQ0) ? ctx->decodeSerial->cmdBufferClear : NULL;

    /* decoders (outer scope for error-exit condition) */
    EntropyDecoder_t residualDecoders[RCLayerCountDDS] = {{0}};
    EntropyDecoder_t temporalDecoder = {0};

    /* functions */
    TransformFunction_t transformFn;
    ResidualFunction_t applyFn;
    ResidualFunction_t writeFn;
    ResidualArgs_t residualArgs = {0};
    ConvertFunction_t convertFn = NULL;
    ConvertArgs_t convertArgs = {0};
    const ResidualMode_t residualMode = ctx->highlightState[loq].enabled ? RM_Highlight : RM_Add;

    VN_PROFILE_START_DYNAMIC("apply_plane loq=%d plane=%d", (loq == LOQ0) ? 0 : 1, applyData->plane);

    residualArgs.highlight = &ctx->highlightState[loq];

    if (ctx->generateSurfaces) {
        /* @todo: probably should be able to use external surfaces even when not generating surfaces */
        if (ctx->useExternalSurfaces && !ctx->convertS8) {
            residualArgs.dst = &plane->externalSurfaces[loq];
        } else {
            residualArgs.dst =
                (loq == LOQ0) ? &plane->temporalBuffer[applyData->fieldType] : &plane->basePixels;
        }
    } else if (applyData->temporal) {
        residualArgs.dst = &plane->temporalBuffer[applyData->fieldType];
    } else if (applyData->dst) {
            /* Use the external surface stride. */
            residualArgs.dst = applyData->dst;
    } else {
        /* NULL pointers for destination surfaces is a feature request to allow just a temporal
           update, to facilitate a frame-drop mechanism by an integration. Such a decode does not
           apply temporal, and does not supply a valid surface (as it was a frame-drop), so it
           simply needs to do nothing now. */
        return 0;
    }

    /* Setup residual function. @todo: Caller could provide surfaces they wish to write to. */
    transformFn = transformGetFunction(data->transform, scaling, ctx->cpuFeatures);
    applyFn = getResidualFunction(residualMode, dds, residualArgs.dst->type);
    writeFn = getResidualFunction(RM_Write, dds, residualArgs.dst->type);

    VN_CHECKJ(surfaceGetChannelSkipOffset(residualArgs.dst, applyData->dstChannel,
                                          &residualArgs.skip, &residualArgs.offset));

    if (loq == LOQ0 && applyData->temporal) {
        if (ctx->generateSurfaces && ctx->useExternalSurfaces && !ctx->convertS8) {
            temporalSurface = &plane->externalSurfaces[loq];
        } else {
            temporalSurface = &plane->temporalBuffer[applyData->fieldType];
        }
    }

    /* Setup conversion if needed. */
    if (ctx->generateSurfaces && ctx->convertS8) {
        convertFn = getConvertFunction(dds);

        convertArgs.src = residualArgs.dst;
        convertArgs.srcSkip = residualArgs.skip;
        convertArgs.srcOffset = residualArgs.offset;

        if (ctx->useExternalSurfaces) {
            convertArgs.dst = &plane->externalSurfaces[loq];
        } else {
            convertArgs.dst = (loq == LOQ0) ? &plane->temporalBufferU8 : &plane->basePixelsU8;
        }

        VN_CHECKJ(surfaceGetChannelSkipOffset(convertArgs.dst, applyData->dstChannel,
                                              &convertArgs.dstSkip, &convertArgs.dstOffset));
    }

    for (int32_t tileIndex = 0; tileIndex < applyData->tileCount; ++tileIndex) {
        int16_t coeffs[RCLayerCountDDS] = {0};
        int16_t residuals[RCLayerCountDDS] = {0};
        int32_t zeros[RCLayerCountDDS] = {0}; /* Current zero run in each layer */
        int32_t temporalRun = 0;              /* Current symbol run in temporal layer */
        uint32_t tuIndex = 0;
        TUState_t tuArg = {0};
        TileState_t* tile = &applyData->tiles[tileIndex];
        uint32_t x = tile->x;
        uint32_t y = tile->y;
        uint8_t decodedTemporalSignal = 0;
        bool decodedIntraBlockStart = 0;
        uint16_t coeffsNonzeroMask = 0;

        /* Setup decoders */
        VN_CHECKJ(prepareLayerDecoders(ctx, tile, residualDecoders, &temporalDecoder, numLayers));

        /* Setup TU */
        VN_CHECKJ(tuStateInitialise(&tuArg, tile, dds ? 4 : 2));

        if (loq == LOQ0 && applyData->temporal && data->temporalUseReducedSignalling &&
            (data->temporalStepWidthModifier != 0)) {
            const uint32_t blockCount = tuArg.block.blocksPerRow * tuArg.block.blocksPerCol;
            temporalBlockSignal = VN_CALLOC_T_ARR(ctx->memory, uint8_t, blockCount);
        }

        /* Break loop once tile is fully decoded. */
        while (true) {
            int32_t minZeroCount = INT_MAX;
            TemporalSignal_t temporal = TSInter;
            const bool blockStart = (x % BSTemporal == 0) && (y % BSTemporal == 0);

            /* Decode bitstream and track zero runs */
            for (uint32_t i = 0; i < numLayers; i++) {
                if (zeros[i] > 0) {
                    --zeros[i];
                    coeffs[i] = 0;

                    /* clear i-th bit */
                    coeffsNonzeroMask &= ~(1 << i);
                } else {
                    if (tile->chunks) {
                        const int32_t layerZero = entropyDecode(&residualDecoders[i], &coeffs[i]);
                        zeros[i] = (layerZero == EntropyNoData) ? (int32_t)(tuArg.tuTotal - 1) : layerZero;
                        VN_CHECKJ(zeros[i]);

                        /* set i-th bit */
                        coeffsNonzeroMask |= (1 << i);
                    } else {
                        /* No decoder, skip over whole surface. */
                        zeros[i] = tuArg.tuTotal - 1;
                        coeffs[i] = 0;
                    }
                }

                /* Calculate lowest common zero run */
                if (minZeroCount > zeros[i]) {
                    minZeroCount = zeros[i];
                }
            }

            /* Perform user data modification if needed. */
            stripUserData(loq, userData, coeffs);

            /* Decode temporal and track temporal run */
            if (applyData->temporal && tile->temporalChunk) {
                if (temporalRun > 0) {
                    --temporalRun;
                } else {
                    int32_t temporalCount =
                        entropyDecodeTemporal(&temporalDecoder, &decodedTemporalSignal);

                    decodedIntraBlockStart = 0;

                    if (temporalCount == EntropyNoData) {
                        temporalRun = (int32_t)tuArg.tuTotal;
                    } else {
                        temporalRun = temporalCount;
                    }

                    /* Decrement run by 1 if just decoded. Temporal signal run is inclusive
                       of current symbol. RLE signals run is exclusive of current symbol.
                       All the processing assumes the run is the number after the current
                       symbol */
                    if (temporalRun <= 0) {
                        VN_ERROR(ctx->log, "invalid temporal_run value %d\n", temporalRun);
                        res = -1; /* show we failed */
                        break;    /* exit while(true) loop */
                    }
                    temporalRun -= 1;
                }

                /* Load up currently decoded temporal signal. */
                temporal = (TemporalSignal_t)decodedTemporalSignal;

                /* Process the intra blocks when running reduced signaling. This can occur
                 * at any point during a temporal run of Intra signals. So must be tracked
                 * and only performed when the first Intra signal to touch a block start
                 * is encountered, all subsequent Intra signals are guaranteed to be
                 * block start signals so consider them here. */
                if (data->temporalUseReducedSignalling && (decodedTemporalSignal == TSIntra) &&
                    blockStart && !decodedIntraBlockStart) {
                    uint32_t blockTUIndex = tuIndex;
                    uint32_t blockX = x;
                    uint32_t blockY = y;
                    uint32_t blockTUCount, block_width, block_height;
                    uint32_t temporalCount = temporalRun + 1; /* Reintroduce the initial decremented 1. */

                    /* Prepare state for block run. */
                    temporalRun = 0;
                    decodedIntraBlockStart = 1;

                    while (temporalCount) {
                        tuCoordsBlockDetails(&tuArg, blockX, blockY, &block_width, &block_height,
                                             &blockTUCount);
                        temporalRun += blockTUCount;

                        if (generateCmdBuffers && cmdBufClear) {
                            cmdBufferAppendCoord(cmdBufClear, (int16_t)blockX, (int16_t)blockY);
                        } else {
                            /* Reset block on temporal surface */
                            clearPatch(temporalSurface, blockX, blockY, sizeof(int16_t),
                                       block_width, block_height);

                            if (ctx->generateSurfaces && ctx->convertS8) {
                                clearPatch(convertArgs.dst, blockX, blockY, sizeof(uint8_t),
                                           block_width, block_height);
                            }
                        }

                        /* Update block signaling. */
                        if (temporalBlockSignal) {
                            uint32_t blockIndex;
                            VN_CHECKJ(tuCoordsBlockIndex(&tuArg, blockX, blockY, &blockIndex));
                            temporalBlockSignal[blockIndex] = TSIntra;
                        }

                        /* Move onto next block signal. */
                        temporalCount--;

                        if (temporalCount) {
                            blockTUIndex += blockTUCount;
                            if (tuCoordsBlockRaster(&tuArg, blockTUIndex, &blockX, &blockY) < 0) {
                                res = -1;
                                VN_ERROR(ctx->log, "Error obtaining temporal block coords, index: %d\n",
                                         blockTUIndex);
                                goto error_exit;
                            }
                        }
                    }

                    /* Similar to run decode, reduce run by 1. */
                    temporalRun -= 1;
                }

                /* Calculate lowest common run of zeros. We can jump over temporal runs
                 * if we're running Inter, as we don't need to clear each transform. Or
                 * if we've already processed all intra block in this temporal run and
                 * cleared them up front. */
                if (temporal == TSInter || decodedIntraBlockStart) {
                    if (minZeroCount > temporalRun) {
                        minZeroCount = temporalRun;
                    }
                } else {
                    assert(temporal == TSIntra);
                    minZeroCount = 0;
                }

                /* When running reduced signaling, and the temporal step-width modifier
                 * is not 0 (implied by temporal_block_signal being valid) then we need
                 * to check if the block is intra, if so then the signal is intra too. */
                if (data->temporalUseReducedSignalling && temporalBlockSignal) {
                    uint32_t blockIndex;
                    VN_CHECKJ(tuCoordsBlockIndex(&tuArg, x, y, &blockIndex));

                    if (temporalBlockSignal[blockIndex] == TSIntra) {
                        /* As a temporal_block_signal can only be Intra if the blocks
                         * have been processed there is no need to modify min_zero_cnt. */
                        temporal = TSIntra;
                    }
                }
            }

            /* Only actually apply if there is some meaningful data and the operation
             * will have side-effects. */
            if (!applyData->temporal || temporal != TSInter || coeffsNonzeroMask != 0) {
                /* Apply SW to coeffs - this is not performed in decode loop as the temporal
                 * signal residual could be zero (implied inter), however the block signal
                 * could be intra. */
                for (uint32_t i = 0; i < numLayers; i++) {
                    const int16_t coeffSign = (coeffs[i] > 0) ? 1 : ((coeffs[i] < 0) ? (-1) : 0);
                    coeffs[i] *= dequant->stepWidth[temporal][i];            /* Simple dequant */
                    coeffs[i] += (coeffSign * dequant->offset[temporal][i]); /* Applying dead zone */
                }

                /* Inverse hadamard */
                transformFn(coeffs, residuals);

                /* Apply deblocking coefficients when enabled. */
                if ((LOQ1 == loq) && dds && data->deblock.enabled) {
                    deblockResiduals(&data->deblock, residuals);
                }

                if (generateCmdBuffers) {
                    CmdBuffer_t* dstCmdBuffer =
                        ((temporal == TSInter) && (loq == LOQ0)) ? cmdBufInter : cmdBufIntra;

                    if (dds) {
                        cmdbufferAppendDDS(dstCmdBuffer, (int16_t)x, (int16_t)y, residuals);
                    } else {
                        cmdBufferAppend(dstCmdBuffer, (int16_t)x, (int16_t)y, residuals);
                    }
                } else {
                    if (temporal == TSInter) {
                        applyFn(&residualArgs, x, y, residuals);
                    } else {
                        writeFn(&residualArgs, x, y, residuals);
                    }

                    /* Optionally convert. */
                    if (convertFn) {
                        convertFn(&convertArgs, x, y);
                    }
                }
            }

            /* Surface traversal. Move onto the next coord, skipping as many as we can.
             * tu_index will always step at least one unit. */
            tuIndex += (1 + minZeroCount);

            if (data->temporalEnabled || (data->tileDimensions != TDTNone)) {
                res = tuCoordsBlockRaster(&tuArg, tuIndex, &x, &y);

                if (applyData->temporal && tile->temporalChunk) {
                    temporalRun -= minZeroCount;
                }
            } else {
                res = tuCoordsSurfaceRaster(&tuArg, tuIndex, &x, &y);
            }

            VN_CHECKJ(res);

            if (res > 0) {
                break;
            }

            for (uint32_t i = 0; i < numLayers; ++i) {
                zeros[i] -= minZeroCount;
            }
        }

        releaseLayerDecoders(residualDecoders, &temporalDecoder);
        VN_FREE(ctx->memory, temporalBlockSignal);
    }

error_exit:
    releaseLayerDecoders(residualDecoders, &temporalDecoder);
    VN_FREE(ctx->memory, temporalBlockSignal);
    VN_PROFILE_STOP();
    return (res < 0) ? res : 0;
}

/*------------------------------------------------------------------------------*/

static int32_t applyResidualExecute(Context_t* ctx, const DecodeSerialArgs_t* params)
{
    int32_t res = 0;
    ThreadManager_t* threadManager = &ctx->threadManager;
    DeserialisedData_t* data = &ctx->deserialised;
    const LOQIndex_t loq = params->loq;
    const bool temporalEnabled = (loq == LOQ0) && data->temporalEnabled;
    const int32_t planeCount = data->numPlanes;
    ApplyResidualJobData_t threadData[AC_MaxResidualParallel] = {{0}};
    int32_t planeIndex = 0;

    assert(planeCount <= AC_MaxResidualParallel);

    VN_PROFILE_START_DYNAMIC("apply_residual_execute %s", loqIndexToString(loq));

    for (; planeIndex < planeCount && planeIndex < RCMaxPlanes; planeIndex += 1) {
        CacheTileData_t* tileCache = &ctx->decodeSerial->tiles[planeIndex];
        const int32_t planeWidth = (int32_t)params->dst[planeIndex]->width;
        const int32_t planeHeight = (int32_t)params->dst[planeIndex]->height;
        const int32_t tileCount = data->tileCount[planeIndex][loq];
        const int32_t tileWidth = data->tileWidth[planeIndex];
        const int32_t tileHeight = data->tileHeight[planeIndex];

        VN_CHECKJ(tilesCheckAlloc(ctx, planeIndex, tileCount));

        for (int32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
            TileState_t* tile = &tileCache->tiles[tileIndex];

            const int32_t tileIndexX = tileIndex % data->tilesAcross[planeIndex][loq];
            const int32_t tileIndexY = tileIndex / data->tilesAcross[planeIndex][loq];

            assert(tileIndexY < data->tilesDown[planeIndex][loq]);

            tile->x = tileIndexX * tileWidth;
            tile->y = tileIndexY * tileHeight;
            tile->width = minS32(tileWidth, planeWidth - tile->x);
            tile->height = minS32(tileHeight, planeHeight - tile->y);

            VN_CHECKJ(deserialiseGetTileLayerChunks(data, planeIndex, loq, tileIndex, &tile->chunks));

            if (loq == LOQ0) {
                VN_CHECKJ(deserialiseGetTileTemporalChunk(data, planeIndex, tileIndex, &tile->temporalChunk));
            } else {
                tile->temporalChunk = NULL;
            }
        }

        threadData[planeIndex].dequant = contextGetDequant(ctx, planeIndex, loq);
        threadData[planeIndex].ctx = ctx;
        threadData[planeIndex].dst = params->dst[planeIndex];
        threadData[planeIndex].plane = planeIndex;
        threadData[planeIndex].loq = loq;
        threadData[planeIndex].fieldType = data->fieldType;
        threadData[planeIndex].temporal = temporalEnabled;
        threadData[planeIndex].tiles = tileCache->tiles;
        threadData[planeIndex].tileCount = tileCount;
    }

    res = threadingExecuteJobs(threadManager, applyResidualJob, threadData, planeIndex,
                               sizeof(ApplyResidualJobData_t))
              ? 0
              : -1;

error_exit:
    VN_PROFILE_STOP();
    return res;
}

int32_t decodeSerial(Context_t* ctx, const DecodeSerialArgs_t* params)
{
    DecodeSerial_t decode = ctx->decodeSerial;

    if (!decode) {
        VN_ERROR(ctx->log, "Attempted to perform decoding without initialising the decoder");
        return -1;
    }

    /* Check that the plane configurations are valid. Either Y or YUV must be present. */
    int32_t planeCheck = 0;
    for (int32_t i = 0; i < 3; ++i) {
        planeCheck |= (params->dst[i] != NULL) ? (1 << i) : 0;
    }

    if ((planeCheck != 1) && (planeCheck != 7)) {
        VN_ERROR(ctx->log, "No destination surfaces supplied\n");
        return -1;
    }

    /* Ensure LOQ is valid. */
    if (params->loq != LOQ0 && params->loq != LOQ1) {
        VN_ERROR(ctx->log, "Supplied LOQ is invalid, must be LOQ-0 or LOQ-1\n");
        return -1;
    }

    if (decode->generateCmdBuffers) {
        if (params->loq == LOQ1) {
            cmdBufferReset(decode->cmdBufferIntra[LOQ1], ctx->deserialised.numLayers);
        } else {
            cmdBufferReset(decode->cmdBufferIntra[LOQ0], ctx->deserialised.numLayers);
            cmdBufferReset(decode->cmdBufferInter, ctx->deserialised.numLayers);
            cmdBufferReset(decode->cmdBufferClear, 0);
        }
    }

    return applyResidualExecute(ctx, params);
}

bool decodeSerialInitialize(Memory_t memory, DecodeSerial_t* decode, bool generateCmdBuffers)
{
    DecodeSerial_t result = VN_CALLOC_T(memory, struct DecodeSerial);

    if (!result) {
        return false;
    }

    result->memory = memory;

    if (generateCmdBuffers) {
        for (int32_t i = 0; i < LOQEnhancedCount; ++i) {
            if (!cmdBufferInitialise(memory, &result->cmdBufferIntra[i], CBTResiduals)) {
                goto error_exit;
            }
        }

        if (!cmdBufferInitialise(memory, &result->cmdBufferInter, CBTResiduals)) {
            goto error_exit;
        }

        if (!cmdBufferInitialise(memory, &result->cmdBufferClear, CBTCoordinates)) {
            goto error_exit;
        }
    }

    result->generateCmdBuffers = generateCmdBuffers;

    *decode = result;
    return true;

error_exit:
    decodeSerialRelease(result);
    return false;
}

void decodeSerialRelease(DecodeSerial_t decode)
{
    if (!decode) {
        return;
    }

    Memory_t memory = decode->memory;

    for (int32_t i = 0; i < AC_MaxResidualParallel; ++i) {
        VN_FREE(memory, decode->tiles[i].tiles);
    }

    for (int32_t i = 0; i < LOQEnhancedCount; ++i) {
        cmdBufferFree(decode->cmdBufferIntra[i]);
    }

    cmdBufferFree(decode->cmdBufferInter);
    cmdBufferFree(decode->cmdBufferClear);

    VN_FREE(memory, decode);
}

CmdBuffer_t* decodeSerialGetTileClearCmdBuffer(const DecodeSerial_t decode)
{
    return decode->cmdBufferClear;
}

CmdBuffer_t* decodeSerialGetResidualCmdBuffer(const DecodeSerial_t decode,
                                              TemporalSignal_t temporal, LOQIndex_t loq)
{
    if (temporal == TSInter) {
        return decode->cmdBufferInter;
    }

    if (temporal == TSIntra) {
        return decode->cmdBufferIntra[loq];
    }

    return NULL;
}

/*------------------------------------------------------------------------------*/
