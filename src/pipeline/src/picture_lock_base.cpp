/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#include <LCEVC/pipeline/picture_lock_base.h>
//
#include <LCEVC/pipeline/picture_base.h>

namespace lcevc_dec::pipeline {

namespace {
    extern const LdpPictureLockFunctions kPictureLockFunctions;
}

PictureLockBase::PictureLockBase(PictureBase* src, LdpAccess access)
    : LdpPictureLock{&kPictureLockFunctions}
{
    this->picture = src;
    this->access = access;
}

PictureLockBase::~PictureLockBase() { assert(this->picture); }

bool PictureLockBase::getBufferDesc(LdpPictureBufferDesc* desc) const
{
    assert(desc);

    if (!desc) {
        return false;
    }

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

bool PictureLockBase::getPlaneDesc(uint32_t planeIndex, LdpPicturePlaneDesc* planeDescOut) const
{
    assert(planeIndex < kLdpPictureMaxColorComponents);
    assert(planeDescOut);

    if (!picture || !planeDescOut || planeIndex >= kLdpPictureMaxColorComponents) {
        return false;
    }

    static_cast<PictureBase*>(picture)->getPlaneDescInternal(planeIndex, *planeDescOut);
    return true;
}

// C function table to connect to C++ class
//
namespace {
    bool getBufferDesc(const LdpPictureLock* pictureLock, LdpPictureBufferDesc* desc)
    {
        if (!pictureLock || !desc) {
            return false;
        }
        return static_cast<const PictureLockBase*>(pictureLock)->getBufferDesc(desc);
    }

    bool getPlaneDesc(const LdpPictureLock* pictureLock, uint32_t planeIndex,
                      LdpPicturePlaneDesc planeDescOut[kLdpPictureMaxNumPlanes])
    {
        if (!pictureLock || !planeDescOut) {
            return false;
        }
        return static_cast<const PictureLockBase*>(pictureLock)->getPlaneDesc(planeIndex, planeDescOut);
    }

    const LdpPictureLockFunctions kPictureLockFunctions = {
        getBufferDesc,
        getPlaneDesc,
    };
} // namespace

} // namespace lcevc_dec::pipeline
