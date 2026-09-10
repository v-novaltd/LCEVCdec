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

#include <LCEVC/common/simple_allocator.h>
//
#include <LCEVC/common/bitutils.h>
#include <LCEVC/common/check.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/stdlib.h>

// Implements a simple block/chunk/free list memory allocator.
//
// Large blocks are allocated from the parent allocator, then split into chunks
// to satisfy allocations as appropriate. The blocks are only returned to the parent
// when the simple allocator is destroyed.

typedef struct BlockHeader
{
    size_t size;                    // Number of bytes in block, including this header
    LdcLinkedNode node;             // Link allocated blocks together
    LdcMemoryAllocation allocation; // Backing allocation details
} BlockHeader;

typedef struct ChunkHeader
{
    size_t size;        // Size of this chunk, including this header
    BlockHeader* block; // Owning block
    LdcLinkedNode node; // Link free chunks together
} ChunkHeader;

static const size_t kChunkHeaderSize = sizeof(ChunkHeader);
static const size_t kChunkFooterSize = sizeof(size_t);
static const size_t kBlockHeaderSize = sizeof(BlockHeader);
static const size_t kMinimumAlignment = sizeof(void*);

static const size_t kDefaultBlockSize = (size_t)64 * 1024;

// MSVC cannot handle this as a constant
#define VN_MINIMUM_CHUNK_SIZE (sizeof(ChunkHeader) + kChunkFooterSize + sizeof(void*))

// Work out free list index for given size of block
static inline size_t freeListIndexForSize(size_t size)
{
    assert(size > 0);
    // floor(log2) of size
    const size_t log = log2_64(size);

    if (log < kFreeListMinLog2) {
        return 0;
    }
    if (log > kFreeListMaxLog2) {
        return kFreeListCount - 1;
    }
    return (size_t)(log - kFreeListMinLog2);
}

static inline size_t minimumChunkSizeForRequest(size_t payloadSize, size_t alignment)
{
    const size_t total = kChunkHeaderSize + kChunkFooterSize + payloadSize;
    return total + (alignment - kMinimumAlignment);
}

static inline LdcLinkedList* freeListForSize(LdcMemorySimpleAllocator* simpleAllocator, size_t size)
{
    const size_t index = freeListIndexForSize(size);
    assert(index < kFreeListCount);

    return &simpleAllocator->freeLists[index];
}

static inline uintptr_t align(uintptr_t value, size_t alignment)
{
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);
    return (value + (alignment - 1)) & ~(uintptr_t)(alignment - 1);
}

static inline bool isFree(const ChunkHeader* chunk)
{
    // Can tell if block is free by looking to see if it is on a free list
    return chunk->node.next != NULL;
}

static inline ChunkHeader* nodeToChunk(LdcLinkedNode* node)
{
    return VNContainerOf(node, ChunkHeader, node);
}

static inline BlockHeader* nodeToBlock(LdcLinkedNode* node)
{
    return VNContainerOf(node, BlockHeader, node);
}

static inline ChunkHeader* firstChunk(const BlockHeader* block)
{
    return (ChunkHeader*)((uint8_t*)block + kBlockHeaderSize);
}

static inline size_t* chunkFooterPointer(const ChunkHeader* chunk)
{
    return (size_t*)((uint8_t*)chunk + chunk->size - kChunkFooterSize);
}

static inline void writeChunkFooter(ChunkHeader* chunk)
{
    assert(((uintptr_t)chunk & (kMinimumAlignment - 1)) == 0);
    *chunkFooterPointer(chunk) = chunk->size;
}

static inline uintptr_t blockEndAddress(const BlockHeader* block)
{
    return (uintptr_t)block + block->size;
}

static inline ChunkHeader* nextChunk(const ChunkHeader* chunk)
{
    const uintptr_t nextAddress = (uintptr_t)chunk + chunk->size;
    if (nextAddress >= blockEndAddress(chunk->block)) {
        return NULL;
    }
    return (ChunkHeader*)nextAddress;
}

static inline ChunkHeader* prevChunk(const ChunkHeader* chunk)
{
    const BlockHeader* block = chunk->block;
    const uintptr_t firstAddress = (uintptr_t)firstChunk(block);
    const uintptr_t chunkAddress = (uintptr_t)chunk;

    if (chunkAddress == firstAddress) {
        return NULL;
    }

    assert(chunkAddress >= firstAddress + kChunkFooterSize);

    const size_t prevSize = *(size_t*)(chunkAddress - kChunkFooterSize);
    if (prevSize < VN_MINIMUM_CHUNK_SIZE) {
        return NULL;
    }

    const uintptr_t prevAddress = chunkAddress - prevSize;
    assert(prevAddress >= firstAddress);

    ChunkHeader* const previous = (ChunkHeader*)prevAddress;
    assert(previous->block == block);

    return previous;
}

