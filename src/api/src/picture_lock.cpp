/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "picture_lock.h"

#include "picture.h"

#include <LCEVC/lcevc_dec.h>

using namespace lcevc_dec::api_utility;

namespace lcevc_dec::decoder {

PictureLock::PictureLock(Picture& src, Access access)
    : m_owner(src)
{
    // Get buffer desc.
    LCEVC_PictureBufferDesc bufferDesc;
    if (src.getBufferDesc(bufferDesc)) {
        PictureBufferDesc temp;
        fromLCEVCPictureBufferDesc(bufferDesc, temp);
        m_bufferDesc = std::make_unique<PictureBufferDesc>(temp);
        if (access == Access::Write && m_bufferDesc->data != nullptr) {
            memset(m_bufferDesc->data, 0, m_bufferDesc->byteSize);
        }
    }

    // Get plane descs.
    std::array<PicturePlaneDesc, PictureFormatDesc::kMaxNumPlanes> planeDescArr = {};
    if (src.getPlaneDescArr(planeDescArr.data())) {
        m_planeDescs = std::make_unique<std::array<PicturePlaneDesc, 4>>(planeDescArr);

        // If we didn't get a buffer, but we need to clear this plane, then we have to do that here
        if (m_bufferDesc == nullptr && access == Access::Write) {
            for (uint32_t planeIdx = 0; planeIdx < src.getNumPlanes(); planeIdx++) {
                if ((*m_planeDescs)[planeIdx].firstSample != nullptr) {
                    memset((*m_planeDescs)[planeIdx].firstSample, 0, src.getPlaneMemorySize(planeIdx));
                }
            }
        }
    }
}

bool PictureLock::unlock() const { return m_owner.unlock(); }

} // namespace lcevc_dec::decoder
