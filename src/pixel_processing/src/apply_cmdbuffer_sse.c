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

#include <LCEVC/build_config.h>

#if VN_CORE_FEATURE(SSE)

#include "fp_types.h"

#include <assert.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/sse.h>

/*------------------------------------------------------------------------------*/

static inline void loadResidualsDD(const int16_t* data, __m128i dst[2])
{
    dst[0] = _mm_loadl_epi64((const __m128i*)data);
    dst[1] = _mm_bsrli_si128(dst[0], 4);
}

static inline void loadResidualsDDS(const int16_t* data, __m128i dst[4])
{
    dst[0] = _mm_loadu_si128((const __m128i*)data);
    dst[2] = _mm_loadu_si128((const __m128i*)(data + 8));

    dst[1] = _mm_bsrli_si128(dst[0], 8);
    dst[3] = _mm_bsrli_si128(dst[2], 8);
}

/*------------------------------------------------------------------------------*/
/* Apply ADDs */
/*------------------------------------------------------------------------------*/

static void addDD_U8(const ApplyCmdBufferArgs* args)
{
    assert(!fixedPointIsSigned(args->fixedPoint));

    static const int32_t kShift = 7;
    const __m128i usToSOffset = _mm_set1_epi16(0x4000);
    const __m128i fractOffset = _mm_set1_epi16(0x40);
    const __m128i signOffset = _mm_set1_epi16(0x80);

    uint8_t* pixels = (uint8_t*)args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;
    __m128i residuals[2];
    loadResidualsDD(args->residuals, residuals);

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        __m128i ssePixels = _mm_cvtepu8_epi16(_mm_loadu_si16((const __m128i*)pixels));

        /* val <<= shift */
        ssePixels = _mm_slli_epi16(ssePixels, kShift);

        /* val -= 0x4000 */
        ssePixels = _mm_sub_epi16(ssePixels, usToSOffset);

        /* val += src */
        ssePixels = _mm_adds_epi16(ssePixels, residuals[row]);

        /* val += rounding */
        ssePixels = _mm_adds_epi16(ssePixels, fractOffset);

        /* val >>= shift */
        ssePixels = _mm_srai_epi16(ssePixels, kShift);

        /* val += sign offset */
        ssePixels = _mm_add_epi16(ssePixels, signOffset);

        /* Clamp to unsigned range and store */
        _mm_storeu_si16((__m128i*)pixels, _mm_packus_epi16(ssePixels, ssePixels));

        pixels += args->rowPixelStride;
    }
}

#define VN_ADD_CONSTANTS_U16()                                      \
    const __m128i usToSOffset = _mm_set1_epi16(16384);              \
    const __m128i roundingOffsetV = _mm_set1_epi16(roundingOffset); \
    const __m128i signOffsetV = _mm_set1_epi16(signOffset);         \
    const __m128i minV = _mm_set1_epi16(0);                         \
    const __m128i maxV = _mm_set1_epi16(resultMax);

static inline void addDD_UBase(const ApplyCmdBufferArgs* args, int32_t shift,
                               int16_t roundingOffset, int16_t signOffset, int16_t resultMax)
{
    assert(!fixedPointIsSigned(args->fixedPoint));

    VN_ADD_CONSTANTS_U16()

    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;
    __m128i residuals[2];
    loadResidualsDD(args->residuals, residuals);

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        /* Load as int16_t, source data is maximally unsigned 14-bit so will fit. */
        __m128i ssePixels = _mm_loadu_si32((const __m128i*)pixels);

        /* val <<= shift */
        ssePixels = _mm_slli_epi16(ssePixels, shift);

        /* val -= 0x4000 */
        ssePixels = _mm_sub_epi16(ssePixels, usToSOffset);

        /* val += src */
        ssePixels = _mm_adds_epi16(ssePixels, residuals[row]);

        /* val += rounding */
        ssePixels = _mm_adds_epi16(ssePixels, roundingOffsetV);

        /* val >>= shift */
        ssePixels = _mm_srai_epi16(ssePixels, shift);

        /* val += sign offset */
        ssePixels = _mm_add_epi16(ssePixels, signOffsetV);

        /* Clamp to unsigned range */
        ssePixels = _mm_max_epi16(_mm_min_epi16(ssePixels, maxV), minV);

        /* Store */
        _mm_storeu_si32((__m128i*)pixels, ssePixels);
        pixels += args->rowPixelStride;
    }
}

