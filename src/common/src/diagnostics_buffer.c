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
#include <LCEVC/common/check.h>
#include <LCEVC/common/diagnostics_buffer.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/threads.h>

void ldcDiagnosticsBufferInitialize(LdcDiagnosticsBuffer* diagnosticsBuffer, uint32_t capacity,
                                    uint32_t varDataCapacity, LdcMemoryAllocator* allocator)
{
    assert(allocator);
    assert(VNIsPowerOfTwo(capacity));
    assert(VNIsPowerOfTwo(varDataCapacity));

    VNClear(diagnosticsBuffer);

    VNAllocateArray(allocator, &diagnosticsBuffer->ringAllocation, LdcDiagRecord, capacity);
    VNCheck(VNAllocationSucceeded(diagnosticsBuffer->ringAllocation));

    VNAllocateArray(allocator, &diagnosticsBuffer->varDataAllocation, uint8_t, varDataCapacity);
    VNCheck(VNAllocationSucceeded(diagnosticsBuffer->varDataAllocation));

    diagnosticsBuffer->ring = VNAllocationPtr(diagnosticsBuffer->ringAllocation, LdcDiagRecord);
    diagnosticsBuffer->ringCapacity = capacity;
    diagnosticsBuffer->ringMask = capacity - 1;

    diagnosticsBuffer->varData = VNAllocationPtr(diagnosticsBuffer->varDataAllocation, uint8_t);
    diagnosticsBuffer->varDataCapacity = varDataCapacity;
    diagnosticsBuffer->varDataMask = varDataCapacity - 1;

    VNCheck(threadMutexInitialize(&diagnosticsBuffer->mutex) == ThreadResultSuccess);
    VNCheck(threadCondVarInitialize(&diagnosticsBuffer->notEmpty) == ThreadResultSuccess);
    VNCheck(threadCondVarInitialize(&diagnosticsBuffer->notFull) == ThreadResultSuccess);

    diagnosticsBuffer->allocator = allocator;
}

void ldcDiagnosticsBufferDestroy(LdcDiagnosticsBuffer* diagnosticsBuffer)
{
    VNFree(diagnosticsBuffer->allocator, &diagnosticsBuffer->ringAllocation);
    VNFree(diagnosticsBuffer->allocator, &diagnosticsBuffer->varDataAllocation);

    threadCondVarDestroy(&diagnosticsBuffer->notEmpty);
    threadCondVarDestroy(&diagnosticsBuffer->notFull);
    threadMutexDestroy(&diagnosticsBuffer->mutex);
}
