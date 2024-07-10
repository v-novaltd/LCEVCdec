/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "common/cmdbuffer.h"
#include "common/neon.h"
#include "common/types.h"
#include "context.h"
#include "decode/apply_cmdbuffer_common.h"
#include "surface/surface.h"

#include <assert.h>

#if VN_CORE_FEATURE(NEON)

/*------------------------------------------------------------------------------*/

static inline int16x4_t loadPixelsDD(const int16_t* src)
{
    int16x4_t res = vld1_dup_s16(src);
    res = vld1_lane_s16(src + 1, res, 1);
    return res;
}

static inline uint8x8_t loadPixelsDD_U8(const uint8_t* src)
{
    uint8x8_t res = vld1_dup_u8(src);
    res = vld1_lane_u8(src + 1, res, 1);
    return res;
}

static inline uint8x8_t loadPixelsDDS_U8(const uint8_t* src)
{
    uint8x8_t res = vld1_dup_u8(src);
    res = vld1_lane_u8(src + 1, res, 1);
    res = vld1_lane_u8(src + 2, res, 2);
    res = vld1_lane_u8(src + 3, res, 3);
    return res;
}

static inline void storePixelsDD(int16_t* dst, int16x4_t data)
{
    vst1_lane_s16(dst, data, 0);
    vst1_lane_s16(dst + 1, data, 1);
}

static inline void storePixelsDD_U8(uint8_t* dst, int16x4_t data)
{
    const uint8x8_t res = vqmovun_s16(vcombine_s16(data, data));
    vst1_lane_u8(dst, res, 0);
    vst1_lane_u8(dst + 1, res, 1);
}

static inline void storePixelsDDS_U8(uint8_t* dst, int16x4_t data)
{
    const uint8x8_t res = vqmovun_s16(vcombine_s16(data, data));
    vst1_lane_u8(dst, res, 0);
    vst1_lane_u8(dst + 1, res, 1);
    vst1_lane_u8(dst + 2, res, 2);
    vst1_lane_u8(dst + 3, res, 3);
}

static inline int16x4_t loadResidualsDD(const int16_t* src) { return vld1_s16(src); }

static inline int16x4x4_t loadResidualsDDS(const int16_t* src)
{
    int16x4x4_t res;
    res.val[0] = vld1_s16(src);
    res.val[1] = vld1_s16(src + 4);
    res.val[2] = vld1_s16(src + 8);
    res.val[3] = vld1_s16(src + 12);
    return res;
}

/*------------------------------------------------------------------------------*/
/* Apply ADDs */
/*------------------------------------------------------------------------------*/

static inline void addDD_U8(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    const int16x4_t shiftDown = vdup_n_s16(-7);
    const int16x4_t usToSOffset = vdup_n_s16(16384);
    const int16x4_t signOffsetV = vdup_n_s16(0x80);

    uint8_t* pixels = (uint8_t*)args->surfaceData + (args->y * args->surfaceStride) + args->x;
    int16x4_t residuals = loadResidualsDD(args->residuals);

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        const uint8x8_t neonPixelsU8 = loadPixelsDD_U8(pixels);

        /* val <<= shift */
        int16x4_t neonPixels = vget_low_s16(vreinterpretq_s16_u16(vshll_n_u8(neonPixelsU8, 7)));

        /* val -= 0x4000 */
        neonPixels = vsub_s16(neonPixels, usToSOffset);

        /* val += src */
        neonPixels = vqadd_s16(neonPixels, residuals);

        /* val >>= 5 */
        neonPixels = vrshl_s16(neonPixels, shiftDown);

        /* val += sign offset */
        neonPixels = vadd_s16(neonPixels, signOffsetV);

        /* Clamp to unsigned range and store */
        storePixelsDD_U8(pixels, neonPixels);

        /* Move down next 2 elements for storage. */
        residuals = vext_s16(residuals, residuals, 2);
        pixels += args->surfaceStride;
    }
}

#define VN_ADD_CONSTANTS_U16()                            \
    const int16x4_t shiftUp = vdup_n_s16(shift);          \
    const int16x4_t shiftDown = vdup_n_s16(-shift);       \
    const int16x4_t usToSOffset = vdup_n_s16(16384);      \
    const int16x4_t signOffsetV = vdup_n_s16(signOffset); \
    const int16x4_t minV = vdup_n_s16(0);                 \
    const int16x4_t maxV = vdup_n_s16(resultMax);

