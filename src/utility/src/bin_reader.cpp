/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Reader for V-Nova internal .bin format.
//
#include "LCEVC/utility/bin_reader.h"

#include "bin_format.h"
#include "LCEVC/utility/byte_order.h"

#include <fmt/core.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string_view>

namespace lcevc_dec::utility {

BinReader::BinReader(std::unique_ptr<std::istream> stream)
    : m_stream(std::move(stream))
{}

bool BinReader::readHeader()
{
    char magic[8] = {};
    uint32_t version = 0;
    if (!m_stream->read(magic, sizeof(magic)) || !readBigEndian(*m_stream, version)) {
        fmt::print(stderr, "Short BIN header.");
        return false;
    }

    if (std::memcmp(magic, kMagicBytes, sizeof(magic)) != 0 || version != kVersion) {
        fmt::print(stderr, "Bad BIN header.");
        return false;
    }

    return true;
}

bool BinReader::read(int64_t& decodeIndex, int64_t& presentationIndex, std::vector<uint8_t>& payload)
{
    // Block header
    uint16_t type = 0;
    uint32_t size = 0;

    if (!readBigEndian(*m_stream, type)) {
        // End of file
        return false;
    }

    if (!readBigEndian(*m_stream, size)) {
        fmt::print(stderr, "Short BIN block.");
        return false;
    }

    // Payload header
    if (type != static_cast<uint16_t>(BlockTypes::LCEVCPayload) || size < 16) {
        fmt::print(stderr, "Unrecognized BIN block.");
        return false;
    }

    if (!readBigEndian(*m_stream, decodeIndex) || !readBigEndian(*m_stream, presentationIndex)) {
        fmt::print(stderr, "Short Payload block.");
        return false;
    }

    // Payload
    payload.resize(size - (sizeof(decodeIndex) + sizeof(presentationIndex)));
    char* data = static_cast<char*>(static_cast<void*>(payload.data()));
    auto sz = static_cast<std::streamsize>(payload.size());
    if (!m_stream->read(data, sz)) {
        fmt::print(stderr, "Short payload.");
        return false;
    }

    return true;
}

uint64_t BinReader::offset() const { return m_stream->tellg(); }

std::istream* BinReader::stream() const { return m_stream.get(); }

// Create an LCEVC BIN file reader
//

std::unique_ptr<BinReader> createBinReader(std::unique_ptr<std::istream> stream)
{
    std::unique_ptr<BinReader> reader(new BinReader(std::move(stream)));

    if (!reader->readHeader()) {
        return nullptr;
    }

    return reader;
}

std::unique_ptr<BinReader> createBinReader(std::string_view name)
{
    auto stream = std::make_unique<std::ifstream>(std::string(name), std::ios::binary);
    if (!stream->good()) {
        return nullptr;
    }

    return createBinReader(std::move(stream));
}

} // namespace lcevc_dec::utility
