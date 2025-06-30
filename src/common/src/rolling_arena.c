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

#include <LCEVC/common/check.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/rolling_arena.h>
#include <LCEVC/common/threads.h>
//
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Minimum alignment of allocations in bytes
#define kMinAlignment 64

static const LdcMemoryAllocatorFunctions kRollingArenaFunctions;

static void rollingArenaDoubleSlots(LdcMemoryAllocatorRollingArena* arena)
{
    uint32_t newSlotsCount = arena->slotsCount * 2;
    assert(VNIsPowerOfTwo(newSlotsCount));

    arena->slots = VNReallocateArray(arena->parentAllocator, &arena->slotsAllocation,
                                     struct LdcRollingArenaSlot, newSlotsCount);
    VNCheck(arena->slots != NULL);

    if (arena->slotFront < arena->slotBack) {
        // Copy front of ring up into new memory and adjust front
        memcpy(arena->slots + arena->slotsCount, arena->slots,
               arena->slotFront * sizeof(struct LdcRollingArenaSlot));
        arena->slotFront += arena->slotsCount;
    }

    arena->slotsCount = newSlotsCount;
    arena->slotsMask = newSlotsCount - 1;
}

static void rollingArenaAddBuffer(LdcMemoryAllocatorRollingArena* arena, uint32_t bufferSize)
{
    assert(VNIsPowerOfTwo(bufferSize));

    // Pick next buffer
    VNCheck(arena->bufferCount < kRollingArenaMaxBuffers);
    struct LdcRollingArenaBuffer* buffer = &arena->buffers[arena->bufferCount];
    arena->bufferCount++;

    // Allocate the new memory buffer
    VNCheck(VNAllocateAlignedArray(arena->parentAllocator, &buffer->memory, uint8_t, kMinAlignment,
                                   bufferSize) != NULL);
    buffer->allocationCount = 0;

    // Adjust buffer state to refer to this new buffer
    arena->bufferSize = bufferSize;
    arena->bufferMask = bufferSize - 1;
    arena->bufferFront = 0;
    arena->bufferBack = 0;
}

LdcMemoryAllocator* ldcRollingArenaInitialize(LdcMemoryAllocatorRollingArena* rollingArenaAllocator,
                                              LdcMemoryAllocator* bufferCount,
                                              uint32_t initialCount, uint32_t initialSize)
{
    VNCheck(VNIsPowerOfTwo(initialCount));
    VNCheck(VNIsPowerOfTwo(initialSize));

    // Allocate the new arana
    LdcMemoryAllocatorRollingArena* arena = rollingArenaAllocator;

    if (arena == NULL) {
        return NULL;
    }

    // Configure the arena
    VNClear(arena);
    arena->allocator.functions = &kRollingArenaFunctions;
    arena->parentAllocator = bufferCount;
    VNCheck(threadMutexInitialize(&arena->mutex) == ThreadResultSuccess);

    // Initialise slot table
    arena->slotFront = 0;
    arena->slotBack = 0;
    arena->allocationIndexNext = 0;
    arena->allocationIndexOldest = 0;

    arena->slotsCount = initialCount;
    arena->slotsMask = initialCount - 1;

    arena->slots = VNAllocateArray(arena->parentAllocator, &arena->slotsAllocation,
                                   struct LdcRollingArenaSlot, arena->slotsCount);
    VNCheck(arena->slots != NULL);

    // Allocate the starting buffer
    arena->bufferCount = 0;
    rollingArenaAddBuffer(arena, initialSize);

    return &arena->allocator;
}

void ldcRollingArenaDestroy(LdcMemoryAllocatorRollingArena* arena)
{
    // Release buffers
    for (uint32_t buffer = 0; buffer < arena->bufferCount; ++buffer) {
        VNFree(arena->parentAllocator, &arena->buffers[buffer].memory);
    }

    // Release slots
    VNFree(arena->parentAllocator, &arena->slotsAllocation);
}