static inline void addDD_UBase(const ApplyCmdBufferArgs_t* args, int32_t shift, int16_t signOffset,
                               int16_t resultMax)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    VN_ADD_CONSTANTS_U16()

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    int16x4_t residuals = loadResidualsDD(args->residuals);

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        int16x4_t neonPixels = loadPixelsDD(pixels);

        /* val <<= shift */
        neonPixels = vshl_s16(neonPixels, shiftUp);

        /* val -= 0x4000 */
        neonPixels = vsub_s16(neonPixels, usToSOffset);

        /* val += src */
        neonPixels = vqadd_s16(neonPixels, residuals);

        /* val >>= 5 */
        neonPixels = vrshl_s16(neonPixels, shiftDown);

        /* val += sign offset */
        neonPixels = vadd_s16(neonPixels, signOffsetV);

        /* Clamp to unsigned range */
        neonPixels = vmax_s16(vmin_s16(neonPixels, maxV), minV);

        /* Store */
        storePixelsDD(pixels, neonPixels);

        /* Move down next 2 elements for storage. */
        residuals = vext_s16(residuals, residuals, 2);
        pixels += args->surfaceStride;
    }
}

static void addDD_U10(const ApplyCmdBufferArgs_t* args) { addDD_UBase(args, 5, 512, 1023); }

static void addDD_U12(const ApplyCmdBufferArgs_t* args) { addDD_UBase(args, 3, 2048, 4095); }

static void addDD_U14(const ApplyCmdBufferArgs_t* args) { addDD_UBase(args, 1, 8192, 16383); }

static inline void addDD_S16(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    int16x4_t residuals = loadResidualsDD(args->residuals);

    for (int32_t row = 0; row < CBKTUSizeDD; ++row) {
        const int16x4_t neonPixels = loadPixelsDD(pixels);

        storePixelsDD((int16_t*)pixels, vqadd_s16(neonPixels, residuals));

        /* Move down next 2 elements. */
        residuals = vext_s16(residuals, residuals, 2);
        pixels += args->surfaceStride;
    }
}

static inline void addDDS_U8(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    const int16x4_t shiftDown = vdup_n_s16(-7);
    const int16x4_t usToSOffset = vdup_n_s16(16384);
    const int16x4_t signOffsetV = vdup_n_s16(0x80);

    uint8_t* pixels = (uint8_t*)args->surfaceData + (args->y * args->surfaceStride) + args->x;
    int16x4x4_t residuals = loadResidualsDDS(args->residuals);

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        const uint8x8_t neonPixelsU8 = loadPixelsDDS_U8(pixels);

        /* val <<= shift */
        int16x4_t neonPixels = vget_low_s16(vreinterpretq_s16_u16(vshll_n_u8(neonPixelsU8, 7)));

        /* val -= 0x4000 */
        neonPixels = vsub_s16(neonPixels, usToSOffset);

        /* val += src */
        neonPixels = vqadd_s16(neonPixels, residuals.val[row]);

        /* val >>= 5 */
        neonPixels = vrshl_s16(neonPixels, shiftDown);

        /* val += sign offset */
        neonPixels = vadd_s16(neonPixels, signOffsetV);

        /* Clamp to unsigned range and store */
        storePixelsDDS_U8(pixels, neonPixels);

        pixels += args->surfaceStride;
    }
}

static inline void addDDS_UBase(const ApplyCmdBufferArgs_t* args, int32_t shift, int16_t signOffset,
                                int16_t resultMax)
{
    assert(args->surface->interleaving == ILNone);
    assert(!fixedPointIsSigned(args->surface->type));

    VN_ADD_CONSTANTS_U16()

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    int16x4x4_t residuals = loadResidualsDDS(args->residuals);

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        /* Load as int16_t, source data is maximally unsigned 14-bit so will fit. */
        int16x4_t neonPixels = vld1_s16((const int16_t*)pixels);

        /* val <<= shift */
        neonPixels = vshl_s16(neonPixels, shiftUp);

        /* val -= 0x4000 */
        neonPixels = vsub_s16(neonPixels, usToSOffset);

        /* val += src */
        neonPixels = vqadd_s16(neonPixels, residuals.val[row]);

        /* val >>= 5 */
        neonPixels = vrshl_s16(neonPixels, shiftDown);

        /* val += sign offset */
        neonPixels = vadd_s16(neonPixels, signOffsetV);

        /* Clamp to unsigned range */
        neonPixels = vmax_s16(vmin_s16(neonPixels, maxV), minV);

        /* Store */
        vst1_s16((int16_t*)pixels, neonPixels);
        pixels += args->surfaceStride;
    }
}

