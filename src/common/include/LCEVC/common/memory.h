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

#ifndef VN_LCEVC_COMMON_MEMORY_H
#define VN_LCEVC_COMMON_MEMORY_H

#include <LCEVC/common/platform.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// NOLINTBEGIN(modernize-use-using)

#ifdef __cplusplus
extern "C"
{
#endif

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
} LdcMemoryAllocation;

/*!
 * Memory allocation functions
 */
typedef struct
{
    /*!
     * Allocate a block of memory of given size, using aligned from allocation.
     *
     * @param[in]       allocator     The memory allocator to allocate with.
     * @param[inout]    allocation    ADetails of allocation are written back to here.
     * @param[in]       size          The number of bytes to allocate.
     *
     * @return          A valid pointer to some memory of at least `size` bytes, or NULL.
     */
    void* (*allocate)(struct LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation,
                      size_t size, size_t alignment);
    /*!
     * Adjust an allocation, given a new size. Any previous data is copied to new block, up to the minimum of the old and new sizes.
     *
     * @param[in]       allocator     The memory allocator to allocate with.
     * @param[inout]    allocation    Details of allocation are to be adjusted.
     * @param[in]       size          The number of bytes to allocate.
     *
     * @return          A valid pointer to some memory of at least `size` bytes, or NULL.
     */
    void* (*reallocate)(struct LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation, size_t size);
    /*!
     * Release an allocation
     *
     * Any allocated block will be freed, and the allocation marked as empty.
     *
     * @param[in]       allocator     The memory allocator to free with.
     * @param[inout]    allocation    Details of allocation to be freed.
     */
    void (*free)(struct LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation);
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
 *
 * @return                        A valid pointer to some memory of at least `size` bytes, or NULL on failure.
 */
void* ldcMemoryAllocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation, size_t size,
                        size_t alignment, bool clearToZero);

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
 *
 * @return          A valid pointer to some memory of at least `size` bytes, or NULL.
 */
void* ldcMemoryReallocate(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation, size_t size);

/*!
 * Perform dynamic memory freeing.
 *
 * Any allocated block will be freed, and the allocation marked as empty.
 *
 * @param[in]       allocator     The memory allocator to free with.
 * @param[inout]    allocation    Details of allocation to be freed.
 */
void ldcMemoryFree(LdcMemoryAllocator* allocator, LdcMemoryAllocation* allocation);

/*! Get a wrapper for the standard C library heap allocator, if supported.
 *
 * @return          A pointer to an allocator that uses the standard C library malloc/free entry points, or NULL if not supported.
 */
LdcMemoryAllocator* ldcMemoryAllocatorMalloc(void);

/* clang-format off */

#if !defined(__cplusplus)
/** Helper for performing malloc for a single object. */
#define VNAllocate(allocator, allocation, type) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateArray(allocator, allocation, type, count) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), false)

/** Helper for performing calloc for a single object. */
#define VNAllocateZero(allocator, allocation, type) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), true)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateZeroArray(allocator, allocation, type, count) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), true)

/** Helper for performing malloc for a single object. */
#define VNAllocateAligned(allocator, allocation, type, align) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type), align, false)

/** Helper for performing malloc for an array of objects. */
#define VNAllocateAlignedArray(allocator, allocation, type, align, count) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, false)

/** Helper for performing calloc for a single object. */
#define VNAllocateAlignedZero(allocator, allocation, type, align) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type), align, true)

/** Helper for performing calloc for an array of objects. */
#define VNAllocateAlignedZeroArray(allocator, allocation, type, align, count) (type*)ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, true)

/** Helper for performing realloc for a single object. */
#define VNReallocate(allocator, allocation, type) (type*)ldcMemoryReallocate(allocator, allocation, (void*)(ptr), sizeof(type))

/** Helper for performing realloc for an array of objects. */
#define VNReallocateArray(allocator, allocation, type, count) (type*)ldcMemoryReallocate(allocator, allocation, sizeof(type) * (count))
#else
#define VNAllocate(allocator, allocation, type) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), false))
#define VNAllocateArray(allocator, allocation, type, count) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), false))
#define VNAllocateZero(allocator, allocation, type) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type), VNAlignof(type), true))
#define VNAllocateZeroArray(allocator, allocation, type, count) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), VNAlignof(type), true))

#define VNAllocateAligned(allocator, allocation, type, align) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type), align, false))
#define VNAllocateAlignedArray(allocator, allocation, type, align, count) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, false))
#define VNAllocateAlignedZero(allocator, allocation, type, align) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type), align, true))
#define VNAllocateAlignedZeroArray(allocator, allocation, type, align, count) static_cast<type*>(ldcMemoryAllocate(allocator, allocation, sizeof(type) * (count), align, true))

#define VNReallocate(allocator, allocation, type) static_cast<type*>(ldcMemoryReallocate(allocator, allocation, (void*)(ptr), sizeof(type)))
#define VNReallocateArray(allocator, allocation, type, count) static_cast<type*>(ldcMemoryReallocate(allocator, allocation, sizeof(type) * (count)))
#endif

/** Helper for freeing an allocation performed with one of the above macros. */
#define VNFree(allocator, allocation) do { ldcMemoryFree(allocator, allocation); } while(false)

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

/* clang-format on */

#ifdef __cplusplus
}
#endif

// NOLINTEND(modernize-use-using)

#endif // VN_LCEVC_COMMON_MEMORY_H
