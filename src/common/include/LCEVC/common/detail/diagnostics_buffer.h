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

#ifndef VN_LCEVC_COMMON_DETAIL_DIAGNOSTICS_BUFFER_H
#define VN_LCEVC_COMMON_DETAIL_DIAGNOSTICS_BUFFER_H

#include <assert.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/threads.h>
#include <string.h>

// The guts of the diagnostics buffer implementation.
//
struct LdcDiagnosticsBuffer
{
    LdcDiagRecord* ring;   // The record ring
    uint32_t ringCapacity; // Number or records allocated for ring - should be a power of 2
    uint32_t ringMask;     // Mask to bring ring offset into range

    uint8_t* varData; // The variable record data buffer
    size_t varDataCapacity; // Number of bytes allocated for variable record data buffer - should be a power of 2
    size_t varDataMask; // Mask to bring offset with varData buffer

    size_t varNext; // Next offset to use for variable size data - NB: this is NOT wrapped within buffer, to allow detecting overrun

    uint32_t front; // Next slot to push record into
    uint32_t back;  // Next slot to pull record from

    ThreadMutex mutex;
    ThreadCondVar notEmpty;
    ThreadCondVar notFull;

    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation ringAllocation;
    LdcMemoryAllocation varDataAllocation;
};

// Compare 'close' uint32_t offsets - allow wrapping

static inline int compareOffsets(size_t lhs, size_t rhs)
{
    const intptr_t delta = (intptr_t)(lhs - rhs);
    if (delta < 0) {
        return -1;
    }
    if (delta > 0) {
        return 1;
    }
    return 0;
}

static inline void ldcDiagnosticsBufferPush(LdcDiagnosticsBuffer* buffer, const LdcDiagRecord* diagRecord,
                                            const uint8_t* varData, size_t varSize)
{
    threadMutexLock(&buffer->mutex);

    // Wait while buffer is full
    uint32_t next = 0;
    while ((next = (buffer->front + 1) & buffer->ringMask) == buffer->back) {
        threadCondVarWait(&buffer->notFull, &buffer->mutex);
    }

    // Signal consumer if buffer was empty
    // NB: this assumes a single consumer - which is true for diagnostics
    if (buffer->front == buffer->back) {
        threadCondVarSignal(&buffer->notEmpty);
    }

    // Add record to ring
    LdcDiagRecord* dest = &buffer->ring[buffer->front];

    *dest = *diagRecord;
    dest->size = (uint32_t)varSize;

    // Add optional variable data into the separate ring buffer - make sure the block is contiguous
    if (varData && varSize <= buffer->varDataCapacity) {
        // If there is not enough space at end of buffer - move on to next start
        if (buffer->varDataCapacity - (buffer->varNext & buffer->varDataMask) < varSize) {
            buffer->varNext = (buffer->varNext + buffer->varDataMask) & ~buffer->varDataMask;
        }

        dest->value.varDataOffset = buffer->varNext;
        memcpy(buffer->varData + (dest->value.varDataOffset & buffer->varDataMask), varData, varSize);
        // NB the variable data ring is NOT wrapped at this point - it is done
        // at pop time, to spot overwrites.
        buffer->varNext += varSize;
    }

    buffer->front = next;
    threadMutexUnlock(&buffer->mutex);
}

static inline bool ldcDiagnosticsBufferPop(LdcDiagnosticsBuffer* buffer, LdcDiagRecord* diagRecord,
                                           uint8_t* varData, size_t varMaxSize, size_t* varSize)
{
    threadMutexLock(&buffer->mutex);

    // Wait while buffer is empty
    while (buffer->front == buffer->back) {
        threadCondVarWait(&buffer->notEmpty, &buffer->mutex);
    }

    const LdcDiagRecord* src = &buffer->ring[buffer->back];
    *diagRecord = *src;
    buffer->back = (buffer->back + 1) & buffer->ringMask;

    size_t copySize = 0;

    if (varData && src->size && varMaxSize > 0) {
        // Did later records' variable data overrun this record?
        const size_t overrunOffset = (uint32_t)src->value.varDataOffset + buffer->varDataCapacity;
        if (compareOffsets(buffer->varNext, overrunOffset) <= 0) {
            // The data has not been overrun - copy data out into callers buffer
            copySize = (src->size < varMaxSize) ? src->size : varMaxSize;
            memcpy(varData, buffer->varData + (src->value.varDataOffset & buffer->varDataMask), copySize);
            if (varSize) {
                *varSize = copySize;
            }
        }
    }

    if (varSize) {
        *varSize = copySize;
    }

    const bool isEmpty = (buffer->front == buffer->back);

    // Signal producers
    threadCondVarSignal(&buffer->notFull);
    threadMutexUnlock(&buffer->mutex);

    return isEmpty;
}

