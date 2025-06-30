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

#ifndef VN_LCEVC_COMMON_ROLLING_ARENA_H
#define VN_LCEVC_COMMON_ROLLING_ARENA_H

#include <LCEVC/common/memory.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! @file
 * @brief A memory arena that implements the MemoryAllocator API
 *
 * Thread safe
 *
 * A memory allocator that is optimized to support a sequence of roughly FIFO memory allocations.
 *
 * The allocator is backed by a contiguous chunk of memory allocated from another system, typically the C runtime.
 *
 * The chunk is treated as a ring buffer.
 * New allocations come from the bumping the head of chunk ring buffer. The buffer is wrapped to make sure
 * each allocation is contiguous.
 *
 * As allocations are free the tail of the buffer is moved up as far as possible.
 *
 * Reallocations on the most recent allocation can be very fast - this works well with a pattern of:
 * "allocate a large output buffer, do work, reallocate to trim to actual output size".
 *
 * If there is no space for an allocation - a new larger chunk is allocated. Previous chunks will
 * be released when all their allocations have been freed. This is not great for general allocation,
 * but works well with the broadly FIFO workload this is designed for.
 *
 * The intended use is for the intermediate data of the pipeline - unencapsulated bytes, command buffers, etc.
 * that will only live for the pipeline latency, and are roughly consumed in order of production.
 */

typedef struct LdcMemoryAllocatorRollingArena LdcMemoryAllocatorRollingArena;

/*! Create a new area with an initial size.
 *
 * @param[out]      rollingArenaAllocator   The allocator to be initialized.
 * @param[in]       parentAllocator         The underlying allocator to allocate blocks from.
 * @param[in]       initialCount            Starting number of allocation slots.
 * @param[in]       initialSize             Starting size of arena in bytes of arena.
 *
 * @return          A pointer to an allocator - as passed in via `rollingArenaAllocator`
 */
LdcMemoryAllocator* ldcRollingArenaInitialize(LdcMemoryAllocatorRollingArena* rollingArenaAllocator,
                                              LdcMemoryAllocator* parentAllocator,
                                              uint32_t initialCount, uint32_t initialSize);

/*! Destroy a previously initialized rolling arena.
 */
void ldcRollingArenaDestroy(LdcMemoryAllocatorRollingArena* arena);

// Specializations of the memory macros that can be adjusted later to call directly into the arena code (which can then be inlined)

/**! Helper for performing malloc for a single object. */
#define VNRollingArenaAllocate(allocator, allocation, type) VNAllocate(allocator, allocation, type)

/**! Helper for performing malloc for an array of objects. */
#define VNRollingArenaAllocateArray(allocator, allocation, type, count) \
    VNAllocateArray(allocator, allocation, type, count)

/**! Helper for performing realloc for a single object. */
#define VNRollingArenaReallocate(allocator, allocation, type) \
    VNReallocate(allocator, allocation, type)

/**! Helper for performing realloc for an array of objects. */
#define VNRollingArenaReallocateArray(allocator, allocation, type, prevCount, count) \
    VNReallocateArray(allocator, allocation, type, count)

/**! Helper for freeing an allocation performed with one of the above macros. */
#define VNRollingArenaFree(allocator, allocation) VNRollingArenaFree(allocator, allocation)

// Allocator definition and inline fast paths
//
#include "detail/rolling_arena.h"

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_ROLLING_ARENA_H
