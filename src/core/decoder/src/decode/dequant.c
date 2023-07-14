/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "decode/dequant.h"

#include "common/memory.h"

#include <math.h>

/*------------------------------------------------------------------------------*/

static const uint8_t kQuantMatrixDefaultDD1D[LOQEnhancedCount][RCLayerCountDD] = {
    {0u, 2u, 0u, 0u},
    {0u, 3u, 0u, 32u},
};

static const uint8_t kQuantMatrixDefaultDD2D[LOQEnhancedCount][RCLayerCountDD] = {
    {32u, 3u, 0u, 32u},
    {0u, 3u, 0u, 32u},
};

static const uint8_t kQuantMatrixDefaultDDS1D[LOQEnhancedCount][RCLayerCountDDS] = {
    {13u, 26u, 19u, 32u, 52u, 1u, 78u, 9u, 13u, 26u, 19u, 32u, 150u, 91u, 91u, 19u},
    {0u, 0u, 0u, 2u, 52u, 1u, 78u, 9u, 26u, 72u, 0u, 3u, 150u, 91u, 91u, 19u},
};

static const uint8_t kQuantMatrixDefaultDDS2D[LOQEnhancedCount][RCLayerCountDDS] = {
    {13u, 26u, 19u, 32u, 52u, 1u, 78u, 9u, 26u, 72u, 0u, 3u, 150u, 91u, 91u, 19u},
    {0u, 0u, 0u, 2u, 52u, 1u, 78u, 9u, 26u, 72u, 0u, 3u, 150u, 91u, 91u, 19u},
};

static inline const uint8_t* quantMatrixGetDefault(ScalingMode_t scaling, TransformType_t transform,
                                                   LOQIndex_t index)
{
    if (scaling == Scale1D) {
        return (transform == TransformDDS) ? kQuantMatrixDefaultDDS1D[index]
                                           : kQuantMatrixDefaultDD1D[index];
    }

    return (transform == TransformDDS) ? kQuantMatrixDefaultDDS2D[index] : kQuantMatrixDefaultDD2D[index];
}

void quantMatrixSetDefault(QuantMatrix_t* matrix, ScalingMode_t loq0Scaling, TransformType_t transform)
{
    const size_t layerCount = transformTypeLayerCount(transform);
    memorySet(matrix, 0, sizeof(QuantMatrix_t));
    memoryCopy(matrix->values[LOQ0], quantMatrixGetDefault(loq0Scaling, transform, LOQ0),
               layerCount * sizeof(uint8_t));
    memoryCopy(matrix->values[LOQ1], quantMatrixGetDefault(loq0Scaling, transform, LOQ1),
               layerCount * sizeof(uint8_t));
}

void quantMatrixDuplicateLOQs(QuantMatrix_t* matrix)
{
    memoryCopy(matrix->values[LOQ1], matrix->values[LOQ0], RCLayerCountDDS * sizeof(uint8_t));
}

uint8_t* quantMatrixGetValues(QuantMatrix_t* matrix, LOQIndex_t index)
{
    if (!matrix || ((uint32_t)index >= LOQEnhancedCount)) {
        return NULL;
    }

    return matrix->values[index];
}

/*------------------------------------------------------------------------------*/

/* Constants for step-width & offset formulas [Section 8.5.3]  */
enum DequantConstants
{
    Aconst = 39,             /* 0.0006 * (1 << 16) 16-bit integer representation */
    Bconst = 126484,         /* 1.9200 * (1 << 16) 16-bit integer representation */
    Cconst = 5242,           /* 0.0800 * (1 << 16) 16-bit integer representation */
    Dconst = 99614,          /* 1.5200 * (1 << 16) 16-bit integer representation */
    DivShift = 15,           /* val / 32768 */
    QMScaleMax = 196608,     /* 3 << 16 */
    DeadZoneSWLimit = 12249, /* Largest stepwidth that does njot overflow deadzone calculation */
};

/*------------------------------------------------------------------------------*/

