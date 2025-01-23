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

#include "utils.h"

#include "LCEVC/utility/picture_layout.h"

#include <picture.h>

#include <memory>
#include <vector>

using namespace lcevc_dec::decoder;

EnhancementWithData getEnhancement(int64_t pts, const std::vector<uint8_t> kValidEnhancements[3])
{
    const size_t idx = pts % 3;
    const auto size = static_cast<uint32_t>(kValidEnhancements[idx].size());
    return EnhancementWithData(kValidEnhancements[idx].data(), size);
}

void setupPictureExternal(LCEVC_PictureBufferDesc& bufferDescOut, SmartBuffer& bufferOut,
                          LCEVC_PicturePlaneDesc planeDescArrOut[kMaxNumPlanes],
                          LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                          LCEVC_AccelBufferHandle accelBufferHandle, LCEVC_Access access)
{
    PictureLayout pictureLayout(format, width, height);
    // make the buffers
    bufferOut = std::make_shared<std::vector<uint8_t>>(pictureLayout.size());

    bufferDescOut = {
        bufferOut->data(),
        static_cast<uint32_t>(bufferOut->size()),
        accelBufferHandle,
        access,
    };

    uint8_t* curDataPtr = bufferOut->data();
    for (uint32_t planeIdx = 0; planeIdx < pictureLayout.planes(); planeIdx++) {
        planeDescArrOut[planeIdx] = {curDataPtr, pictureLayout.rowStride(planeIdx)};
        curDataPtr += pictureLayout.planeSize(planeIdx);
    }
}