static void addDD_U10(const ApplyCmdBufferArgs* args) { addDD_UBase(args, 5, 16, 512, 1023); }

static void addDD_U12(const ApplyCmdBufferArgs* args) { addDD_UBase(args, 3, 4, 2048, 4095); }

static void addDD_U14(const ApplyCmdBufferArgs* args) { addDD_UBase(args, 1, 1, 8192, 16383); }

static void addDD_S16(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;
    __m128i residuals[2];
    loadResidualsDD(args->residuals, residuals);

    for (int32_t row = 0; row < CBCKTUSizeDD; ++row) {
        const __m128i ssePixels = _mm_loadu_si32((const __m128i*)pixels);
        _mm_storeu_si32((__m128i*)pixels, _mm_adds_epi16(ssePixels, residuals[row]));
        pixels += args->rowPixelStride;
    }
}

static void addDDS_U8(const ApplyCmdBufferArgs* args)
{
    assert(!fixedPointIsSigned(args->fixedPoint));

    static const int32_t kShift = 7;
    const __m128i usToSOffset = _mm_set1_epi16(0x4000);
    const __m128i fractOffset = _mm_set1_epi16(0x40);
    const __m128i signOffset = _mm_set1_epi16(0x80);

    uint8_t* pixels = (uint8_t*)args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;
    __m128i residuals[4];
    loadResidualsDDS(args->residuals, residuals);

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        __m128i ssePixels = _mm_cvtepu8_epi16(_mm_loadu_si32((const __m128i*)pixels));

        /* val <<= shift */
        ssePixels = _mm_slli_epi16(ssePixels, kShift);

        /* val -= 0x4000 */
        ssePixels = _mm_sub_epi16(ssePixels, usToSOffset);

        /* val += src */
        ssePixels = _mm_adds_epi16(ssePixels, residuals[row]);

        /* val += rounding */
        ssePixels = _mm_adds_epi16(ssePixels, fractOffset);

        /* val >>= shift */
        ssePixels = _mm_srai_epi16(ssePixels, kShift);

        /* val += sign offset */
        ssePixels = _mm_add_epi16(ssePixels, signOffset);

        /* Clamp to unsigned range and store */
        _mm_storeu_si32((__m128i*)pixels, _mm_packus_epi16(ssePixels, ssePixels));

        pixels += args->rowPixelStride;
    }
}

static inline void addDDS_UBase(const ApplyCmdBufferArgs* args, int32_t shift,
                                int16_t roundingOffset, int16_t signOffset, int16_t resultMax)
{
    assert(!fixedPointIsSigned(args->fixedPoint));

    VN_ADD_CONSTANTS_U16()

    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;
    __m128i residuals[4];
    loadResidualsDDS(args->residuals, residuals);

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        /* Load as int16_t, source data is maximally unsigned 14-bit so will fit. */
        __m128i ssePixels = _mm_loadl_epi64((const __m128i*)pixels);

        /* val <<= shift */
        ssePixels = _mm_slli_epi16(ssePixels, shift);

        /* val -= 0x4000 */
        ssePixels = _mm_sub_epi16(ssePixels, usToSOffset);

        /* val += src */
        ssePixels = _mm_adds_epi16(ssePixels, residuals[row]);

        /* val += rounding */
        ssePixels = _mm_adds_epi16(ssePixels, roundingOffsetV);

        /* val >>= shift */
        ssePixels = _mm_srai_epi16(ssePixels, shift);

        /* val += sign offset */
        ssePixels = _mm_add_epi16(ssePixels, signOffsetV);

        /* Clamp to unsigned range */
        ssePixels = _mm_max_epi16(_mm_min_epi16(ssePixels, maxV), minV);

        /* Store */
        _mm_storel_epi64((__m128i*)pixels, ssePixels);
        pixels += args->rowPixelStride;
    }
}

static void addDDS_U10(const ApplyCmdBufferArgs* args) { addDDS_UBase(args, 5, 16, 512, 1023); }

static void addDDS_U12(const ApplyCmdBufferArgs* args) { addDDS_UBase(args, 3, 4, 2048, 4095); }

