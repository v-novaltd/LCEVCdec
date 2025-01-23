/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
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

#pragma once

#include "LCEVC/utility/types.h"

#include <functional>

namespace lcevc_dec::utility {

// -------------------------------------------------------------------------

typedef std::function<bool(uint8_t byte)> BitStreamByteWriter;

// -------------------------------------------------------------------------

/// @brief Wrapper class for constructing a bitstream.
///
/// This class buffers bits into a single byte, once full the byte is written
/// through the supplied function.
class BitStreamWriter
{
public:
    explicit BitStreamWriter(BitStreamByteWriter byteWriter)
        : m_byteWriter(byteWriter)
    {}

    bool WriteBits(uint8_t numBits, uint32_t value, bool bFinish = false);
    bool Finish();

    uint64_t BitSize() const { return m_bitSize; }
    uint64_t ByteSize() const { return (m_bitSize + 7) >> 3; }

    static BitStreamWriter OfRawMemory(uint8_t* data, uint32_t size);

private:
    BitStreamByteWriter m_byteWriter;
    uint64_t m_bitSize = 0;
    uint8_t m_byte = 0;
    uint8_t m_remainingBits = 8;
};

// -------------------------------------------------------------------------

/// @brief Functor for use with BitStreamWriter that takes a pointer and
///        size of memory to write to.
class BitStreamByteWriterRawMemory
{
public:
    BitStreamByteWriterRawMemory(uint8_t* data, uint32_t size);

    bool operator()(uint8_t byte) noexcept;

private:
    uint8_t* m_data = nullptr;
    uint32_t m_size = 0;
    uint32_t m_position = 0;
};

// -------------------------------------------------------------------------

} // namespace lcevc_dec::utility
