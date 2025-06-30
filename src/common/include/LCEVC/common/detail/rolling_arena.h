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

#ifndef VN_LCEVC_COMMON_DETAIL_ROLLING_ARENA_H
#define VN_LCEVC_COMMON_DETAIL_ROLLING_ARENA_H

#include <LCEVC/common/memory.h>
#include <LCEVC/common/threads.h>
#include <stdint.h>

// Number of reallocation buffers that can be pending
//
#define kRollingArenaMaxBuffers 16

struct LdcRollingArenaSlot
{
    uint32_t beginOffset; // First offset in chunk covered by slot - NB: returned pointer may be further along to account for alignment and wrapping
    uint32_t endOffset; // Where this chunk ends (exclusive) - the start of any next allocated chunk
    uint32_t bufferIndex; // The containing buffer
};

struct LdcRollingArenaBuffer
{
    LdcMemoryAllocation memory; // The actual block of memory
    uint32_t allocationCount;   // Count of remaining allocations in block
};

struct LdcMemoryAllocatorRollingArena
{
    LdcMemoryAllocator allocator;

    // Allocator is thread safe
    ThreadMutex mutex;

    // Where to allocate chunks from
    LdcMemoryAllocator* parentAllocator;

    // Incrementing index for allocations
    uint32_t allocationIndexNext;
    // Oldest live allocation index
    uint32_t allocationIndexOldest;

    // The allocations between Oldest and Next are mapped
    // to a ring buffer of slots, with details of the allocation
    struct LdcRollingArenaSlot* slots;

    // Number of slots in allocation ring
    uint32_t slotsCount;
    // Bitmask to wrap indices in ring
    uint32_t slotsMask;

    // Current front and back slot in slot ring corresponding to Next and Oldest
    uint32_t slotFront;
    uint32_t slotBack;

    // The array slots
    LdcMemoryAllocation slotsAllocation;

    // The memory for each allocation comes from a big 'ArenaBuffer' ring buffer
    // If the current buffer gets exhausted, a new bigger one is allocated.
    //
    uint32_t bufferSize;
    uint32_t bufferMask;

    // Current head and tail buffer offsets
    uint32_t bufferFront;
    uint32_t bufferBack;

    // Buffers
    struct LdcRollingArenaBuffer buffers[kRollingArenaMaxBuffers];

    uint32_t bufferCount;
};

#endif // VN_LCEVC_COMMON_DETAIL_ROLLING_ARENA_H
