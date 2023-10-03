// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// writer for V-Nova internal .bin format.
//
#ifndef VN_LCEVC_UTILITY_BIN_WRITER_H
#define VN_LCEVC_UTILITY_BIN_WRITER_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

class BinWriter
{
public:
    bool write(int64_t decodeIndex, int64_t presentationIndex, const uint8_t* payloadData,
               uint32_t payloadSize);

    uint64_t offset() const;

    std::ostream* stream() const;

private:
    explicit BinWriter(std::unique_ptr<std::ostream> stream);
    friend std::unique_ptr<BinWriter> createBinWriter(std::unique_ptr<std::ostream> stream);
    friend std::unique_ptr<BinWriter> createBinWriter(std::string_view name);

    bool writeHeader();

    std::unique_ptr<std::ostream> m_stream;
};

/*!
 * \brief Create a BinWriter, given an output stream
 *
 * @param[in]       stream Output stream to use - will take ownership
 * @return          Unique ptr to the new BinWriter, or nullptr if failed
 */
std::unique_ptr<BinWriter> createBinWriter(std::unique_ptr<std::ostream> stream);

/*!
 * \brief Create a BinWriter, given a filename
 *
 * @param[in]       name Filename of LCEVC BIN file
 * @return          Unique pointer to the new BinWriter, or nullptr if failed
 */
std::unique_ptr<BinWriter> createBinWriter(std::string_view name);

} // namespace lcevc_dec::utility

#endif
