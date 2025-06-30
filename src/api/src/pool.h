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

#ifndef VN_LCEVC_API_POOL_H
#define VN_LCEVC_API_POOL_H

#include "handle.h"
//
#include <LCEVC/common/class_utils.hpp>
//
#include <cassert>
#include <memory>
#include <vector>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {
// Pool class, to glue instances of objects to the handles that we output in the API. It is
// recommended that T be a parent-most class (i.e. have no parents), but that you allocate
// child-most classes in `allocate`.

template <typename T>
class Pool
{
public:
    ~Pool() = default;

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

    Handle<T> add(T* ptrToT)
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

    Handle<T> add(std::unique_ptr<T>&& ptrToT) { return add(ptrToT.release()); }

    T* remove(Handle<T> handle)
    {
        if (!isValid(handle)) {
            assert(false);
            return nullptr;
        }
        const size_t idx = handleIndex(handle);

        // Bump generation and assert even (because even means "not allocated").
        m_generations[idx]++;
        assert((m_generations[idx] & 1) == 0);

        // Add back to free list
        m_freeIndices.push_back(idx);

        T* const ret = std::move(m_objects[idx]);
        m_objects[idx] = nullptr;
        return ret;
    }

    T* lookup(Handle<T> handle)
    {
        if (!isValid(handle)) {
            assert(false);
            return nullptr;
        }
        return m_objects[handleIndex(handle)];
    }

    const T* lookup(Handle<T> handle) const
    {
        if (!isValid(handle)) {
            assert(false);
            return nullptr;
        }
        return m_objects[handleIndex(handle)];
    }

    Handle<T> reverseLookup(const T* ptr) const
    {
        if (ptr == nullptr) {
            return Handle<T>{kInvalidHandle};
        }

        // Look for native pointer in pool
        for (size_t idx = 0; idx < m_objects.size(); ++idx) {
            if (m_objects[idx] == ptr) {
                return handleMake(idx, m_generations[idx]);
            }
        }

        // Not found
        return Handle<T>{kInvalidHandle};
    }

    bool isValid(Handle<T> handle) const
    {
        const size_t index = handleIndex(handle);
        if (index >= m_generations.size()) {
            // Invalid index (out of range)
            return false;
        }

        if (m_generations[index] != handleGeneration(handle)) {
            // Stale generation (this includes handle==kInvalidHandle)
            return false;
        }

        return true;
    }

    size_t size() const { return m_objects.size() - m_freeIndices.size(); }

    Handle<T> at(size_t num) const
    {
        // Find the nth set handle
        for (size_t idx = 0; idx < m_objects.size(); ++idx) {
            if ((m_generations[idx] & 1) == 1) {
                if (num == 0) {
                    return handleMake(idx, m_generations[idx]);
                }
                --num;
            }
        }
        return Handle<T>{kInvalidHandle};
    }

    VNNoCopyNoMove(Pool);

private:
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
    std::vector<T*> m_objects;
    std::vector<uint16_t> m_generations;
    std::vector<size_t> m_freeIndices;
};

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_API_POOL_H
