/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "bitstream_writer.h"

namespace lcevc_dec::utility {

// -------------------------------------------------------------------------

bool BitStreamWriter::WriteBits(uint8_t numBits, uint32_t value, bool bFinish)
{
    // Clear high bits of value for safety during the first iteration.
    value &= ((1 << numBits) - 1);

    while (numBits) {
        if (m_remainingBits == 0) {
            if (!m_byteWriter(m_byte))
                return false;

            m_byte = 0;
            m_remainingBits = 8;
        }

        const auto writeAmount = std::min(m_remainingBits, numBits);

        // Shift number of bits to be written such that LSB is in bit 0
        // Cast will chop any bits already written in previous loop.
        const auto writeValue = (uint8_t)(value >> (numBits - writeAmount));

        // Note: It is not required to zero out bits already written. When
        //       starting a new byte after having written some bits of `value`
        //       into the previous byte, the high bits will still be present
        //       and initially appear in the new byte, but as more bits are
        //       put in those high bits will be shifted out (or finished).
        m_byte <<= writeAmount; // Make some room in the byte.
        m_byte |= writeValue;   // Push bits into the bottom of the byte.

        m_remainingBits -= writeAmount;
        numBits -= writeAmount;

        // Accumulate number of bits written, this includes bits pending to
        // be written)
        m_bitSize += writeAmount;
    }

    return bFinish ? Finish() : true;
}

bool BitStreamWriter::Finish()
{
    // No more bits to write out.
    if (m_remainingBits == 8) {
        return true;
    }

    // Flush remaining byte.
    const uint8_t outputByte = m_byte << m_remainingBits;
    m_byte = 0;
    m_remainingBits = 8;

    return m_byteWriter(outputByte);
}

BitStreamWriter BitStreamWriter::OfRawMemory(uint8_t* data, uint32_t size)
{
    return BitStreamWriter{BitStreamByteWriterRawMemory{data, size}};
}

// -------------------------------------------------------------------------

BitStreamByteWriterRawMemory::BitStreamByteWriterRawMemory(uint8_t* data, uint32_t size)
    : m_data(data)
    , m_size(size)
{}

bool BitStreamByteWriterRawMemory::operator()(uint8_t byte) noexcept
{
    if (m_position >= m_size)
        return false;

    m_data[m_position++] = byte;
    return true;
}

// -------------------------------------------------------------------------

} // namespace lcevc_dec::utility
