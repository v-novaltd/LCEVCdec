/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "common/cmdbuffer.h"

#include "context.h"

#include <assert.h>

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
 *  \param entryByteSize     The new byte size for each entry in the storage.
 *
 *  \return true on success, otherwise false.
 */
static bool cmdBufferStorageResize(CmdBufferStorage_t* store, uint32_t capacity)
{
    assert(store);

    if (capacity == store->end - store->start) {
        return true;
    }

    if (store->start) {
        /* Assume that the storage is never contracted only ever expanded as needed. */
        const size_t dataOffset = (size_t)(store->end - store->currentData);
        const size_t commandOffset = (size_t)(store->currentCommand - store->start);

        store->start = VN_REALLOC_T_ARR(store->memory, store->start, uint8_t, capacity);
        if (!store->start) {
            return false;
        }
        /* Realloc may have moved data, so reset to valid values*/
        store->currentData = store->start + store->allocatedCapacity - dataOffset;
        store->currentCommand = store->start + commandOffset;

        /* Command-side of the buffer is unchanged, because realloc only extends the end, so
         * we just need to extend the far-side of the buffer. */
        uint8_t* newEnd = store->start + capacity;
        memcpy((void* const)(newEnd - dataOffset), store->currentData, dataOffset);
        store->currentData = newEnd - dataOffset;
    } else {
        store->start = VN_MALLOC_T_ARR(store->memory, uint8_t, capacity);

        if (!store->start) {
            return false;
        }

        store->currentCommand = store->start;
        store->currentData = store->start + capacity - 32;
    }

    store->end = store->start + capacity;
    store->allocatedCapacity = capacity;

    return true;
}

/*! \brief Initializes a store object with an initial capacity and entry byte size.
 *
 *  \param[in/out] store            The store to initialise.
 *  \param     initialCapacity  The initial capacity to prepare the storage with.
 *  \param     entryByteSize    The initial byte size for each entry in the storage.
 *
 *  \return true on success, otherwise false.
 */
static bool cmdBufferStorageInitialise(Memory_t memory, CmdBufferStorage_t* store, int32_t initialCapacity)
{
    assert(store);
    assert(initialCapacity >= 1);

    memorySet(store, 0, sizeof(CmdBufferStorage_t));

    store->memory = memory;

    return cmdBufferStorageResize(store, initialCapacity);
}

/*! \brief Releases all the memory associated with a store object.
 *
 *  \param store   The store to free.
 */
static void cmdBufferStorageFree(CmdBufferStorage_t* store)
{
    assert(store);

    if (store->memory && store->start) {
        VN_FREE(store->memory, store->start);
    }
    memorySet(store, 0, sizeof(CmdBufferStorage_t));
}

/*! \brief Resets a store object back to the beginning of its memory.
 *
 * \param store   The store to reset.
 */
static void cmdBufferStorageReset(CmdBufferStorage_t* store)
{
    assert(store);

    store->currentCommand = store->start;
    store->currentData = store->end;
}

/*------------------------------------------------------------------------------*/

bool cmdBufferInitialise(Memory_t memory, CmdBuffer_t** cmdBuffer, uint16_t numEntryPoints)
{
    assert(cmdBuffer);

    CmdBuffer_t* newBuffer = VN_CALLOC_T(memory, CmdBuffer_t);
    if (!newBuffer) {
        return false;
    }

    newBuffer->memory = memory;

    if (!cmdBufferStorageInitialise(memory, &newBuffer->data, CBKDefaultInitialCapacity)) {
        cmdBufferFree(newBuffer);
        return false;
    }

    newBuffer->numEntryPoints = numEntryPoints;
    if (numEntryPoints > 0) {
        newBuffer->entryPoints = VN_CALLOC_T_ARR(memory, CmdBufferEntryPoint_t, numEntryPoints);
    }

    *cmdBuffer = newBuffer;

    return true;
}

void cmdBufferFree(CmdBuffer_t* cmdBuffer)
{
    if (!cmdBuffer) {
        return;
    }

    if (cmdBuffer->numEntryPoints > 0) {
        VN_FREE(cmdBuffer->memory, cmdBuffer->entryPoints);
        cmdBuffer->numEntryPoints = 0;
    }

    cmdBufferStorageFree(&cmdBuffer->data);

    VN_FREE(cmdBuffer->memory, cmdBuffer);
}

bool cmdBufferReset(CmdBuffer_t* cmdBuffer, uint8_t layerCount)
{
    assert(cmdBuffer);

    cmdBufferStorageReset(&cmdBuffer->data);
    cmdBuffer->count = 0;

    /* Already with the right number of layers. */
    if (layerCount == 0 || (layerCount == cmdBuffer->layerCount)) {
        return true;
    }

    if (!cmdBufferStorageResize(&cmdBuffer->data, cmdBuffer->data.allocatedCapacity)) {
        return false;
    }

    cmdBuffer->layerCount = layerCount;

    return true;
}