static inline bool isChunkSuitable(const ChunkHeader* chunk, size_t size, size_t alignment)
{
    const uintptr_t chunkStart = (uintptr_t)chunk;
    const uintptr_t dataStart = chunkStart + kChunkHeaderSize;
    const uintptr_t alignedData = align(dataStart, alignment);
    const uintptr_t chunkEnd = chunkStart + chunk->size;
    return (alignedData + size) <= (chunkEnd - kChunkFooterSize);
}

static ChunkHeader* findSuitableChunk(const LdcMemorySimpleAllocator* simpleAllocator, size_t size,
                                      size_t alignment)
{
    // What should the chunk total size be?
    const size_t requiredSize = minimumChunkSizeForRequest(size, alignment);

    // Start  with free list for at least that size
    const size_t startIndex = freeListIndexForSize(requiredSize);
    assert(startIndex < kFreeListCount);

    for (size_t classIndex = startIndex; classIndex < kFreeListCount; ++classIndex) {
        const LdcLinkedList* freeList = &simpleAllocator->freeLists[classIndex];

        const LdcLinkedNode* const tailSentinel = (LdcLinkedNode*)&freeList->tail;
        for (LdcLinkedNode* node = freeList->head; node != tailSentinel; node = node->next) {
            ChunkHeader* chunk = nodeToChunk(node);
            if (isChunkSuitable(chunk, size, alignment)) {
                return chunk;
            }
        }
    }

    return NULL;
}

static void addFreeChunk(LdcMemorySimpleAllocator* simpleAllocator, ChunkHeader* chunk)
{
    // Can this be added to previous chunk?
    ChunkHeader* const previous = prevChunk(chunk);
    if (previous && isFree(previous)) {
        ldcLinkedRemove(&previous->node);
        previous->size += chunk->size;
        chunk = previous;
    }

    // Can this be added to next chunk?
    ChunkHeader* const next = nextChunk(chunk);
    if (next && isFree(next)) {
        ldcLinkedRemove(&next->node);
        chunk->size += next->size;
    }

    // Update size in the footer
    writeChunkFooter(chunk);

    // Add chunk into sorted position within the appropriate free list
    LdcLinkedList* const list = freeListForSize(simpleAllocator, chunk->size);

    LdcLinkedNode* const tailSentinel = (LdcLinkedNode*)&list->tail;
    LdcLinkedNode* insertBefore = tailSentinel;
    for (LdcLinkedNode* node = list->head; node != tailSentinel; node = node->next) {
        const ChunkHeader* const existing = nodeToChunk(node);
        if (chunk->size <= existing->size) {
            insertBefore = node;
            break;
        }
    }
    ldcLinkedInsertBefore(insertBefore, &chunk->node);
}

static void splitChunkTail(LdcMemorySimpleAllocator* simpleAllocator, ChunkHeader* chunk,
                           uintptr_t payloadStart, size_t payloadSize)
{
    const uintptr_t chunkStart = (uintptr_t)chunk;
    const size_t originalSize = chunk->size;

    const uintptr_t payloadEnd = payloadStart + payloadSize;
    size_t usedBytes = (size_t)((payloadEnd + kChunkFooterSize) - chunkStart);
    if (usedBytes > originalSize) {
        usedBytes = originalSize;
    }

    const size_t remaining = originalSize - usedBytes;
    if (remaining < VN_MINIMUM_CHUNK_SIZE) {
        // Tail is too small - leave chunk as is
        return;
    }

    // Update the footer of the allocated chunk
    chunk->size = usedBytes;
    writeChunkFooter(chunk);

    // Make the new tail chunk
    ChunkHeader* const tail = (ChunkHeader*)(chunkStart + usedBytes);
    tail->size = remaining;
    tail->block = chunk->block;
    writeChunkFooter(tail);

    // Add it to the free list
    addFreeChunk(simpleAllocator, tail);
}

