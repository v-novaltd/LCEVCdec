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

#include <LCEVC/common/deque.h>
//
#include <LCEVC/common/check.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/memory.h>
//
#include <assert.h>
#include <stdio.h>

void ldcDequeInitialize(LdcDeque* deque, uint32_t reserved, uint32_t elementSize, LdcMemoryAllocator* allocator)
{
    assert(allocator);

    reserved = nextPowerOfTwoU32(reserved);

    VNClear(deque);
    VNAllocateArray(allocator, &deque->dataAllocation, uint8_t, reserved * elementSize);
    VNCheck(VNAllocationSucceeded(deque->dataAllocation));

    deque->data = VNAllocationPtr(deque->dataAllocation, uint8_t);
    deque->reserved = reserved;
    deque->mask = reserved - 1;
    deque->elementSize = elementSize;

    deque->allocator = allocator;
}

void ldcDequeDestroy(LdcDeque* deque)
{
    //
    VNFree(deque->allocator, &deque->dataAllocation);
}

// Double capacity of the deque
void ldcDequeGrow(LdcDeque* deque)
{
    assert(deque);
    assert(VNIsPowerOfTwo(deque->reserved));
    assert(VNIsAllocated(deque->dataAllocation));

    // Bump reservation size
    const uint32_t prevReserved = deque->reserved;
    deque->reserved *= 2;
    deque->mask = deque->reserved - 1;

    // Did it wrap?
    VNCheck(deque->reserved > prevReserved);

    // Reallocate the memory
    VNReallocateArray(deque->allocator, &deque->dataAllocation, uint8_t,
                      deque->reserved * deque->elementSize);
    VNCheck(VNAllocationSucceeded(deque->dataAllocation));
    deque->data = VNAllocationPtr(deque->dataAllocation, uint8_t);

    if (deque->front <= deque->back) {
        // elements are not wrapped over end of buffer
        return;
    }

    // The previous entries were wrapped around end of buffer, move them up to new end
    memmove(deque->data + (deque->front + prevReserved) * deque->elementSize,
            deque->data + (deque->front) * deque->elementSize,
            (prevReserved - deque->front) * deque->elementSize);
    deque->front += prevReserved;
}
