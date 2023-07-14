/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "common/memory.h"

#include "common/log.h"
#include "common/threading.h"
#include "common/types.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

typedef struct MemoryTrace MemoryTrace_t;

typedef struct Memory
{
    MemoryTrace_t* trace;
    void* userData;
    AllocateFunction_t allocFn;
    AllocateZeroFunction_t allocZeroFn;
    FreeFunction_t freeFn;
    ReallocFunction_t reallocateFn;
}* Memory_t;

typedef enum MemoryAllocationType
{
    MATAlloc,
    MATAllocZero,
    MATRealloc,
    MATCount
} MemoryAllocationType_t;

/*------------------------------------------------------------------------------*/

#if VN_CORE_FEATURE(TRACE_MEMORY)

enum MemoryTraceConstants
{
    MTC_PreAllocatedNodeCount = 16384,
    MTC_HistogramSize = 64
};

/* clang-format off */

static const char* kHistogramRangeStrings[MTC_HistogramSize] =
{
	"1 -> 2:",         "2 -> 4:",          "4 -> 8:",           "8 -> 16:",          "16 -> 32:",
	"32 -> 64:",       "64 -> 128:",       "128 -> 256:",       "256 -> 512:",       "512 -> 1KiB:",

	"1KiB -> 2KiB:",   "2KiB -> 4kiB:",    "4KiB -> 8KiB:",     "8KiB -> 16KiB:",    "16KiB -> 32KiB:", 
	"32KiB -> 64KiB:", "64KiB -> 128KiB:", "128KiB -> 256KiB:", "256KiB -> 512KiB:", "512KiB -> 1MiB:", 

	"1MiB -> 2MiB:",   "2MiB -> 4MiB:",    "4MiB -> 8MiB:",     "8MiB -> 16MiB:",    "16MiB -> 32MiB:",
	"32MiB -> 64MiB:", "64MiB -> 128MiB:", "128MiB -> 256MiB:", "256MiB -> 512MiB:", "512MiB -> 1GiB:",

	"1GiB -> 2GiB:",   "2GiB -> 4GiB:",    "4GiB -> 8GiB:",     "8GiB -> 16GiB:",    "16GiB -> 32GiB:",
	"32GiB -> 64GiB:", "64GiB -> 128GiB:", "128GiB -> 256GiB:", "256GiB -> 512GiB:", "512GiB -> 1TiB:",

	/* Above TiB not required. */
	"","","","","","","","","", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};

typedef struct MemoryNode MemoryNode_t;

/*! This stores details about an individual allocation, and maintains an intrusive
 *  linked list. */
typedef struct MemoryNode
{
	void*                  pointer; /**< Pointer to the allocation memory */
	const char*            file;    /**< File where allocation was performed */
	uint32_t               line;    /**< Line in the file */
	size_t                 size;    /**< Byte size of the allocation */
	MemoryAllocationType_t type;    /**< Type of allocation performed */
	MemoryNode_t*          next;    /**< Intrusive linked list. */
} MemoryNode_t;

/*! Caches nodes to prevent excessive allocations of nodes. */
typedef struct MemoryNodeCache
{
	Memory_t      memory;                                   /**< Owning memory object. */
	MemoryNode_t  preallocNodes[MTC_PreAllocatedNodeCount]; /**< Initial list of available nodes for tracing with. */
	size_t        preallocIndex;                            /**< Index into `preallocNodes` for the next free node. */
	MemoryNode_t* freeNodes;                                /**< Head pointer to a linked list of available nodes. */
	size_t        heapAllocatedNodeCount;                   /**< Number of nodes allocated on the heap after preallocation has been fully utilised. */
} MemoryNodeCache_t;

/*! Stores statistics about a particular type of allocation. */
typedef struct MemoryAllocationStats
{
	size_t currentSize;                  /**< Current cumulative allocation size */
	size_t peakSize;                     /**< Peak current cumulative allocation size */
	size_t totalSize;                    /**< Total cumulative allocation size */
	size_t maxSize;                      /**< Maximum individual allocation size */
	size_t minSize;                      /**< Minimum individual allocation size */
	size_t count;                        /**< Number of individual allocations */
	size_t histogram[MTC_HistogramSize]; /**< Cumulative allocation sizes across power of 2 ranges. */
} MemoryAllocationStats_t;

