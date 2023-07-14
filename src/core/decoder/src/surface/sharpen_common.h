/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_SHARPEN_COMMON_H_
#define VN_DEC_CORE_SHARPEN_COMMON_H_

#include "common/platform.h"

/*------------------------------------------------------------------------------*/

typedef struct Surface Surface_t;
typedef struct Dither* Dither_t;

/*------------------------------------------------------------------------------*/

typedef struct SharpenArgs
{
    const Surface_t* src;        /**< Surface to apply sharpen to. */
    const Surface_t* tmpSurface; /**< Intermediate surface to store temporary results. */
    Dither_t dither;             /**< If non-NULL dither will be applied. */
    float strength;              /**< Filter strength.  */
    uint32_t offset;             /**< Row offset to start processing from. */
    uint32_t count;              /**< Number of rows to process. */
} SharpenArgs_t;

typedef void (*SharpenFunction_t)(const SharpenArgs_t* args);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_SHARPEN_COMMON_H_ */