static inline void addDDS_U10(const ApplyCmdBufferArgs_t* args)
{
    addDDS_UBase(args, 5, 512, 1023);
}

static inline void addDDS_U12(const ApplyCmdBufferArgs_t* args)
{
    addDDS_UBase(args, 3, 2048, 4095);
}

static inline void addDDS_U14(const ApplyCmdBufferArgs_t* args)
{
    addDDS_UBase(args, 1, 8192, 16383);
}

static inline void addDDS_S16(const ApplyCmdBufferArgs_t* args)
{
    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    const int16x4x4_t residuals = loadResidualsDDS(args->residuals);

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        const int16x4_t neonPixels = vld1_s16(pixels);
        vst1_s16(pixels, vqadd_s16(neonPixels, residuals.val[row]));
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
    int16x4_t residuals = loadResidualsDD(args->residuals);

    vst1_lane_s16(pixels, residuals, 0);
    vst1_lane_s16(pixels + 1, residuals, 1);
    vst1_lane_s16(pixels + args->surfaceStride, residuals, 2);
    vst1_lane_s16(pixels + args->surfaceStride + 1, residuals, 3);
}

static inline void setDDS(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;

    int16x4x4_t residuals = loadResidualsDDS(args->residuals);

    for (int32_t row = 0; row < CBKTUSizeDDS; ++row) {
        vst1_s16(pixels, residuals.val[row]);
        pixels += args->surfaceStride;
    }
}

static inline void setZeroDD(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    int16x4_t neonZeros = vmov_n_s16(0);

    storePixelsDD(pixels, neonZeros);
    storePixelsDD(pixels + args->surfaceStride, neonZeros);
}

static inline void setZeroDDS(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    assert(fixedPointIsSigned(args->surface->type));

    int16_t* pixels = args->surfaceData + (args->y * args->surfaceStride) + args->x;
    int16x4_t neonZeros = vmov_n_s16(0);

    vst1_s16(pixels, neonZeros);
    vst1_s16(pixels + args->surfaceStride, neonZeros);
    vst1_s16(pixels + (args->surfaceStride * 2), neonZeros);
    vst1_s16(pixels + (args->surfaceStride * 3), neonZeros);
}

/*------------------------------------------------------------------------------*/
/* Apply CLEARs */
/*------------------------------------------------------------------------------*/

static inline void clear(const ApplyCmdBufferArgs_t* args)
{
    assert(args->surface->interleaving == ILNone);
    const uint16_t x = args->x;
    const uint16_t y = args->y;

    const uint16_t clearWidth = minU16(BSTemporal, (uint16_t)(args->surface->height - y));
    const uint16_t clearHeight = minU16(BSTemporal, (uint16_t)(args->surface->width - x));

    int16_t* pixels = args->surfaceData + (y * args->surfaceStride) + x;

    if (clearHeight == BSTemporal && clearWidth == BSTemporal) {
        int16x8x4_t neonZeros = {0};
        for (int32_t yPos = 0; yPos < BSTemporal; ++yPos) {
            vst4q_s16(pixels, neonZeros);
            pixels += args->surfaceStride;
        }
    } else {
        const size_t clearBytes = clearHeight * sizeof(int16_t);
        for (int32_t row = 0; row < clearWidth; ++row) {
            memset(pixels, 0, clearBytes);
            pixels += args->surfaceStride;
        }
    }
}

#define cmdBufferApplicatorBlockTemplate cmdBufferApplicatorBlockNEON
#define cmdBufferApplicatorSurfaceTemplate cmdBufferApplicatorSurfaceNEON
#include "apply_cmdbuffer_applicator.h"

#else

bool cmdBufferApplicatorBlockNEON(const TileState_t* tile, size_t entryPointIdx,
                                  const Surface_t* surface, const Highlight_t* highlight)
{
    VN_UNUSED_CMDBUFFER_APPLICATOR()
}

bool cmdBufferApplicatorSurfaceNEON(const TileState_t* tile, size_t entryPointIdx,
                                    const Surface_t* surface, const Highlight_t* highlight)
{
    VN_UNUSED_CMDBUFFER_APPLICATOR()
}

#endif
