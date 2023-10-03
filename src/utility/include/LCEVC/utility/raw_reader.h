// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
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
