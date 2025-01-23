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

    std::istream* stream() const;

private:
    explicit BinReader(std::unique_ptr<std::istream> stream);
    friend std::unique_ptr<BinReader> createBinReader(std::unique_ptr<std::istream> stream);
    friend std::unique_ptr<BinReader> createBinReader(std::string_view name);

    bool readHeader();

    std::unique_ptr<std::istream> m_stream;
};

/*!
 * \brief Create a BinReader, given an input stream
 *
 * @param[in]     stream Input stream to use - will take ownership
 * @return        Unique pointer to the new BinReader, or nullptr if failed
 */
std::unique_ptr<BinReader> createBinReader(std::unique_ptr<std::istream> stream);

/*!
 * \brief Create a BinReader, given a filename
 *
 * @param[in]     name Filename of LCEVC BIN file
 * @return        Unique pointer to the new BinReader, or nullptr if failed
 */
std::unique_ptr<BinReader> createBinReader(std::string_view name);

} // namespace lcevc_dec::utility

#endif
