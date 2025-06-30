/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#ifndef VN_LCEVC_COMMON_DETAIL_RING_BUFFER_H
#define VN_LCEVC_COMMON_DETAIL_RING_BUFFER_H

#include <LCEVC/common/memory.h>

// The guts of the ring buffer implementation.
//
struct LdcRingBuffer
{
    uint8_t* data;     // The record ring
    uint32_t capacity; // Number or records allocated for ring - should be a power of 2  (actual capacity is 1 less to keep in != out)
    uint32_t mask;     // Mask to bring ring offset into range

    uint32_t elementSize; // Number or records allocated for ring - should be a power of 2

    uint32_t front; // Next slot to push record into
    uint32_t back;  // Current slot to pull record from

    ThreadMutex mutex;
    ThreadCondVar notEmpty;
    ThreadCondVar notFull;

    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation dataAllocation;
};

static inline void ldcRingBufferPush(LdcRingBuffer* ringBuffer, const void* element)
{
    threadMutexLock(&ringBuffer->mutex);

    // Wait while buffer is full
    uint32_t next = 0;
    while ((next = (ringBuffer->front + 1) & ringBuffer->mask) == ringBuffer->back) {
        threadCondVarWait(&ringBuffer->notFull, &ringBuffer->mutex);
    }
    // Signal consumer
    threadCondVarSignal(&ringBuffer->notEmpty);

    memcpy(ringBuffer->data + ringBuffer->front * ringBuffer->elementSize, element, ringBuffer->elementSize);
    ringBuffer->front = next;

    threadMutexUnlock(&ringBuffer->mutex);
}

static inline bool ldcRingBufferTryPush(LdcRingBuffer* ringBuffer, const void* element)
{
    threadMutexLock(&ringBuffer->mutex);

    // Wait while buffer is full
    uint32_t next = 0;
    if ((next = (ringBuffer->front + 1) & ringBuffer->mask) == ringBuffer->back) {
        threadMutexUnlock(&ringBuffer->mutex);
        return false;
    }
    // Signal consumer
    threadCondVarSignal(&ringBuffer->notEmpty);

    memcpy(ringBuffer->data + ringBuffer->front * ringBuffer->elementSize, element, ringBuffer->elementSize);
    ringBuffer->front = next;

    threadMutexUnlock(&ringBuffer->mutex);
    return true;
}

static inline void ldcRingBufferPop(LdcRingBuffer* ringBuffer, void* element)
{
    threadMutexLock(&ringBuffer->mutex);

    // Wait while buffer is empty
    while (ringBuffer->front == ringBuffer->back) {
        threadCondVarWait(&ringBuffer->notEmpty, &ringBuffer->mutex);
    }

    memcpy(element, ringBuffer->data + ringBuffer->back * ringBuffer->elementSize, ringBuffer->elementSize);
    ringBuffer->back = (ringBuffer->back + 1) & ringBuffer->mask;

    threadCondVarSignal(&ringBuffer->notFull);
    threadMutexUnlock(&ringBuffer->mutex);
}

static inline bool ldcRingBufferTryPop(LdcRingBuffer* ringBuffer, void* element)
{
    threadMutexLock(&ringBuffer->mutex);

    // Check if buffer is empty
    if (ringBuffer->front == ringBuffer->back) {
        threadMutexUnlock(&ringBuffer->mutex);
        return false;
    }

    memcpy(element, ringBuffer->data + ringBuffer->back * ringBuffer->elementSize, ringBuffer->elementSize);
    ringBuffer->back = (ringBuffer->back + 1) & ringBuffer->mask;

    threadCondVarSignal(&ringBuffer->notFull);
    threadMutexUnlock(&ringBuffer->mutex);

    return true;
}

static inline uint32_t ldcRingBufferCapacity(const LdcRingBuffer* buffer)
{
    // Actual capacity is 1 less than storage (in==out can't be distinguished)
    return buffer->capacity - 1;
}

static inline uint32_t ldcRingBufferSize(LdcRingBuffer* buffer)
{
    threadMutexLock(&buffer->mutex);
    uint32_t size = (buffer->capacity + buffer->front - buffer->back) & buffer->mask;
    threadMutexUnlock(&buffer->mutex);
    return size;
}

static inline bool ldcRingBufferIsEmpty(LdcRingBuffer* buffer)
{
    threadMutexLock(&buffer->mutex);
    bool isEmpty = buffer->front == buffer->back;
    threadMutexUnlock(&buffer->mutex);
    return isEmpty;
}

static inline bool ldcRingBufferIsFull(LdcRingBuffer* buffer)
{
    threadMutexLock(&buffer->mutex);
    bool isFull = ((buffer->front + 1) & buffer->mask) == buffer->back;
    threadMutexUnlock(&buffer->mutex);
    return isFull;
}

#endif // VN_LCEVC_COMMON_DETAIL_RING_BUFFER_H