typedef struct MemoryTrace
{
	Memory_t                memory;           /**< Owning memory object. */
	MemoryNodeCache_t       nodeCache;        /**< Storage for node entries */
	MemoryNode_t*           records;          /**< Head pointer to a linked list of details to currently active allocations */
	MemoryAllocationStats_t stats[MATCount];  /**< Stats for tracking memory allocation details on a per-type basis. */
	size_t                  currentTotalSize; /**< Current cumulative allocation size across all stats. */
	size_t                  peakTotalSize;    /**< Peak current cumulation allocation size across all stats. */
	bool                    enableMutex;      /**< If true the mutex is used to protect state mutation within this struct. */
    bool                    enableTracing;    /**< If true tracing will be performed for any allocations (both allocation and free). */
	Mutex_t*                mutex;            /**< Used to protect this struct in a threaded environment. */
} MemoryTrace_t;

/* clang-format on */

/*------------------------------------------------------------------------------*/

/*! Prepares the node cache. */
static void nodeCacheInitialise(MemoryNodeCache_t* cache, Memory_t memory)
{
    cache->memory = memory;
}

/*! Releases any resources allocated by the cache. */
static void nodeCacheRelease(MemoryNodeCache_t* cache)
{
    const MemoryNode_t* preallocBegin = cache->preallocNodes;
    const MemoryNode_t* preallocEnd = preallocBegin + MTC_PreAllocatedNodeCount;
    Memory_t memory = cache->memory;

    MemoryNode_t* node = cache->freeNodes;
    while (node) {
        MemoryNode_t* next = node->next;

        /* Ensure to only free heap allocated free-standing nodes. */
        if (node < preallocBegin || node >= preallocEnd) {
            memory->freeFn(memory->userData, node);
        }

        node = next;
    }
}

/*! \brief Retrieve a node from the cache.
 *
 *  This may perform a heap allocation if the cache runs out of preallocated nodes. */
static MemoryNode_t* nodeCacheGet(MemoryNodeCache_t* cache)
{
    if (cache->freeNodes) {
        /* Pop head of the list and return it. */
        MemoryNode_t* res = cache->freeNodes;
        cache->freeNodes = res->next;
        return res;
    } else if (cache->preallocIndex < MTC_PreAllocatedNodeCount) {
        return &cache->preallocNodes[cache->preallocIndex++];
    }

    /* Need to heap alloc a new node. */
    Memory_t memory = cache->memory;
    cache->heapAllocatedNodeCount++;
    return (MemoryNode_t*)memory->allocFn(memory->userData, sizeof(MemoryNode_t));
}

/*! Return a node back to the cache.
 *
 * This stores nodes on a free-list so they may be recycled. */
static void nodeCacheReturn(MemoryNodeCache_t* cache, MemoryNode_t* node)
{
    /* Push onto the head of the list. */
    node->next = cache->freeNodes;
    cache->freeNodes = node->next;
}

/*! Records stats for the memory allocation. */
static void memoryStatsAdd(MemoryTrace_t* trace, const MemoryNode_t* node)
{
    MemoryAllocationStats_t* stats = &trace->stats[node->type];

    stats->currentSize += node->size;
    stats->totalSize += node->size;
    stats->peakSize = maxSize(stats->peakSize, stats->currentSize);
    stats->minSize = minSize(stats->minSize, node->size);
    stats->maxSize = maxSize(stats->maxSize, node->size);
    stats->count++;

    trace->currentTotalSize += node->size;
    trace->peakTotalSize = maxSize(trace->peakTotalSize, trace->currentTotalSize);

    /* Store histogram */
    const size_t index = bitScanReverse(node->size);
    assert(index < MTC_HistogramSize);
    stats->histogram[index]++;
}

/*! Removes any ephemeral stats for the memory allocation. */
static void memoryStatsRemove(MemoryTrace_t* trace, const MemoryNode_t* node)
{
    MemoryAllocationStats_t* stats = &trace->stats[node->type];

    assert(stats->currentSize >= node->size);
    stats->currentSize -= node->size;
    trace->currentTotalSize -= node->size;
}

