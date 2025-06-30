/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova International Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_LCEVC_COMMON_RING_BUFFER_HPP
#define VN_LCEVC_COMMON_RING_BUFFER_HPP

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/ring_buffer.h>
#include <LCEVC/common/memory.h>

namespace lcevc_dec::common {

// Type templated C++ wrapper for the LdcRingBuffer
//
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(uint32_t capacity, LdcMemoryAllocator* allocator) {
        ldcRingBufferInitialize(&m_ringBuffer, capacity, sizeof(T), allocator);
    }
    explicit RingBuffer(uint32_t capacity) {
        ldcRingBufferInitialize(&m_ringBuffer, capacity, sizeof(T), ldcMemoryAllocatorMalloc());
    }
    ~RingBuffer() {
        ldcRingBufferDestroy(&m_ringBuffer);
    }

    void push(const T& element) { ldcRingBufferPush(&m_ringBuffer, &element); }
    bool tryPush(const T& element) { return ldcRingBufferTryPush(&m_ringBuffer, &element); }
    void pop(T& element) { ldcRingBufferPop(&m_ringBuffer, &element); }
    bool tryPop(T& element) { return ldcRingBufferTryPop(&m_ringBuffer, &element); }

    uint32_t capacity() const { return ldcRingBufferCapacity(&m_ringBuffer); }
    uint32_t size() const { return ldcRingBufferSize(&m_ringBuffer); }
    bool isEmpty() const { return ldcRingBufferIsEmpty(&m_ringBuffer); }

    VNNoCopyNoMove(RingBuffer);
private:
    // this contains a mutex, so make it mutable to make the query functions appear const.
    mutable LdcRingBuffer m_ringBuffer{};
};

} // namespace lcevc_dec::common

#endif // VN_LCEVC_COMMON_RING_BUFFER_H
