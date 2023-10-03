/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#include "common/cmdbuffer.h"

#include "common/memory.h"
#include "context.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

/*! \brief Arbitrary constants used for the command buffer storage behavior. */
enum CmdBufferConstants
{
    CBCStoreGrowFactor = 2, /**< The factory multiply current capacity by when growing the buffer. */
    CBCDefaultInitialCapacity = 1024, /**< The default initial capacity of a cmdbuffer. */
};

/*! \brief Dynamically growing managing the memory for a command buffer instance.
 *
 *  The storage can be resized after initialization. This can be performed after
 *  changing both the capacity and entry size..
 *
 *  \note This does not contract itself overtime.
 */
typedef struct CmdBufferStorage
{
    Memory_t memory;            /**< Decoder instance this belongs to. */
    uint8_t* backing;           /**< Pointer to the start of the storage. */
    uint8_t* current;           /**< Pointer to the current write position in the storage. */
    uint8_t* end;               /**< Pointer to the end of the storage. */
    int32_t allocatedEntrySize; /**< Number of bytes allocated for each entry. */
    int32_t allocatedCapacity;  /**< Number of elements allocated. */
} CmdBufferStorage_t;

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
 *  \param capacity          The new capacity to resize the storage with.
 *  \param entryByteSize   The new byte size fort each entry in the storage.
 *
 *  \return true on success, otherwise false.
 */
