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

#include <LCEVC/common/atomic.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/stdlib.h>

typedef struct
{
    LdcMemoryAllocator allocator;
    LdcAtomicSizeT allocatedBytes;
    LdcAtomicSizeT allocations;
    LdcAtomicSizeT totalAllocations;
    LdcAtomicSizeT totalReallocations;
} LdcMemoryAllocatorMalloc;

static LdcMemoryAllocatorMalloc mallocMemoryAllocator; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

LdcMemoryAllocator* ldcMemoryAllocatorMalloc(void) { return &mallocMemoryAllocator.allocator; }

static void mallocAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                           size_t size, size_t alignment, const LdcDiagSite* site)
{
    VNUnused(site);
    LdcMemoryAllocatorMalloc* ma = (LdcMemoryAllocatorMalloc*)allocator;
    void* ptr = NULL;
    if (alignment > sizeof(void*)) {
#if VN_OS(WINDOWS)
        ptr = _aligned_malloc(size, alignment);
#elif VN_OS(ANDROID) || VN_OS(APPLE)
        int r = posix_memalign(&ptr, alignment, size);
        if (r != 0) {
            return;
        }
#else
        ptr = aligned_alloc(alignment, size);
#endif
    } else {
        ptr = malloc(size);
    }

    if (ptr == NULL) {
        allocation->ptr = NULL;
        return;
    }

    allocation->ptr = ptr;
    allocation->size = size;
    allocation->alignment = alignment;
    allocation->allocatorData = 0;

    atomicFetchAddSizeT(&ma->allocations, 1);
    atomicFetchAddSizeT(&ma->allocatedBytes, size);
    atomicFetchAddSizeT(&ma->totalAllocations, 1);

    VNMetricUInt64("mallocAllocations", atomicLoadSizeT(&ma->allocations));
    VNMetricUInt64("mallocAllocatedBytes", atomicLoadSizeT(&ma->allocatedBytes));
    VNMetricUInt64("mallocTotalAllocations", atomicLoadSizeT(&ma->totalAllocations));
}

static void mallocFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                       const LdcDiagSite* site)
{
    VNUnused(site);
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

    atomicFetchSubSizeT(&ma->allocations, 1);
    atomicFetchSubSizeT(&ma->allocatedBytes, allocation->size);

    VNMetricUInt64("mallocAllocations", atomicLoadSizeT(&ma->allocations));
    VNMetricUInt64("mallocAllocatedBytes", atomicLoadSizeT(&ma->allocatedBytes));
}

static void mallocReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                             size_t size, const LdcDiagSite* site)
{
    LdcMemoryAllocatorMalloc* ma = (LdcMemoryAllocatorMalloc*)allocator;

    if (allocation->alignment < sizeof(void*) && allocation->ptr != NULL && allocation->size > 0 &&
        size > 0) {
        // Actually reallocating without large alignment
        void* newPtr = realloc(allocation->ptr, size);
        if (!newPtr) {
            allocation->ptr = NULL;
            return;
        }

        allocation->ptr = newPtr;

        atomicFetchSubSizeT(&ma->allocatedBytes, allocation->size);
        atomicFetchAddSizeT(&ma->allocatedBytes, size);
        atomicFetchAddSizeT(&ma->totalReallocations, 1);

        allocation->size = size;

        VNMetricUInt64("mallocAllocatedBytes", atomicLoadSizeT(&ma->allocatedBytes));
        VNMetricUInt64("mallocTotalAllocations", atomicLoadSizeT(&ma->totalAllocations));

    } else {
        // Large alignment, or not really a realloc() - do alloc/copy/free
        LdcMemoryAllocation prev = *allocation;
        *allocation = (LdcMemoryAllocation){0, 0, allocation->alignment, 0};

        if (size) {
            mallocAllocate(allocator, allocation, size, allocation->alignment, site);
            void* const newPtr = allocation->ptr;
            if (!newPtr) {
                return;
            }

            const size_t copySize = (prev.size < size) ? prev.size : size;
            if (allocation->ptr && prev.ptr && copySize > 0) {
                memcpy(allocation->ptr, prev.ptr, copySize);
            }

            allocation->ptr = newPtr;
        }

        if (prev.ptr) {
            mallocFree(allocator, &prev, site);
        }
    }
    VNMetricUInt64("mallocTotalReallocations", atomicLoadSizeT(&ma->totalReallocations));
}

/*
 */
/* clang-format off */

static const LdcMemoryAllocatorFunctions kMallocMemoryFunctions = {
    mallocAllocate,
    mallocReallocate,
    mallocFree
};

//NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static LdcMemoryAllocatorMalloc mallocMemoryAllocator = {
    .allocator = { &kMallocMemoryFunctions, NULL }
};
/* clang-format on */
