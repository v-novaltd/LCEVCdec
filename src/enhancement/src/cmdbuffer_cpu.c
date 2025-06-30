/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <memory.h>

/*! \brief Private constants used for the command buffer format and storage behavior. */
enum CmdBufferCpuConfig
{
    CBCKStoreGrowFactor = 2, /**< The factory multiply current capacity by when growing the buffer. */
    CBCKInitialCapacity = 32768,   /**< The default initial capacity of a cmdbuffer. */
    CBCKExtraBigJump = UINT16_MAX, /**< Max 16-bit value before overflowing to a 24-bit jump value */
    CBCKMaxEntryPoints = 16,       /**< Maximum number of entry points */
};

/*------------------------------------------------------------------------------*/

/*! \brief Resizes a store object for a new capacity and/or entry byte size.
 *
 *  When resizing just the capacity, `current` is repositioned relative to the
 *  allocation start at the same offset it was before the resize.
 *
 *  When resizing the entry byte size, `current` is reset back to the beginning
 *  of the buffer.
 *
 *  If a resize results in the same number of allocated bytes then no allocation is
 *  performed, the same memory is reused.
 *
 *  \param store             The store to resize.
 *  \param capacity          The new capacity to resize the storage with in bytes.
 *
 *  \return true on success, otherwise false.
 */
static bool cmdBufferStorageResize(LdeCmdBufferCpuStorage* store, uint32_t capacity)
{
    assert(store);

    if (capacity == store->end - store->start) {
        return true;
    }

    if (store->start) {
        /* Assume that the storage is never contracted only ever expanded as needed. */
        const size_t dataOffset = (size_t)(store->end - store->currentResidual);
        const size_t commandOffset = (size_t)(store->currentCommand - store->start);

        store->start = VNReallocateArray(store->allocator, &store->allocation, uint8_t, capacity);
        if (!store->start) {
            return false;
        }
        /* Realloc may have moved data, so reset to valid values*/
        store->currentResidual = store->start + store->allocatedCapacity - dataOffset;
        store->currentCommand = store->start + commandOffset;

        /* Command-side of the buffer is unchanged, because realloc only extends the end, so
         * we just need to extend the far-side of the buffer. */
        uint8_t* newEnd = store->start + capacity;
        memmove((void* const)(newEnd - dataOffset), store->currentResidual, dataOffset);
        store->currentResidual = newEnd - dataOffset;
    } else {
        store->start = VNAllocateArray(store->allocator, &store->allocation, uint8_t, capacity);

        if (!store->start) {
            return false;
        }

        store->currentCommand = store->start;
        store->currentResidual = store->start + capacity - 32;
    }

    store->end = store->start + capacity;
    store->allocatedCapacity = capacity;

    return true;
}

/*! \brief Initializes a store object with an initial capacity and entry byte size.
 *
 *  \param     allocator        Memory allocation for this storage.
 *  \param     store            The store to initialize.
 *  \param     initialCapacity  The initial capacity to prepare the storage with.
 *
 *  \return true on success, otherwise false.
 */
static bool cmdBufferStorageInitialize(LdcMemoryAllocator* allocator, LdeCmdBufferCpuStorage* store,
                                       int32_t initialCapacity)
{
    assert(store);
    assert(initialCapacity >= 1);

    memset(store, 0, sizeof(LdeCmdBufferCpuStorage));

    store->allocator = allocator;

    return cmdBufferStorageResize(store, initialCapacity);
}

/*! \brief Releases all the memory associated with a store object.
 *
 *  \param store   The store to free.
 */
static void cmdBufferStorageFree(LdeCmdBufferCpuStorage* store)
{
    assert(store);

    if (store->allocator && store->start) {
        LdcMemoryAllocation storeAllocation = store->allocation;
        VNFree(store->allocator, &storeAllocation);
    }
    memset(store, 0, sizeof(LdeCmdBufferCpuStorage));
}

/*! \brief Resets a store object back to the beginning of its memory.
 *
 * \param store   The store to reset.
 */
