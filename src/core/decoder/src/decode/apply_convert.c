
#include "decode/apply_convert.h"

#include "common/cmdbuffer.h"
#include "surface/surface.h"

#include <assert.h>

typedef struct ConvertArgs
{
    const Surface_t* src;
    int32_t srcSkip;
    int32_t srcOffset;
    Surface_t* dst;
    int32_t dstSkip;
    int32_t dstOffset;
} ConvertArgs_t;

/* Converts S87 value in a source buffer to an S8 representation in the dest buffer for a DD transform */
static void convertDD_S87_S8(const ConvertArgs_t* args, int32_t x, int32_t y)
{
    const Surface_t* src = args->src;
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
static void convertDDS_S87_S8(const ConvertArgs_t* args, int32_t x, int32_t y)
{
    const Surface_t* src = args->src;
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

bool applyConvert(Context_t* context, const CmdBuffer_t* buffer, const Surface_t* src,
                  Surface_t* dst, CPUAccelerationFeatures_t accel)
{
    const TransformType_t transform = transformTypeFromLayerCount(cmdBufferGetLayerCount(buffer));
    CmdBufferData_t bufferData = cmdBufferGetData(buffer);

    const ConvertArgs_t args = {
        .src = src,
        .srcSkip = 1,
        .srcOffset = 0,
        .dst = dst,
        .dstSkip = 1,
        .dstOffset = 0,
    };

    for (int32_t i = 0; i < bufferData.count; ++i) {
        //
        const int32_t x = bufferData.coordinates[0];
        const int32_t y = bufferData.coordinates[1];
        bufferData.coordinates += 2;

        // umm what about edge pixels? gonna get some OOB actions here?

        if (transform == TransformDD) {
            convertDD_S87_S8(&args, x, y);
        } else {
            convertDDS_S87_S8(&args, x, y);
        }
    }

    return true;
}

/*------------------------------------------------------------------------------*/