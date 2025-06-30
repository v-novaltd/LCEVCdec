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

#include <LCEVC/common/ring_buffer.h>
//
#include <LCEVC/common/check.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/threads.h>
//
#include <assert.h>

void ldcRingBufferInitialize(LdcRingBuffer* ringBuffer, uint32_t capacity, uint32_t elementSize,
                             LdcMemoryAllocator* allocator)
{
    assert(allocator);
    assert(VNIsPowerOfTwo(capacity));

    VNClear(ringBuffer);

    VNAllocateArray(allocator, &ringBuffer->dataAllocation, uint8_t, capacity * elementSize);
    VNCheck(VNAllocationSucceeded(ringBuffer->dataAllocation));

    ringBuffer->data = VNAllocationPtr(ringBuffer->dataAllocation, uint8_t);
    ringBuffer->capacity = capacity;
    ringBuffer->mask = capacity - 1;
    ringBuffer->elementSize = elementSize;

    VNCheck(threadMutexInitialize(&ringBuffer->mutex) == ThreadResultSuccess);
    VNCheck(threadCondVarInitialize(&ringBuffer->notEmpty) == ThreadResultSuccess);
    VNCheck(threadCondVarInitialize(&ringBuffer->notFull) == ThreadResultSuccess);

    ringBuffer->allocator = allocator;
}

void ldcRingBufferDestroy(LdcRingBuffer* buffer)
{
    VNFree(buffer->allocator, &buffer->dataAllocation);

    threadCondVarDestroy(&buffer->notEmpty);
    threadCondVarDestroy(&buffer->notFull);
    threadMutexDestroy(&buffer->mutex);
}
