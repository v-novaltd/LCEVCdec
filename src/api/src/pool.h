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

#ifndef VN_API_POOL_H_
#define VN_API_POOL_H_

#include "handle.h"

#include <cassert>
#include <memory>
#include <vector>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {
// Pool class, to glue instances of objects to the handles that we output in the API. It is
// recommended that T be a parent-most class (i.e. have no parents), but that you allocate
// child- most classes in `allocate`.

// Also, avoid putting the function definitions in the source file. If you're on Windows, it's
// tempting (because it will compile), but it won't work on our Linux builds.

template <typename T>
class Pool
{
public:
    virtual ~Pool() = default;

    explicit Pool(size_t capacity)
    {
        // This guarantees that kInvalidHandle is always invalid (and why would you make a pool
        // with such a high capacity?)
        assert(capacity < handleIndex(kInvalidHandle));

        m_objects.resize(capacity);

        m_generations.resize(capacity);

        m_freeIndices.reserve(capacity);
        for (size_t i = 0; i < capacity; i++) {
            m_freeIndices.push_back(i);
        }
    }

    // Note that this does not allocate the memory, just the handle. This will actually move the
    // pointer itself, so if you've assigned the freshly-made pointer to some variable before
    // allocating it to a spot here, that variable will not generally be valid after calling
    // this function.
    virtual Handle<T> allocate(std::unique_ptr<T>&& ptrToT)
    {
        if (m_freeIndices.empty() || ptrToT == nullptr) {
            return kInvalidHandle;
        }

        const size_t idx = m_freeIndices.back();
        m_freeIndices.pop_back();

        // Bump generation and assert odd (because odd means "currently allocated").
        m_generations[idx]++;
        assert((m_generations[idx] & 1) == 1);

        const Handle<T> handleOut = handleMake(idx, m_generations[idx]);
        m_objects[idx] = std::move(ptrToT);

        return handleOut;
    }

    void release(Handle<T> handle)
    {
        if (!isValid(handle)) {
            assert(false);
            return;
        }
        const size_t idx = handleIndex(handle);

        // Bump generation and assert even (because even means "not allocated").
        m_generations[idx]++;
        assert((m_generations[idx] & 1) == 0);

        // Add back to free list
        m_freeIndices.push_back(idx);

        m_objects[idx].reset();
    }

    T* lookup(Handle<T> handle)
    {
        if (!isValid(handle)) {
            assert(false);
            return nullptr;
        }
        return m_objects[handleIndex(handle)].get();
    }

    const T* lookup(Handle<T> handle) const
    {
        if (!isValid(handle)) {
            assert(false);
            return nullptr;
        }
        return m_objects[handleIndex(handle)].get();
    }

    bool isValid(Handle<T> handle) const
    {
        const size_t index = handleIndex(handle);
        if (index >= m_generations.capacity()) {
            // Invalid index (out of range)
            return false;
        }

        if (m_generations[index] != handleGeneration(handle)) {
            // Stale generation (this includes handle==kInvalidHandle)
            return false;
        }

        return true;
    }

protected:
    static const size_t kGenerationBits = 16;

    static size_t handleIndex(Handle<T> handle) { return handle.handle >> kGenerationBits; }
    static uint16_t handleGeneration(Handle<T> handle)
    {
        return handle.handle & ((1 << kGenerationBits) - 1);
    }
    static Handle<T> handleMake(size_t index, size_t generation)
    {
        return (index << kGenerationBits) | generation;
    }

    // m_generations is essentially an array of counters, indicating how many times each of its
    // indices has been used (+1 for allocation AND +1 for release)
    std::vector<std::unique_ptr<T>> m_objects;
    std::vector<uint16_t> m_generations;
    std::vector<size_t> m_freeIndices;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_PICTURE_POOL_H_
