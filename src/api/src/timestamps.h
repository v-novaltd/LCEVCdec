/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

#ifndef VN_API_TIMESTAMPS_H_
#define VN_API_TIMESTAMPS_H_

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// - Constants --------------------------------------------------------------------------------

constexpr uint64_t kInvalidTimehandle = UINT64_MAX;

// - Timehandle manipulation ------------------------------------------------------------------

// timehandle is a uint64_t number consisting of:
// MSB uint16_t cc
// LSB  int48_t timestamp
// Note: the following four functions are endian independent
inline uint64_t getTimehandle(const uint16_t cc, const int64_t timestamp)
{
    return ((uint64_t)cc << 48) | (timestamp & 0x0000FFFFFFFFFFFF);
}

inline uint16_t timehandleGetCC(const uint64_t handle) { return (uint16_t)(handle >> 48); }

inline int64_t timehandleGetTimestamp(const uint64_t handle)
{
    return (int64_t)(handle << 16) >> 16;
}

inline bool timestampIsValid(const int64_t timestamp)
{
    return (timestamp & 0x0000FFFFFFFFFFFF) == timestamp;
}

} // namespace lcevc_dec::decoder

#endif // VN_API_TIMESTAMPS_H_