/* @todo(bob): Reporting mechanism needs to go via user supplied log messages. */

static const char* memoryAllocationTypeToString(MemoryAllocationType_t type)
{
    switch (type) {
        case MATAlloc: return "malloc";
        case MATAllocZero: return "calloc";
        case MATRealloc: return "realloc";
        case MATCount: break;
    }

    return "unknown";
}

static double bytesToMiB(size_t size)
{
    static const double scale = 1.0 / (1024.0 * 1024.0);
    return (double)size * scale;
}

static void memoryTraceLock(MemoryTrace_t* trace)
{
    if (!trace->enableMutex) {
        return;
    }

    mutexLock(trace->mutex);
}

static void memoryTraceUnlock(MemoryTrace_t* trace)
{
    if (!trace->enableMutex) {
        return;
    }

    mutexUnlock(trace->mutex);
}

static void memoryTraceInitialise(Memory_t memory, MemoryTrace_t** trace)
{
    MemoryTrace_t* ptr = (MemoryTrace_t*)memory->allocFn(memory->userData, sizeof(MemoryTrace_t));

    if (!ptr) {
        *trace = NULL;
        return;
    }

    memorySet(ptr, 0, sizeof(MemoryTrace_t));

    for (int32_t i = 0; i < MATCount; ++i) {
        MemoryAllocationStats_t* stats = &ptr->stats[i];
        stats->minSize = SIZE_MAX;
    }

    nodeCacheInitialise(&ptr->nodeCache, memory);

    ptr->memory = memory;
    ptr->enableMutex = true;

    *trace = ptr;

    ptr->enableTracing = false;
    mutexInitialise(memory, &ptr->mutex);
    ptr->enableTracing = true;
}

static void memoryTraceRelease(MemoryTrace_t* trace)
{
    if (trace == NULL) {
        return;
    }

    /* Prevent mutex utilization once we're releasing as there's a circular
     * dependency between releasing mutex memory and taking the mutex lock to
     * untrack that allocation. At this point we expect this to be called from
     * the main thread so it's safe to disable thread safety now. */
    trace->enableMutex = false;

    /* Move outstanding nodes back to the cache. */
    MemoryNode_t* node = trace->records;
    while (node) {
        MemoryNode_t* next = node->next;
        nodeCacheReturn(&trace->nodeCache, node);
        node = next;
    }

    nodeCacheRelease(&trace->nodeCache);

    trace->enableTracing = false;
    mutexRelease(trace->mutex);
    Memory_t memory = trace->memory;
    memory->freeFn(memory->userData, trace);
}

static void memoryReportStats(const MemoryAllocationStats_t* stats, MemoryAllocationType_t type,
                              Logger_t* log)
{
    const size_t averageSize = stats->count ? (stats->totalSize / stats->count) : 0;
    const size_t minSize = stats->count > 0 ? stats->minSize : 0;

    VN_DEBUG(log, "Allocation Stats [%s]\n", memoryAllocationTypeToString(type));
    VN_DEBUG(log, "  Peak size:      %-10zu [%7.4fMiB]\n", stats->peakSize, bytesToMiB(stats->peakSize));
    VN_DEBUG(log, "  Total size:     %-10zu [%7.4fMiB]\n", stats->totalSize, bytesToMiB(stats->totalSize));
    VN_DEBUG(log, "  Min alloc size: %-10zu [%7.4fMiB]\n", minSize, bytesToMiB(minSize));
    VN_DEBUG(log, "  Max alloc size: %-10zu [%7.4fMiB]\n", stats->maxSize, bytesToMiB(stats->maxSize));
    VN_DEBUG(log, "  Avg alloc size: %-10zu [%7.4fMiB]\n", averageSize, bytesToMiB(averageSize));
    VN_DEBUG(log, "  Count:          %zu\n", stats->count);

    if (stats->count) {
        VN_DEBUG(log, "  Histogram:\n");
        for (int32_t i = 0; i < MTC_HistogramSize; ++i) {
            if (stats->histogram[i]) {
                VN_DEBUG(log, "    %-17s %zu\n", kHistogramRangeStrings[i], stats->histogram[i]);
            }
        }
    }
}

