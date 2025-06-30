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

#ifndef VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_SCALAR_H
#define VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_SCALAR_H

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
 * \param strength Dither strength to scale the random value by
 */
static inline void ldppDitherApply(int32_t* value, const uint16_t** ditherBuffer,
                                   const uint8_t shift, const uint8_t strength)
{
    *value += (strength - ((**ditherBuffer * (strength * 2 + 1)) >> 16)) << shift;
    *ditherBuffer += 1;
}

#endif // VN_LCEVC_PIXEL_PROCESSING_DETAIL_APPLY_DITHER_SCALAR_H
