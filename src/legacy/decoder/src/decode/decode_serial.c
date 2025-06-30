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

#include "decode/decode_serial.h"

#include "common/cmdbuffer.h"
#include "common/log.h"
#include "common/memory.h"
#include "common/platform.h"
#include "common/threading.h"
#include "common/tile.h"
#include "common/types.h"
#include "context.h"
#include "decode/apply_cmdbuffer.h"
#include "decode/decode_common.h"
#include "decode/dequant.h"
#include "decode/entropy.h"
#include "decode/transform.h"
#include "decode/transform_unit.h"
#include "surface/blit.h"
#include "surface/surface.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
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
    Highlight_t* highlight;
    uint32_t skip;
    uint32_t offset;
} ResidualArgs_t;

/*------------------------------------------------------------------------------*/

#define VN_RESIDUAL_FN_BOILERPLATE(dataType) \
    Surface_t* dst = args->dst;              \
    dataType* pel = (dataType*)dst->data;    \
    const uint32_t stride = dst->stride;     \
    const uint32_t skip = args->skip;        \
    pel += args->offset + (x * skip) + (y * stride);

#define VN_DEFINE_ADD_RESIDUALS_DD(bits, dataType)                                                             \
    static void addResidualsDD_U##bits(ResidualArgs_t* args, uint32_t x, uint32_t y, const int16_t* residuals) \
    {                                                                                                          \
        VN_RESIDUAL_FN_BOILERPLATE(dataType)                                                                   \
        assert(dst->type == FPU##bits);                                                                        \
        int32_t pelFP[4];                                                                                      \
                                                                                                               \
        /* Load pel and convert to S8_7 */                                                                     \
        pelFP[0] = fpU##bits##ToS##bits(pel[0]);                                                               \
        pelFP[1] = fpU##bits##ToS##bits(pel[skip]);                                                            \
        pelFP[2] = fpU##bits##ToS##bits(pel[stride]);                                                          \
        pelFP[3] = fpU##bits##ToS##bits(pel[skip + stride]);                                                   \
                                                                                                               \
        /* Apply & write back */                                                                               \
        pel[0] = fpS##bits##ToU##bits(pelFP[0] + residuals[0]);                                                \
        pel[skip] = fpS##bits##ToU##bits(pelFP[1] + residuals[1]);                                             \
        pel[stride] = fpS##bits##ToU##bits(pelFP[2] + residuals[2]);                                           \
        pel[skip + stride] = fpS##bits##ToU##bits(pelFP[3] + residuals[3]);                                    \
    }

VN_DEFINE_ADD_RESIDUALS_DD(8, uint8_t)
VN_DEFINE_ADD_RESIDUALS_DD(10, uint16_t)
VN_DEFINE_ADD_RESIDUALS_DD(12, uint16_t)
VN_DEFINE_ADD_RESIDUALS_DD(14, uint16_t)

/* Add inverse to S8.7/S10.5/S12.3/S14.1 buffer and write back. */
static void addResidualsDD_S16(ResidualArgs_t* args, uint32_t x, uint32_t y, const int16_t* residuals)
{
    VN_RESIDUAL_FN_BOILERPLATE(int16_t)

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Add inverse to S8.7/S10.5/S12.3/S14.1 buffer. */
    pel[0] = saturateS16(pel[0] + residuals[0]);
    pel[skip] = saturateS16(pel[skip] + residuals[1]);
    pel[stride] = saturateS16(pel[stride] + residuals[2]);
    pel[skip + stride] = saturateS16(pel[skip + stride] + residuals[3]);
}

/* Write inverse to S8.7/S10.5/S12.3/S14.1 buffer. */
static void writeResidualsDD_S16(ResidualArgs_t* args, uint32_t x, uint32_t y, const int16_t* residuals)
{
    VN_RESIDUAL_FN_BOILERPLATE(int16_t)

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Write out. */
    pel[0] = residuals[0];
    pel[skip] = residuals[1];
    pel[stride] = residuals[2];
    pel[skip + stride] = residuals[3];
}

#define VN_DEFINE_WRITE_HIGHLIGHT_DD(fpType, dataType, val)                                    \
    static void writeHighlightDD_##fpType(ResidualArgs_t* args, uint32_t x, uint32_t y,        \
                                          const int16_t* residuals)                            \
    {                                                                                          \
        VN_UNUSED(residuals);                                                                  \
                                                                                               \
        VN_RESIDUAL_FN_BOILERPLATE(dataType)                                                   \
        assert(ldlFixedPointByteSize(dst->type) == sizeof(dataType));                          \
                                                                                               \
        const dataType highlight = (dataType)args->highlight->val;                             \
                                                                                               \
        /* Ugly bodge way of checking that we don't have a signed/unsigned mismatch. This will \
         * only catch mismatches where the signed thing is negative, but that's the only times \
         * this matters anyway*/                                                               \
        assert((int64_t)args->highlight->val == (int64_t)highlight);                           \
                                                                                               \
        /* Write out. */                                                                       \
        pel[0] = highlight;                                                                    \
        pel[skip] = highlight;                                                                 \
        pel[stride] = highlight;                                                               \
        pel[skip + stride] = highlight;                                                        \
    }

VN_DEFINE_WRITE_HIGHLIGHT_DD(U8, uint8_t, valUnsigned)
VN_DEFINE_WRITE_HIGHLIGHT_DD(U16, uint16_t, valUnsigned)
VN_DEFINE_WRITE_HIGHLIGHT_DD(S16, int16_t, valSigned)

/*------------------------------------------------------------------------------*/

