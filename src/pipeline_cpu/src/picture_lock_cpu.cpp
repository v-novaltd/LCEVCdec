/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include "picture_lock_cpu.h"
//
#include "picture_cpu.h"

namespace lcevc_dec::pipeline_cpu {

namespace {
    extern const LdpPictureLockFunctions kPictureLockFunctions;
}

PictureLock::PictureLock(PictureCPU* src, LdpAccess access)
    : LdpPictureLock{&kPictureLockFunctions}
{
    this->picture = src;
    this->access = access;
}

PictureLock::~PictureLock() { assert(this->picture); }

bool PictureLock::getBufferDesc(LdpPictureBufferDesc* desc) const
{
    assert(desc);

    if (mapping.ptr == nullptr) {
        return false;
    }

    // Transcribe stuff from mapping into a LdpPictureBufferDesc
    desc->data = mapping.ptr + mapping.offset;
    desc->byteSize = mapping.size;
    desc->access = access;
    desc->accelBuffer = nullptr;

    return true;
}

bool PictureLock::getPlaneDesc(uint32_t planeIndex, LdpPicturePlaneDesc* planeDescOut) const
{
    assert(planeIndex < kLdpPictureMaxColorComponents);
    assert(planeDescOut);

    if (planeIndex >= kLdpPictureMaxColorComponents) {
        return false;
    }

    static_cast<PictureCPU*>(picture)->getPlaneDescInternal(planeIndex, *planeDescOut);
    return true;
}

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

} // namespace lcevc_dec::pipeline_cpu
