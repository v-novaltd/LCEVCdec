/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "picture_lock.h"

#include "picture.h"

#include <LCEVC/lcevc_dec.h>

namespace lcevc_dec::decoder {

PictureLock::PictureLock(Picture& src, Access access)
    : m_owner(src)
{
    // Get buffer descs.
    for (uint32_t bufIdx = 0; bufIdx < src.getBufferCount(); bufIdx++) {
        // Save some effort by reusing the external-facing buffer-getter
        LCEVC_PictureBufferDesc desc;
        src.getBufferDesc(bufIdx, desc);
        fromLCEVCPictureBufferDesc(desc, m_bufferDescs[bufIdx]);
        if (access == Access::Write && m_bufferDescs[bufIdx].data != nullptr) {
            memset(m_bufferDescs[bufIdx].data, 0, m_bufferDescs[bufIdx].byteSize);
        }
    }

    // Get plane descs.
    for (uint32_t planeIdx = 0; planeIdx < src.getNumPlanes(); planeIdx++) {
        m_planeDescs[planeIdx].firstSample = src.getPlaneFirstSample(planeIdx);
        m_planeDescs[planeIdx].rowByteStride = src.getPlaneByteStride(planeIdx);
        m_planeDescs[planeIdx].sampleByteStride = src.getPlaneBytesPerPixel(planeIdx);
        m_planeDescs[planeIdx].sampleByteSize = src.getBytedepth();
        m_planeDescs[planeIdx].rowByteSize = src.getPlaneWidthBytes(planeIdx);
        m_planeDescs[planeIdx].width = src.getPlaneWidth(planeIdx);
        m_planeDescs[planeIdx].height = src.getPlaneHeight(planeIdx);

        if (access == Access::Write) {
            memset(m_planeDescs[planeIdx].firstSample, 0, src.getPlaneMemorySize(planeIdx));
        }
    }
}

bool PictureLock::unlock() const { return m_owner.unlock(); }

} // namespace lcevc_dec::decoder