#define VN_DEFINE_ADD_RESIDUALS_DDS(bits, dataType)                                                             \
    static void addResidualsDDS_U##bits(ResidualArgs_t* args, uint32_t x, uint32_t y, const int16_t* residuals) \
    {                                                                                                           \
        VN_RESIDUAL_FN_BOILERPLATE(dataType)                                                                    \
                                                                                                                \
        assert(dst->type == FPU##bits);                                                                         \
        int32_t pelFP[16];                                                                                      \
                                                                                                                \
        /* Load pel and convert to S##bits##.7 */                                                               \
        pelFP[0] = fpU##bits##ToS##bits(pel[0]);                                                                \
        pelFP[1] = fpU##bits##ToS##bits(pel[skip]);                                                             \
        pelFP[2] = fpU##bits##ToS##bits(pel[stride]);                                                           \
        pelFP[3] = fpU##bits##ToS##bits(pel[skip + stride]);                                                    \
        pelFP[4] = fpU##bits##ToS##bits(pel[2 * skip]);                                                         \
        pelFP[5] = fpU##bits##ToS##bits(pel[3 * skip]);                                                         \
        pelFP[6] = fpU##bits##ToS##bits(pel[2 * skip + stride]);                                                \
        pelFP[7] = fpU##bits##ToS##bits(pel[3 * skip + stride]);                                                \
        pelFP[8] = fpU##bits##ToS##bits(pel[2 * stride]);                                                       \
        pelFP[9] = fpU##bits##ToS##bits(pel[skip + 2 * stride]);                                                \
        pelFP[10] = fpU##bits##ToS##bits(pel[3 * stride]);                                                      \
        pelFP[11] = fpU##bits##ToS##bits(pel[skip + 3 * stride]);                                               \
        pelFP[12] = fpU##bits##ToS##bits(pel[2 * skip + 2 * stride]);                                           \
        pelFP[13] = fpU##bits##ToS##bits(pel[3 * skip + 2 * stride]);                                           \
        pelFP[14] = fpU##bits##ToS##bits(pel[2 * skip + 3 * stride]);                                           \
        pelFP[15] = fpU##bits##ToS##bits(pel[3 * skip + 3 * stride]);                                           \
                                                                                                                \
        /* Apply & write back */                                                                                \
        pel[0] = fpS##bits##ToU##bits(pelFP[0] + residuals[0]);                                                 \
        pel[skip] = fpS##bits##ToU##bits(pelFP[1] + residuals[1]);                                              \
        pel[stride] = fpS##bits##ToU##bits(pelFP[2] + residuals[2]);                                            \
        pel[skip + stride] = fpS##bits##ToU##bits(pelFP[3] + residuals[3]);                                     \
        pel[2 * skip] = fpS##bits##ToU##bits(pelFP[4] + residuals[4]);                                          \
        pel[3 * skip] = fpS##bits##ToU##bits(pelFP[5] + residuals[5]);                                          \
        pel[2 * skip + stride] = fpS##bits##ToU##bits(pelFP[6] + residuals[6]);                                 \
        pel[3 * skip + stride] = fpS##bits##ToU##bits(pelFP[7] + residuals[7]);                                 \
        pel[2 * stride] = fpS##bits##ToU##bits(pelFP[8] + residuals[8]);                                        \
        pel[skip + 2 * stride] = fpS##bits##ToU##bits(pelFP[9] + residuals[9]);                                 \
        pel[3 * stride] = fpS##bits##ToU##bits(pelFP[10] + residuals[10]);                                      \
        pel[skip + 3 * stride] = fpS##bits##ToU##bits(pelFP[11] + residuals[11]);                               \
        pel[2 * skip + 2 * stride] = fpS##bits##ToU##bits(pelFP[12] + residuals[12]);                           \
        pel[3 * skip + 2 * stride] = fpS##bits##ToU##bits(pelFP[13] + residuals[13]);                           \
        pel[2 * skip + 3 * stride] = fpS##bits##ToU##bits(pelFP[14] + residuals[14]);                           \
        pel[3 * skip + 3 * stride] = fpS##bits##ToU##bits(pelFP[15] + residuals[15]);                           \
    }

VN_DEFINE_ADD_RESIDUALS_DDS(8, uint8_t)
VN_DEFINE_ADD_RESIDUALS_DDS(10, uint16_t)
VN_DEFINE_ADD_RESIDUALS_DDS(12, uint16_t)
VN_DEFINE_ADD_RESIDUALS_DDS(14, uint16_t)

/* Add inverse to S8.7/S10.5/S12.3/S14.1 buffer and write back. */
static void addResidualsDDS_S16(ResidualArgs_t* args, uint32_t x, uint32_t y, const int16_t* residuals)
{
    VN_RESIDUAL_FN_BOILERPLATE(int16_t)

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Add inverse to S8.7/S10.4/S12.3/S14.1 buffer. */
    /* clang-format off */
    pel[0]                     = saturateS16(pel[0] + residuals[0]);
    pel[skip]                  = saturateS16(pel[skip] + residuals[1]);
    pel[stride]                = saturateS16(pel[stride] + residuals[2]);
    pel[skip + stride]         = saturateS16(pel[skip + stride] + residuals[3]);
    pel[2 * skip]              = saturateS16(pel[2 * skip] + residuals[4]);
    pel[3 * skip]              = saturateS16(pel[3 * skip] + residuals[5]);
    pel[2 * skip + stride]     = saturateS16(pel[2 * skip + stride] + residuals[6]);
    pel[3 * skip + stride]     = saturateS16(pel[3 * skip + stride] + residuals[7]);
    pel[2 * stride]            = saturateS16(pel[2 * stride] + residuals[8]);
    pel[skip + 2 * stride]     = saturateS16(pel[skip + 2 * stride] + residuals[9]);
    pel[3 * stride]            = saturateS16(pel[3 * stride] + residuals[10]);
    pel[skip + 3 * stride]     = saturateS16(pel[skip + 3 * stride] + residuals[11]);
    pel[2 * skip + 2 * stride] = saturateS16(pel[2 * skip + 2 * stride] + residuals[12]);
    pel[3 * skip + 2 * stride] = saturateS16(pel[3 * skip + 2 * stride] + residuals[13]);
    pel[2 * skip + 3 * stride] = saturateS16(pel[2 * skip + 3 * stride] + residuals[14]);
    pel[3 * skip + 3 * stride] = saturateS16(pel[3 * skip + 3 * stride] + residuals[15]);
    /* clang-format on */
}

/* Write inverse to S8.7/S10.5/S12.3/S14.1 buffer. */
static void writeResidualsDDS_S16(ResidualArgs_t* args, uint32_t x, uint32_t y, const int16_t* residuals)
{
    VN_RESIDUAL_FN_BOILERPLATE(int16_t)

    assert((dst->type == FPS8) || (dst->type == FPS10) || (dst->type == FPS12) || (dst->type == FPS14));

    /* Write out. */
    pel[0] = residuals[0];
    pel[skip] = residuals[1];
    pel[stride] = residuals[2];
    pel[skip + stride] = residuals[3];
    pel[2 * skip] = residuals[4];
    pel[3 * skip] = residuals[5];
    pel[2 * skip + stride] = residuals[6];
    pel[3 * skip + stride] = residuals[7];
    pel[2 * stride] = residuals[8];
    pel[skip + 2 * stride] = residuals[9];
    pel[3 * stride] = residuals[10];
    pel[skip + 3 * stride] = residuals[11];
    pel[2 * skip + 2 * stride] = residuals[12];
    pel[3 * skip + 2 * stride] = residuals[13];
    pel[2 * skip + 3 * stride] = residuals[14];
    pel[3 * skip + 3 * stride] = residuals[15];
}

#define VN_DEFINE_WRITE_HIGHLIGHT_DDS(fpType, dataType, val)                             \
    static void writeHighlightDDS_##fpType(ResidualArgs_t* args, uint32_t x, uint32_t y, \
                                           const int16_t* residuals)                     \
    {                                                                                    \
        VN_UNUSED(residuals);                                                            \
        VN_RESIDUAL_FN_BOILERPLATE(dataType)                                             \
        const dataType highlight = (dataType)args->highlight->val;                       \
                                                                                         \
        pel[0] = highlight;                                                              \
        pel[skip] = highlight;                                                           \
        pel[stride] = highlight;                                                         \
        pel[skip + stride] = highlight;                                                  \
        pel[2 * skip] = highlight;                                                       \
        pel[3 * skip] = highlight;                                                       \
        pel[2 * skip + stride] = highlight;                                              \
        pel[3 * skip + stride] = highlight;                                              \
        pel[2 * stride] = highlight;                                                     \
        pel[skip + 2 * stride] = highlight;                                              \
        pel[3 * stride] = highlight;                                                     \
        pel[skip + 3 * stride] = highlight;                                              \
        pel[2 * skip + 2 * stride] = highlight;                                          \
        pel[3 * skip + 2 * stride] = highlight;                                          \
        pel[2 * skip + 3 * stride] = highlight;                                          \
        pel[3 * skip + 3 * stride] = highlight;                                          \
    }