static void addDDS_U14(const ApplyCmdBufferArgs* args) { addDDS_UBase(args, 1, 1, 8192, 16383); }

static inline void addDDS_S16(const ApplyCmdBufferArgs* args)
{
    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;
    __m128i residuals[4];
    loadResidualsDDS(args->residuals, residuals);

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        const __m128i ssePixels = _mm_loadl_epi64((const __m128i*)pixels);
        _mm_storel_epi64((__m128i*)pixels, _mm_adds_epi16(ssePixels, residuals[row]));
        pixels += args->rowPixelStride;
    }
}

/*------------------------------------------------------------------------------*/
/* Apply SETs */
/*------------------------------------------------------------------------------*/

static inline void setDD(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;

    __m128i residuals[2];
    loadResidualsDD(args->residuals, residuals);

    _mm_storeu_si32((__m128i*)pixels, residuals[0]);
    _mm_storeu_si32((__m128i*)(pixels + args->rowPixelStride), residuals[1]);
}

static inline void setDDS(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;

    __m128i residuals[4];
    loadResidualsDDS(args->residuals, residuals);

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        _mm_storel_epi64((__m128i*)pixels, residuals[row]);
        pixels += args->rowPixelStride;
    }
}

static inline void setZeroDD(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;

    __m128i sseZeros[2] = {0};

    _mm_storeu_si32((__m128i*)pixels, sseZeros[0]);
    _mm_storeu_si32((__m128i*)(pixels + args->rowPixelStride), sseZeros[1]);
}

static inline void setZeroDDS(const ApplyCmdBufferArgs* args)
{
    assert(fixedPointIsSigned(args->fixedPoint));

    int16_t* pixels = args->firstSample + (args->y * (size_t)args->rowPixelStride) + args->x;
    __m128i sseZeros[4] = {0};

    for (int32_t row = 0; row < CBCKTUSizeDDS; ++row) {
        _mm_storel_epi64((__m128i*)pixels, sseZeros[row]);
        pixels += args->rowPixelStride;
    }
}

/*------------------------------------------------------------------------------*/
/* Apply CLEARs */
/*------------------------------------------------------------------------------*/

static inline void clear(const ApplyCmdBufferArgs* args)
{
    const uint16_t x = args->x;
    const uint16_t y = args->y;

    const uint16_t clearWidth = minU16(ACBKBlockSize, args->height - y);
    const uint16_t clearHeight = minU16(ACBKBlockSize, args->width - x);

    int16_t* pixels = args->firstSample + (y * (size_t)args->rowPixelStride) + x;

    if (clearHeight == ACBKBlockSize && clearWidth == ACBKBlockSize) {
        for (int32_t yPos = 0; yPos < ACBKBlockSize; ++yPos) {
            _mm_storeu_si128((__m128i*)pixels, _mm_setzero_si128());
            _mm_storeu_si128((__m128i*)(pixels + 8), _mm_setzero_si128());
            _mm_storeu_si128((__m128i*)(pixels + 16), _mm_setzero_si128());
            _mm_storeu_si128((__m128i*)(pixels + 24), _mm_setzero_si128());
            pixels += args->rowPixelStride;
        }
    } else {
        const size_t clearBytes = clearHeight * sizeof(int16_t);
        for (int32_t row = 0; row < clearWidth; ++row) {
            memset(pixels, 0, clearBytes);
            pixels += args->rowPixelStride;
        }
    }
}

#define cmdBufferApplicatorBlockTemplate cmdBufferApplicatorBlockSSE
#define cmdBufferApplicatorSurfaceTemplate cmdBufferApplicatorSurfaceSSE
#include "apply_cmdbuffer_applicator.h"

#else

bool cmdBufferApplicatorBlockSSE(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                 const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint, bool highlight)
{
    VN_UNUSED_CMDBUFFER_APPLICATOR()
}

bool cmdBufferApplicatorSurfaceSSE(const LdpEnhancementTile* enhancementTile, size_t entryPointIdx,
                                   const LdpPicturePlaneDesc* plane, LdpFixedPoint fixedPoint, bool highlight)
{
    VN_UNUSED_CMDBUFFER_APPLICATOR()
}

#endif