static int32_t calculateDequantOffsetActual(uint32_t layerSW, uint32_t masterSW,
                                            int32_t dequantOffset, DequantOffsetMode_t mode)
{
    int64_t dequantOffsetActual = 0;
    const int32_t logLayerSW = (int32_t)(-Cconst * log(layerSW));
    const int32_t logMasterSW = (int32_t)(Cconst * log(masterSW));

    if (dequantOffset == -1 || dequantOffset == 0) {
        return 0;
    }

    if (mode == DQMDefault) {
        dequantOffsetActual = (int64_t)dequantOffset << 11;
    } else if (mode == DQMConstOffset) {
        dequantOffsetActual = (int64_t)dequantOffset << 9;
    }

    dequantOffsetActual = ((logLayerSW + dequantOffsetActual + logMasterSW) * layerSW);

    return (int32_t)(dequantOffsetActual >> 16);
}

static int32_t calculateStepWidthModifier(uint32_t layerSW, int32_t dequantOffsetActual,
                                          int32_t offset, DequantOffsetMode_t mode)
{
    if (offset == -1) {
        const int64_t logByLayerSW = (int64_t)(Dconst - Cconst * log(layerSW));
        const int64_t logByLayerSWPow = (logByLayerSW * layerSW * layerSW);
        const int64_t intLogByLayerSWDiv = (logByLayerSWPow >> DivShift);

        return (int32_t)(intLogByLayerSWDiv >> 16);
    }

    if (mode == DQMDefault) {
        const int64_t stepWidthModifier = ((int64_t)dequantOffsetActual * layerSW);
        return (int32_t)(stepWidthModifier >> DivShift);
    }

    /*mode == DQM_ConstOffset*/
    return 0;
}

static int32_t calculateDeadzoneWidth(uint32_t masterSW, uint32_t layerSW)
{
    if (masterSW <= 16) {
        return (int32_t)(masterSW >> 1);
    }

    if (layerSW > DeadZoneSWLimit) {
        return INT32_MAX;
    }

    return ((((1 << 16) - ((int32_t)((Aconst * layerSW) + Bconst) >> 1)) * (int32_t)layerSW) >> 16);
}

static int16_t calculateAppliedDequantOffset(uint32_t dequantOffsetActual, int32_t deadzoneWidth,
                                             int32_t offset, DequantOffsetMode_t mode)
{
    if (offset == -1 || mode == DQMDefault) {
        return (int16_t)-deadzoneWidth;
    }

    if (mode == DQMConstOffset) {
        return (int16_t)(dequantOffsetActual - deadzoneWidth);
    }

    return 0;
}

static uint32_t applyChromaSWMultiplier(uint32_t stepwidth, uint8_t multiplier)
{
    return clampU32((stepwidth * (uint32_t)multiplier) >> 6, 1u, 32767u);
}

static uint32_t calculateLOQStepWidth(const DequantArgs_t* args, int32_t planeIdx, LOQIndex_t loqIdx)
{
    return (planeIdx > 0 && loqIdx == LOQ0)
               ? applyChromaSWMultiplier(args->stepWidth[loqIdx], args->chromaStepWidthMultiplier)
               : args->stepWidth[loqIdx];
}

