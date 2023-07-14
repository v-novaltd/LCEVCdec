// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Reader for V-Nova internal .bin format.
//
#ifndef VN_LCEVC_UTILITY_BIN_READER_H
#define VN_LCEVC_UTILITY_BIN_READER_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

class BinReader
{
public:
    bool read(int64_t& decodeIndex, int64_t& presentationIndex, std::vector<uint8_t>& payload);

    uint64_t offset() const;

private:
    BinReader(std::unique_ptr<std::istream> stream);
    friend std::unique_ptr<BinReader> createBinReader(std::string_view name);

    bool readHeader();

    std::unique_ptr<std::istream> m_stream;
};

/*!
 * \brief Create a BinReader, given a filename
 *
 * @param[in]       name Filename of LCEVC BIN file
 * @return          Unique ptr to the new BinReader, or nullptr if failed
 */
std::unique_ptr<BinReader> createBinReader(std::string_view name);

} // namespace lcevc_dec::utility

#endif