VN_DEFINE_WRITE_HIGHLIGHT_DDS(U8, uint8_t, valUnsigned)
VN_DEFINE_WRITE_HIGHLIGHT_DDS(U16, uint16_t, valUnsigned)
VN_DEFINE_WRITE_HIGHLIGHT_DDS(S16, int16_t, valSigned)

/*------------------------------------------------------------------------------*/

typedef void (*ResidualFunction_t)(ResidualArgs_t* args, uint32_t x, uint32_t y, const int16_t* residuals);

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
    {{addResidualsDD_S16, writeResidualsDD_S16, writeHighlightDD_S16}, {addResidualsDDS_S16, writeResidualsDDS_S16, writeHighlightDDS_S16}}, /* S14.1 */
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
    uint32_t srcSkip;
    uint32_t srcOffset;
    Surface_t* dst;
    uint32_t dstSkip;
    uint32_t dstOffset;
} ConvertArgs_t;

#define VN_CONVERT_FN_BOILERPLATE(srcVarType, dstVarType)         \
    Surface_t* src = args->src;                                   \
    Surface_t* dst = args->dst;                                   \
    dstVarType* dstPels = (dstVarType*)dst->data;                 \
    const srcVarType* srcPels = (const srcVarType*)src->data;     \
    const uint32_t dstStride = dst->stride;                       \
    const uint32_t srcStride = src->stride;                       \
    const uint32_t dstSkip = args->dstSkip;                       \
    const uint32_t srcSkip = args->srcSkip;                       \
    dstPels += args->dstOffset + (x * dstSkip) + (y * dstStride); \
    srcPels += args->srcOffset + (x * srcSkip) + (y * srcStride);

/* Converts S87 value in a source buffer to an S8 representation in the dest buffer for a DD transform */
static void convertDD_S87_S8(ConvertArgs_t* args, uint32_t x, uint32_t y)
{
    VN_CONVERT_FN_BOILERPLATE(int16_t, uint8_t)
    assert(src->type == FPS8);

    /* clang-format off */
    dstPels[0]                   = (uint8_t)(srcPels[0] >> 8);
    dstPels[dstSkip]             = (uint8_t)(srcPels[srcSkip] >> 8);
    dstPels[dstStride]           = (uint8_t)(srcPels[srcStride] >> 8);
    dstPels[dstSkip + dstStride] = (uint8_t)(srcPels[srcSkip + srcStride] >> 8);
    /* clang-format on */
}

