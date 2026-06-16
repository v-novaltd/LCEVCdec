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

#ifndef VN_LCEVC_COMMON_MEMORY_H
#define VN_LCEVC_COMMON_MEMORY_H

#include <LCEVC/common/platform.h>
//
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// NOLINTBEGIN(modernize-use-using)
// NOLINTBEGIN(cppcoreguidelines-avoid-do-while)

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct LdcDiagSite LdcDiagSite;

/*! @file
 * @brief Dynamic memory functionality.
 *
 * The underlying heap allocation is provided by an instance of MemoryAllocator and MemoryFunctions.
 *
 * If supported by the target, an implementation that uses the standard C library can be retrieved
 * using LdcMemoryAllocatorMalloc()
 *
 * For all allocation functions the allocation may fail, and the user must check for this and react accordingly.
 */

/*!
 * Record of an allocation, possibly empty.
 *
 * The initial values of `allocationData`, `ptr`, and `size` should be zeros to mark it as empty.
 *
 * This can be moved around by client - allocators will not rely on the allocation structure
 * staying at the same address.
 */
struct LdcMemoryAllocator;

typedef struct
{
    void* ptr;               /**< Pointer to allocated data, or NULL if empty  */
    size_t size;             /**< Size in bytes to allocated data, or 0 if empty  */
    size_t alignment;        /**< Alignment required for this allocation, or 0 for default  */
    uintptr_t allocatorData; /**< Opaque data for use by allocator */

#if VN_SDK_FEATURE(MEMORY_DIAGNOSTICS)
    const LdcDiagSite* site; /**< Diagnostic site where allocation was made */
#endif
} LdcMemoryAllocation;

/*!
 * Memory allocation functions
 */
typedef struct
{
    /*!
     * Allocate a block of memory of given size, using aligned allocation - record details in
     * 'allocation'. `allocation->ptr` will be set to NULL if allocation failed.
     *
     * @param[in]       allocator     The memory allocator to allocate with.
     * @param[inout]    allocation    Details of allocation are written back to here.
     * @param[in]       size          The number of bytes to allocate.
     * @param[in]       site          If not null, the static diagnostic site for the allocation.
     */
    void (*allocate)(struct LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                     size_t size, size_t alignment, const LdcDiagSite* site);
    /*!
     * Adjust an allocation, given a new size. Any previous data is copied to new block, up to the minimum of the old and new sizes.
     * `allocation->ptr` will set to NULL if the reallocation failed.
     *
     * @param[in]       allocator     The memory allocator to allocate with.
     * @param[inout]    allocation    Details of allocation are to be adjusted.
     * @param[in]       size          The new size of the allocation.
     * @param[in]       site          If not null, the static diagnostic site for the reallocation.
     */
    void (*reallocate)(struct LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                       size_t size, const LdcDiagSite* site);
    /*!
     * Release an allocation
     *
     * Any allocated block will be freed, and the allocation marked as empty.
     *
     * @param[in]       allocator     The memory allocator to free with.
     * @param[inout]    allocation    Details of allocation to be freed.
     * @param[in]       site          If not null, the static diagnostic site for the free.
     */
    void (*free)(struct LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                 const LdcDiagSite* site);
} LdcMemoryAllocatorFunctions;

/*! MemoryAllocator
 *
 * Common part of memory allocation interface
 */
typedef struct LdcMemoryAllocator
{
    const LdcMemoryAllocatorFunctions* functions; /**< Function table of allocator operations */
    void* allocatorData;                          /**< Data pointer for use by allocator     */
} LdcMemoryAllocator;

/*------------------------------------------------------------------------------*/

/*!
 * Perform a dynamic memory allocation.
 *
 * If successful this function will allocate at least `size` bytes of memory, aligned as specified in `allocation->alignment`.
 * The pointer to the allocated memory will be recorded in `allocation->ptr`, and the size in `allocation->size`.
 *
 * @param[in]       allocator     The memory allocator to allocate with.
 * @param[inout]    allocation    Details of allocation are written back to here
 * @param[in]       size          The number of bytes to allocate.
 * @param[in]       alignment     The alignment for this block - must be a 0 (default) , or a power of 2
 * @param[in]       clearToZero   True if memory should be cleared to zero
 * @param[in]       diagSite      If not null, a pointer to a static LdcDiagSite`
 * @param[in]       diagId        A 64 bit id to be attached to diagnostics
 */
void ldcMemoryAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation, size_t size,
                       size_t alignment, bool clearToZero, const LdcDiagSite* diagSite, uint64_t diagId);

/*!
 * Perform a dynamic memory reallocation.
 *
 * If successful this function will allocate at least `size` bytes of memory, aligned as specified in `allocation->alignment`.
 * The pointer to the allocated memory will be recorded in `allocation->ptr`, and the size in `allocation->size`.
 *
 * If the given allocation already has an associated block of memory, then it will be reallocated, and
 * the contents copied to any new block (up to the minimum of the two block sizes)
 *
 * If `size` is zero, then no new block will be allocated, and the allocation will be left empty.
 *
 * @param[in]       allocator     The memory allocator to allocate with.
 * @param[inout]    allocation    Details of allocation are to be adjusted
 * @param[in]       size          The number of bytes to allocate.
 * @param[in]       diagSite      If not null, a pointer to a static `LdcDiagSite`
 * @param[in]       diagId        A 64 bit id to be attached to diagnostics
 */
void ldcMemoryReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                         size_t size, const LdcDiagSite* diagSite, uint64_t diagId);

/*!
 * Perform dynamic memory freeing.
 *
 * Any allocated block will be freed, and the allocation marked as empty.
 *
 * @param[in]       allocator     The memory allocator to free with.
 * @param[inout]    allocation    Details of allocation to be freed.
 * @param[in]       diagSite      If not null, a pointer to a static `LdcDiagSite`
 * @param[in]       diagId        A 64 bit id to be attached to diagnostics
 */
void ldcMemoryFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                   const LdcDiagSite* diagSite, uint64_t diagId);

/*! Get a wrapper for the standard C library heap allocator, if supported.
 *
 * @return          A pointer to an allocator that uses the standard C library malloc/free entry points, or NULL if not supported.
 */
LdcMemoryAllocator* ldcMemoryAllocatorMalloc(void);

/* clang-format off */

#if !VN_SDK_FEATURE(MEMORY_DIAGNOSTICS)
/** Helper for performing malloc for a single object. */
#define VNAllocate(allocator, allocation, type, debugName) do {                           \
    ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), false, NULL, 0); \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateArray(allocator, allocation, type, count, debugName) do {                             \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), false, NULL, 0); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateZero(allocator, allocation, type, debugName) do {                          \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), true, NULL, 0); \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateZeroArray(allocator, allocation, type, count, debugName) do {                        \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), true, NULL, 0); \
    } while(false)

/** Helper for performing malloc for a single object. */
#define VNAllocateAligned(allocator, allocation, type, align, debugName) do         \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), align, false, NULL, 0); \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateAlignedArray(allocator, allocation, type, align, count, debugName) do {     \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, false, NULL, 0); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateAlignedZero(allocator, allocation, type, align, debugName)  do { \
    ldcMemoryAllocate(allocator, allocation, sizeof(type), align, true, NULL, 0);     \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateAlignedZeroArray(allocator, allocation, type, align, count, debugName)  do { \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, true, NULL, 0);  \
    } while(false)

/** Helper for performing realloc for a single object. */
#define VNReallocate(allocator, allocation, type, debugName)  do {                    \
        ldcMemoryReallocate(allocator, allocation, (void*)(ptr), sizeof(type), NULL, 0); \
    } while(false)

/** Helper for performing realloc for an array of objects. */
#define VNReallocateArray(allocator, allocation, type, count, debugName)  do {    \
        ldcMemoryReallocate(allocator, allocation, sizeof(type) * (count), NULL, 0); \
    } while(false)

/** Helper for freeing an allocation performed with one of the above macros. */
#define VNFree(allocator, allocation) do { \
        ldcMemoryFree(allocator, allocation, NULL, 0); \
    } while(false)

// Versions of 'Allocate...' than can associate a 64 bit ID with the diagnostics - ignored
// when memory diagnostics are turned off.
//
/** Helper for performing malloc for a single object. */
#define VNAllocateId(allocator, allocation, type, debugName, id) do {                           \
    ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), false, NULL, 0); \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateIdArray(allocator, allocation, type, count, debugName, id) do {                             \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), false, NULL, 0); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateIdZero(allocator, allocation, type, debugName, id) do {                          \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), true, NULL, 0); \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateIdZeroArray(allocator, allocation, type, count, debugName, id) do {                        \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), true, NULL, 0); \
    } while(false)

