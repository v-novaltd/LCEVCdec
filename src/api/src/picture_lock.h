/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_PICTURE_LOCK_H_
#define VN_API_PICTURE_LOCK_H_

#include "interface.h"
#include "uPictureFormatDesc.h"

#include <array>

namespace lcevc_dec::decoder {

class Picture;

class PictureLock
{
    static constexpr uint8_t arrSize = lcevc_dec::utility::PictureFormatDesc::kMaxNumPlanes;

public:
    PictureLock(Picture& src, Access access);
    ~PictureLock() { unlock(); }
    PictureLock& operator=(PictureLock&& other) = delete;
    PictureLock& operator=(const PictureLock& other) = delete;
    PictureLock(PictureLock&& moveSrc) = delete;
    PictureLock(const PictureLock& copySrc) = delete;

    const PictureBufferDesc& getBufferDesc(size_t bufIdx) const { return m_bufferDescs[bufIdx]; }
    const PicturePlaneDesc& getPlaneDesc(size_t planeIdx) const { return m_planeDescs[planeIdx]; }

protected:
    bool unlock() const;

    std::array<PictureBufferDesc, arrSize> m_bufferDescs;
    std::array<PicturePlaneDesc, arrSize> m_planeDescs;

    Picture& m_owner;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_PICTURE_LOCK_H_