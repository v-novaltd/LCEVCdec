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

#ifndef VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_NEON_H
#define VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_NEON_H

#include <LCEVC/build_config.h>
#if VN_CORE_FEATURE(NEON)
#include <LCEVC/common/neon.h>

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
static inline void ldppDitherApplyNEON(int16x8x2_t* values, const uint16_t** buffer,
                                       const uint8_t shift, const uint8_t strength)
{
    const int16x8_t offset = vdupq_n_s16(strength);
    const int16x8_t shft = vdupq_n_s16(shift);

    // Load and increment dither buffer pointer
    uint16x8x2_t dither = vld2q_u16(*buffer);
    *buffer += 16;

    // Multiply by scalar
    uint32x4x2_t lowHalf;
    lowHalf.val[0] = vmull_n_u16(vget_low_u16(dither.val[0]), strength * 2 + 1);
    lowHalf.val[1] = vmull_n_u16(vget_low_u16(dither.val[1]), strength * 2 + 1);

#if defined(__aarch64__)
    dither.val[0] = vuzp2q_u16(vreinterpretq_u16_u32(lowHalf.val[0]),
                               vreinterpretq_u16_u32(vmull_high_n_u16(dither.val[0], strength * 2 + 1)));
    dither.val[1] = vuzp2q_u16(vreinterpretq_u16_u32(lowHalf.val[1]),
                               vreinterpretq_u16_u32(vmull_high_n_u16(dither.val[1], strength * 2 + 1)));
#else
    uint32x4x2_t highHalf;
    highHalf.val[0] = vmull_n_u16(vget_high_u16(dither.val[0]), strength * 2 + 1);
    highHalf.val[1] = vmull_n_u16(vget_high_u16(dither.val[1]), strength * 2 + 1);

    dither.val[0] =
        vuzpq_u16(vreinterpretq_u16_u32(lowHalf.val[0]), vreinterpretq_u16_u32(highHalf.val[0])).val[1];
    dither.val[1] =
        vuzpq_u16(vreinterpretq_u16_u32(lowHalf.val[1]), vreinterpretq_u16_u32(highHalf.val[1])).val[1];
#endif

    // Subtract offset to get values into -strength to +strength range
    dither.val[0] = vreinterpretq_u16_s16(vqsubq_s16(offset, vreinterpretq_s16_u16(dither.val[0])));
    dither.val[1] = vreinterpretq_u16_s16(vqsubq_s16(offset, vreinterpretq_s16_u16(dither.val[1])));

    // Add dither pixel values (saturating to avoid overflow)
    values->val[0] = vqaddq_s16(values->val[0], vshlq_s16(vreinterpretq_s16_u16(dither.val[0]), shft));
    values->val[1] = vqaddq_s16(values->val[1], vshlq_s16(vreinterpretq_s16_u16(dither.val[1]), shft));
}

#endif
#endif // VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_NEON_H
