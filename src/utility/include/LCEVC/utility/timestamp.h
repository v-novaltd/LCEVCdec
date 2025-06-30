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

#ifndef VN_LCEVC_UTILITY_TIMESTAMP_H
#define VN_LCEVC_UTILITY_TIMESTAMP_H

#include <cstdint>

namespace lcevc_dec::utility {

// - Timestamp manipulation ------------------------------------------------------------------

/*!
 * \brief The LCEVCdec API methods expect unique monotonically increasing timestamps as a uint64
 *        type, libav and other base decoders express presentation timestamps as a int64. This
 *        utility both converts the sign and reserves the most significant 16 bits for a
 *        discontinuity counter that can be used to join any breaks in a stream/PTS sequence to a
 *        single sequence.
 *
 * @param[in]   discontinuityCount  A counter starting from zero for discontinuous streams/PTSs,
 *                                  ownership and incrementing of the counter is expected by the
 *                                  client
 * @param[in]   pts                 Timestamps from the base decoder.
 *
 * @return                          Unique pointer to a new base Decoder, or nullptr if failed.
 */
inline uint64_t getUniqueTimestamp(const uint16_t discontinuityCount, const int64_t pts)
{
    return ((uint64_t)discontinuityCount << 48) | ((pts + (1LL << 47)) & 0x0000FFFFFFFFFFFF);
}

} // namespace lcevc_dec::utility

#endif // VN_LCEVC_UTILITY_TIMESTAMP_H
