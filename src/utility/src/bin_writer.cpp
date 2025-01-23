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

// Writer for V-Nova internal .bin format.
//
#include "bin_writer.h"

#include "bin_format.h"
#include "LCEVC/utility/byte_order.h"

#include <fmt/core.h>

#include <algorithm>
#include <fstream>
#include <ios>
#include <string_view>

namespace lcevc_dec::utility {

BinWriter::BinWriter(std::unique_ptr<std::ostream> stream)
    : m_stream(std::move(stream))
{}

bool BinWriter::writeHeader()
{
    if (!m_stream->write(kMagicBytes, sizeof(kMagicBytes)) || !writeBigEndian(*m_stream, kVersion)) {
        return false;
    }

    return true;
}

bool BinWriter::write(int64_t decodeIndex, int64_t presentationIndex, const uint8_t* payloadData,
                      uint32_t payloadSize)
{
    // Block header
    if (!writeBigEndian(*m_stream, static_cast<uint16_t>(BlockTypes::LCEVCPayload))) {
        return false;
    }

    if (!writeBigEndian(*m_stream, static_cast<uint32_t>(payloadSize + sizeof(decodeIndex) +
                                                         sizeof(presentationIndex)))) {
        return false;
    }

    // Payload header
    if (!writeBigEndian(*m_stream, decodeIndex) || !writeBigEndian(*m_stream, presentationIndex)) {
        return false;
    }

    // Payload
    if (!m_stream->write(static_cast<const char*>(static_cast<const void*>(payloadData)),
                         static_cast<std::streamsize>(payloadSize))) {
        return false;
    }

    return true;
}

uint64_t BinWriter::offset() const { return m_stream->tellp(); }

std::ostream* BinWriter::stream() const { return m_stream.get(); }

// Create an LCEVC BIN file writer
//
std::unique_ptr<BinWriter> createBinWriter(std::unique_ptr<std::ostream> stream)
{
    std::unique_ptr<BinWriter> writer(new BinWriter(std::move(stream)));

    if (!writer->writeHeader()) {
        return nullptr;
    }

    return writer;
}

// Create an LCEVC BIN file writer
//
std::unique_ptr<BinWriter> createBinWriter(std::string_view name)
{
    auto stream = std::make_unique<std::ofstream>(std::string(name), std::ios::binary | std::ios::trunc);
    if (!stream->good()) {
        return nullptr;
    }

    return createBinWriter(std::move(stream));
}

} // namespace lcevc_dec::utility
