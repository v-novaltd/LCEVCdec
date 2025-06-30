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

#include "decode/dequant.h"

#include "common/memory.h"
#include "common/types.h"
#include "decode/deserialiser.h"

#include <assert.h>
#include <LCEVC/build_config.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*- Constants -----------------------------------------------------------------------------------*/

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

/* Constants for step-width & offset formulas [Section 8.5.3]. Note that divisors cannot be
 * trivially replaced with shifts, since they may be operating on signed data. */
static const int32_t kA = 39;            /* 0.0006 * (1 << 16) 16-bit integer representation */
static const int32_t kB = 126484;        /* 1.9200 * (1 << 16) 16-bit integer representation */
static const int32_t kC = 5242;          /* 0.0800 * (1 << 16) 16-bit integer representation */
static const int32_t kD = 99614;         /* 1.5200 * (1 << 16) 16-bit integer representation */
static const int32_t kSWDivisor = 32768; /* Like a right-shift of 15, but unambiguous on signed ints */
static const int64_t kSWDivisorNoDQOffset =
    2147483648; /* Like a right-shift of 31, but unambiguous on signed ints */
static const int32_t kQMScaleMax = 196608; /* 3 << 16 */
static const int32_t kDeadZoneSWLimit =
    12249; /* Largest stepwidth that does not overflow deadzone calculation */

/* 1/255, expressed as a U0_16 fixed point. floor((1.0f / 255.0f) * (1 << 16));*/
static const uint16_t kFPOneOver255 = 257;

/*- QuantMatrix ---------------------------------------------------------------------------------*/

static inline const uint8_t* quantMatrixGetDefault(ScalingMode_t scaling, TransformType_t transform,
                                                   LOQIndex_t index)
{
    if (scaling == Scale1D) {
        return (transform == TransformDDS) ? kQuantMatrixDefaultDDS1D[index]
                                           : kQuantMatrixDefaultDD1D[index];
    }

    return (transform == TransformDDS) ? kQuantMatrixDefaultDDS2D[index] : kQuantMatrixDefaultDD2D[index];
}

void ldlQuantMatrixSetDefault(QuantMatrix_t* matrix, ScalingMode_t loq0Scaling,
                              TransformType_t transform, LOQIndex_t index)
{
    const size_t layerCount = transformTypeLayerCount(transform);
    memoryCopy(matrix->values[index], quantMatrixGetDefault(loq0Scaling, transform, index),
               layerCount * sizeof(uint8_t));
}

void ldlQuantMatrixDuplicateLOQs(QuantMatrix_t* matrix)
{
    memoryCopy(matrix->values[LOQ1], matrix->values[LOQ0], RCLayerCountDDS * sizeof(uint8_t));
}

/*- Dequant (private functions) -----------------------------------------------------------------*/

double ldlCalculateFixedPointU12_4Ln(int32_t stepWidth)
{
    /* Calculate the natural log (ln) of stepWidth, with U12_4 fixed-point precision. */
    const uint8_t integerLogSw = (uint8_t)(floor(log(stepWidth)));
    /* The maximum value of stepWidth is 32768, and ln(32768) = 10.3972. Therefore, integerLogSw
     * should really never exceed 10, but all that really matters is that it remains within 4
     * bits, i.e. less than 16. */
    assert(integerLogSw < 16);
    const double fractionalLogSw = floor((log(stepWidth) - integerLogSw) * 4096) / 4096;
    return integerLogSw + fractionalLogSw;
}

uint16_t ldlCalculateFixedPointTemporalSW(uint32_t temporalSWModifier, int16_t temporalSWUnmodified)
{
    /* Calculate the modified temporal step width, treating floats as U16s with no whole-number
     * part, i.e. 0.0 to 1.0 is mapped to 0 to 65536 (1<<16) */

    /* clamp between 0 and 0.5, noting that 0.5 is represented as (1<<16)/2 */
    const uint16_t stepWidthModifier =
        clampU16((uint16_t)(temporalSWModifier * kFPOneOver255), 0, 1 << 15);
    const uint32_t stepWidthMultiplier = (1 << 16) - stepWidthModifier;
    const uint32_t flooredStepWidth = (stepWidthMultiplier * temporalSWUnmodified) >> 16;
    return clampU16((int16_t)flooredStepWidth, QMinStepWidth, QMaxStepWidth);
}