static bool memoryTraceHasLeaks(MemoryTrace_t* trace, Logger_t* log)
{
    /* Log is a special case since we don't free it until after we report (as reporting is
     * performed over the log interface). */
    return trace->records && ((trace->records->pointer != log) || (trace->records->next != NULL));
}

static void memoryTraceReport(MemoryTrace_t* trace, Logger_t log)
{
    size_t totalSize = 0;
    for (int32_t i = 0; i < MATCount; i++) {
        const MemoryAllocationStats_t* stats = &trace->stats[i];
        totalSize += stats->totalSize;
        memoryReportStats(stats, (MemoryAllocationType_t)i, log);
    }

    VN_DEBUG(log, "Allocation Stats [total]\n");
    VN_DEBUG(log, "  Peak size:      %-10zu [%7.4fMiB]\n", trace->peakTotalSize,
             bytesToMiB(trace->peakTotalSize));
    VN_DEBUG(log, "  Total size:     %-10zu [%7.4fMiB]\n", totalSize, bytesToMiB(totalSize));
    VN_DEBUG(log, "Node cache stats: [pre-alloc: %zu, heap: %zu]\n", trace->nodeCache.preallocIndex,
             trace->nodeCache.heapAllocatedNodeCount);

    if (memoryTraceHasLeaks(trace, log)) {
        VN_DEBUG(log, "Memory leaks detected\n");

        MemoryNode_t* node = trace->records;

        while (node) {
            VN_DEBUG(log, "%s(%u): Leak of size %zu bytes [%s]\n", node->file, node->line,
                     node->size, memoryAllocationTypeToString(node->type));
            node = node->next;
        }
    } else {
        printf("No memory leaks detected\n");
    }
}

static void* memoryTraceRecordAllocation(MemoryTrace_t* trace, void* ptr, const char* file,
                                         uint32_t line, size_t size, MemoryAllocationType_t type)
{
    if (!trace || !ptr || !trace->enableTracing) {
        return ptr;
    }

    MemoryNode_t* node = nodeCacheGet(&trace->nodeCache);

    if (node) {
        memoryTraceLock(trace);

        node->file = file;
        node->line = line;
        node->pointer = ptr;
        node->size = size;
        node->next = trace->records;
        node->type = type;
        trace->records = node;

        memoryStatsAdd(trace, node);
        memoryTraceUnlock(trace);
    }

    return ptr;
}

static void memoryTraceRemoveAllocation(MemoryTrace_t* trace, void* ptr)
{
    if (!trace || !ptr || !trace->enableTracing) {
        return;
    }

    memoryTraceLock(trace);

    MemoryNode_t* node = trace->records;
    MemoryNode_t* lastNode = NULL;

    while (node) {
        if (node->pointer == ptr) {
            if (lastNode) {
                lastNode->next = node->next;
            } else {
                assert(node == trace->records);
                trace->records = node->next;
            }

            memoryStatsRemove(trace, node);
            nodeCacheReturn(&trace->nodeCache, node);
            break;
        }

        lastNode = node;
        node = node->next;
    }

    memoryTraceUnlock(trace);
}

#else

static void memoryTraceInitialise(Memory_t memory, MemoryTrace_t** trace)
{
    VN_UNUSED(memory);
    VN_UNUSED(trace);
}

static void memoryTraceRelease(MemoryTrace_t* trace) { VN_UNUSED(trace); }

static void memoryTraceReport(MemoryTrace_t* trace, Logger_t log)
{
    VN_UNUSED(trace);
    VN_UNUSED(log);
}

static void* memoryTraceRecordAllocation(MemoryTrace_t* trace, void* ptr, const char* file,
                                         uint32_t line, size_t size, MemoryAllocationType_t type)
{
    VN_UNUSED(trace);
    VN_UNUSED(file);
    VN_UNUSED(line);
    VN_UNUSED(size);
    VN_UNUSED(type);
    return ptr;
}

static void memoryTraceRemoveAllocation(MemoryTrace_t* trace, void* ptr)
{
    VN_UNUSED(trace);
    VN_UNUSED(ptr);
}

#endif

