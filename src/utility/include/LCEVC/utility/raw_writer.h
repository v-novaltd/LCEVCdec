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

// Class for writing raw image files to streams or filesystem.
//
#ifndef VN_LCEVC_UTILITY_RAW_WRITER_H
#define VN_LCEVC_UTILITY_RAW_WRITER_H

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/picture_layout.h"

#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

class RawWriter
{
public:
    // Information
    const LCEVC_PictureDesc& description() const;
    const PictureLayout& layout() const;

    // Write from picture
    bool write(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture);

    // Write from  memory
    bool write(std::vector<uint8_t>& memory);

    // Byte offset in stream
    uint64_t offset() const;

private:
    explicit RawWriter(const LCEVC_PictureDesc& description, std::unique_ptr<std::ostream>&& stream);

    friend std::unique_ptr<RawWriter> createRawWriter(const LCEVC_PictureDesc& description,
                                                      std::string_view name);
    friend std::unique_ptr<RawWriter> createRawWriter(const LCEVC_PictureDesc& description,
                                                      std::unique_ptr<std::ostream> stream);

    LCEVC_PictureDesc m_description;
    PictureLayout m_layout;
    std::unique_ptr<std::ostream> m_stream;
};

/*!
 * \brief Create a YUVWriter, given a filename.
 * The picture description will be takedn from the first written picture
 *
 * @param[in]       name            Filename of raw output file
 * @return                          Unique pointer to the new YuvWriter, or nullptr if failed
 */
std::unique_ptr<RawWriter> createRawWriter(std::string_view filename);

/*!
 * \brief Create a YUVWriter, given a picture descrption,and a filename
 * Written pictures must match the description.
 *
 * @param[in]       _description    Description of picture (format & size)
 * @param[in]       name            Filename of raw output file
 * @return                          Unique pointer to the new YuvWriter, or nullptr if failed
 */
std::unique_ptr<RawWriter> createRawWriter(const LCEVC_PictureDesc& description, std::string_view name);

/*!
 * \brief Create a YUVWriter, given an ostream
 *
 * @param[in]       description     Description of picture (format & size)
 * @param[in]       stream          Output stream to use - will take ownership
 * @return                          Unique pointer to the new YuvWriter, or nullptr if failed
 */
std::unique_ptr<RawWriter> createRawWriter(const LCEVC_PictureDesc& description,
                                           std::unique_ptr<std::ostream> stream);

} // namespace lcevc_dec::utility

#endif
