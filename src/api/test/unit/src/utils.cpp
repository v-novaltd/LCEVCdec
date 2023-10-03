/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "utils.h"

#include <picture.h>

#include <memory>
#include <vector>

using namespace lcevc_dec::decoder;
using namespace lcevc_dec::api_utility;

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
    PictureFormatDesc fullFormat;
    const PictureFormat::Enum internalFormat = fromLCEVCDescColorFormat(format);
    const PictureInterleaving::Enum interleaving = fromLCEVCDescInterleaving(format);
    fullFormat.Initialise(internalFormat, width, height, interleaving);

    // make the buffers
    bufferOut = std::make_shared<std::vector<uint8_t>>(fullFormat.GetMemorySize());

    bufferDescOut = {
        bufferOut->data(),
        static_cast<uint32_t>(bufferOut->size()),
        accelBufferHandle,
        access,
    };

    uint8_t* curDataPtr = bufferOut->data();
    for (uint32_t planeIdx = 0; planeIdx < fullFormat.GetPlaneCount(); planeIdx++) {
        planeDescArrOut[planeIdx] = {curDataPtr, fullFormat.GetPlaneStrideBytes(planeIdx)};
        curDataPtr += fullFormat.GetPlaneMemorySize(planeIdx);
    }
}
