/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_BUFFER_MANAGER_H_
#define VN_API_BUFFER_MANAGER_H_

#include "uTypes.h"

#include <functional>
#include <memory>
#include <set>

// - Forward declarations -------------------------------------------------------------------------

namespace lcevc_dec::decoder {
using PictureBuffer = lcevc_dec::utility::DataBuffer;

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