static bool cmdBufferStorageResize(CmdBufferStorage_t* store, int32_t capacity, int32_t entryByteSize)
{
    assert(store);

    const size_t allocSize = (size_t)entryByteSize * capacity;
    const size_t currentSize = (size_t)store->allocatedCapacity * store->allocatedEntrySize;

    if (allocSize != currentSize) {
        if (store->backing) {
            const uintptr_t currOffset = (uintptr_t)(store->current - store->backing);
            uint8_t* newPtr = VN_REALLOC_T_ARR(store->memory, store->backing, uint8_t, allocSize);

            if (!newPtr) {
                return false;
            }

            store->backing = newPtr;

            if (store->allocatedEntrySize == entryByteSize) {
                /* Assume that the storage is never contracted only ever expanded as needed. */
                assert(currOffset <= allocSize);
                store->current = store->backing + currOffset;
            }
        } else {
            store->backing = VN_MALLOC_T_ARR(store->memory, uint8_t, allocSize);

            if (!store->backing) {
                return false;
            }

            store->current = store->backing;
        }
    }

    /* If the resize changes the dimensions of the entries, then the current must
     * be reset as it is expected that all data in storage is now invalid. */
    if (store->allocatedEntrySize != entryByteSize) {
        store->current = store->backing;
    }

    store->end = store->backing + allocSize;
    store->allocatedCapacity = capacity;
    store->allocatedEntrySize = entryByteSize;

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
static bool cmdBufferStorageInitialise(Memory_t memory, CmdBufferStorage_t* store,
                                       int32_t initialCapacity, int32_t entryByteSize)
{
    assert(store);
    assert(initialCapacity >= 1);

    memorySet(store, 0, sizeof(CmdBufferStorage_t));

    store->memory = memory;

    return cmdBufferStorageResize(store, initialCapacity, entryByteSize);
}

/*! \brief Releases all the memory associated with a store object.
 *
 *  \param store   The store to free.
 */
static void cmdBufferStorageFree(CmdBufferStorage_t* store)
{
    assert(store);

    if (store->memory && store->backing) {
        VN_FREE(store->memory, store->backing);
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

    store->current = store->backing;
}

/*! \brief Moves the storage onto the next entry.
 *
 *  The intention is that a user will populate the current entry in the storage
 *  and then indicate they are completed with it by calling this API. This API
 *  will automatically allocate more storage if there is not enough room for the
 *  next entry.
 *
 *  \param store   The store to move to the next element on.
 *
 *  \return true on success, otherwise false.
 */
static bool cmdBufferStorageNext(CmdBufferStorage_t* store)
{
    assert(store);
    assert(store->allocatedEntrySize != 0);

    store->current += store->allocatedEntrySize;

    if ((store->current == store->end) &&
        !cmdBufferStorageResize(store, store->allocatedCapacity * CBCStoreGrowFactor,
                                store->allocatedEntrySize)) {
        return false;
    }

    assert(store->current <= (store->end - store->allocatedEntrySize));

    return true;
}

/*------------------------------------------------------------------------------*/

typedef struct CmdBuffer
{
    Memory_t memory;      /**< Memory allocator this command buffer uses. */
    CmdBufferType_t type; /**< The type of the command buffer. */
    int32_t count;        /**< Number of entries added to the command buffer. */
    int32_t layerCount;   /**< Number of layers the command buffer is initialised for. */
    CmdBufferStorage_t coordinateStore; /**< Memory storage for the coordinates. */
    CmdBufferStorage_t residualStore;   /**< Memory storage for the residuals. */
} CmdBuffer_t;

static int32_t cmdBufferEntrySize(int32_t layerCount)
{
    /* [4, 16] values */
    assert((layerCount == 4) || (layerCount == 16));
    return (int32_t)(sizeof(int16_t) * layerCount);
}

bool cmdBufferInitialise(Memory_t memory, CmdBuffer_t** cmdBuffer, CmdBufferType_t type)
{
    assert(cmdBuffer);

    CmdBuffer_t* newBuffer = VN_CALLOC_T(memory, CmdBuffer_t);

    if (!newBuffer) {
        return false;
    }

    memorySet(newBuffer, 0, sizeof(CmdBuffer_t));
    newBuffer->memory = memory;
    newBuffer->type = type;

    if (!cmdBufferStorageInitialise(memory, &newBuffer->coordinateStore, CBCDefaultInitialCapacity,
                                    sizeof(int16_t) * 2)) {
        cmdBufferFree(newBuffer);
        return false;
    }

    if ((type == CBTResiduals) &&
        !cmdBufferStorageInitialise(memory, &newBuffer->residualStore, CBCDefaultInitialCapacity,
                                    sizeof(int16_t) * 4)) {
        cmdBufferFree(newBuffer);
        return false;
    }

    *cmdBuffer = newBuffer;

    return true;
}

void cmdBufferFree(CmdBuffer_t* cmdBuffer)
{
    if (!cmdBuffer) {
        return;
    }

    cmdBufferStorageFree(&cmdBuffer->coordinateStore);
    cmdBufferStorageFree(&cmdBuffer->residualStore);
    VN_FREE(cmdBuffer->memory, cmdBuffer);
}

bool cmdBufferReset(CmdBuffer_t* cmdBuffer, int32_t layerCount)
{
    assert(cmdBuffer);

    cmdBufferStorageReset(&cmdBuffer->coordinateStore);
    cmdBufferStorageReset(&cmdBuffer->residualStore);
    cmdBuffer->count = 0;

    /* Already with the right number of layers. */
    if (layerCount == 0 || (layerCount == cmdBuffer->layerCount)) {
        return true;
    }

    if (!cmdBufferStorageResize(&cmdBuffer->residualStore, cmdBuffer->residualStore.allocatedCapacity,
                                cmdBufferEntrySize(layerCount))) {
        return false;
    }

    cmdBuffer->layerCount = layerCount;

    return true;
}

int32_t cmdBufferGetLayerCount(const CmdBuffer_t* cmdBuffer)
{
    assert(cmdBuffer);

    return cmdBuffer->layerCount;
}

CmdBufferData_t cmdBufferGetData(const CmdBuffer_t* cmdBuffer)
{
    CmdBufferData_t data;
    data.coordinates = (const int16_t*)cmdBuffer->coordinateStore.backing;
    data.residuals = (const int16_t*)cmdBuffer->residualStore.backing;
    data.count = cmdBuffer->count;
    return data;
}

bool cmdBufferIsEmpty(const CmdBuffer_t* cmdBuffer) { return (cmdBuffer->count == 0); }

int32_t cmdBufferGetCount(const CmdBuffer_t* cmdBuffer) { return cmdBuffer->count; }

bool cmdBufferAppend(CmdBuffer_t* cmdBuffer, int16_t x, int16_t y, const int16_t* values)
{
    assert(cmdBuffer);
    assert(cmdBuffer->layerCount > 0);

    if (cmdBuffer->type != CBTResiduals) {
        return false;
    }

    int16_t* dstCoord = (int16_t*)cmdBuffer->coordinateStore.current;
    int16_t* dst = (int16_t*)cmdBuffer->residualStore.current;
    *dstCoord++ = x;
    *dstCoord++ = y;
    memoryCopy(dst, values, sizeof(int16_t) * cmdBuffer->layerCount);

    cmdBuffer->count++;
    return cmdBufferStorageNext(&cmdBuffer->coordinateStore) &&
           cmdBufferStorageNext(&cmdBuffer->residualStore);
}

bool cmdBufferAppendCoord(CmdBuffer_t* cmdBuffer, int16_t x, int16_t y)
{
    assert(cmdBuffer);
    assert(cmdBuffer->layerCount == 0);

    if (cmdBuffer->type != CBTCoordinates) {
        return false;
    }

    int16_t* dst = (int16_t*)cmdBuffer->coordinateStore.current;
    *dst++ = x;
    *dst++ = y;

    cmdBuffer->count++;
    return cmdBufferStorageNext(&cmdBuffer->coordinateStore);
}

/*------------------------------------------------------------------------------*/