static bool allocateBlock(LdcMemorySimpleAllocator* simpleAllocator, size_t size, size_t alignment)
{
    const size_t worstCasePadding = alignment - 1;
    const size_t minimumBlock =
        kBlockHeaderSize + kChunkHeaderSize + kChunkFooterSize + size + worstCasePadding;

    // Make sure allocated block is big enough
    const size_t requestedSize =
        (simpleAllocator->blockSize < minimumBlock) ? minimumBlock : simpleAllocator->blockSize;

    // Try and allocate block from parent allocator
    LdcMemoryAllocation blockAllocation = {0};
    VNAllocateAlignedArray(simpleAllocator->parentAllocator, &blockAllocation, uint8_t, kMinimumAlignment,
                           align(requestedSize, kMinimumAlignment), "SimpleAllocator_Block");

    if (!VNIsAllocated(blockAllocation)) {
        return false;
    }

    // Add block to block list
    BlockHeader* const block = (BlockHeader*)blockAllocation.ptr;
    block->size = blockAllocation.size;
    block->allocation = blockAllocation;
    ldcLinkedPushBack(&simpleAllocator->blockList, &block->node);

    // Create free chunk that covers while block
    ChunkHeader* const chunk = firstChunk(block);
    chunk->size = block->size - kBlockHeaderSize;
    chunk->block = block;
    writeChunkFooter(chunk);

    addFreeChunk(simpleAllocator, chunk);

    return true;
}

static void allocatorAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                              size_t size, size_t alignment, const LdcDiagSite* site)
{
    assert(allocator);
    assert(allocation);
    VNUnused(site);
    LdcMemorySimpleAllocator* simpleAllocator = (LdcMemorySimpleAllocator*)allocator->allocatorData;
    assert(simpleAllocator);

    VNClear(allocation);
    allocation->alignment = alignment;

    if (size == 0) {
        return;
    }

    threadMutexLock(&simpleAllocator->mutex);

    // Alignment is always at least kMinimumAlignment
    const size_t effectiveAlignment = (alignment < kMinimumAlignment) ? kMinimumAlignment : alignment;

    // Anything in current free lists?
    ChunkHeader* chunk = findSuitableChunk(simpleAllocator, size, effectiveAlignment);
    if (!chunk) {
        // No - add another block
        if (!allocateBlock(simpleAllocator, size, effectiveAlignment)) {
            threadMutexUnlock(&simpleAllocator->mutex);
            return;
        }

        // That should now be satisfied from the new block
        chunk = findSuitableChunk(simpleAllocator, size, effectiveAlignment);
        if (!chunk) {
            threadMutexUnlock(&simpleAllocator->mutex);
            return;
        }
    }

    // Take chunk out of free list
    ldcLinkedRemove(&chunk->node);

    const uintptr_t chunkStart = (uintptr_t)chunk;
    const uintptr_t dataStart = chunkStart + kChunkHeaderSize;
    const uintptr_t alignedData = align(dataStart, effectiveAlignment);
    assert(alignedData);

    const uintptr_t alignedSize = align(size, kMinimumAlignment);

    const uintptr_t chunkEnd = chunkStart + chunk->size;
    VNUnused(chunkEnd);
    assert(alignedData + alignedSize <= (chunkEnd - kChunkFooterSize));

    // Return unused end of block to free list
    splitChunkTail(simpleAllocator, chunk, alignedData, alignedSize);

    // Fill in allocation
    allocation->ptr = (void*)alignedData;
    allocation->size = size;
    allocation->allocatorData = (uintptr_t)chunk;

    threadMutexUnlock(&simpleAllocator->mutex);
}

static void allocatorFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                          const LdcDiagSite* site)
{
    assert(allocator);
    assert(allocation);
    VNUnused(site);
    LdcMemorySimpleAllocator* simpleAllocator = (LdcMemorySimpleAllocator*)allocator->allocatorData;
    assert(simpleAllocator);

    if (!VNIsAllocated(*allocation)) {
        return;
    }
    threadMutexLock(&simpleAllocator->mutex);

    ChunkHeader* chunk = (ChunkHeader*)allocation->allocatorData;
    assert(chunk);
    addFreeChunk(simpleAllocator, chunk);

    VNClear(allocation);

    threadMutexUnlock(&simpleAllocator->mutex);
}