/*------------------------------------------------------------------------------*/

static bool memoryValidateUserFunctions(const MemorySettings_t* settings)
{
    /* A user must supply either all or none of the function pointers (excepting
     * AllocateZero - that will be simulated if user allocate is present.s */
    const bool hasUserAllocate = settings->userAllocate != NULL;
    const bool hasUserFree = settings->userFree != NULL;
    const bool hasUserReallocate = settings->userReallocate != NULL;

    /* Allocate zero is optional. */
    return hasUserAllocate == hasUserFree && hasUserAllocate == hasUserReallocate;
}

static void* wrapperMalloc(void* userData, size_t size)
{
    VN_UNUSED(userData);
    return malloc(size);
}

static void* wrapperCalloc(void* userData, size_t size)
{
    VN_UNUSED(userData);
    return calloc(1, size);
}

static void* wrapperRealloc(void* userData, void* ptr, size_t size)
{
    VN_UNUSED(userData);
    return realloc(ptr, size);
}

static void wrapperFree(void* userData, void* ptr)
{
    VN_UNUSED(userData);
    free(ptr);
}

bool memoryInitialise(Memory_t* handle, const MemorySettings_t* settings)
{
    if (!memoryValidateUserFunctions(settings)) {
        return false;
    }

    const AllocateFunction_t allocFunc = settings->userAllocate ? settings->userAllocate : &wrapperMalloc;
    const size_t memorySize = sizeof(struct Memory);
    Memory_t memory = (Memory_t)allocFunc(settings->userData, memorySize);

    if (!memory) {
        return false;
    }

    memorySet(memory, 0, memorySize);

    memory->userData = settings->userData;
    memory->allocFn = allocFunc;
    memory->freeFn = settings->userFree ? settings->userFree : &wrapperFree;
    memory->reallocateFn = settings->userReallocate ? settings->userReallocate : &wrapperRealloc;

    if (settings->userAllocateZero) {
        memory->allocZeroFn = settings->userAllocateZero;
    } else if (!settings->userAllocate) {
        memory->allocZeroFn = &wrapperCalloc;
    } else {
        /* This will fall back to calling alloc and memset. */
        memory->allocZeroFn = NULL;
    }

    memoryTraceInitialise(memory, &memory->trace);

    *handle = (Memory_t)memory;

    return true;
}

void memoryRelease(Memory_t memory)
{
    memoryTraceRelease(memory->trace);
    memory->freeFn(memory->userData, memory);
}

void memoryReport(Memory_t memory, Logger_t log) { memoryTraceReport(memory->trace, log); }

void* memoryAllocate(Memory_t memory, size_t size, bool zero, const char* file, uint32_t line)
{
    void* ptr = NULL;

    if (zero) {
        if (memory->allocZeroFn) {
            ptr = memory->allocZeroFn(memory->userData, size);
        } else {
            assert(memory->allocFn);
            ptr = memory->allocFn(memory->userData, size);
            memorySet(ptr, 0, size);
        }
    } else {
        ptr = memory->allocFn(memory->userData, size);
    }

    return memoryTraceRecordAllocation(memory->trace, ptr, file, line, size, zero ? MATAllocZero : MATAlloc);
}

void* memoryReallocate(Memory_t memory, void* ptr, size_t size, const char* file, uint32_t line)
{
    void* newPtr = memory->reallocateFn(memory->userData, ptr, size);

    if (!newPtr) {
        return NULL;
    }

    memoryTraceRemoveAllocation(memory->trace, ptr);
    memoryTraceRecordAllocation(memory->trace, newPtr, file, line, size, MATRealloc);

    return newPtr;
}

void memoryFree(Memory_t memory, void** ptr)
{
    memoryTraceRemoveAllocation(memory->trace, *ptr);
    memory->freeFn(memory->userData, *ptr);
    *ptr = NULL;
}

void memoryCopy(void* dst, const void* src, size_t size)
{
    /* NOLINTNEXTLINE */
    memcpy(dst, src, size);
}

void memorySet(void* dst, int32_t value, size_t size)
{
    /* NOLINTNEXTLINE */
    memset(dst, value, size);
}

/*------------------------------------------------------------------------------*/