/* Converts S87 value in a source buffer to an S8 representation in the dest buffer for a DDS transform */
static void convertDDS_S87_S8(ConvertArgs_t* args, uint32_t x, uint32_t y)
{
    VN_CONVERT_FN_BOILERPLATE(int16_t, uint8_t)
    assert(src->type == FPS8);

    /* clang-format off */
    dstPels[0]                           = (uint8_t)(srcPels[0] >> 8);
    dstPels[dstSkip]                     = (uint8_t)(srcPels[srcSkip] >> 8);
    dstPels[dstStride]                   = (uint8_t)(srcPels[srcStride] >> 8);
    dstPels[dstSkip + dstStride]         = (uint8_t)(srcPels[srcSkip + srcStride] >> 8);
    dstPels[2 * dstSkip]                 = (uint8_t)(srcPels[2 * srcSkip] >> 8);
    dstPels[3 * dstSkip]                 = (uint8_t)(srcPels[3 * srcSkip] >> 8);
    dstPels[2 * dstSkip + dstStride]     = (uint8_t)(srcPels[2 * srcSkip + srcStride] >> 8);
    dstPels[3 * dstSkip + dstStride]     = (uint8_t)(srcPels[3 * srcSkip + srcStride] >> 8);
    dstPels[2 * dstStride]               = (uint8_t)(srcPels[2 * srcStride] >> 8);
    dstPels[dstSkip + 2 * dstStride]     = (uint8_t)(srcPels[srcSkip + 2 * srcStride] >> 8);
    dstPels[3 * dstStride]               = (uint8_t)(srcPels[3 * srcStride] >> 8);
    dstPels[dstSkip + 3 * dstStride]     = (uint8_t)(srcPels[srcSkip + 3 * srcStride] >> 8);
    dstPels[2 * dstSkip + 2 * dstStride] = (uint8_t)(srcPels[2 * srcSkip + 2 * srcStride] >> 8);
    dstPels[3 * dstSkip + 2 * dstStride] = (uint8_t)(srcPels[3 * srcSkip + 2 * srcStride] >> 8);
    dstPels[2 * dstSkip + 3 * dstStride] = (uint8_t)(srcPels[2 * srcSkip + 3 * srcStride] >> 8);
    dstPels[3 * dstSkip + 3 * dstStride] = (uint8_t)(srcPels[3 * srcSkip + 3 * srcStride] >> 8);
    /* clang-format on */
}

/*------------------------------------------------------------------------------*/

typedef void (*ConvertFunction_t)(ConvertArgs_t* args, uint32_t x, uint32_t y);

static const ConvertFunction_t kConvertTable[2] = {convertDD_S87_S8, convertDDS_S87_S8};

static inline ConvertFunction_t getConvertFunction(bool bDDS) { return kConvertTable[bDDS]; }

/* ----------------------------------------------------------------------------*/

static void clearBlock(Surface_t* dst, uint32_t x, uint32_t y, uint32_t elementSize,
                       uint32_t patchWidth, uint32_t patchHeight)
{
    assert(dst);

    const uint32_t width = dst->width;
    const uint32_t height = dst->height;
    const uint32_t stride = dst->stride;
    const uint32_t byteCount = minU32(patchWidth, width - x) * elementSize;
    const uint32_t yMax = minU32(y + patchHeight, height);
    const uint32_t step = stride * elementSize;

    uint8_t* pels = &dst->data[(y * stride * elementSize) + (x * elementSize)];

    assert(x < width);

    for (; y < yMax; y++) {
        memset(pels, 0, byteCount);
        pels += step;
    }
}

/* ----------------------------------------------------------------------------*/

int32_t prepareLayerDecoders(Logger_t log, const TileState_t* tile,
                             EntropyDecoder_t residualDecoders[RCLayerCountDDS],
                             EntropyDecoder_t* temporalDecoder, int32_t layerCount, uint8_t bitstreamVersion)
{
    int32_t res = 0;

    if (tile->chunks) {
        for (int32_t layerIdx = 0; layerIdx < layerCount; ++layerIdx) {
            VN_CHECK(entropyInitialise(log, &residualDecoders[layerIdx], &tile->chunks[layerIdx],
                                       EDTDefault, bitstreamVersion));
        }
    }

    if (tile->temporalChunk) {
        VN_CHECK(entropyInitialise(log, temporalDecoder, tile->temporalChunk, EDTTemporal, bitstreamVersion));
    }

    return 0;
}

/* ----------------------------------------------------------------------------*/

typedef struct DecodeSerial
{
    Memory_t memory;
    CacheTileData_t tileDataPerPlane[AC_MaxResidualParallel];
} * DecodeSerial_t;

/*------------------------------------------------------------------------------*/

typedef struct ApplyResidualJobData
{
    Context_t* ctx;
    Memory_t memory;
    Logger_t log;
    uint32_t plane;
    LOQIndex_t loq;
    const Dequant_t* dequant;
    FieldType_t fieldType;
    uint8_t bitstreamVersion;
    bool tuCoordsAreInSurfaceRasterOrder;
    bool applyTemporal;
    Surface_t* dst;
    uint32_t dstChannel;
    TileState_t* tiles;
    uint32_t tileCount;
} ApplyResidualJobData_t;

/*------------------------------------------------------------------------------*/

