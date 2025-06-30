/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#include "apply_cmdbuffer_common.h"
#include "fp_types.h"

#include <assert.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <stdint.h>
#include <stdio.h>

/*------------------------------------------------------------------------------*/
/* Apply ADDs */
/*------------------------------------------------------------------------------*/

static inline void addDD_U8(const ApplyCmdBufferArgs* args)
{
    uint8_t* pixels = (uint8_t*)args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDD; ++column) {
            pixels[column] = fpS8ToU8(fpU8ToS8(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDD;
        pixels += args->rowPixelStride;
    }
}

static inline void addDD_U10(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDD; ++column) {
            pixels[column] = fpS10ToU10(fpU10ToS10(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDD;
        pixels += args->rowPixelStride;
    }
}

static inline void addDD_U12(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDD; ++column) {
            pixels[column] = fpS12ToU12(fpU12ToS12(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDD;
        pixels += args->rowPixelStride;
    }
}

static inline void addDD_U14(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDD; ++column) {
            pixels[column] = fpS14ToU14(fpU14ToS14(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDD;
        pixels += args->rowPixelStride;
    }
}

static inline void addDD_S16(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDD; ++column) {
            pixels[column] = saturateS16(pixels[column] + residuals[column]);
        }
        residuals += CBCKTUSizeDD;
        pixels += args->rowPixelStride;
    }
}

static inline void addDDS_U8(const ApplyCmdBufferArgs* args)
{
    uint8_t* pixels = (uint8_t*)args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDDS; ++column) {
            pixels[column] = fpS8ToU8(fpU8ToS8(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDDS;
        pixels += args->rowPixelStride;
    }
}

static inline void addDDS_U10(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDDS; ++column) {
            pixels[column] = fpS10ToU10(fpU10ToS10(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDDS;
        pixels += args->rowPixelStride;
    }
}

static inline void addDDS_U12(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDDS; ++column) {
            pixels[column] = fpS12ToU12(fpU12ToS12(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDDS;
        pixels += args->rowPixelStride;
    }
}

static inline void addDDS_U14(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDDS; ++column) {
            pixels[column] = fpS14ToU14(fpU14ToS14(pixels[column]) + residuals[column]);
        }
        residuals += CBCKTUSizeDDS;
        pixels += args->rowPixelStride;
    }
}

static inline void addDDS_S16(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBCKTUSizeDDS; ++column) {
            pixels[column] = saturateS16(pixels[column] + residuals[column]);
        }
        residuals += CBCKTUSizeDDS;
        pixels += args->rowPixelStride;
    }
}

/*------------------------------------------------------------------------------*/
/* Apply SETs */
/*------------------------------------------------------------------------------*/

static inline void setDD(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;

    memcpy(pixels, args->residuals, CBCKTUSizeDD * sizeof(int16_t));
    memcpy(pixels + args->rowPixelStride, args->residuals + CBCKTUSizeDD, CBCKTUSizeDD * sizeof(int16_t));
}

static inline void setDDS(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;

    memcpy(pixels, args->residuals, CBCKTUSizeDDS * sizeof(int16_t));
    memcpy(pixels + args->rowPixelStride, args->residuals + CBCKTUSizeDDS, CBCKTUSizeDDS * sizeof(int16_t));
    memcpy(pixels + (args->rowPixelStride * 2), args->residuals + (CBCKTUSizeDDS * 2),
           CBCKTUSizeDDS * sizeof(int16_t));
    memcpy(pixels + (args->rowPixelStride * 3), args->residuals + (CBCKTUSizeDDS * 3),
           CBCKTUSizeDDS * sizeof(int16_t));
}

static inline void setZeroDD(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;

    memset(pixels, 0, CBCKTUSizeDD * sizeof(int16_t));
    memset(pixels + args->rowPixelStride, 0, CBCKTUSizeDD * sizeof(int16_t));
}

static inline void setZeroDDS(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;

    memset(pixels, 0, CBCKTUSizeDDS * sizeof(int16_t));
    memset(pixels + args->rowPixelStride, 0, CBCKTUSizeDDS * sizeof(int16_t));
    memset(pixels + (args->rowPixelStride * 2), 0, CBCKTUSizeDDS * sizeof(int16_t));
    memset(pixels + (args->rowPixelStride * 3), 0, CBCKTUSizeDDS * sizeof(int16_t));
}

/*------------------------------------------------------------------------------*/
/* Apply CLEARs */
/*------------------------------------------------------------------------------*/

static inline void clear(const ApplyCmdBufferArgs* args)
{
    const uint16_t clearHeight = minU16(ACBKBlockSize, args->height - args->y);
    const uint16_t clearWidth = minU16(ACBKBlockSize, args->width - args->x);
    const size_t clearBytes = clearWidth * sizeof(int16_t);

    int16_t* pixels = args->firstSample + (args->y * args->rowPixelStride) + args->x;

    for (int32_t row = 0; row < clearHeight; ++row) {
        memset(pixels, 0, clearBytes);
        pixels += args->rowPixelStride;
    }
}

#define cmdBufferApplicatorBlockTemplate cmdBufferApplicatorBlockScalar
#define cmdBufferApplicatorSurfaceTemplate cmdBufferApplicatorSurfaceScalar
#include "apply_cmdbuffer_applicator.h"
