// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Scoped type to manage a lock on an LCEVC_Picture
//
#ifndef VN_LCEVC_UTILITY_PICTURE_LOCK_H
#define VN_LCEVC_UTILITY_PICTURE_LOCK_H

#include "LCEVC/lcevc_dec.h"

#include <cassert>
#include <memory>
#include <vector>

namespace lcevc_dec::utility {

/*!
 * \brief Scoped management of picture lock
 */
class PictureLock
{
public:
    // Construct a lock from a picture
    PictureLock(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, LCEVC_Access access);
    ~PictureLock() { unlock(); }

    // Can be moved but not copied
    PictureLock(PictureLock&&) = default;
    PictureLock& operator=(PictureLock&&) = default;
    PictureLock(const PictureLock&) = delete;
    PictureLock& operator=(const PictureLock&) = delete;

    // Release the lock
    void unlock();

    // Number of planes in image
    uint32_t numPlanes() const { return static_cast<uint32_t>(m_planeDescs.size()); }

    // Get refernece to particular plane description
    const LCEVC_PicturePlaneDesc& planeDesc(uint32_t plane) const
    {
        assert(plane < m_planeDescs.size());
        return m_planeDescs[plane];
    }

    // Get pointer to start of given row
    template <typename T=void>
    T *rowData(uint32_t plane, uint32_t row) const {
        assert(plane < m_planeDescs.size());
        assert(row < m_planeDescs[plane].height);

        return static_cast<T *>(static_cast<void *>(m_planeDescs[plane].firstSample + static_cast<size_t>(row * m_planeDescs[plane].rowByteStride)));
    }

    // Get row size in bytes of given plane
    uint32_t rowSize(uint32_t plane) const {
        assert(plane < m_planeDescs.size());

        return m_planeDescs[plane].rowByteSize;
    }

    uint32_t height(uint32_t plane) const {
        return m_planeDescs[plane].height;
    }

    // Handle to locked picture
    LCEVC_PictureHandle picture() const { return m_picture; }

private:
    LCEVC_DecoderHandle m_decoder{};
    LCEVC_PictureHandle m_picture{};
    LCEVC_PictureLockHandle m_lock{};

    std::vector<LCEVC_PicturePlaneDesc> m_planeDescs;
};

} // namespace lcevc_dec::utility

#endif