static int32_t calculatePlaneLOQ(Dequant_t* dst, const DequantArgs_t* args, int32_t planeIdx, LOQIndex_t loqIdx)
{
    const uint8_t* quantMatrix = quantMatrixGetValues(args->quantMatrix, loqIdx);
    const uint32_t loqSW = calculateLOQStepWidth(args, planeIdx, loqIdx);

    /* Calculate individual layer step-widths for each temporal type. */
    for (uint32_t temporalIdx = 0; temporalIdx < TSCount; ++temporalIdx) {
        uint32_t temporalSW = loqSW;

        /* Modify the step-width in the inter-case based upon temporal step width
         * modifier */
        if ((temporalIdx == TSInter) && (loqIdx == LOQ0) && args->temporalEnabled && !args->temporalRefresh) {
            const float modifier =
                1.0f - clampF32(((float)args->temporalStepWidthModifier / 255.0f), 0.0f, 0.5f);
            const uint32_t flooredSW = (uint32_t)floorF32(modifier * (float)temporalSW);
            temporalSW = clampU32(flooredSW, QMinStepWidth, QMaxStepWidth);
        }

        for (uint32_t layerIdx = 0; layerIdx < args->layerCount; ++layerIdx) {
            /* Calculate a scaled QM - rounding up. */
            uint32_t layerQM = (quantMatrix[layerIdx] * temporalSW) + (1 << 16);

            /* Clamp into maximum scaling range. */
            layerQM = clampU32(layerQM, 0, QMScaleMax);

            /* Scale layer SW using QM and shift out. This assumes the result of
             * the multiplication uses less-than 48-bits. Current implementation
             * uses 33-bits based upon the value
             * of qm_scale_max and max_step_width. */
            uint32_t layerSW = (uint32_t)(((uint64_t)layerQM * temporalSW) >> 16);

            /* Finally clamp SW into valid range. */
            layerSW = clampU32(layerSW, QMinStepWidth, QMaxStepWidth);

            int32_t dequantOffsetActual = calculateDequantOffsetActual(
                layerSW, temporalSW, args->dequantOffset, args->dequantOffsetMode);
            int32_t stepWidthModifier = calculateStepWidthModifier(
                layerSW, dequantOffsetActual, args->dequantOffset, args->dequantOffsetMode);

            layerSW = (uint32_t)clampS32(((int32_t)layerSW + stepWidthModifier), QMinStepWidth,
                                         QMaxStepWidth);
            int32_t deadzoneWidth = calculateDeadzoneWidth(temporalSW, layerSW);

            /* @note: Step width can wrap int16_t. Though a max step-width (32768)
             * will have disabled the layer at the encode end, as such all residuals
             * will be 0, so its not super dangerous, unless someone uses
             * layer_step_widths[i] at some point for anything other than
             * dequantization. */
            dst->stepWidth[temporalIdx][layerIdx] = (int16_t)layerSW;

            dst->offset[temporalIdx][layerIdx] = calculateAppliedDequantOffset(
                dequantOffsetActual, deadzoneWidth, args->dequantOffset, args->dequantOffsetMode);
        }

#if VN_CORE_FEATURE(SSE)
        dst->stepWidthVector[temporalIdx][0] =
            _mm_load_si128((const __m128i*)&dst->stepWidth[temporalIdx][0]);
        dst->stepWidthVector[temporalIdx][1] =
            _mm_load_si128((const __m128i*)&dst->stepWidth[temporalIdx][8]);
        dst->offsetVector[temporalIdx][0] = _mm_load_si128((const __m128i*)&dst->offset[temporalIdx][0]);
        dst->offsetVector[temporalIdx][1] = _mm_load_si128((const __m128i*)&dst->offset[temporalIdx][8]);
#elif VN_CORE_FEATURE(NEON)
        dst->stepWidthVector[temporalIdx][0] = vld1q_s16(&dst->stepWidth[temporalIdx][0]);
        dst->stepWidthVector[temporalIdx][1] = vld1q_s16(&dst->stepWidth[temporalIdx][8]);
        dst->offsetVector[temporalIdx][0] = vld1q_s16(&dst->offset[temporalIdx][0]);
        dst->offsetVector[temporalIdx][1] = vld1q_s16(&dst->offset[temporalIdx][8]);
#endif
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

int32_t dequantCalculate(DequantParams_t* params, const DequantArgs_t* args)
{
    memorySet(params, 0, sizeof(DequantParams_t));

    for (uint32_t planeIdx = 0; planeIdx < args->planeCount; ++planeIdx) {
        for (uint32_t loqIdx = 0; loqIdx < LOQEnhancedCount; ++loqIdx) {
            const LOQIndex_t loqIndex = (LOQIndex_t)loqIdx;

            if (calculatePlaneLOQ(&params->values[loqIdx][planeIdx], args, (int32_t)planeIdx,
                                  loqIndex) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

/*------------------------------------------------------------------------------*/