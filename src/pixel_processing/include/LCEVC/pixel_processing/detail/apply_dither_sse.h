/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#ifndef VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_SSE_H
#define VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_SSE_H

#include <LCEVC/build_config.h>
#if VN_CORE_FEATURE(SSE)
#include <LCEVC/common/sse.h>

/*!
 * Apply dithering to values using supplied host buffer pointer containing pre-randomised
 * values.
 *
 * This function loads in values from the entropy buffer and scales the random value by the
 * desired amount then moves the buffer pointer forward for the next invocation of this function.
 *
 * \param values   The values to apply dithering to.
 * \param buffer   A double pointer to the dither buffer
 * \param shift    The left shift to apply to the dither to account for the fixed point format of
 *                 the incoming pixel values (see ldppDitherGetShiftS16)
 * \param strength Dithering strength to scale the random value by
 */
static inline void ldppDitherApplySSE(__m128i values[2], const uint16_t** ditherBuffer,
                                      const uint8_t shift, const uint8_t strength)
{
    const __m128i scalar = _mm_set1_epi16(strength * 2 + 1);
    const __m128i offset = _mm_set1_epi16(strength);
    __m128i dither[2];

    // Load and increment dither buffer pointer
    dither[0] = _mm_loadu_si128((const __m128i*)*ditherBuffer);
    *ditherBuffer += 8;
    dither[1] = _mm_loadu_si128((const __m128i*)*ditherBuffer);
    *ditherBuffer += 8;

    // Multiply by scalar
    dither[0] = _mm_mulhi_epu16(dither[0], scalar);
    dither[1] = _mm_mulhi_epu16(dither[1], scalar);

    // Subtract offset to get values into -strength to +strength range
    dither[0] = _mm_sub_epi16(offset, dither[0]);
    dither[1] = _mm_sub_epi16(offset, dither[1]);

    // Add dither pixel values (saturating to avoid overflow)
    values[0] = _mm_adds_epi16(values[0], _mm_slli_epi16(dither[0], shift));
    values[1] = _mm_adds_epi16(values[1], _mm_slli_epi16(dither[1], shift));
}

#endif
#endif // VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_SSE_H