/** Helper for performing malloc for a single object. */
#define VNAllocateIdAligned(allocator, allocation, type, align, debugName, id) do         \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), align, false, NULL, 0); \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateIdAlignedArray(allocator, allocation, type, align, count, debugName, id) do {     \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, false, NULL, 0); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateIdAlignedZero(allocator, allocation, type, align, debugName, id)  do { \
    ldcMemoryAllocate(allocator, allocation, sizeof(type), align, true, NULL, 0);     \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateIdAlignedZeroArray(allocator, allocation, type, align, count, debugName, id)  do { \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, true, NULL, 0);  \
    } while(false)

/** Helper for performing realloc for a single object. */
#define VNReallocateId(allocator, allocation, type, debugName, id)  do {                    \
        ldcMemoryReallocate(allocator, allocation, (void*)(ptr), sizeof(type), NULL, 0); \
    } while(false)

/** Helper for performing realloc for an array of objects. */
#define VNReallocateIdArray(allocator, allocation, type, count, debugName, id)  do {    \
        ldcMemoryReallocate(allocator, allocation, sizeof(type) * (count), NULL, 0); \
    } while(false)

/** Helper for freeing an allocation performed with one of the above macros. */
#define VNFreeId(allocator, allocation, id) do { \
        ldcMemoryFree(allocator, allocation, NULL, 0); \
    } while(false)
#else

// Versions of above that record calls site and debug name for allocation
//

#define _VNAllocSite(type, debugName) \
        static const LdcDiagArg args_[] = {LdcDiagArgId, LdcDiagArgUInt32, LdcDiagArgVoidPtr}; \
        static const LdcDiagSite site_ = {type,  __FILE__, __LINE__,                                     \
                                          LdcLogLevelNone, debugName,      3,         \
                                          args_, NULL,   LdcDiagArgNone};                              \

#define _VNFreeSite(type) \
        static const LdcDiagArg args_[] = {LdcDiagArgId, LdcDiagArgUInt32, LdcDiagArgVoidPtr}; \
        static const LdcDiagSite site_ = {type,  __FILE__, __LINE__,                                     \
                                          LdcLogLevelNone, "",      3,         \
                                          args_, NULL,   LdcDiagArgNone};                              \

#define _VNReallocSite(type, debugName) \
        static const LdcDiagArg args_[] = {LdcDiagArgId, LdcDiagArgUInt32, LdcDiagArgVoidPtr, LdcDiagArgVoidPtr}; \
        static const LdcDiagSite site_ = {type,  __FILE__, __LINE__,                                     \
                                          LdcLogLevelNone, debugName,      4,         \
                                          args_, NULL,   LdcDiagArgNone};                              \

/** Helper for performing malloc for a single object. */
#define VNAllocate(allocator, allocation, type, debugName) do {                               \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), false, &site_, 0);   \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateArray(allocator, allocation, type, count, debugName) do {                               \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), false, &site_, 0); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateZero(allocator, allocation, type, debugName) do {                            \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), true, &site_, 0); \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateZeroArray(allocator, allocation, type, count, debugName) do {                          \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), true, &site_, 0); \
    } while(false)

/** Helper for performing malloc for a single object. */
#define VNAllocateAligned(allocator, allocation, type, align, debugName) do           \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), align, false, &site_, 0); \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateAlignedArray(allocator, allocation, type, align, count, debugName) do {       \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, false, &site_, 0); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateAlignedZero(allocator, allocation, type, align, debugName)  do { \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
    ldcMemoryAllocate(allocator, allocation, sizeof(type), align, true, &site_, 0);   \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateAlignedZeroArray(allocator, allocation, type, align, count, debugName)  do { \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, true, &site_, 0); \
    } while(false)

/** Helper for performing realloc for a single object. */
#define VNReallocate(allocator, allocation, type, debugName)  do {                      \
        _VNReallocSite(LdcDiagTypeMemoryReallocate, debugName) \
        ldcMemoryReallocate(allocator, allocation, (void*)(ptr), sizeof(type), &site_, 0); \
    } while(false)

/** Helper for performing realloc for an array of objects. */
#define VNReallocateArray(allocator, allocation, type, count, debugName)  do {      \
        _VNReallocSite(LdcDiagTypeMemoryReallocate, debugName) \
        ldcMemoryReallocate(allocator, allocation, sizeof(type) * (count), &site_, 0); \
    } while(false)

