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

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
//
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    LdcMemoryAllocator allocator;
    atomic_size_t allocatedBytes;
    atomic_uint_fast32_t allocations;
    atomic_uint_fast32_t totalAllocations;
    atomic_uint_fast32_t totalReallocations;
} LdcMemoryAllocatorMalloc;

static LdcMemoryAllocatorMalloc mallocMemoryAllocator; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

LdcMemoryAllocator* ldcMemoryAllocatorMalloc(void) { return &mallocMemoryAllocator.allocator; }

static void* mallocAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                            size_t size, size_t alignment)
{
    LdcMemoryAllocatorMalloc* ma = (LdcMemoryAllocatorMalloc*)allocator;

    void* ptr = NULL;
    if (alignment > sizeof(void*)) {
#if VN_OS(WINDOWS)
        ptr = _aligned_malloc(size, alignment);
#elif VN_OS(ANDROID) || VN_OS(APPLE)
        int r = posix_memalign(&ptr, alignment, size);
        if (r != 0) {
            return NULL;
        }
#else
        ptr = aligned_alloc(alignment, size);
#endif
    } else {
        ptr = malloc(size);
    }

    if (ptr == NULL) {
        return NULL;
    }

    allocation->ptr = ptr;
    allocation->size = size;
    allocation->alignment = alignment;
    allocation->allocatorData = 0;

    atomic_fetch_add(&ma->allocations, 1);
    atomic_fetch_add(&ma->allocatedBytes, size);
    atomic_fetch_add(&ma->totalAllocations, 1);

    VNMetricUInt32("mallocAllocations", atomic_load(&ma->allocations));
    VNMetricUInt32("mallocAllocatedBytes", atomic_load(&ma->allocatedBytes));
    VNMetricUInt32("mallocTotalAllocations", atomic_load(&ma->totalAllocations));

    return ptr;
}

static void mallocFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation)
{
    LdcMemoryAllocatorMalloc* ma = (LdcMemoryAllocatorMalloc*)allocator;

#if VN_OS(WINDOWS)
    if (allocation->alignment > sizeof(void*)) {
        _aligned_free(allocation->ptr);
    } else {
        free(allocation->ptr);
    }
#else
    free(allocation->ptr);
#endif

    atomic_fetch_sub(&ma->allocations, 1);
    atomic_fetch_sub(&ma->allocatedBytes, allocation->size);

    VNMetricUInt32("mallocAllocations", atomic_load(&ma->allocations));
    ;
    VNMetricUInt32("mallocAllocatedBytes", atomic_load(&ma->allocatedBytes));
}

static void* mallocReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation, size_t size)
{
    LdcMemoryAllocatorMalloc* ma = (LdcMemoryAllocatorMalloc*)allocator;

    if (allocation->alignment < sizeof(void*) && allocation->ptr != NULL && allocation->size > 0 &&
        size > 0) {
        // Actually reallocating without large alignment
        void* newPtr = realloc(allocation->ptr, size);
        if (!newPtr) {
            return NULL;
        }

        allocation->ptr = newPtr;

        atomic_fetch_sub(&ma->totalAllocations, (uint32_t)allocation->size);
        atomic_fetch_add(&ma->totalAllocations, (uint32_t)size);
        atomic_fetch_add(&ma->totalAllocations, 1);

        allocation->size = size;

        VNMetricUInt32("mallocAllocatedBytes", atomic_load(&ma->allocatedBytes));
        VNMetricUInt32("mallocTotalAllocations", atomic_load(&ma->totalAllocations));

    } else {
        // Large alignment, or not really a realloc() - do alloc/copy/free
        LdcMemoryAllocation prev = *allocation;
        *allocation = (LdcMemoryAllocation){0, 0, allocation->alignment, 0};

        if (size) {
            void* newPtr = mallocAllocate(allocator, allocation, size, allocation->alignment);
            const size_t copySize = (prev.size < size) ? prev.size : size;
            if (allocation->ptr && prev.ptr && copySize > 0) {
                memcpy(allocation->ptr, prev.ptr, copySize);
            }
            if (!newPtr) {
                return NULL;
            }
            allocation->ptr = newPtr;
        }

        if (prev.ptr) {
            mallocFree(allocator, &prev);
        }
    }
    VNMetricUInt32("mallocTotalReallocations", atomic_load(&ma->totalReallocations));

    return allocation->ptr;
}

/*
 */
/* clang-format off */

static const LdcMemoryAllocatorFunctions kMallocMemoryFunctions = {
    mallocAllocate,
    mallocReallocate,
    mallocFree
};

static LdcMemoryAllocatorMalloc mallocMemoryAllocator = {   //NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    { &kMallocMemoryFunctions, NULL },
    0
};
/* clang-format on */
