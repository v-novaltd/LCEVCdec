/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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

//
#include <LCEVC/common/check.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/recycling_allocator.h>
#include <LCEVC/common/threads.h>
#include <LCEVC/common/vector.h>
//
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const LdcMemoryAllocatorFunctions kRecyclingAllocatorFunctions;

LdcMemoryAllocator* ldcRecyclingAllocatorInitialize(ldcMemoryRecyclingAllocator* recyclingAllocator,
                                                    LdcMemoryAllocator* parentAllocator, uint32_t size)
{
    ldcMemoryRecyclingAllocator* const ralloc = recyclingAllocator;

    VNClear(ralloc);
    ralloc->allocator.functions = &kRecyclingAllocatorFunctions;
    ralloc->parentAllocator = parentAllocator;
    ralloc->size = size;

    //  Initialize the allocations cache
    ldcVectorInitialize(&ralloc->allocations, sizeof(LdcMemoryAllocation), size, parentAllocator);

    VNCheck(threadMutexInitialize(&ralloc->mutex) == ThreadResultSuccess);

    return &ralloc->allocator;
}

void ldcRecyclingAllocatorDestroy(ldcMemoryRecyclingAllocator* recyclingAllocator)
{
    assert(recyclingAllocator);
    ldcMemoryRecyclingAllocator* const ralloc = recyclingAllocator;
    threadMutexLock(&ralloc->mutex);

    // Free all cached blocks
    for (uint32_t idx = 0; idx < ldcVectorSize(&ralloc->allocations); ++idx) {
        LdcMemoryAllocation* const alloc = ldcVectorAt(&ralloc->allocations, idx);
        ralloc->parentAllocator->functions->free(ralloc->parentAllocator, alloc, NULL);
    }

    ldcVectorDestroy(&ralloc->allocations);

    threadMutexUnlock(&ralloc->mutex);
}

static void recyclingAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                              size_t size, size_t alignment, const LdcDiagSite* site)
{
    assert(allocator);
    assert(allocation);
    ldcMemoryRecyclingAllocator* const ralloc = (ldcMemoryRecyclingAllocator*)allocator;
    threadMutexLock(&ralloc->mutex);

    // Does the requested size exist in cache?
    uint32_t idx = 0;
    LdcMemoryAllocation* alloc = NULL;

    for (idx = 0; idx < ldcVectorSize(&ralloc->allocations); ++idx) {
        alloc = ldcVectorAt(&ralloc->allocations, idx);
        if (size == alloc->size) {
            break;
        }
        alloc = NULL;
    }

    if (alloc) {
        // Found one
        ralloc->reuseCount++;
        VNMetricUInt64("recyclerReuseCount", ralloc->reuseCount);
        *allocation = *alloc;
        ldcVectorRemove(&ralloc->allocations, alloc);
    } else {
        // Forward to parent allocator
        ralloc->allocationCount++;
        VNMetricUInt64("recyclerAllocationCount", ralloc->allocationCount);
        ralloc->parentAllocator->functions->allocate(ralloc->parentAllocator, allocation, size,
                                                     alignment, site);
    }

    threadMutexUnlock(&ralloc->mutex);
}

static void recyclingFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                          const LdcDiagSite* site)
{
    assert(allocator);
    assert(allocation);
    ldcMemoryRecyclingAllocator* const ralloc = (ldcMemoryRecyclingAllocator*)allocator;
    threadMutexLock(&ralloc->mutex);

    // Cache is full - free oldest
    if (ldcVectorSize(&ralloc->allocations) >= ralloc->size) {
        LdcMemoryAllocation* alloc = ldcVectorAt(&ralloc->allocations, 0);
        ralloc->parentAllocator->functions->free(ralloc->parentAllocator, alloc, site);
        ldcVectorRemove(&ralloc->allocations, alloc);
    }

    // Add block to cache
    ldcVectorAppend(&ralloc->allocations, allocation);

    threadMutexUnlock(&ralloc->mutex);
}

static void recyclingReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                                size_t size, const LdcDiagSite* site)
{
    LdcMemoryAllocation newAllocation = {0};
    if (size) {
        // Allocate the new buffer
        recyclingAllocate(allocator, &newAllocation, size, allocation->alignment, site);
    }

    if (allocation->size) {
        // Release old buffer
        recyclingFree(allocator, allocation, site);
    }

    //
    if (allocation->ptr && newAllocation.ptr) {
        memcpy(newAllocation.ptr, allocation->ptr, minU64(allocation->size, newAllocation.size));
    }
    *allocation = newAllocation;
}

/* Memory Allocator function table
 */
static const LdcMemoryAllocatorFunctions kRecyclingAllocatorFunctions = {
    recyclingAllocate, recyclingReallocate, recyclingFree};
