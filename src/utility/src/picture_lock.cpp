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

#include <LCEVC/api_utility/picture_layout.h>
#include <LCEVC/utility/check.h>
#include <LCEVC/utility/picture_lock.h>

namespace lcevc_dec::utility {

PictureLock::PictureLock(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, LCEVC_Access access)
    : m_decoder(decoder)
    , m_picture(picture)
{
    VN_LCEVC_CHECK(LCEVC_LockPicture(decoder, picture, access, &m_lock));
    VN_LCEVC_CHECK(LCEVC_GetPictureDesc(decoder, picture, &m_desc));

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

uint32_t PictureLock::rowSize(uint32_t planeIdx) const
{
    LCEVC_PictureDesc desc = {};
    if (LCEVC_GetPictureDesc(m_decoder, m_picture, &desc) != LCEVC_Success) {
        return 0;
    }
    PictureLayout layout(desc);
    return layout.rowStride(planeIdx);
}

uint32_t PictureLock::height(uint32_t planeIdx) const
{
    LCEVC_PictureDesc desc = {};
    if (LCEVC_GetPictureDesc(m_decoder, m_picture, &desc) != LCEVC_Success) {
        return 0;
    }

    return (desc.height >> PictureLayout::getPlaneHeightShift(desc.colorFormat, planeIdx));
}

} // namespace lcevc_dec::utility