static inline int32_t entropyDecodeAllLayers(const uint8_t numLayers, const bool decoderExists,
                                             const int32_t tuTotal,
                                             EntropyDecoder_t residualDecoders[RCLayerCountDDS],
                                             int32_t zerosOut[RCLayerCountDDS],
                                             int16_t coeffsOut[RCLayerCountDDS], int32_t* minZeroCountOut)
{
    int32_t coeffsNonZeroMask = 0;
    for (uint8_t layer = 0; layer < numLayers; layer++) {
        if (zerosOut[layer] > 0) {
            zerosOut[layer]--;
            coeffsOut[layer] = 0;
        } else if (decoderExists) {
            const int32_t layerZero = ldlEntropyDecode(&residualDecoders[layer], &coeffsOut[layer]);
            zerosOut[layer] = (layerZero == EntropyNoData) ? (tuTotal - 1) : layerZero;
            if (zerosOut[layer] < 0) {
                return zerosOut[layer];
            }

            /* set i-th bit if nonzero */
            coeffsNonZeroMask |= ((coeffsOut[layer] != 0) << layer);
        } else {
            /* No decoder, skip over whole surface. */
            zerosOut[layer] = tuTotal - 1;
            coeffsOut[layer] = 0;
        }

        /* Calculate lowest common zero run */
        if (*minZeroCountOut > zerosOut[layer]) {
            *minZeroCountOut = zerosOut[layer];
        }
    }
    return coeffsNonZeroMask;
}