static inline void* internalAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                                     size_t size)
{
    LdcMemoryAllocatorRollingArena* arena = (LdcMemoryAllocatorRollingArena*)allocator;

    assert(size > 0);

    // Extra size for alignment
    uintptr_t align = (allocation->alignment > kMinAlignment) ? allocation->alignment : kMinAlignment;
    const size_t alignedSize = size + align - 1;

    // Get buffer region
    uint32_t oldBufferFront = 0;
    uint32_t offset = ~0;

    do {
        // Remember first point in buffer
        oldBufferFront = arena->bufferFront;

        // How many bytes left in current buffer?
        const uint32_t freeSize =
            arena->bufferSize -
            ((arena->bufferFront + arena->bufferSize - arena->bufferBack) & arena->bufferMask) - 1;

        // Might be able to fit into existing buffer ...
        if (freeSize >= alignedSize && (arena->bufferSize - arena->bufferFront) >= alignedSize) {
            // Requested buffer will fit in remaining part of buffer
            offset = arena->bufferFront;
            arena->bufferFront = (arena->bufferFront + alignedSize) & arena->bufferMask;
        } else if (freeSize >= alignedSize && arena->bufferBack >= alignedSize) {
            // Requested buffer can fit at start of buffer
            offset = 0;
            arena->bufferFront = (uint32_t)alignedSize;
        } else {
            // Add a new buffer with enough space to hold requested size and try again
            uint32_t nextSize = arena->bufferSize * 2;
            while (nextSize < freeSize + alignedSize) {
                nextSize *= 2;
            }
            rollingArenaAddBuffer(arena, nextSize);
        }
    } while (offset == ~0);

    // Get slot for this allocation
    if (((arena->slotFront + 1) & arena->slotsMask) == arena->slotBack) {
        // Run out of slots - reallocate slots table
        rollingArenaDoubleSlots(arena);
    }

    uint32_t allocationIndex = arena->allocationIndexNext;
    uint32_t slot = arena->slotFront;

    arena->allocationIndexNext++;
    arena->slotFront = (arena->slotFront + 1) & arena->slotsMask;

    // Fill in slot with range - NB: actual returned pointer and allocated size may not
    // match this due to alignment and wrapping
    arena->slots[slot].beginOffset = oldBufferFront;
    arena->slots[slot].endOffset = arena->bufferFront;
    arena->slots[slot].bufferIndex = arena->bufferCount - 1;

    // Mark active buffer as having another allocation
    arena->buffers[arena->bufferCount - 1].allocationCount++;

    // Align the pointer as required
    void* ptr = (uint8_t*)arena->buffers[arena->bufferCount - 1].memory.ptr + offset;
    ptr = (void*)(((uintptr_t)ptr + align - 1) & ~(align - 1));

    // Fill in allocation
    allocation->ptr = ptr;
    allocation->size = (uint32_t)size;
    allocation->allocatorData = allocationIndex;

    return ptr;
}

static inline void internalFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation)
{
    assert(allocator);
    assert(allocation);
    LdcMemoryAllocatorRollingArena* arena = (LdcMemoryAllocatorRollingArena*)allocator;

    // Get allocation's index and wrap it back into current slot ring buffer
    const uint32_t allocationIndex = (uint32_t)allocation->allocatorData;
    assert(allocationIndex >= arena->allocationIndexOldest);
    assert(allocationIndex < arena->allocationIndexNext);

    const uint32_t slot =
        (allocationIndex - arena->allocationIndexOldest + arena->slotBack) & arena->slotsMask;
    assert(slot < arena->slotsCount);

    // Get slot state and clear
    const uint32_t beginOffset = arena->slots[slot].beginOffset;
    const uint32_t buffer = arena->slots[slot].bufferIndex;

    // Mark slot as empty
    arena->slots[slot].beginOffset = arena->slots[slot].endOffset;

    if (buffer == arena->bufferCount - 1) {
        // Allocation is in active buffer ...
        if (beginOffset == arena->bufferBack) {
            // Oldest slot in active buffer  - bump buffer back over empty slots
            uint32_t s = slot;
            while (s != arena->slotFront && (arena->slots[s].beginOffset == arena->slots[s].endOffset)) {
                arena->bufferBack = arena->slots[s].endOffset;
                s = (s + 1) & arena->slotsMask;
            }
        }
    }

    if (slot == arena->slotBack) {
        // Oldest slot - bump back pointer over empty slots
        while (arena->slotBack != arena->slotFront &&
               (arena->slots[arena->slotBack].beginOffset == arena->slots[arena->slotBack].endOffset)) {
            arena->slotBack = (arena->slotBack + 1) & arena->slotsMask;
            arena->allocationIndexOldest++;
        }
    }

    // Drop count to containing buffer
    assert(buffer < kRollingArenaMaxBuffers);
    assert(arena->buffers[buffer].allocationCount > 0);
    arena->buffers[buffer].allocationCount--;

    if (arena->buffers[buffer].allocationCount == 0 && buffer != arena->bufferCount - 1) {
        // Buffer can be released
        VNFree(arena->parentAllocator, &arena->buffers[buffer].memory);
    }
}