static int32_t calculateDequantOffsetActual(int32_t layerSW, int32_t masterSW,
                                            int32_t dequantOffset, DequantOffsetMode_t mode)
{
    int64_t dequantOffsetActual = 0;
    const int32_t logLayerSW = (int32_t)(-kC * ldlCalculateFixedPointU12_4Ln(layerSW));
    const int32_t logMasterSW = (int32_t)(kC * ldlCalculateFixedPointU12_4Ln(masterSW));

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

static int32_t calculateStepWidthModifier(int32_t layerSW, int32_t dequantOffsetActual,
                                          int32_t offset, DequantOffsetMode_t mode)
{
    /* See 8.5.3 of the standard. Note that the standard doesn't have any formatting, so it says
     * "qm[x...][y]2" to indicated "layerSW squared".*/
    if (offset == -1) {
        const int64_t logByLayerSW = (int64_t)(kD - kC * ldlCalculateFixedPointU12_4Ln(layerSW));
        const int64_t logByLayerSWPow = (logByLayerSW * layerSW * layerSW);
        const int64_t intLogByLayerSWDiv = (logByLayerSWPow / kSWDivisorNoDQOffset);

        return (int32_t)intLogByLayerSWDiv;
    }

    if (mode == DQMDefault) {
        const int64_t stepWidthModifier = ((int64_t)dequantOffsetActual * layerSW);
        return (int32_t)(stepWidthModifier / kSWDivisor);
    }

    /*mode == DQM_ConstOffset*/
    return 0;
}

static int32_t calculateDeadzoneWidth(int32_t masterSW, int32_t layerSW)
{
    if (masterSW <= 16) {
        return (masterSW >> 1);
    }

    if (layerSW > kDeadZoneSWLimit) {
        return INT32_MAX;
    }

    return ((((1 << 16) - (((kA * layerSW) + kB) >> 1)) * layerSW) >> 16);
}

static int16_t calculateAppliedDequantOffset(int32_t dequantOffsetActual, int32_t deadzoneWidth,
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

static int32_t applyChromaSWMultiplier(int32_t stepwidth, uint8_t multiplier)
{
    return clampS32((stepwidth * (int32_t)multiplier) >> 6, QMinStepWidth, QMaxStepWidth);
}

static int32_t calculateLOQStepWidth(const DequantArgs_t* args, int32_t planeIdx, LOQIndex_t loqIdx)
{
    return (planeIdx > 0 && loqIdx == LOQ0)
               ? applyChromaSWMultiplier(args->stepWidth[loqIdx], args->chromaStepWidthMultiplier)
               : args->stepWidth[loqIdx];
}

static int32_t calculatePlaneLOQ(Dequant_t* dst, const DequantArgs_t* args, int32_t planeIdx, LOQIndex_t loqIdx)
{
    const uint8_t* quantMatrix = quantMatrixGetValuesConst(args->quantMatrix, loqIdx);
    assert(quantMatrix != NULL);
    const int32_t loqSW = calculateLOQStepWidth(args, planeIdx, loqIdx);

    /* Calculate individual layer step-widths for each temporal type. */
    for (uint32_t temporalIdx = 0; temporalIdx < TSCount; ++temporalIdx) {
        int32_t temporalSW = loqSW;

        /* Modify the step-width in the inter-case based upon temporal step width
         * modifier */
        if ((temporalIdx == TSInter) && (loqIdx == LOQ0) && args->temporalEnabled && !args->temporalRefresh) {
            temporalSW =
                ldlCalculateFixedPointTemporalSW(args->temporalStepWidthModifier, (int16_t)temporalSW);
        }

        for (uint32_t layerIdx = 0; layerIdx < args->layerCount; ++layerIdx) {
            /* Calculate a scaled QM - rounding up (and clamped to maximum range). layerQM is
             * called qm_p in the standard. */
            int64_t layerQM = quantMatrix[layerIdx];
            layerQM *= temporalSW;
            layerQM += (1 << 16);
            layerQM = clampS64(layerQM, 0, kQMScaleMax);
            layerQM *= temporalSW;
            layerQM >>= 16;

            /* Scale layer SW using QM and shift out. Safe because layerQM and temporalSW are, at
             * most, 17 and 16 bits respectively. */
            int32_t layerSW = (int32_t)clampS64(layerQM, QMinStepWidth, QMaxStepWidth);

            int32_t dequantOffsetActual = calculateDequantOffsetActual(
                layerSW, temporalSW, args->dequantOffset, args->dequantOffsetMode);
            int32_t stepWidthModifier = calculateStepWidthModifier(
                layerSW, dequantOffsetActual, args->dequantOffset, args->dequantOffsetMode);

            layerSW = clampS32((layerSW + stepWidthModifier), QMinStepWidth, QMaxStepWidth);
            /* @note: this is safe, because we've clamped layerSW to QMaxStepWidth, which is INT16_MAX */
            dst->stepWidth[temporalIdx][layerIdx] = (int16_t)layerSW;

            const int32_t deadzoneWidth = calculateDeadzoneWidth(temporalSW, layerSW);
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

/*- Dequant (public functions) ------------------------------------------------------------------*/

int32_t initialiseDequantArgs(const DeserialisedData_t* data, DequantArgs_t* args)
{
    args->planeCount = data->numPlanes;
    args->layerCount = data->numLayers;
    args->dequantOffsetMode = data->dequantOffsetMode;
    args->dequantOffset = data->dequantOffset;
    args->temporalEnabled = data->temporalEnabled;
    args->temporalRefresh = data->temporalRefresh;
    args->temporalStepWidthModifier = data->temporalStepWidthModifier;
    args->stepWidth[LOQ0] = data->stepWidths[LOQ0];
    args->stepWidth[LOQ1] = data->stepWidths[LOQ1];
    args->chromaStepWidthMultiplier = data->chromaStepWidthMultiplier;
    args->quantMatrix = &data->quantMatrix;

    return 0;
}

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

/*-----------------------------------------------------------------------------------------------*/
