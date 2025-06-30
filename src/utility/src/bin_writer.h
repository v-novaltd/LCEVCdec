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

#endif // VN_LCEVC_UTILITY_BIN_WRITER_H