static inline uint32_t ldcDiagnosticsBufferCapacity(const LdcDiagnosticsBuffer* buffer)
{
    return buffer->ringCapacity;
}

static inline uint32_t ldcDiagnosticsBufferSize(LdcDiagnosticsBuffer* buffer)
{
    threadMutexLock(&buffer->mutex);
    uint32_t size = (buffer->ringCapacity + buffer->front - buffer->back) & buffer->ringMask;
    threadMutexUnlock(&buffer->mutex);
    return size;
}

static inline bool ldcDiagnosticsBufferIsEmpty(LdcDiagnosticsBuffer* buffer)
{
    threadMutexLock(&buffer->mutex);
    bool isEmpty = buffer->front == buffer->back;
    threadMutexUnlock(&buffer->mutex);
    return isEmpty;
}

static inline bool ldcDiagnosticsBufferIsFull(LdcDiagnosticsBuffer* buffer)
{
    threadMutexLock(&buffer->mutex);
    bool isFull = ((buffer->front + 1) & buffer->ringMask) == buffer->back;
    threadMutexUnlock(&buffer->mutex);
    return isFull;
}

// Split push for use by inlined diagnostics that can write values straight into ring
//
static inline LdcDiagRecord* ldcDiagnosticsBufferPushBegin(LdcDiagnosticsBuffer* buffer, size_t varSize)
{
    assert(buffer);
    assert(varSize <= buffer->varDataCapacity);

    threadMutexLock(&buffer->mutex);

    // Wait while buffer is full
    uint32_t next = 0;
    while ((next = (buffer->front + 1) & buffer->ringMask) == buffer->back) {
        threadCondVarWait(&buffer->notFull, &buffer->mutex);
    }

    // Signal consumer if buffer was empty
    // NB: this assumes a single consumer - which is true for the diagnostics implementation
    if (buffer->front == buffer->back) {
        threadCondVarSignal(&buffer->notEmpty);
    }

    LdcDiagRecord* dest = &buffer->ring[buffer->front];
    buffer->front = next;

    // Add optional variable data into the separate ring buffer - make sure the block is contiguous
    if (varSize > 0) {
        // If there is not enough space at end of buffer - move on to next start
        if (buffer->varDataCapacity - (buffer->varNext & buffer->varDataMask) < varSize) {
            buffer->varNext = (buffer->varNext + buffer->varDataMask) & ~buffer->varDataMask;
        }

        dest->value.varDataOffset = buffer->varNext;
        dest->size = (uint32_t)varSize;

        // NB: the variable data ring offset is NOT wrapped at this point - it is done
        // at pop time, to spot overwrites.
        buffer->varNext += varSize;
    } else {
        // Mark record as having no data
        dest->size = 0;
    }

    return dest;
}

static inline uint8_t* ldcDiagnosticsBufferVarData(const LdcDiagnosticsBuffer* buffer,
                                                   const LdcDiagRecord* rec)
{
    assert(buffer);
    assert(buffer->varData);

    return buffer->varData + (rec->value.varDataOffset & buffer->varDataMask);
}

static inline void ldcDiagnosticsBufferPushEnd(LdcDiagnosticsBuffer* buffer)
{
    threadMutexUnlock(&buffer->mutex);
}

#endif // VN_LCEVC_COMMON_DETAIL_DIAGNOSTICS_BUFFER_H
