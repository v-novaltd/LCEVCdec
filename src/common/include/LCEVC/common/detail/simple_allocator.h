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

#ifndef VN_LCEVC_COMMON_DETAIL_SIMPLE_ALLOCATOR_H
#define VN_LCEVC_COMMON_DETAIL_SIMPLE_ALLOCATOR_H

#include <LCEVC/common/memory.h>
#include <LCEVC/common/threads.h>

// Implementation of LdcMemoryAllocator interface
//
// This is private to the implementation but declared here so callers can embed the struct directly
// without requiring an additional allocation.
//

// Range of powers of 2 for free lists - inclusive
//
#define kFreeListMinLog2 8
#define kFreeListMaxLog2 20

#define kFreeListCount (kFreeListMaxLog2 - kFreeListMinLog2 + 1)

typedef struct LdcMemorySimpleAllocator
{
    LdcMemoryAllocator allocator;

    // Allocator is thread safe
    ThreadMutex mutex;

    // Parent allocator used to acquire backing blocks
    LdcMemoryAllocator* parentAllocator;

    // Linked list of blocks
    LdcLinkedList blockList;

    // Segregated free lists for chunks by size class
    LdcLinkedList freeLists[kFreeListCount];

    // Current block size to allocate from the parent
    size_t blockSize;

} LdcMemorySimpleAllocator;

#endif // VN_LCEVC_COMMON_DETAIL_SIMPLE_ALLOCATOR_H
