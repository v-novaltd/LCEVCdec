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

#include <assert.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <string.h>

void ldcMemoryInitialize(LdcMemoryAllocator* allocator, size_t alignment, LdcMemoryAllocation* allocation)
{
    VNUnused(allocator);

    assert(allocator);
    assert(allocator->functions);
    assert(allocator->functions->allocate);
    assert(allocation);

    allocation->allocatorData = 0;
    allocation->ptr = NULL;
    allocation->size = 0;
    allocation->alignment = alignment;
}

void* ldcMemoryAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation, size_t size,
                        size_t align, bool clearToZero)
{
    assert(allocator);
    assert(allocator->functions);
    assert(allocator->functions->allocate);
    assert(allocation);

    void* ret = allocator->functions->allocate(allocator, allocation, size, align);

    if (ret && clearToZero) {
        memset(ret, 0, size);
    }

    return ret;
}

void* ldcMemoryReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation, size_t size)
{
    assert(allocator);
    assert(allocator->functions);
    assert(allocator->functions->reallocate);
    assert(allocation);

    void* ret = allocator->functions->reallocate(allocator, allocation, size);

    return ret;
}

void ldcMemoryFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation)
{
    assert(allocator);
    assert(allocator->functions);
    assert(allocator->functions->free);
    assert(allocation);

    allocator->functions->free(allocator, allocation);

    allocation->ptr = NULL;
    allocation->size = 0;
    allocation->allocatorData = 0;
}
