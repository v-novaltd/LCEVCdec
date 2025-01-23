/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#include "common/cmdbuffer.h"
#include "common/types.h"
#include "decode/apply_cmdbuffer_common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/*------------------------------------------------------------------------------*/
/* Apply ADDs */
/*------------------------------------------------------------------------------*/

static inline void addDD_U8(const ApplyCmdBufferArgs_t* args)
{
    uint8_t* pixels = (uint8_t*)args->surfaceData + (args->y * (size_t)args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDD; ++column) {
            pixels[column] = fpS8ToU8(fpU8ToS8(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDD;
        pixels += args->surfaceStride;
    }
}

static inline void addDD_U10(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * (size_t)args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDD; ++column) {
            pixels[column] = fpS10ToU10(fpU10ToS10(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDD;
        pixels += args->surfaceStride;
    }
}

static inline void addDD_U12(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDD; ++column) {
            pixels[column] = fpS12ToU12(fpU12ToS12(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDD;
        pixels += args->surfaceStride;
    }
}

static inline void addDD_U14(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDD; ++column) {
            pixels[column] = fpS14ToU14(fpU14ToS14(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDD;
        pixels += args->surfaceStride;
    }
}

static inline void addDD_S16(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDD; ++column) {
            pixels[column] = saturateS16(pixels[column] + residuals[column]);
        }
        residuals += CBKTUSizeDD;
        pixels += args->surfaceStride;
    }
}

static inline void addDDS_U8(const ApplyCmdBufferArgs_t* args)
{
    uint8_t* pixels = (uint8_t*)args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDDS; ++column) {
            pixels[column] = fpS8ToU8(fpU8ToS8(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDDS;
        pixels += args->surfaceStride;
    }
}

static inline void addDDS_U10(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDDS; ++column) {
            pixels[column] = fpS10ToU10(fpU10ToS10(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDDS;
        pixels += args->surfaceStride;
    }
}

static inline void addDDS_U12(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDDS; ++column) {
            pixels[column] = fpS12ToU12(fpU12ToS12(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDDS;
        pixels += args->surfaceStride;
    }
}

static inline void addDDS_U14(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDDS; ++column) {
            pixels[column] = fpS14ToU14(fpU14ToS14(pixels[column]) + residuals[column]);
        }
        residuals += CBKTUSizeDDS;
        pixels += args->surfaceStride;
    }
}

static inline void addDDS_S16(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16_t* residuals = args->residuals;

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        for (int32_t column = 0; column < CBKTUSizeDDS; ++column) {
            pixels[column] = saturateS16(pixels[column] + residuals[column]);
        }
        residuals += CBKTUSizeDDS;
        pixels += args->surfaceStride;
    }
}

/*------------------------------------------------------------------------------*/
/* Apply SETs */
/*------------------------------------------------------------------------------*/

static inline void setDD(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;

    memcpy(pixels, args->residuals, CBKTUSizeDD * sizeof(int16_t));
    memcpy(pixels + args->surfaceStride, args->residuals + CBKTUSizeDD, CBKTUSizeDD * sizeof(int16_t));
}

static inline void setDDS(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;

    memcpy(pixels, args->residuals, CBKTUSizeDDS * sizeof(int16_t));
    memcpy(pixels + args->surfaceStride, args->residuals + CBKTUSizeDDS, CBKTUSizeDDS * sizeof(int16_t));
    memcpy(pixels + (args->surfaceStride * 2), args->residuals + (CBKTUSizeDDS * 2),
           CBKTUSizeDDS * sizeof(int16_t));
    memcpy(pixels + (args->surfaceStride * 3), args->residuals + (CBKTUSizeDDS * 3),
           CBKTUSizeDDS * sizeof(int16_t));
}

static inline void setZeroDD(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;

    memset(pixels, 0, CBKTUSizeDD * sizeof(int16_t));
    memset(pixels + args->surfaceStride, 0, CBKTUSizeDD * sizeof(int16_t));
}

static inline void setZeroDDS(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;

    memset(pixels, 0, CBKTUSizeDDS * sizeof(int16_t));
    memset(pixels + args->surfaceStride, 0, CBKTUSizeDDS * sizeof(int16_t));
    memset(pixels + (args->surfaceStride * 2), 0, CBKTUSizeDDS * sizeof(int16_t));
    memset(pixels + (args->surfaceStride * 3), 0, CBKTUSizeDDS * sizeof(int16_t));
}

/*------------------------------------------------------------------------------*/
/* Apply CLEARs */
/*------------------------------------------------------------------------------*/

static inline void clear(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);

    const uint16_t clearHeight = minU16(BSTemporal, (uint16_t)(args->surface->height - args->y));
    const uint16_t clearWidth = minU16(BSTemporal, (uint16_t)(args->surface->width - args->x));
    const size_t clearBytes = clearWidth * sizeof(int16_t);

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;

    for (int32_t row = 0; row < clearHeight; ++row) {
        memset(pixels, 0, clearBytes);
        pixels += args->surfaceStride;
    }
}

#define cmdBufferApplicatorBlockTemplate cmdBufferApplicatorBlockScalar
#define cmdBufferApplicatorSurfaceTemplate cmdBufferApplicatorSurfaceScalar
#include "apply_cmdbuffer_applicator.h"