static void allocatorReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                                size_t size, const LdcDiagSite* site)
{
    LdcMemorySimpleAllocator* simpleAllocator = (LdcMemorySimpleAllocator*)allocator->allocatorData;
    assert(simpleAllocator);

    // Simple case - just allocate
    if (!VNIsAllocated(*allocation)) {
        allocatorAllocate(allocator, allocation, size, allocation->alignment, site);
        return;
    }

    // Simple case - just free
    if (size == 0) {
        allocatorFree(allocator, allocation, site);
        return;
    }

    threadMutexLock(&simpleAllocator->mutex);

    // Got a real reallocate
    ChunkHeader* chunk = (ChunkHeader*)allocation->allocatorData;
    assert(chunk);

    const uintptr_t chunkStart = (uintptr_t)chunk;
    const uintptr_t payloadStart = (uintptr_t)allocation->ptr;
    const size_t payloadOffset = (size_t)(payloadStart - chunkStart);

    size_t currentCapacity = chunk->size - payloadOffset - kChunkFooterSize;

    const uintptr_t alignedSize = align(size, kMinimumAlignment);

    // Can current chunk be trimmed to new capacity?
    if (size <= currentCapacity) {
        splitChunkTail(simpleAllocator, chunk, payloadStart, alignedSize);
        allocation->size = size;
        threadMutexUnlock(&simpleAllocator->mutex);
        return;
    }

    // Try to add any free following chunks to meet required size
    ChunkHeader* next = nextChunk(chunk);
    while (next && isFree(next)) {
        ldcLinkedRemove(&next->node);
        chunk->size += next->size;
        writeChunkFooter(chunk);
        next = nextChunk(chunk);
        currentCapacity = chunk->size - payloadOffset - kChunkFooterSize;
        if (currentCapacity >= size) {
            break;
        }
    }

    currentCapacity = chunk->size - payloadOffset - kChunkFooterSize;
    if (currentCapacity >= size) {
        // Managed to extend existing chunk - trim remaining free tail
        splitChunkTail(simpleAllocator, chunk, payloadStart, alignedSize);
        allocation->size = size;
        threadMutexUnlock(&simpleAllocator->mutex);
        return;
    }

    threadMutexUnlock(&simpleAllocator->mutex);

    // Need to allocate and copy
    LdcMemoryAllocation newAllocation = {0};
    allocatorAllocate(allocator, &newAllocation, size, allocation->alignment, site);
    if (!VNIsAllocated(newAllocation)) {
        return;
    }

    // Copy contents
    const size_t copySize = allocation->size < size ? allocation->size : size;
    memcpy(newAllocation.ptr, allocation->ptr, copySize);

    // Free old allocation
    allocatorFree(allocator, allocation, site);

    // and update the allocation
    *allocation = newAllocation;
}

// Function table for LdcMemoryAllocator interface
static const LdcMemoryAllocatorFunctions kAllocatorFunctions = {
    allocatorAllocate,
    allocatorReallocate,
    allocatorFree,
};

LdcMemoryAllocator* ldcMemorySimpleAllocatorInitialize(LdcMemorySimpleAllocator* simpleAllocator,
                                                       LdcMemoryAllocator* parentAllocator)
{
    assert(simpleAllocator);
    assert(parentAllocator);

    simpleAllocator->allocator.functions = &kAllocatorFunctions;
    simpleAllocator->allocator.allocatorData = simpleAllocator;
    simpleAllocator->parentAllocator = parentAllocator;
    VNCheck(threadMutexInitialize(&simpleAllocator->mutex) == ThreadResultSuccess);

    ldcLinkedListInitialize(&simpleAllocator->blockList);
    for (size_t idx = 0; idx < kFreeListCount; ++idx) {
        ldcLinkedListInitialize(&simpleAllocator->freeLists[idx]);
    }
    simpleAllocator->blockSize = kDefaultBlockSize;

    return &simpleAllocator->allocator;
}

void ldcMemorySimpleAllocatorDestroy(LdcMemorySimpleAllocator* simpleAllocator)
{
    assert(simpleAllocator);

    const LdcLinkedList* blockList = &simpleAllocator->blockList;

    for (LdcLinkedNode* node = ldcLinkedFront(blockList); node != NULL; node = ldcLinkedFront(blockList)) {
        BlockHeader* block = nodeToBlock(node);

        ldcLinkedRemove(&block->node);

        LdcMemoryAllocation blockAllocation = block->allocation;
        VNFree(simpleAllocator->parentAllocator, &blockAllocation);
    }

    ldcLinkedListInitialize(&simpleAllocator->blockList);
    for (size_t idx = 0; idx < kFreeListCount; ++idx) {
        ldcLinkedListInitialize(&simpleAllocator->freeLists[idx]);
    }
}

