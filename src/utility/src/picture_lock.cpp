// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/picture_lock.h"

#include "LCEVC/utility/check.h"
#include "LCEVC/utility/picture_layout.h"

namespace lcevc_dec::utility {

PictureLock::PictureLock(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, LCEVC_Access access)
    : m_decoder(decoder)
    , m_picture(picture)
{
    VN_LCEVC_CHECK(LCEVC_LockPicture(decoder, picture, access, &m_lock));

    uint32_t numPlanes = 0;

    VN_LCEVC_CHECK(LCEVC_GetPicturePlaneCount(decoder, picture, &numPlanes));
    m_planeDescs.resize(numPlanes);

    for (uint32_t plane = 0; plane < numPlanes; ++plane) {
        VN_LCEVC_CHECK(LCEVC_GetPictureLockPlaneDesc(decoder, m_lock, plane, &m_planeDescs[plane]));
    }
}


void PictureLock::unlock()
{
    if (m_lock.hdl) {
        VN_LCEVC_CHECK(LCEVC_UnlockPicture(m_decoder, m_lock));
    }

    m_decoder.hdl = {0};
    m_picture.hdl = {0};
    m_lock.hdl = {0};
    m_planeDescs.clear();
}

} // namespace lcevc_dec::utility