bool cmdBufferAppend(CmdBuffer_t* cmdBuffer, CmdBufferCmd_t command, const int16_t* values, uint32_t jump)
{
    assert(cmdBuffer);
    assert(cmdBuffer->layerCount > 0);
    CmdBufferStorage_t* dataStore = &cmdBuffer->data;

    if (jump < CBKBigJump) {
        *dataStore->currentCommand = command | (uint8_t)jump;
        dataStore->currentCommand++;
    } else if (jump < CBKExtraBigJump) {
        dataStore->currentCommand[0] = command | (uint8_t)CBKBigJump;
        dataStore->currentCommand[1] = jump & 0xff;
        dataStore->currentCommand[2] = (jump >> 8) & 0xff;
        dataStore->currentCommand += 3;
    } else {
        assert(jump < 0x1000000);
        dataStore->currentCommand[0] = command | (uint8_t)CBKExtraBigJumpSignal;
        dataStore->currentCommand[1] = jump & 0xff;
        dataStore->currentCommand[2] = (jump >> 8) & 0xff;
        dataStore->currentCommand[3] = (jump >> 16) & 0xff;
        dataStore->currentCommand += 4;
    }

    const uint8_t layerSize = (cmdBuffer->layerCount == CBKDDSLayers ? CBKDDSLayerSize : CBKDDLayerSize);
    if (command == CBCAdd || command == CBCSet) {
        dataStore->currentData -= layerSize;
        if (cmdBuffer->layerCount == CBKDDSLayers) {
            /* Note that we reorder the values when we copy here. This differs from the
             * non-command-buffer implementation, where the reordering is done at the apply stage,
             * rather than the residual-generation stage. */
            int16_t* dstData = (int16_t*)dataStore->currentData;
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
            memoryCopy((int16_t*)dataStore->currentData, values, layerSize);
        }
    }

    cmdBuffer->count++;

    if ((size_t)(dataStore->currentData - dataStore->currentCommand) < layerSize + sizeof(int32_t) + 1 &&
        !cmdBufferStorageResize(dataStore, dataStore->allocatedCapacity * CBKStoreGrowFactor)) {
        return false;
    }

    return true;
}

void cmdBufferSplit(const CmdBuffer_t* cmdBuffer)
{
    const uint16_t numEntryPoints = cmdBuffer->numEntryPoints;
    CmdBufferEntryPoint_t* entryPoints = cmdBuffer->entryPoints;
    const uint32_t groupSize = cmdBuffer->count / numEntryPoints;
    const uint8_t blockShift = (cmdBuffer->layerCount == 16) ? 6 : 8;
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
    for (; cmdCount < cmdBuffer->count; cmdCount++) {
        const uint8_t* commandPtr = (const uint8_t*)(cmdBuffer->data.start) + cmdOffset;
        const CmdBufferCmd_t command = (const CmdBufferCmd_t)(*commandPtr & 0xC0);
        const uint8_t jumpSignal = *commandPtr & 0x3F;

        uint32_t jump = 0;
        int32_t cmdIncrement = 0;
        if (jumpSignal < CBKBigJump) {
            jump = jumpSignal;
            cmdIncrement++;
        } else if (jumpSignal == CBKBigJump) {
            jump = commandPtr[1] + (commandPtr[2] << 8);
            cmdIncrement += 3;
        } else { // jumpSignal == CBKExtraBigJump
            jump = commandPtr[1] + (commandPtr[2] << 8) + (commandPtr[3] << 16);
            cmdIncrement += 4;
        }

        const int32_t currentBlock = (tuIndex + jump) >> blockShift;
        if ((uint32_t)cmdCount > splitPoint && bufferIndex < (numEntryPoints - 1) &&
            currentBlock != lastCmdBlock) {
            entryPoints[bufferIndex].count = cmdCount - lastBufferCount;
            bufferIndex++;
            entryPoints[bufferIndex].initialJump = tuIndex;
            entryPoints[bufferIndex].commandOffset = cmdOffset;
            entryPoints[bufferIndex].dataOffset =
                (int32_t)(dataOffset * cmdBuffer->layerCount * sizeof(int16_t));
            splitPoint += groupSize;
            lastBufferCount = cmdCount;
        }
        lastCmdBlock = currentBlock;

        cmdOffset += cmdIncrement;
        tuIndex += jump;
        if (command == CBCSet || command == CBCAdd) {
            dataOffset++;
        }
    }
    entryPoints[bufferIndex].count = cmdCount - lastBufferCount;
}

/*------------------------------------------------------------------------------*/
