/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#ifndef VN_API_PICTURE_LOCK_H_
#define VN_API_PICTURE_LOCK_H_

#include "interface.h"
#include "LCEVC/utility/picture_layout.h"

#include <array>
#include <memory>

namespace lcevc_dec::decoder {

class Picture;

class PictureLock
{
    static constexpr uint8_t arrSize = lcevc_dec::utility::PictureLayout::kMaxNumPlanes;

public:
    PictureLock(Picture& src, Access access);
    ~PictureLock() { unlock(); }
    PictureLock& operator=(PictureLock&& other) = delete;
    PictureLock& operator=(const PictureLock& other) = delete;
    PictureLock(PictureLock&& moveSrc) = delete;
    PictureLock(const PictureLock& copySrc) = delete;

    const PictureBufferDesc* getBufferDesc() const { return m_bufferDesc.get(); }
    const std::array<PicturePlaneDesc, arrSize>* getPlaneDescArr() const
    {
        return m_planeDescs.get();
    }

protected:
    bool unlock() const;

    std::unique_ptr<PictureBufferDesc> m_bufferDesc = nullptr;
    std::unique_ptr<std::array<PicturePlaneDesc, arrSize>> m_planeDescs = nullptr;

    Picture& m_owner;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_PICTURE_LOCK_H_