// Checks LdcMemorySimpleAllocator data structures for internal consistency
//
bool ldcMemoryAllocatorSimpleDebugCheck(const LdcMemorySimpleAllocator* simpleAllocator)
{
    if (!simpleAllocator) {
        return false;
    }

    const LdcLinkedList* blockList = &simpleAllocator->blockList;
    const LdcLinkedNode* blockTailSentinel = (const LdcLinkedNode*)&blockList->tail;

    size_t totalFreeChunks = 0;
    size_t blockGuard = 0;

    for (LdcLinkedNode* node = blockList->head; node != blockTailSentinel; node = node->next) {
        if (!node || !node->next) {
            return false;
        }

        if (++blockGuard > (1u << 20)) {
            return false;
        }

        BlockHeader* block = nodeToBlock(node);
        if (!block) {
            return false;
        }

        if (block->size < kBlockHeaderSize + kChunkHeaderSize + kChunkFooterSize) {
            return false;
        }

        const uintptr_t blockStart = (uintptr_t)block;
        const uintptr_t blockEnd = blockEndAddress(block);

        size_t offset = kBlockHeaderSize;
        ChunkHeader* previousChunk = NULL;
        const size_t maxChunks = (block->size - kBlockHeaderSize) / VN_MINIMUM_CHUNK_SIZE + 1;
        size_t chunkGuard = 0;

        for (ChunkHeader* chunk = firstChunk(block); chunk != NULL; chunk = nextChunk(chunk)) {
            if (++chunkGuard > maxChunks) {
                return false;
            }

            if (chunk->block != block) {
                return false;
            }

            if (chunk->size < kChunkHeaderSize + kChunkFooterSize) {
                return false;
            }

            if (*chunkFooterPointer(chunk) != chunk->size) {
                return false;
            }

            const uintptr_t chunkStart = (uintptr_t)chunk;
            const uintptr_t chunkEnd = chunkStart + chunk->size;
            if (chunkStart < blockStart + kBlockHeaderSize || chunkEnd > blockEnd) {
                return false;
            }

            if (previousChunk != NULL) {
                size_t prevSize = *(size_t*)((uint8_t*)chunk - kChunkFooterSize);
                if (prevSize != previousChunk->size) {
                    return false;
                }

                if ((ChunkHeader*)((uint8_t*)chunk - prevSize) != previousChunk) {
                    return false;
                }
            }

            const bool chunkIsFree = isFree(chunk);
            if (chunkIsFree) {
                if (chunk->node.next == NULL || chunk->node.prev == NULL) {
                    return false;
                }

                if (chunk->node.prev->next != &chunk->node) {
                    return false;
                }

                if (chunk->node.next->prev != &chunk->node) {
                    return false;
                }

                ++totalFreeChunks;
            } else {
                if (chunk->node.next != NULL || chunk->node.prev != NULL) {
                    return false;
                }
            }

            offset += chunk->size;
            previousChunk = chunk;
        }

        if (offset != block->size) {
            return false;
        }
    }

    size_t freeCount = 0;
    for (size_t classIndex = 0; classIndex < kFreeListCount; ++classIndex) {
        const LdcLinkedList* freeList = &simpleAllocator->freeLists[classIndex];
        const LdcLinkedNode* freeTailSentinel = (const LdcLinkedNode*)&freeList->tail;
        size_t freeGuard = 0;

        for (LdcLinkedNode* node = freeList->head; node != freeTailSentinel; node = node->next) {
            if (!node || !node->next || !node->prev) {
                return false;
            }

            if (node->prev->next != node || node->next->prev != node) {
                return false;
            }

            if (++freeGuard > (totalFreeChunks + 1)) {
                return false;
            }

            ChunkHeader* chunk = nodeToChunk(node);
            if (!isFree(chunk)) {
                return false;
            }

            BlockHeader* block = chunk->block;
            if (!block) {
                return false;
            }

            const uintptr_t blockStart = (uintptr_t)block;
            const uintptr_t blockEnd = blockEndAddress(block);
            const uintptr_t chunkStart = (uintptr_t)chunk;
            const uintptr_t chunkEnd = chunkStart + chunk->size;
            if (chunkStart < blockStart + kBlockHeaderSize || chunkEnd > blockEnd) {
                return false;
            }

            ++freeCount;
        }
    }

    if (freeCount != totalFreeChunks) {
        return false;
    }

    return true;
}