/** Helper for freeing an allocation performed with one of the above macros. */
#define VNFree(allocator, allocation) do {              \
        _VNFreeSite(LdcDiagTypeMemoryFree) \
        ldcMemoryFree(allocator, allocation, &site_, 0);       \
    } while(false)


// Versions of 'Allocate...' than can associate a 64 bit ID with the diagnostics
//
/** Helper for performing malloc for a single object. */
#define VNAllocateId(allocator, allocation, type, debugName, id) do {                               \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), false, &site_, id);   \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateIdArray(allocator, allocation, type, count, debugName, id) do {                               \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), false, &site_, id); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateIdZero(allocator, allocation, type, debugName, id) do {                            \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), true, &site_, id); \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateIdZeroArray(allocator, allocation, type, count, debugName, id) do {                          \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), true, &site_, id); \
    } while(false)

/** Helper for performing malloc for a single object. */
#define VNAllocateIdAligned(allocator, allocation, type, align, debugName, id) do           \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), align, false, &site_, id); \
    } while(false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateIdAlignedArray(allocator, allocation, type, align, count, debugName, id) do {       \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, false, &site_, id); \
    } while(false)

/** Helper for performing calloc for a single object. */
#define VNAllocateIdAlignedZero(allocator, allocation, type, align, debugName, id)  do { \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type), align, true, &site_, id);   \
    } while(false)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateIdAlignedZeroArray(allocator, allocation, type, align, count, debugName, id)  do { \
        _VNAllocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, true, &site_, id); \
    } while(false)

/** Helper for performing realloc for a single object. */
#define VNReallocateId(allocator, allocation, type, debugName, id)  do {                      \
        _VNReallocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryReallocate(allocator, allocation, (void*)(ptr), sizeof(type), &site_, id); \
    } while(false)

/** Helper for performing realloc for an array of objects. */
#define VNReallocateIdArray(allocator, allocation, type, count, debugName, id)  do {      \
        _VNReallocSite(LdcDiagTypeMemoryAllocate, debugName) \
        ldcMemoryReallocate(allocator, allocation, sizeof(type) * (count), &site_, id); \
    } while(false)

/** Helper for freeing an allocation performed with one of the above macros. */
#define VNFreeId(allocator, allocation, id) do {              \
        _VNFreeSite(LdcDiagTypeMemoryFree) \
        ldcMemoryFree(allocator, allocation, &site_, id);       \
    } while(false)

#endif


/** Helper for clearing a structure */
#define VNClear(ptr) do { memset((ptr), 0, sizeof(*(ptr))); } while(false)

/** Helper for clearing an array  structure */
#define VNClearArray(ptr, count) do { memset(ptr, 0, sizeof(*(ptr)) * (count)); } while(false)

/** Helper for checking if a size is a power of 2 */
#define VNIsPowerOfTwo(size) (((size) & ((size)-1)) == 0)

/** Helper for checking allocation is valid */
#define VNIsAllocated(allocation) ((allocation).ptr != NULL)

/** Helper for getting allocation pointer as a given type */
#if !defined(__cplusplus)
#define VNAllocationPtr(allocation, type) ((type*)((allocation).ptr))
#else
#define VNAllocationPtr(allocation, type) (static_cast<type*>((allocation).ptr))
#endif

#define VNAllocationSize(allocation, type) ((allocation).size / sizeof(type))

/** Helper for check if allocation is OK */
#define VNAllocationSucceeded(allocation) ((allocation).ptr != NULL)

/** Helper for working out the size of an array */
#define VNArraySize(array) (sizeof(array)/sizeof((array)[0]))

/** Helper to align a size to a given multiple by rounding up if necessary */
#define VNAlignSize(sz, align) (((sz) + ((align)-1)) & ~((align) -1))

/** Helper to get from a pointer to structure member to the container - eg: from linked list node*/
#define VNContainerOf(ptr, type, member) ((type*)((char*)(ptr)-offsetof(type, member)))

/* clang-format on */

#ifdef __cplusplus
}
#endif

// NOLINTEND(cppcoreguidelines-avoid-do-while)
// NOLINTEND(modernize-use-using)

#endif // VN_LCEVC_COMMON_MEMORY_H
