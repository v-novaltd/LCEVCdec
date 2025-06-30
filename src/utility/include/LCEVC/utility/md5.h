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

// Compute MD5 checksum
//
// See https://en.wikipedia.org/wiki/MD5
//
#ifndef VN_LCEVC_UTILITY_MD5_H
#define VN_LCEVC_UTILITY_MD5_H

#include <cstdint>
#include <string>

namespace lcevc_dec::utility {

class MD5
{
public:
    MD5() { reset(); }

    // Reset to initial state
    void reset();

    // Append data to message
    void update(const uint8_t* data, size_t size);

    // Finish message, and fetch the message's digest as bytes
    void digest(uint8_t output[16]);

    // Finish message, and fetch the message's digest as a string of hex digits
    std::string hexDigest();

private:
    // Size of each chunk in bytes
    static constexpr uint32_t kChunkSize = 64;

    // Process one 512 bit chunk of message
    void chunk(const uint8_t* data);

    // Finish processing message
    void finish();

    // Current sum
    uint32_t m_a0{0};
    uint32_t m_b0{0};
    uint32_t m_c0{0};
    uint32_t m_d0{0};

    // Length in bits
    uint64_t m_length{0};

    // Pending message data
    uint8_t m_chunk[kChunkSize]{};
    size_t m_chunkSize{0};

    bool m_finished{false};
};

} // namespace lcevc_dec::utility
#endif // VN_LCEVC_UTILITY_MD5_H
