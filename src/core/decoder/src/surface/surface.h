/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_SURFACE_H_
#define VN_DEC_CORE_SURFACE_H_

#include "common/types.h"

/*------------------------------------------------------------------------------*/

/*
 * \brief Surface is a representation of a block of memory containing raw pixel data.
 */
typedef struct Surface
{
    Context_t* ctx;
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
int32_t surfaceInitialise(Context_t* ctx, Surface_t* surface, FixedPoint_t type, uint32_t width,
                          uint32_t height, uint32_t stride, Interleaving_t interleaving);

int32_t surfaceInitialiseExt(Context_t* ctx, Surface_t* surface, void* data, FixedPoint_t type,
                             uint32_t width, uint32_t height, uint32_t stride,
                             Interleaving_t interleaving);

int32_t surfaceInitialiseExt2(Context_t* ctx, Surface_t* surface, FixedPoint_t type, uint32_t width,
                              uint32_t height, uint32_t stride, Interleaving_t interleaving);

void surfaceRelease(Context_t* ctx, Surface_t* surface);

void surfaceIdle(Context_t* ctx, Surface_t* surface);

uint8_t surfaceIsIdle(Context_t* ctx, const Surface_t* surface);

uint8_t surfaceCompatible(Context_t* ctx, const Surface_t* surface, FixedPoint_t type,
                          uint32_t stride, uint32_t height, Interleaving_t interleaving);

void surfaceZero(Context_t* ctx, Surface_t* surface);

void surfaceToFile(Context_t* ctx, const Surface_t* surfaces, uint32_t planeCount, const char* filePath);

int32_t surfaceGetChannelSkipOffset(const Surface_t* surface, uint32_t channel, int32_t* skip,
                                    int32_t* offset);

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
int32_t surfaceDumpCacheInitialise(Context_t* ctx, SurfaceDumpCache_t** cache);

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
int32_t surfaceDump(Context_t* ctx, const Surface_t* surface, const char* idFormat, ...);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_SURFACE_H_ */