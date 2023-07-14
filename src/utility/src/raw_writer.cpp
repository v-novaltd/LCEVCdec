// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Class for writing raw image files to streams or filesystem.
//
#include "LCEVC/utility/raw_writer.h"

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/check.h"
#include "LCEVC/utility/picture_layout.h"
#include "LCEVC/utility/picture_lock.h"
#include "string_utils.h"

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

    if(m_description.colorFormat == LCEVC_ColorFormat_Unknown) {
        // If the current description was unknown, initialize from first picture
        LCEVC_GetPictureDesc(decoder, picture, &m_description);
        m_layout = PictureLayout(m_description);
    } else {
        // Check incoming picture is compatible with current descriptions
        PictureLayout thisLayout(decoder, picture);
        if(!m_layout.isCompatible(thisLayout)) {
            return false;
        }
    }

   PictureLock lock(decoder, picture, LCEVC_Access_Read);

    for (unsigned plane = 0; plane < lock.numPlanes(); ++plane) {
        for (unsigned row = 0; row < lock.height(plane); ++row) {
            m_stream->write(lock.rowData<const char>(plane,row), lock.rowSize(plane));
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
    if(filename.empty()) {
        return nullptr;
    }

    std::unique_ptr<std::ostream> stream = std::make_unique<std::ofstream>(std::string(filename));
    if (!stream->good()) {
        return nullptr;
    }

    return std::unique_ptr<RawWriter>(new RawWriter(pictureDescription, std::move(stream)));
}

std::unique_ptr<RawWriter> createRawWriter(std::string_view filename)
{
    LCEVC_PictureDesc unknownDesc {};
    return createRawWriter(unknownDesc, filename);
}

std::unique_ptr<RawWriter> createRawWriter(const LCEVC_PictureDesc& description,
                                           std::unique_ptr<std::ostream> stream)
{
    if(!stream) {
        return nullptr;
    }

    if (!stream->good()) {
        return nullptr;
    }

    return std::unique_ptr<RawWriter>(new RawWriter(description, std::move(stream)));
}

} // namespace lcevc_dec::utility
