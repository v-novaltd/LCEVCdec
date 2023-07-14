// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Functions for common Picture operations - read/write/dump.
//
#include <LCEVC/utility/picture_functions.h>
//
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/utility/check.h>
#include <LCEVC/utility/picture_layout.h>
#include <LCEVC/utility/raw_reader.h>
#include <LCEVC/utility/raw_writer.h>
#include <fmt/core.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <set>

namespace lcevc_dec::utility {

/* Read a picture from a file
 */
LCEVC_ReturnCode readPictureFromRaw(LCEVC_DecoderHandle decoder, std::string_view filename,
                                    LCEVC_PictureHandle& picture)
{
    auto rawReader = createRawReader(filename);
    if (!rawReader) {
        return LCEVC_Error;
    }

    VN_LCEVC_CHECK(LCEVC_AllocPicture(decoder, &rawReader->description(), &picture));

    if (!rawReader->read(decoder, picture)) {
        return LCEVC_Error;
    }

    return LCEVC_Success;
}

/* Write a picture to a file
 */
LCEVC_ReturnCode writePictureToRaw(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture,
                                   std::string_view filename)
{
    auto stream = std::make_unique<std::ofstream>(std::string(filename),
                                                  std::ios_base::out | std::ios_base::trunc);

    if (!stream->good()) {
        return LCEVC_Error;
    }

    LCEVC_PictureDesc description = {};
    VN_LCEVC_CHECK(LCEVC_GetPictureDesc(decoder, picture, &description));
    auto rawWriter = createRawWriter(description, std::move(stream));
    if(!rawWriter->write(decoder, picture)){
        return LCEVC_Error;
    }

    return LCEVC_Success;
}

/* Append a picture to a raw file, when dumping is enabled
 */
static bool enableDump = false; // NOLINT

void enableDumpPicture() { enableDump = true; }

void disableDumpPicture() { enableDump = false; }

void dumpPicture(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, std::string_view baseName)
{
    if (!enableDump) {
        return;
    }

    // Record of which dump files have been opened
    static std::set<std::string> dumpNames;

    // Generate full output name
    PictureLayout layout(decoder, picture);
    std::string fullName(layout.makeRawFilename(baseName));

    // Open file - append if it has been seen already, otherwise truncate
    const std::ios_base::openmode mode{dumpNames.count(fullName) ? (std::ios_base::out | std::ios_base::app)
                                                                 : (std::ios_base::out | std::ios_base::trunc)};
    dumpNames.insert(fullName);

    auto stream = std::make_unique<std::ofstream>(fullName, mode);
    if (!stream->good()) {
        return;
    }

    // Add picture to dump
    LCEVC_PictureDesc description = {};
    VN_LCEVC_CHECK(LCEVC_GetPictureDesc(decoder, picture, &description));
    auto rawWriter = createRawWriter(description, std::move(stream));
    rawWriter->write(decoder, picture);
}

} // namespace lcevc_dec::utility
