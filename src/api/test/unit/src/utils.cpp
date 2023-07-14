/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "utils.h"

#include <picture.h>

#include <memory>
#include <vector>

using namespace lcevc_dec::decoder;
using namespace lcevc_dec::utility;

void setupBuffers(std::vector<SmartBuffer>& buffersOut, LCEVC_ColorFormat format, uint32_t width,
                  uint32_t height)
{
    PictureFormatDesc fullFormat;
    const PictureFormat::Enum internalFormat = fromLCEVCDescColorFormat(format);
    const PictureInterleaving::Enum interleaving = fromLCEVCDescInterleaving(format);
    fullFormat.Initialise(internalFormat, width, height, interleaving);

    // make the buffers
    buffersOut.clear();
    buffersOut.reserve(fullFormat.GetPlaneCount());
    for (uint32_t plane = 0; plane < fullFormat.GetPlaneCount(); plane++) {
        buffersOut.push_back(std::make_shared<std::vector<uint8_t>>(fullFormat.GetPlaneMemorySize(plane)));
    }
}

void setupBufferDesc(LCEVC_PictureBufferDesc (&descArrOut)[kMaxNumPlanes],
                     const std::vector<SmartBuffer>& buffers,
                     LCEVC_AccelBufferHandle accelBufferHandle, LCEVC_Access access)
{
    for (uint32_t plane = 0; plane < buffers.size(); plane++) {
        descArrOut[plane] = {
            buffers[plane]->data(),
            static_cast<uint32_t>(buffers[plane]->size()),
            accelBufferHandle,
            access,
        };
    }
}

bool initPictureExternalAndBuffers(PictureExternal& out, std::vector<SmartBuffer>& buffersOut,
                                   LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                                   LCEVC_AccelBufferHandle accelBufferHandle, LCEVC_Access access)
{
    setupBuffers(buffersOut, format, width, height);

    LCEVC_PictureBufferDesc descArr[kMaxNumPlanes] = {};
    setupBufferDesc(descArr, buffersOut, accelBufferHandle, access);

    return out.bindMemoryBuffers(static_cast<uint32_t>(buffersOut.size()), descArr);
}
