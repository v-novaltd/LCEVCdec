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

// Class for reading raw image files from streams or filesystem.
//
#ifndef VN_LCEVC_UTILITY_RAW_READER_H
#define VN_LCEVC_UTILITY_RAW_READER_H

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/picture_layout.h"

#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

class RawReader
{
public:
    // Information
    const LCEVC_PictureDesc& description() const { return m_description; }
    const PictureLayout& layout() const { return m_layout; }

    // Read into memory
    bool read(std::vector<uint8_t>& memory);

    // Read into Picture
    bool read(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle pictureHandle);

    // Byte offset in stream
    uint64_t offset() const;

private:
    explicit RawReader(const LCEVC_PictureDesc& description, std::unique_ptr<std::istream> stream);

    friend std::unique_ptr<RawReader> createRawReader(const LCEVC_PictureDesc& description,
                                                      std::string_view name);
    friend std::unique_ptr<RawReader> createRawReader(const LCEVC_PictureDesc& description,
                                                      std::unique_ptr<std::istream> stream);

    LCEVC_PictureDesc m_description;
    PictureLayout m_layout;
    std::unique_ptr<std::istream> m_stream;
};

/*!
 * \brief Create a YUVReader, given a filename
 *
 * @param[in]       name            Filename of YUV file - filename will be parsed for description.
 * @return                          Unique pointer to the new YuvReader, or nullptr if failed
 */
std::unique_ptr<RawReader> createRawReader(std::string_view name);

/*!
 * \brief Create a YUVReader, given a description and a filename
 *
 * @param[in]       description     Description of picture (format & size)
 * @param[in]       name            Filename of raw file
 * @return                          Unique pointer to the new YuvReader, or nullptr if failed
 */
std::unique_ptr<RawReader> createRawReader(const LCEVC_PictureDesc& description, std::string_view name);

/*!
 * \brief Create a YUVReader, given a description and an istream
 *
 * @param[in]       description     Description of picture (format & size)
 * @param[in]       stream          Stream containing Raw image
 * @return                          Unique ptr to the new YuvReader, or nullptr if failed
 */
std::unique_ptr<RawReader> createRawReader(const LCEVC_PictureDesc& description,
                                           std::unique_ptr<std::istream> stream);

} // namespace lcevc_dec::utility

#endif
