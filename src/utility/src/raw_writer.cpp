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

// Class for writing raw image files to streams or filesystem.
//
#include "LCEVC/utility/raw_writer.h"

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/check.h"
#include "LCEVC/utility/picture_lock.h"
#include "LCEVC/utility/string_utils.h"

#include <LCEVC/api_utility/picture_layout.h>

#include <fstream>
#include <ios>
#include <iostream>

namespace lcevc_dec::utility {

const LCEVC_PictureDesc& RawWriter::description() const { return m_description; }

const PictureLayout& RawWriter::layout() const { return m_layout; }

RawWriter::RawWriter(const LCEVC_PictureDesc& description, std::unique_ptr<std::ostream>&& stream)
    : m_description(description)
    , m_layout(description)
    , m_stream(std::move(stream))
{}

uint64_t RawWriter::offset() const { return m_stream->tellp(); }

bool RawWriter::write(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture)
{
    assert(m_stream);

    if (m_description.colorFormat == LCEVC_ColorFormat_Unknown) {
        // If the current description was unknown, initialize from first picture
        LCEVC_GetPictureDesc(decoder, picture, &m_description);
        m_layout = PictureLayout(m_description);
    } else {
        // Check incoming picture is compatible with current descriptions
        PictureLayout thisLayout(decoder, picture);
        if (!m_layout.isCompatible(thisLayout)) {
            return false;
        }
    }

    PictureLock lock(decoder, picture, LCEVC_Access_Read);

    for (unsigned plane = 0; plane < lock.numPlanes(); ++plane) {
        for (unsigned row = 0; row < lock.height(plane); ++row) {
            m_stream->write(lock.rowData<const char>(plane, row), lock.rowSize(plane));
            if (!m_stream->good()) {
                return false;
            }
        }
    }

    return true;
}

bool RawWriter::write(std::vector<uint8_t>& memory)
{
    assert(m_stream);

    m_stream->write(static_cast<char*>(static_cast<void*>(memory.data())),
                    static_cast<std::streamsize>(memory.size()));

    return m_stream->good();
}

std::unique_ptr<RawWriter> createRawWriter(const LCEVC_PictureDesc& pictureDescription,
                                           std::string_view filename)
{
    if (filename.empty()) {
        return nullptr;
    }

    std::unique_ptr<std::ostream> stream =
        std::make_unique<std::ofstream>(std::string(filename), std::ios::binary);
    if (!stream->good()) {
        return nullptr;
    }

    return std::unique_ptr<RawWriter>(new RawWriter(pictureDescription, std::move(stream)));
}

std::unique_ptr<RawWriter> createRawWriter(std::string_view filename)
{
    LCEVC_PictureDesc unknownDesc{};
    return createRawWriter(unknownDesc, filename);
}

std::unique_ptr<RawWriter> createRawWriter(const LCEVC_PictureDesc& description,
                                           std::unique_ptr<std::ostream> stream)
{
    if (!stream) {
        return nullptr;
    }

    if (!stream->good()) {
        return nullptr;
    }

    return std::unique_ptr<RawWriter>(new RawWriter(description, std::move(stream)));
}

} // namespace lcevc_dec::utility
