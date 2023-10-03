/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_PICTURE_LOCK_H_
#define VN_API_PICTURE_LOCK_H_

#include "interface.h"
#include "uPictureFormatDesc.h"

#include <array>
#include <memory>

namespace lcevc_dec::decoder {

class Picture;

class PictureLock
{
    static constexpr uint8_t arrSize = lcevc_dec::api_utility::PictureFormatDesc::kMaxNumPlanes;

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