static void* rollingArenaAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                                  size_t size, size_t alignment)
{
    assert(allocator);
    assert(allocation);
    LdcMemoryAllocatorRollingArena* arena = (LdcMemoryAllocatorRollingArena*)allocator;
    threadMutexLock(&arena->mutex);

    allocation->alignment = alignment;
    void* ptr = internalAllocate(allocator, allocation, size);

    threadMutexUnlock(&arena->mutex);
    return ptr;
}

static void rollingArenaFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation)
{
    assert(allocator);
    assert(allocation);
    LdcMemoryAllocatorRollingArena* arena = (LdcMemoryAllocatorRollingArena*)allocator;
    threadMutexLock(&arena->mutex);

    internalFree(allocator, allocation);

    threadMutexUnlock(&arena->mutex);
}

static void* rollingArenaReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                                    size_t size)
{
    LdcMemoryAllocatorRollingArena* arena = (LdcMemoryAllocatorRollingArena*)allocator;
    threadMutexLock(&arena->mutex);

    // Get slot number for this allocation
    const uint32_t allocationIndex = (uint32_t)allocation->allocatorData;
    assert(allocationIndex >= arena->allocationIndexOldest);
    assert(allocationIndex < arena->allocationIndexNext);
    const uint32_t slot =
        (allocationIndex - arena->allocationIndexOldest + arena->slotBack) & arena->slotsMask;
    assert(slot < arena->slotsCount);

    const uint32_t buffer = arena->slots[slot].bufferIndex;
    const uint32_t allocationOffset =
        (uint32_t)((uint8_t*)allocation->ptr - (uint8_t*)arena->buffers[buffer].memory.ptr);

    // How much space is available in current allocation?
    size_t currentSize = 0;
    if (arena->slots[slot].endOffset > arena->slots[slot].beginOffset) {
        currentSize = arena->slots[slot].endOffset - allocationOffset;
    } else {
        // Allocation wraps over buffer - only consider end to end of buffer
        currentSize = arena->buffers[buffer].memory.size - allocationOffset;
    }

    // Check requested size vs. buffer
    if (size <= currentSize) {
        // If block can fit in existing buffer (getting smaller, or next slot has been freed)
        // just increase allocation and run away
        allocation->size = size;
    } else {
        // Need to create another allocation
        //
        const size_t preservedSize = (size < allocation->size) ? size : (allocation->size);

        LdcMemoryAllocation newAllocation = {0};
        uint8_t* const newPtr = internalAllocate(allocator, &newAllocation, size);
        VNCheck(newPtr);
        // Copy old to new
        memcpy(newPtr, allocation->ptr, preservedSize);

        // Release old slot
        internalFree(allocator, allocation);

        // Update allocation
        *allocation = newAllocation;
    }

    void* const ptr = allocation->ptr;
    threadMutexUnlock(&arena->mutex);
    return ptr;
}

/* Memory Allocator function table
 */
static const LdcMemoryAllocatorFunctions kRollingArenaFunctions = {
    rollingArenaAllocate, rollingArenaReallocate, rollingArenaFree};