static void cmdBufferStorageReset(LdeCmdBufferCpuStorage* store)
{
    assert(store);

    store->currentCommand = store->start;
    store->currentResidual = store->end;
}

/*------------------------------------------------------------------------------*/

bool ldeCmdBufferCpuInitialize(LdcMemoryAllocator* allocator, LdeCmdBufferCpu* cmdBuffer,
                               uint16_t const numEntryPoints)
{
    cmdBuffer->allocator = allocator;

    if (!cmdBufferStorageInitialize(allocator, &cmdBuffer->data, CBCKInitialCapacity)) {
        ldeCmdBufferCpuFree(cmdBuffer);
        return false;
    }

    if (numEntryPoints > CBCKMaxEntryPoints) {
        return false;
    }
    cmdBuffer->numEntryPoints = numEntryPoints;
    if (numEntryPoints > 0) {
        cmdBuffer->entryPoints = VNAllocateZeroArray(allocator, &cmdBuffer->entryPointsAllocation,
                                                     LdeCmdBufferCpuEntryPoint, numEntryPoints);
    }

    return true;
}

void ldeCmdBufferCpuFree(LdeCmdBufferCpu* cmdBuffer)
{
    if (!cmdBuffer) {
        return;
    }

    if (cmdBuffer->numEntryPoints > 0) {
        VNFree(cmdBuffer->allocator, &cmdBuffer->entryPointsAllocation);
        cmdBuffer->numEntryPoints = 0;
    }

    cmdBufferStorageFree(&cmdBuffer->data);
}

bool ldeCmdBufferCpuReset(LdeCmdBufferCpu* cmdBuffer, uint8_t transformSize)
{
    assert(cmdBuffer);

    cmdBufferStorageReset(&cmdBuffer->data);
    cmdBuffer->count = 0;

    /* Buffer already has the correct number of layers set - nothing to do */
    if (transformSize == cmdBuffer->transformSize) {
        return true;
    }

    if (!cmdBufferStorageResize(&cmdBuffer->data, cmdBuffer->data.allocatedCapacity)) {
        return false;
    }

    cmdBuffer->transformSize = transformSize;

    return true;
}

bool ldeCmdBufferCpuAppend(LdeCmdBufferCpu* cmdBuffer, LdeCmdBufferCpuCmd command,
                           const int16_t* values, uint32_t jump)
{
    assert(cmdBuffer);
    assert(cmdBuffer->transformSize > 0);
    LdeCmdBufferCpuStorage* dataStore = &cmdBuffer->data;

    if (jump < CBCKBigJumpSignal) {
        *dataStore->currentCommand = command | (uint8_t)jump;
        dataStore->currentCommand++;
    } else if (jump < CBCKExtraBigJump) {
        dataStore->currentCommand[0] = command | (uint8_t)CBCKBigJumpSignal;
        dataStore->currentCommand[1] = jump & 0xff;
        dataStore->currentCommand[2] = (jump >> 8) & 0xff;
        dataStore->currentCommand += 3;
    } else {
        assert(jump < 0x1000000);
        dataStore->currentCommand[0] = command | (uint8_t)CBCKExtraBigJumpSignal;
        dataStore->currentCommand[1] = jump & 0xff;
        dataStore->currentCommand[2] = (jump >> 8) & 0xff;
        dataStore->currentCommand[3] = (jump >> 16) & 0xff;
        dataStore->currentCommand += 4;
    }

    const uint8_t layerSize =
        (cmdBuffer->transformSize == CBCKDDSLayers ? CBCKDDSLayerSize : CBCKDDLayerSize);
    if (command == CBCCAdd || command == CBCCSet) {
        dataStore->currentResidual -= layerSize;
        if (cmdBuffer->transformSize == CBCKDDSLayers) {
            /* Note that we reorder the values when we copy here. This differs from the
             * non-command-buffer implementation, where the reordering is done at the apply stage,
             * rather than the residual-generation stage. */
            int16_t* dstData = (int16_t*)dataStore->currentResidual;
            dstData[0] = values[0];
            dstData[1] = values[1];
            dstData[2] = values[4];
            dstData[3] = values[5];
            dstData[4] = values[2];
            dstData[5] = values[3];
            dstData[6] = values[6];
            dstData[7] = values[7];
            dstData[8] = values[8];
            dstData[9] = values[9];
            dstData[10] = values[12];
            dstData[11] = values[13];
            dstData[12] = values[10];
            dstData[13] = values[11];
            dstData[14] = values[14];
            dstData[15] = values[15];
        } else {
            memcpy((int16_t*)dataStore->currentResidual, values, layerSize);
        }
    }

    cmdBuffer->count++;

    if ((size_t)(dataStore->currentResidual - dataStore->currentCommand) < layerSize + sizeof(int32_t) + 1 &&
        !cmdBufferStorageResize(dataStore, dataStore->allocatedCapacity * CBCKStoreGrowFactor)) {
        return false;
    }

    return true;
}

