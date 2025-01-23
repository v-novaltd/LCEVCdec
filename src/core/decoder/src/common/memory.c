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

#include "common/memory.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

typedef struct Memory
{
    void* userData;
    AllocateFunction_t allocFn;
    AllocateZeroFunction_t allocZeroFn;
    FreeFunction_t freeFn;
    ReallocFunction_t reallocateFn;
} * Memory_t;

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

    *handle = (Memory_t)memory;

    return true;
}

void memoryRelease(Memory_t memory) { memory->freeFn(memory->userData, memory); }

void* memoryAllocate(Memory_t memory, size_t size, bool zero)
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

    return ptr;
}

void* memoryReallocate(Memory_t memory, void* ptr, size_t size)
{
    void* newPtr = memory->reallocateFn(memory->userData, ptr, size);

    if (!newPtr) {
        return NULL;
    }

    return newPtr;
}

void memoryFree(Memory_t memory, void** ptr)
{
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
