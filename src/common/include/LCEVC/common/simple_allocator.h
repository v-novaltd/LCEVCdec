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

#ifndef VN_LCEVC_COMMON_SIMPLE_ALLOCATOR_H
#define VN_LCEVC_COMMON_SIMPLE_ALLOCATOR_H

#include <LCEVC/common/linked_list.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! @file
 * @brief A simple block/chunk/free list memory allocator.
 *
 * This allocator implements a simple general alloc/free/realloc strategy with a few
 * optimizations that work well for the LCEVCdec use case - a cycle of reuse of roughly similar
 * sized buffers. It does not make any special effort for very small allocations - LCEVCdec
 * generally avoids these, preferring arrays or vectors. It also takes advantage of the
 * `LDCMemoryAllocation` structure to make finding the allocation metadata simpler than a pure
 * malloc() style mechanism that must work back from the returned address.
 *
 * See also `LdcMemoryRecyclingAllocator` that is a further speciailizaion for image buffers.
 *
 * Large constant size blocks are allocated from the parent allocator, then split into chunks
 * to satisfy allocations as needed. The blocks are only returned to the parent
 * when the simple allocator is destroyed.
 */

typedef struct LdcMemorySimpleAllocator LdcMemorySimpleAllocator;

/*! Initialize a new simple allocator.
 *
 * @param[out]      simpleAllocator    The initialized allocator
 * @param[in]       parentAllocator    The allocator to provide underlying chunks
 *
 * @return          A pointer to the new allocator
 */
LdcMemoryAllocator* ldcMemorySimpleAllocatorInitialize(LdcMemorySimpleAllocator* simpleAllocator,
                                                       LdcMemoryAllocator* parentAllocator);

/*!
 * Destroy a simple allocator.
 *
 * Returns chunks to parent allocator.
 *
 * @param[in]      simpleAllocator     The simple allocator to be destroyed
 */
void ldcMemorySimpleAllocatorDestroy(LdcMemorySimpleAllocator* simpleAllocator);

/*! Debug helper to validate simple allocator invariants.
 *
 * @param[in]      simpleAllocator   Allocator instance to be checked.
 *
 * @return         true if allocator internal structures appear consistent.
 */
bool ldcMemoryAllocatorSimpleDebugCheck(const LdcMemorySimpleAllocator* simpleAllocator);

// Allocator definition and inline fast paths
//
#include "detail/simple_allocator.h"

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_SIMPLE_ALLOCATOR_H