void ldeCmdBufferCpuSplit(const LdeCmdBufferCpu* srcBuffer)
{
    const uint16_t numEntryPoints = srcBuffer->numEntryPoints;
    LdeCmdBufferCpuEntryPoint* entryPoints = srcBuffer->entryPoints;
    const uint32_t groupSize = srcBuffer->count / numEntryPoints;
    const uint8_t blockShift = (srcBuffer->transformSize == 16) ? 6 : 8;
    uint32_t splitPoint = groupSize;

    int32_t dataOffset = 0;
    int32_t cmdOffset = 0;
    uint32_t tuIndex = 0;
    uint16_t bufferIndex = 0;

    for (uint16_t i = 0; i < numEntryPoints; i++) {
        entryPoints[i].initialJump = 0;
        entryPoints[i].commandOffset = 0;
        entryPoints[i].dataOffset = 0;
        entryPoints[i].count = 0;
    }

    int32_t lastCmdBlock = -1;
    uint32_t lastBufferCount = 0;
    entryPoints[0].initialJump = 0;
    entryPoints[0].commandOffset = 0;
    entryPoints[0].dataOffset = 0;
    uint32_t cmdCount = 0;
    for (; cmdCount < srcBuffer->count; cmdCount++) {
        const uint8_t* commandPtr = (const uint8_t*)(srcBuffer->data.start) + cmdOffset;
        const LdeCmdBufferCpuCmd command = (const LdeCmdBufferCpuCmd)(*commandPtr & 0xC0);
        const uint8_t jumpSignal = *commandPtr & 0x3F;

        uint32_t jump = 0;
        int32_t cmdIncrement = 0;
        if (jumpSignal < CBCKBigJumpSignal) {
            jump = jumpSignal;
            cmdIncrement++;
        } else if (jumpSignal == CBCKBigJumpSignal) {
            jump = commandPtr[1] + (commandPtr[2] << 8);
            cmdIncrement += 3;
        } else { // jumpSignal == CBKExtraBigJump
            jump = commandPtr[1] + (commandPtr[2] << 8) + (commandPtr[3] << 16);
            cmdIncrement += 4;
        }

        const int32_t currentBlock = (tuIndex + jump) >> blockShift;
        if (cmdCount > splitPoint && bufferIndex < (numEntryPoints - 1) && currentBlock != lastCmdBlock) {
            entryPoints[bufferIndex].count = cmdCount - lastBufferCount;
            bufferIndex++;
            entryPoints[bufferIndex].initialJump = tuIndex;
            entryPoints[bufferIndex].commandOffset = cmdOffset;
            entryPoints[bufferIndex].dataOffset =
                (int32_t)(dataOffset * srcBuffer->transformSize * sizeof(int16_t));
            splitPoint += groupSize;
            lastBufferCount = cmdCount;
        }
        lastCmdBlock = currentBlock;

        cmdOffset += cmdIncrement;
        tuIndex += jump;
        if (command == CBCCSet || command == CBCCAdd) {
            dataOffset++;
        }
    }
    entryPoints[bufferIndex].count = cmdCount - lastBufferCount;
}

/*------------------------------------------------------------------------------*/
