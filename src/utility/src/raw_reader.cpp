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

// Class for reading raw image files from streams or filesystem.
//
#include "parse_raw_name.h"

#include <fmt/core.h>
#include <LCEVC/api_utility/picture_layout.h>
#include <LCEVC/utility/check.h>
#include <LCEVC/utility/picture_lock.h>
#include <LCEVC/utility/raw_reader.h>
#include <LCEVC/utility/string_utils.h>

#include <fstream>

namespace lcevc_dec::utility {

RawReader::RawReader(const LCEVC_PictureDesc& description, std::unique_ptr<std::istream> stream)
    : m_description(description)
    , m_layout(description)
    , m_stream(std::move(stream))
{}

uint64_t RawReader::offset() const { return m_stream->tellg(); }

bool RawReader::read(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture)
{
    VN_LCEVC_CHECK(LCEVC_SetPictureDesc(decoder, picture, &m_description));

    PictureLock lock(decoder, picture, LCEVC_Access_Write);

    for (uint32_t plane = 0; plane < lock.numPlanes(); ++plane) {
        for (unsigned row = 0; row < lock.height(plane); ++row) {
            m_stream->read(lock.rowData<char>(plane, row), lock.rowSize(plane));
            if (!m_stream->good()) {
                return false;
            }
        }
    }

    return true;
}

bool RawReader::read(std::vector<uint8_t>& memory)
{
    memory.resize(m_layout.size());

    m_stream->read(static_cast<char*>(static_cast<void*>(memory.data())), m_layout.size());

    if (!m_stream->good()) {
        return false;
    }

    return true;
}

std::unique_ptr<RawReader> createRawReader(const LCEVC_PictureDesc& pictureDescription,
                                           std::string_view filename)
{
    std::unique_ptr<std::istream> stream =
        std::make_unique<std::ifstream>(std::string(filename), std::ios::binary);
    return createRawReader(pictureDescription, std::move(stream));
}

std::unique_ptr<RawReader> createRawReader(const LCEVC_PictureDesc& pictureDescription,
                                           std::unique_ptr<std::istream> stream)
{
    if (!stream->good()) {
        return nullptr;
    }
    // NB: The constructor is private, so make_unique is not a good option here.
    return std::unique_ptr<RawReader>(new RawReader(pictureDescription, std::move(stream)));
}

std::unique_ptr<RawReader> createRawReader(std::string_view filename)
{
    float rate = 0.0;
    LCEVC_PictureDesc pictureDescription = parseRawName(filename, rate);
    if (pictureDescription.colorFormat == LCEVC_ColorFormat_Unknown ||
        pictureDescription.width == 0 || pictureDescription.height == 0) {
        fmt::print(
            stderr,
            "Could not extract a format from YUV filename, include dimensions and bitdepth\n");
        return nullptr;
    }

    return createRawReader(pictureDescription, filename);
}

} // namespace lcevc_dec::utility