static int32_t applyResidualJob(void* jobData)
{
    ApplyResidualJobData_t* applyData = (ApplyResidualJobData_t*)jobData;
    Context_t* ctx = applyData->ctx;
    DeserialisedData_t* data = &ctx->deserialised;
    int32_t res = 0;

    /* general */
    const LOQIndex_t loq = applyData->loq;
    const Dequant_t* dequant = applyData->dequant;
    const bool applyTemporal = applyData->applyTemporal;
    const uint8_t numLayers = data->numLayers;
    const bool dds = data->transform == TransformDDS;
    const uint8_t tuWidthShift = dds ? 2 : 1; /* The width, log2, of the transform unit */
    const bool temporalReducedSignalling = data->temporalUseReducedSignalling;
    const ScalingMode_t scaling = (LOQ0 == loq) ? data->scalingModes[LOQ0] : Scale2D;
    const bool tuCoordsAreInSurfaceRasterOrder = applyData->tuCoordsAreInSurfaceRasterOrder;
    PlaneSurfaces_t* plane = &ctx->planes[applyData->plane];

    /* user data */
    const UserDataConfig_t* userData = &data->userData;

    /* temporal */
    Surface_t* temporalSurface = NULL;

    /* functions */
    ResidualArgs_t residualArgs = {0};
    ConvertFunction_t convertFn = NULL;
    ConvertArgs_t convertArgs = {0};
    const ResidualMode_t residualMode = ctx->highlightState[loq].enabled ? RM_Highlight : RM_Add;

    residualArgs.highlight = &ctx->highlightState[loq];

    if (ctx->generateSurfaces) {
        /* @todo: probably should be able to use external surfaces even when not generating surfaces */
        if (ctx->useExternalSurfaces && !ctx->convertS8) {
            residualArgs.dst = &plane->externalSurfaces[loq];
        } else {
            residualArgs.dst =
                (loq == LOQ0) ? &plane->temporalBuffer[applyData->fieldType] : &plane->basePixels;
        }
    } else if (applyTemporal) {
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

    /* Setup residual functions, and conversion if needed. @todo: Caller could provide surfaces
     * they wish to write to. */
    TransformFunction_t transformFn = ldlTransformGetFunction(data->transform, scaling, ctx->cpuFeatures);
    ResidualFunction_t applyFn = getResidualFunction(residualMode, dds, residualArgs.dst->type);
    ResidualFunction_t writeFn = getResidualFunction(RM_Write, dds, residualArgs.dst->type);
    VN_CHECKJ(surfaceGetChannelSkipOffset(residualArgs.dst, applyData->dstChannel,
                                          &residualArgs.skip, &residualArgs.offset));

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

    /* Setup temporalSurface if needed. */
    if (loq == LOQ0 && applyTemporal) {
        if (ctx->generateSurfaces && ctx->useExternalSurfaces && !ctx->convertS8) {
            temporalSurface = &plane->externalSurfaces[loq];
        } else {
            temporalSurface = &plane->temporalBuffer[applyData->fieldType];
        }
    }

    for (uint32_t tileIndex = 0; tileIndex < applyData->tileCount; ++tileIndex) {
        int16_t coeffs[RCLayerCountDDS] = {0};
        int16_t residuals[RCLayerCountDDS] = {0};
        int32_t zeros[RCLayerCountDDS] = {0}; /* Current zero run in each layer */
        int32_t temporalRun = 0;              /* Current symbol run in temporal layer */
        uint32_t tuIndex = 0;
        uint32_t lastTuIndex = 0;
        TUState_t tuState = {0};
        TileState_t* tile = &applyData->tiles[tileIndex];
        uint32_t x = tile->x;
        uint32_t y = tile->y;
        const bool tileHasTemporalDecode = (tile->temporalChunk != NULL);
        const bool tileHasEntropyDecode = (tile->chunks != NULL);
        CmdBuffer_t* cmdBuffer = tile->cmdBuffer;
        TemporalSignal_t temporal = TSInter;
        int32_t clearBlockQueue = 0;
        int32_t coeffsNonzeroMask = 0;
        bool clearBlockRemainder = false;

        /* Setup decoders */
        EntropyDecoder_t residualDecoders[RCLayerCountDDS] = {{0}};
        EntropyDecoder_t temporalDecoder = {0};
        VN_CHECKJ(prepareLayerDecoders(applyData->log, tile, residualDecoders, &temporalDecoder,
                                       numLayers, applyData->bitstreamVersion));

        /* Setup TU */
        VN_CHECKJ(tuStateInitialise(&tuState, tile->width, tile->height, tile->x, tile->y, tuWidthShift));

        /* Break loop once tile is fully decoded. */
        while (true) {
            /* Decode bitstream and track zero runs */
            int32_t minZeroCount = INT_MAX;
            coeffsNonzeroMask =
                entropyDecodeAllLayers(numLayers, tileHasEntropyDecode, (int32_t)tuState.tuTotal,
                                       residualDecoders, zeros, coeffs, &minZeroCount);
            VN_CHECKJ(coeffsNonzeroMask);

            /* Perform user data modification if needed. */
            stripUserData(loq, userData, coeffs);

            /* Decode temporal and track temporal run */
            const bool blockStart = (x % BSTemporal == 0) && (y % BSTemporal == 0);
            if (clearBlockQueue == 0 && tileHasTemporalDecode && applyTemporal) {
                if (temporalRun <= 0) {
                    temporalRun = ldlEntropyDecodeTemporal(&temporalDecoder, &temporal);
                    clearBlockRemainder = false;

                    if (temporalRun == EntropyNoData) {
                        temporalRun = (int32_t)tuState.tuTotal;
                    }

                    /* Decrement run by 1 if just decoded. Temporal signal run is inclusive
                       of current symbol. RLE signals run is exclusive of current symbol.
                       All the processing assumes the run is the number after the current
                       symbol */
                    if (temporalRun <= 0) {
                        VN_ERROR(applyData->log, "invalid temporal_run value %d\n", temporalRun);
                        res = -1; /* show we failed */
                        break;    /* exit while(true) loop */
                    }
                }
                temporalRun--;

                /* Process the intra blocks when running reduced signaling. This can occur
                 * at any point during a temporal run of Intra signals, so must be tracked
                 * and only performed when the first Intra signal to touch a block start
                 * is encountered. All subsequent Intra signals are guaranteed to be
                 * block start signals so consider them here. */
                if (blockStart && temporal == TSIntra && temporalReducedSignalling) {
                    /* Given that temporalRun holds the number of blocks that need clearing, set the
                     * clearBlockQueue correctly and calculate the number of TUs until that point
                     * to keep temporalRun accurate until the final clear. */
                    clearBlockQueue = temporalRun + 1;
                    temporalRun = 0;
                    uint32_t futureX = x;
                    uint32_t futureY = y;

                    for (int32_t block = clearBlockQueue; block > 0; block--) {
                        uint32_t futureBlockTUCount = 0;
                        tuBlockTuCount(&tuState, futureX, futureY, &futureBlockTUCount);
                        temporalRun += (int32_t)futureBlockTUCount;
                        tuCoordsBlockRaster(&tuState, tuIndex + temporalRun, &futureX, &futureY);
                    }
                }
            }

            uint32_t blockWidth = 0;
            uint32_t blockHeight = 0;
            uint32_t blockTUCount = 0;
            tuCoordsBlockDetails(&tuState, x, y, &blockWidth, &blockHeight, &blockTUCount);
            bool clearedBlock = false;

            /* Handle clearing (either clear the block, or generate a "clear" command). */
            if (blockStart && clearBlockQueue > 0) {
                if (cmdBuffer) {
                    uint32_t blockAlignedIndex = tuCoordsBlockAlignedIndex(&tuState, x, y);
                    ldlCmdBufferAppend(cmdBuffer, CBCClear, NULL, blockAlignedIndex - lastTuIndex);
                    lastTuIndex = blockAlignedIndex;
                } else {
                    /* Reset block on temporal surface */
                    clearBlock(temporalSurface, x, y, sizeof(int16_t), blockWidth, blockHeight);

                    if (convertFn) {
                        clearBlock(convertArgs.dst, x, y, sizeof(uint8_t), blockWidth, blockHeight);
                    }
                }

                clearedBlock = true;
                clearBlockQueue--;
                if (clearBlockQueue == 0) {
                    clearBlockRemainder = true;
                }
            }

            /* Only actually apply if there is some meaningful data and the operation
             * will have side-effects. */
            if ((coeffsNonzeroMask != 0) || (!clearedBlock && (!applyTemporal || temporal == TSIntra))) {
                if (coeffsNonzeroMask != 0) {
                    /* Apply SW to coeffs - this is not performed in decode loop as the temporal
                     * signal residual could be zero (implied inter), however the block signal
                     * could be intra. */

                    for (uint8_t layer = 0; layer < numLayers; layer++) {
                        if (coeffs[layer] > 0) {
                            coeffs[layer] = (int16_t)clampS32(
                                (int32_t)coeffs[layer] * dequant->stepWidth[temporal][layer] +
                                    dequant->offset[temporal][layer],
                                INT16_MIN, INT16_MAX);
                        } else if (coeffs[layer] < 0) {
                            coeffs[layer] = (int16_t)clampS32(
                                (int32_t)coeffs[layer] * dequant->stepWidth[temporal][layer] -
                                    dequant->offset[temporal][layer],
                                INT16_MIN, INT16_MAX);
                        }
                    }

                    /* Inverse hadamard */
                    transformFn(coeffs, residuals);

                    /* Apply deblocking coefficients when enabled. */
                    if ((LOQ1 == loq) && dds && data->deblock.enabled) {
                        deblockResiduals(&data->deblock, residuals);
                    }
                } else {
                    memset(residuals, 0, RCLayerCountDDS * sizeof(int16_t));
                }

                if (cmdBuffer) {
                    CmdBufferCmd_t command = CBCAdd;
                    if (coeffsNonzeroMask == 0 && temporal == TSIntra) {
                        command = CBCSetZero;
                    } else if (loq == LOQ0 &&
                               (temporal == TSIntra || clearBlockQueue > 0 || clearBlockRemainder)) {
                        command = CBCSet;
                    }

                    /* TODO: This code can be refactored so that we don't do this. Specifically,
                     * we can calculate the tuIndex so that it accounts for block alignment when
                     * generating command buffers. In particular, tuCoordsBlockAlignedRaster should
                     * be able to generate tuIndices from x/y coords. */
                    uint32_t currentIndex = tuIndex;
                    if (!tuCoordsAreInSurfaceRasterOrder &&
                        (tuState.block.tuPerBlockRowRightEdge != 0 || y >= tuState.blockAligned.maxWholeBlockY ||
                         tuState.xOffset != 0 || tuState.yOffset != 0)) {
                        currentIndex = tuCoordsBlockAlignedIndex(&tuState, x, y);
                    }
                    ldlCmdBufferAppend(cmdBuffer, command, residuals, currentIndex - lastTuIndex);
                    lastTuIndex = currentIndex;
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

            /* Logic for finding the next tuIndex to jump to, keeping temporalRun accurate. Note:
             * if not tileHasTemporal, that's the start of a temporal surface or LOQ1, no special
             * logic. */
            if (tileHasTemporalDecode) {
                if (clearedBlock) {
                    /* After clear block has just been run, increment to the next residual or
                     * start of the next block to clear or resume the next temporal run */
                    minZeroCount = minS32(minZeroCount, (int32_t)blockTUCount - 1);
                    temporalRun -= minZeroCount + 1;
                } else if (clearBlockQueue > 0) {
                    /* Case for an upcoming clear block or residual, whichever comes first */
                    const uint32_t yRemaining =
                        ((blockHeight - (y & (BSTemporal - 1))) >> tuWidthShift) - 1;
                    const uint32_t xRemaining =
                        ((blockWidth - (x & (BSTemporal - 1))) >> tuWidthShift) - 1;
                    const int32_t nextBlockStart =
                        (int32_t)((yRemaining * (blockWidth >> tuWidthShift)) + xRemaining);

                    minZeroCount = minS32(nextBlockStart, minZeroCount);
                    temporalRun -= minZeroCount + 1;
                } else if (temporal == TSInter || (clearBlockRemainder && minZeroCount > temporalRun)) {
                    /* Normal operation when not in a clear block, move to the next new residual or
                     * end of the temporal run */
                    minZeroCount = minS32(minZeroCount, temporalRun);
                    temporalRun -= minZeroCount;
                } else if (!clearBlockRemainder) {
                    /* Always just increment one TU after an Intra TU */
                    assert(temporal == TSIntra);
                    minZeroCount = 0;
                } else {
                    /* Case when applying residuals to the last block in a run of clear blocks, keep
                     * temporalRun accurate and move to the next residual */
                    temporalRun -= minZeroCount;
                }
            }

            tuIndex += minZeroCount + 1;

            /* Update the x, y variables from the tuIndex */
            if (tuCoordsAreInSurfaceRasterOrder) {
                res = tuCoordsSurfaceRaster(&tuState, tuIndex, &x, &y);
            } else {
                res = tuCoordsBlockRaster(&tuState, tuIndex, &x, &y);
            }

            VN_CHECKJ(res);

            if (res > 0) {
                break;
            }

            if (minZeroCount > 0) {
                /* Note: if you're tempted to "optimise" this by loop-unrolling, to do 4 operations
                 * at once, don't worry. Compilers are smart enough to do this already (and indeed,
                 * they even turn this into SIMD code, if they have SIMD). */
                for (uint8_t layer = 0; layer < numLayers; layer++) {
                    zeros[layer] -= minZeroCount;
                }
            }
        }

        if (cmdBuffer) {
            cmdBufferSplit(cmdBuffer);
        }
    }

error_exit:
    return (res < 0) ? res : 0;
}

/*------------------------------------------------------------------------------*/

static int32_t applyResidualExecute(Context_t* ctx, const DecodeSerialArgs_t* params)
{
    int32_t res = 0;
    ThreadManager_t* threadManager = &ctx->threadManager;
    DeserialisedData_t* data = &ctx->deserialised;
    const LOQIndex_t loq = params->loq;
    const int32_t planeCount = data->numPlanes;
    ApplyResidualJobData_t threadData[AC_MaxResidualParallel] = {{0}};
    int32_t planeIndex = 0;

    assert(planeCount <= AC_MaxResidualParallel);

    for (; planeIndex < planeCount && planeIndex < RCMaxPlanes; planeIndex += 1) {
        CacheTileData_t* tileCache = &ctx->decodeSerial[loq]->tileDataPerPlane[planeIndex];
        VN_CHECKJ(tileDataInitialize(tileCache, params->memory, data, planeIndex, loq));

        for (uint32_t tileIndex = 0; tileIndex < tileCache->tileCount; ++tileIndex) {
            TileState_t* tile = &tileCache->tiles[tileIndex];

            if (ctx->generateCmdBuffers) {
                if (!tile->cmdBuffer) {
                    cmdBufferInitialise(params->memory, &tile->cmdBuffer, ctx->applyCmdBufferThreads);
                }
                cmdBufferReset(tile->cmdBuffer, ctx->deserialised.numLayers);
            }
        }

        threadData[planeIndex].dequant = contextGetDequant(ctx, planeIndex, loq);
        threadData[planeIndex].ctx = ctx;
        threadData[planeIndex].memory = params->memory;
        threadData[planeIndex].log = params->log;
        threadData[planeIndex].dst = params->dst[planeIndex];
        threadData[planeIndex].plane = planeIndex;
        threadData[planeIndex].loq = loq;
        threadData[planeIndex].fieldType = data->fieldType;
        threadData[planeIndex].bitstreamVersion = params->bitstreamVersion;
        threadData[planeIndex].applyTemporal = params->applyTemporal;
        threadData[planeIndex].tuCoordsAreInSurfaceRasterOrder = params->tuCoordsAreInSurfaceRasterOrder;
        threadData[planeIndex].tiles = tileCache->tiles;
        threadData[planeIndex].tileCount = tileCache->tileCount;
    }

    res = threadingExecuteJobs(threadManager, applyResidualJob, threadData, planeIndex,
                               sizeof(ApplyResidualJobData_t))
              ? 0
              : -1;

error_exit:
    return res;
}

int32_t decodeSerial(Context_t* ctx, const DecodeSerialArgs_t* params)
{
    DecodeSerial_t decode = ctx->decodeSerial[params->loq];

    if (!decode) {
        VN_ERROR(params->log, "Attempted to perform decoding without initialising the decoder");
        return -1;
    }

    if (!ctx->generateCmdBuffers) {
        /* Check that the plane configurations are valid. Either Y or YUV must be present. */
        int32_t planeCheck = 0;
        for (int32_t i = 0; i < 3; ++i) {
            planeCheck |= (params->dst[i] != NULL) ? (1 << i) : 0;
        }

        if ((planeCheck != 1) && (planeCheck != 7)) {
            VN_ERROR(params->log, "No destination surfaces supplied\n");
            return -1;
        }
    }

    /* Ensure LOQ is valid. */
    if (params->loq != LOQ0 && params->loq != LOQ1) {
        VN_ERROR(params->log, "Supplied LOQ is invalid, must be LOQ-0 or LOQ-1\n");
        return -1;
    }

    if (!ctx->deserialised.entropyEnabled[params->loq]) {
        VN_DEBUG(params->log, "Nothing to decode in LOQ%d\n", params->loq);
        return 0;
    }

    int32_t res = 0;
    VN_CHECK(applyResidualExecute(ctx, params));

    if (ctx->applyCmdBuffers) {
        const Highlight_t* highlight = &ctx->highlightState[params->loq];
        /* TODO: thread this per-plane? That's the way that applyResidualExecute is threaded. */
        for (uint32_t planeIdx = 0; planeIdx < AC_MaxResidualParallel; planeIdx++) {
            const CacheTileData_t* tileData = &decode->tileDataPerPlane[planeIdx];
            const uint32_t tileCount = tileData->tileCount;
            for (uint32_t tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
                const TileState_t* tile = &tileData->tiles[tileIndex];
                const Surface_t* cmdBufferDst =
                    (params->applyTemporal ? &ctx->planes[planeIdx].temporalBuffer[FTTop]
                                           : params->dst[planeIdx]);

                VN_CHECK(applyCmdBuffer(params->log, ctx->threadManager, tile, cmdBufferDst,
                                        params->tuCoordsAreInSurfaceRasterOrder, ctx->cpuFeatures,
                                        highlight));
            }
        }
    }

    return res;
}

bool decodeSerialInitialize(Memory_t memory, DecodeSerial_t* decodes)
{
    for (uint32_t loq = 0; loq < LOQEnhancedCount; loq++) {
        DecodeSerial_t result = VN_CALLOC_T(memory, struct DecodeSerial);

        if (!result) {
            return false;
        }

        result->memory = memory;

        decodes[loq] = result;
    }
    return true;
}

void decodeSerialRelease(DecodeSerial_t decode)
{
    if (!decode) {
        return;
    }

    Memory_t memory = decode->memory;

    for (int32_t i = 0; i < AC_MaxResidualParallel; ++i) {
        CacheTileData_t* tileData = &decode->tileDataPerPlane[i];
        const uint32_t tileCount = tileData->tileCount;
        for (uint32_t tileIdx = 0; tileIdx < tileCount; tileIdx++) {
            CmdBuffer_t* cmdBuffer = tileData->tiles[tileIdx].cmdBuffer;
            cmdBufferFree(cmdBuffer);
        }
        VN_FREE(memory, tileData->tiles);
    }

    VN_FREE(memory, decode);
}

uint32_t decodeSerialGetTileCount(const DecodeSerial_t decode, uint8_t planeIdx)
{
    return decode->tileDataPerPlane[planeIdx].tileCount;
}

TileState_t* decodeSerialGetTile(const DecodeSerial_t decode, uint8_t planeIdx)
{
    return decode->tileDataPerPlane[planeIdx].tiles;
}

CmdBuffer_t* decodeSerialGetCmdBuffer(const DecodeSerial_t decode, uint8_t planeIdx, uint8_t tileIdx)
{
    return decode->tileDataPerPlane[planeIdx].tiles[tileIdx].cmdBuffer;
}

CmdBufferEntryPoint_t* decodeSerialGetCmdBufferEntryPoint(const DecodeSerial_t decode, uint8_t planeIdx,
                                                          uint8_t tileIdx, uint16_t entryPointIndex)
{
    return &decodeSerialGetCmdBuffer(decode, planeIdx, tileIdx)->entryPoints[entryPointIndex];
}

/*------------------------------------------------------------------------------*/
