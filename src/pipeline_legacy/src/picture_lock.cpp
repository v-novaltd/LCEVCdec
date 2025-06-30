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

#include "picture_lock.h"
//
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/picture.h>
//
#include "picture.h"
//
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>

namespace lcevc_dec::decoder {

namespace {
    extern const LdpPictureLockFunctions kPictureLockFunctions;
}

PictureLock::PictureLock(Picture* src, LdpAccess access)
    : LdpPictureLock{&kPictureLockFunctions}
{
    this->picture = src;
    this->access = access;

    // Get buffer desc.
    if (LdpPictureBufferDesc bufferDesc; src->getBufferDesc(bufferDesc)) {
        m_bufferDesc = std::make_unique<LdpPictureBufferDesc>(bufferDesc);

        // Clear buffer on write
        // Maybe only do this on debug builds?
        if (access == LdpAccessWrite && m_bufferDesc->data != nullptr) {
            memset(m_bufferDesc->data, 0, m_bufferDesc->byteSize);
        }
    }

    // Get plane descs.
    std::array<LdpPicturePlaneDesc, kLdpPictureMaxNumPlanes> planeDescArr = {};
    if (src->getPlaneDescArr(planeDescArr.data())) {
        m_planeDescs =
            std::make_unique<std::array<LdpPicturePlaneDesc, kLdpPictureMaxNumPlanes>>(planeDescArr);

        // If we didn't get a buffer, but we need to clear this plane, then we have to do that here
        // Maybe only do this on debug builds?
        if (m_bufferDesc == nullptr && access == LdpAccessWrite) {
            for (uint32_t planeIdx = 0; planeIdx < ldpPictureLayoutPlanes(&src->layout); planeIdx++) {
                if ((*m_planeDescs)[planeIdx].firstSample != nullptr) {
                    memset((*m_planeDescs)[planeIdx].firstSample, 0,
                           ldpPictureLayoutPlaneSize(&src->layout, planeIdx));
                }
            }
        }
    }
}

PictureLock::~PictureLock() { assert(this->picture); }

// C function table to connect to C++ class
//
namespace {
    bool getBufferDesc(const LdpPictureLock* pictureLock, LdpPictureBufferDesc* desc)
    {
        return static_cast<const PictureLock*>(pictureLock)->getBufferDesc(desc);
    }

    bool getPlaneDesc(const LdpPictureLock* pictureLock, uint32_t planeIndex,
                      LdpPicturePlaneDesc planeDescOut[kLdpPictureMaxNumPlanes])
    {
        return static_cast<const PictureLock*>(pictureLock)->getPlaneDesc(planeIndex, planeDescOut);
    }

    const LdpPictureLockFunctions kPictureLockFunctions = {
        getBufferDesc,
        getPlaneDesc,
    };
} // namespace

} // namespace lcevc_dec::decoder
