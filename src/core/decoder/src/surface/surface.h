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

#ifndef VN_DEC_CORE_SURFACE_H_
#define VN_DEC_CORE_SURFACE_H_

#include "common/types.h"

#include <stddef.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

typedef struct Context Context_t;
typedef struct Logger* Logger_t;
typedef struct Memory* Memory_t;

/*
 * \brief Surface is a representation of a block of memory containing raw pixel data.
 */
typedef struct Surface
{
    uint8_t* data;     /**< Raw data allocation. */
    FixedPoint_t type; /**< Fixed point type for this surface. */
    uint32_t width;    /**< Width in pixels. */
    uint32_t height;   /**< Height in pixels. */
    uint32_t stride; /**< Stride in pixel elements. For interleaved this would be the number of interleaved components within a line + padding. */
    Interleaving_t interleaving; /**< Interleaving in use. Noting that width is not a factor of ilv, and NV12 is intended only for the chroma planes. */
    bool external;               /**< Whether memory is externally allocated. */
} Surface_t;

/*------------------------------------------------------------------------------*/

/* `stride` is the number of pixel elements to get to the next line. */
int32_t surfaceInitialise(Memory_t memory, Surface_t* surface, FixedPoint_t type, uint32_t width,
                          uint32_t height, uint32_t stride, Interleaving_t interleaving);

int32_t surfaceInitialiseExt(Surface_t* surface, void* data, FixedPoint_t type, uint32_t width,
                             uint32_t height, uint32_t stride, Interleaving_t interleaving);

int32_t surfaceInitialiseExt2(Surface_t* surface, FixedPoint_t type, uint32_t width,
                              uint32_t height, uint32_t stride, Interleaving_t interleaving);

void surfaceRelease(Memory_t memory, Surface_t* surface);

void surfaceIdle(Surface_t* surface);

uint8_t surfaceIsIdle(const Surface_t* surface);

uint8_t surfaceCompatible(const Surface_t* surface, FixedPoint_t type, uint32_t stride,
                          uint32_t height, Interleaving_t interleaving);

void surfaceZero(Memory_t memory, Surface_t* surface);

void surfaceToFile(Logger_t log, Memory_t memory, Context_t* ctx, const Surface_t* surfaces,
                   uint32_t planeCount, const char* filePath);

int32_t surfaceGetChannelSkipOffset(const Surface_t* surface, uint32_t channel, uint32_t* skip,
                                    uint32_t* offset);

uint8_t* surfaceGetLine(const Surface_t* surface, uint32_t y);

size_t surfaceGetStrideInPixels(const Surface_t* surface);

/*------------------------------------------------------------------------------*/

/**! Opaque handle to a cache used for tracking surface dumps. */
typedef struct SurfaceDumpCache SurfaceDumpCache_t;

/**! Surface dump cache initialisation.
 *
 * Handle is allocated within this function, release must be called to ensure all
 * contents are released.
 *
 * This function is not thread-safe.
 *
 * \param cache    Pointer to a surface dump handle.
 * \return 0 on success, otherwise -1
 */
int32_t surfaceDumpCacheInitialise(Memory_t memory, Logger_t log, SurfaceDumpCache_t** cache);

/**! Surface dump cache release.
 *
 * Handle is freed within this function, it will be set to null.
 *
 * This function is not thread-safe.
 *
 * \param cache    Pointer to an initialised surface dump handle.
 */
void surfaceDumpCacheRelease(SurfaceDumpCache_t* cache);

/**! Surface dump entry point.
 *
 * Performs surface writing to file across multiple frames where repeated invocations
 * of this function with the same ID will output to the same file.
 *
 * Internally this function uses vsnprintf to generate the id using the passed in format
 * and variable args - if that fails then this will return -1.
 *
 * This function is thread-safe, and must be called between surface_dump_cache_initialise
 * and surface_dump_cache_release.
 *
 * \param idFormat Format of the ID to dump the surface with.
 * \return 0 on success, otherwise -1
 */
int32_t surfaceDump(Memory_t memory, Logger_t log, Context_t* ctx, const Surface_t* surface,
                    const char* idFormat, ...);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_SURFACE_H_ */
