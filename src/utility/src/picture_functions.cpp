/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Functions for common Picture operations - read/write/dump.
//
#include <LCEVC/utility/picture_functions.h>
//
#include <fmt/core.h>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/utility/check.h>
#include <LCEVC/utility/picture_layout.h>
#include <LCEVC/utility/picture_lock.h>
#include <LCEVC/utility/raw_reader.h>
#include <LCEVC/utility/raw_writer.h>

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
    if (!rawWriter->write(decoder, picture)) {
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
    const std::ios_base::openmode mode{(std::ios_base::binary | std::ios_base::out) | dumpNames.count(fullName)
                                           ? std::ios_base::app
                                           : std::ios_base::trunc};

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

LCEVC_ReturnCode copyPictureFromMemory(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture,
                                       const uint8_t* data, uint32_t size)
{
    PictureLock lock(decoder, picture, LCEVC_Access_Write);
    const uint8_t* const limit = data + size;

    for (uint32_t plane = 0; plane < lock.numPlaneGroups(); ++plane) {
        const uint32_t rowSize = lock.rowSize(plane);
        const uint32_t height = lock.height(plane);

        if (data + static_cast<size_t>(height * rowSize) > limit) {
            return LCEVC_InvalidParam;
        }

        for (unsigned row = 0; row < lock.height(plane); ++row) {
            memcpy(lock.rowData<uint8_t>(plane, row), data, rowSize);
            data += rowSize;
        }
    }

    return LCEVC_Success;
}

LCEVC_ReturnCode copyPictureToMemory(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture,
                                     uint8_t* data, uint32_t size)
{
    PictureLock lock(decoder, picture, LCEVC_Access_Read);
    uint8_t* const limit = data + size;

    for (uint32_t plane = 0; plane < lock.numPlanes(); ++plane) {
        const uint32_t rowSize = lock.rowSize(plane);
        const uint32_t height = lock.height(plane);

        if (data + static_cast<size_t>(height * rowSize) > limit) {
            return LCEVC_InvalidParam;
        }

        for (unsigned row = 0; row < lock.height(plane); ++row) {
            memcpy(data, lock.rowData<char>(plane, row), rowSize);
            data += rowSize;
        }
    }

    return LCEVC_Success;
}

LCEVC_ReturnCode createPaddedDesc(const LCEVC_PictureDesc& srcDesc, const uint8_t* data,
                                  LCEVC_PictureBufferDesc* dstBufferDesc,
                                  LCEVC_PicturePlaneDesc* dstPlaneDesc)
{
    uint32_t rowStrides[PictureLayout::kMaxPlanes] = {0};
    if (!PictureLayout::getPaddedStrides(srcDesc, rowStrides)) {
        return LCEVC_Error;
    }
    PictureLayout baseLayout = PictureLayout(srcDesc);
    PictureLayout descLayout = PictureLayout(srcDesc, rowStrides);
    dstBufferDesc->data = new uint8_t[descLayout.size()];
    dstBufferDesc->byteSize = descLayout.size();

    for (uint32_t plane = 0; plane < baseLayout.planes(); plane++) {
        dstPlaneDesc[plane].firstSample = dstBufferDesc->data + descLayout.planeOffset(plane);
        dstPlaneDesc[plane].rowByteStride = rowStrides[plane];
    }
    const uint8_t* baseData = data;
    uint8_t* descData = dstBufferDesc->data;
    for (uint32_t plane = 0; plane < baseLayout.planeGroups(); plane++) {
        for (unsigned row = 0; row < descLayout.planeHeight(plane); ++row) {
            memcpy(descData, baseData, baseLayout.rowSize(plane));
            baseData += baseLayout.rowStride(plane);
            descData += descLayout.rowStride(plane);
        }
    }

    return LCEVC_Success;
}

} // namespace lcevc_dec::utility
