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

#ifndef VN_API_BUFFER_MANAGER_H_
#define VN_API_BUFFER_MANAGER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <set>

// - Forward declarations -------------------------------------------------------------------------

namespace lcevc_dec::decoder {
using PictureBuffer = std::vector<uint8_t>;

// - Helper types ------------------------------------------------------------------------------------

// Buffers are large, so we hang onto the allocated memory in a set of pointers. To use that set,
// we need to define a custom comparison operator between shared pointers and raw pointers.
template <typename T>
struct PtrCompare
{
    // This is solely to allow it to be used as a comparison operator in std::set:
    using is_transparent = void; // NOLINT(readability-identifier-naming)

    template <typename SmartPtr>
    bool operator()(const SmartPtr& lhs, T* rhs) const
    {
        return lhs.get() < rhs;
    }

    template <typename SmartPtr>
    bool operator()(T* lhs, const SmartPtr& rhs) const
    {
        return lhs < rhs.get();
    }

    template <typename SmartPtr>
    bool operator()(const SmartPtr& lhs, const SmartPtr& rhs) const
    {
        return lhs.get() < rhs.get();
    }
};
using BufSet = std::set<std::shared_ptr<PictureBuffer>, PtrCompare<PictureBuffer>>;

// - BufferManager ----------------------------------------------------------------------------------

// BufferManager manages Buffers in a "free" and a "busy" set.
class BufferManager
{
public:
    BufferManager() = default;
    ~BufferManager() { release(); }
    BufferManager(const BufferManager&) = delete;
    BufferManager(BufferManager&&) = delete;
    BufferManager& operator=(const BufferManager&) = delete;
    BufferManager& operator=(BufferManager&&) = delete;

    // Releases all stored buffers.
    void release();

    PictureBuffer* getBuffer(size_t requiredSize);
    bool releaseBuffer(PictureBuffer* buffer);

private:
    BufSet m_buffersFree;
    BufSet m_buffersBusy;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_BUFFER_MANAGER_H_
