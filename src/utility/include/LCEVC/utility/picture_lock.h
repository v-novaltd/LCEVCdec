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

// Scoped type to manage a lock on an LCEVC_Picture
//
#ifndef VN_LCEVC_UTILITY_PICTURE_LOCK_H
#define VN_LCEVC_UTILITY_PICTURE_LOCK_H

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/picture_layout.h"

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

    // Get reference to particular plane description
    const LCEVC_PicturePlaneDesc& planeDesc(uint32_t plane) const
    {
        assert(plane < m_planeDescs.size());
        return m_planeDescs[plane];
    }

    // Get pointer to start of given row
    template <typename T = void>
    T* rowData(uint32_t plane, uint32_t row) const
    {
        assert(plane < m_planeDescs.size());
        assert(row < height(plane));
        auto layout = PictureLayout(m_desc);
        uint32_t topOffset = m_desc.cropTop >> layout.getPlaneWidthShift(layout.format(), plane);
        uint32_t leftOffset = (m_desc.cropLeft * layout.sampleSize() * layout.planeInterleave(plane)) >>
                              layout.getPlaneWidthShift(layout.format(), plane);

        return static_cast<T*>(static_cast<void*>(
            m_planeDescs[plane].firstSample +
            static_cast<size_t>((row + topOffset) * m_planeDescs[plane].rowByteStride + leftOffset)));
    }

    // Get row size in bytes of given plane
    uint32_t rowSize(uint32_t plane) const;
    uint32_t height(uint32_t plane) const;

    // Handle to locked picture
    LCEVC_PictureHandle picture() const { return m_picture; }

private:
    LCEVC_DecoderHandle m_decoder{};
    LCEVC_PictureHandle m_picture{};
    LCEVC_PictureDesc m_desc{};
    LCEVC_PictureLockHandle m_lock{};

    std::vector<LCEVC_PicturePlaneDesc> m_planeDescs;
};

} // namespace lcevc_dec::utility

#endif
