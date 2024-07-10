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

#include "picture_lock.h"

#include "memory.h"
#include "picture.h"

#include <LCEVC/lcevc_dec.h>

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
    std::array<PicturePlaneDesc, PictureLayout::kMaxPlanes> planeDescArr = {};
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
