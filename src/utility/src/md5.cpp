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

// Compute MD5 checksum
//
// See https://en.wikipedia.org/wiki/MD5
//
#include "LCEVC/utility/md5.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>

namespace lcevc_dec::utility {

// Number of rounds
constexpr uint32_t kRounds = 64;

// s specifies the per-round shift amounts
static const uint32_t s[kRounds] = {
    7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
    14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
    4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
};

// Use binary integer part of the sines of integers (Radians) as constants:
//
//   for i from 0 to 63 do
//      K[i] := floor(232 * abs(sin(i + 1)))
//   end for
//
// Precomputed table:
//
static const uint32_t K[kRounds] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

void MD5::reset()
{
    // Initialize variables
    m_a0 = 0x67452301; // A
    m_b0 = 0xefcdab89; // B
    m_c0 = 0x98badcfe; // C
    m_d0 = 0x10325476; // D

    m_chunkSize = 0;
    m_length = 0;

    m_finished = false;
}

static inline uint32_t leftrotate(uint32_t val, uint32_t dist)
{
    return (val << dist) | (val >> (32 - dist));
}

// Process one 512 bit chunk
void MD5::chunk(const uint8_t* data)
{
    // break chunk into sixteen 32-bit words M[j], 0 <= j <= 15
    uint32_t M[16] = {0};
    for (uint32_t i = 0; i < 16; ++i) {
        // Little endian
        M[i] = data[i * 4 + 0] + (data[i * 4 + 1] << 8) + (data[i * 4 + 2] << 16) + (data[i * 4 + 3] << 24);
    }

    // Initialize hash value for this chunk:
    uint32_t A = m_a0;
    uint32_t B = m_b0;
    uint32_t C = m_c0;
    uint32_t D = m_d0;

    // Main loop:
    for (uint32_t i = 0; i < kRounds; ++i) {
        uint32_t F = 0, g = 0;

        switch (i >> 4) {
            case 0:
                F = (B & C) | ((~B) & D);
                g = i;
                break;
            case 1:
                F = (D & B) | ((~D) & C);
                g = (5 * i + 1) % 16;
                break;
            case 2:
                F = B ^ C ^ D;
                g = (3 * i + 5) % 16;
                break;
            case 3:
                F = C ^ (B | (~D));
                g = (7 * i) % 16;
                break;
        }
        // Be wary of the below definitions of a,b,c,d
        F = F + A + K[i] + M[g]; // M[g] must be a 32-bit block
        A = D;
        D = C;
        C = B;
        B = B + leftrotate(F, s[i]);
    }

    // Add this chunk's hash to result so far:
    m_a0 += A;
    m_b0 += B;
    m_c0 += C;
    m_d0 += D;
}

void MD5::update(const uint8_t* data, uint32_t size)
{
    assert(!m_finished);

    // Update bit length
    m_length += static_cast<uint64_t>(size) * 8;

    assert(data != nullptr);
    assert(m_chunkSize < kChunkSize);

    // If there is buffered message data, accumulate from front of this message data
    if (m_chunkSize > 0) {
        uint32_t sz = std::min(size, kChunkSize - m_chunkSize);
        memcpy(m_chunk + m_chunkSize, data, sz);
        m_chunkSize += sz;
        size -= sz;
        data += sz;
    }

    // If pending block is full - sum it
    if (m_chunkSize == kChunkSize) {
        chunk(m_chunk);
        m_chunkSize = 0;
    }

    // Sum chunks from message data
    while (size >= kChunkSize) {
        chunk(data);
        data += kChunkSize;
        size -= kChunkSize;
    }

    // Copy any remaining message data into buffer
    memcpy(m_chunk, data, size);
    m_chunkSize += size;
}

void MD5::finish()
{
    assert(!m_finished);

    // Append 0x80, and pad with 0x00 bytes so that the message length in bytes == 56 (mod 64).
    assert(m_chunkSize < kChunkSize);
    m_chunk[m_chunkSize++] = 0x80;

    // Zero pad rest of buffer
    memset(m_chunk + m_chunkSize, 0, kChunkSize - m_chunkSize);

    // Sum buffer and pad with zeros if not enough space for length
    if (m_chunkSize > kChunkSize - 8) {
        chunk(m_chunk);
        memset(m_chunk, 0, kChunkSize);
    }

    // Append original length in bits mod 2^64 to message and sum it
    m_chunk[kChunkSize - 8] = m_length & 0xff;
    m_chunk[kChunkSize - 7] = (m_length >> 8) & 0xff;
    m_chunk[kChunkSize - 6] = (m_length >> 16) & 0xff;
    m_chunk[kChunkSize - 5] = (m_length >> 24) & 0xff;
    m_chunk[kChunkSize - 4] = (m_length >> 32) & 0xff;
    m_chunk[kChunkSize - 3] = (m_length >> 40) & 0xff;
    m_chunk[kChunkSize - 2] = (m_length >> 48) & 0xff;
    m_chunk[kChunkSize - 1] = (m_length >> 56) & 0xff;
    chunk(m_chunk);

    m_finished = true;
}

void MD5::digest(uint8_t output[16])
{
    if (!m_finished) {
        finish();
    }

    // char digest[16] := a0 append b0 append c0 append d0 - Output is in little-endian
    output[0] = m_a0 & 0xff;
    output[1] = (m_a0 >> 8) & 0xff;
    output[2] = (m_a0 >> 16) & 0xff;
    output[3] = (m_a0 >> 24) & 0xff;

    output[4] = m_b0 & 0xff;
    output[5] = (m_b0 >> 8) & 0xff;
    output[6] = (m_b0 >> 16) & 0xff;
    output[7] = (m_b0 >> 24) & 0xff;

    output[8] = m_c0 & 0xff;
    output[9] = (m_c0 >> 8) & 0xff;
    output[10] = (m_c0 >> 16) & 0xff;
    output[11] = (m_c0 >> 24) & 0xff;

    output[12] = m_d0 & 0xff;
    output[13] = (m_d0 >> 8) & 0xff;
    output[14] = (m_d0 >> 16) & 0xff;
    output[15] = (m_d0 >> 24) & 0xff;
}

std::string MD5::hexDigest()
{
    uint8_t digestBytes[16];
    digest(digestBytes);

    char digestString[32];
    const char hexDigits[] = "0123456789abcdef";
    for (uint32_t i = 0; i < 16; ++i) {
        digestString[i * 2 + 0] = hexDigits[digestBytes[i] >> 4];
        digestString[i * 2 + 1] = hexDigits[digestBytes[i] & 0xf];
    }

    return {digestString, digestString + 32};
}

} // namespace lcevc_dec::utility
