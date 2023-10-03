// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Writer for V-Nova internal .bin format.
//
#include "bin_writer.h"

#include "LCEVC/utility/byte_order.h"
#include "bin_format.h"

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

    if (!writeBigEndian(*m_stream,
                        (uint32_t)(payloadSize + sizeof(decodeIndex) + sizeof(presentationIndex)))) {
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
