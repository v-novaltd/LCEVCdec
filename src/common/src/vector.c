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

#include <LCEVC/common/vector.h>
//
#include <LCEVC/common/check.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
//
#include <assert.h>

void ldcVectorInitialize(LdcVector* vector, uint32_t elementSize, uint32_t reserved,
                         LdcMemoryAllocator* allocator)
{
    assert(vector);
    assert(allocator);

    VNClear(vector);

    VNAllocateArray(allocator, &vector->dataAllocation, uint8_t, reserved * elementSize);
    VNCheck(VNAllocationSucceeded(vector->dataAllocation));

    vector->data = VNAllocationPtr(vector->dataAllocation, uint8_t);
    vector->size = 0;
    vector->elementSize = elementSize;
    vector->reserved = reserved;
    vector->allocator = allocator;
}

void ldcVectorDestroy(LdcVector* vector)
{
    assert(vector);
    assert(vector->allocator);

    VNFree(vector->allocator, &vector->dataAllocation);
}
