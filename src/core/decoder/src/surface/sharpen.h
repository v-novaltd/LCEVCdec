/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_SHARPEN_H_
#define VN_DEC_CORE_SHARPEN_H_

#include "common/platform.h"
#include "common/types.h"
#include "context.h"

/*------------------------------------------------------------------------------*/

typedef struct Dither* Dither_t;
typedef struct Context Context_t;
typedef struct Surface Surface_t;

/*! Opaque handle to the sharpen module. */
typedef struct Sharpen* Sharpen_t;

/*------------------------------------------------------------------------------*/

/*! TODO
 */
bool sharpenInitialize(Context_t* ctx, Sharpen_t* sharpen, float globalStrength);

/*! TODO
 */
void sharpenRelease(Sharpen_t sharpen);

/*! TODO
 */
bool sharpenSet(Sharpen_t sharpen, SharpenType_t type, float strength);

SharpenType_t sharpenGetMode(const Sharpen_t sharpen);

/*! \brief Returns sharpen strength to be used.
 *
 *  This depends on what is signaled in the bitstream and whether client has asked
 *  to override signaled sharpening behaviour. */
float sharpenGetStrength(const Sharpen_t sharpen);

/*! TODO
 */
bool sharpenIsEnabled(Sharpen_t sharpen);

/*! TODO
 */
bool surfaceSharpen(Sharpen_t sharpen, const Surface_t* surface, Dither_t dither,
                    CPUAccelerationFeatures_t preferredAccel);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_SHARPEN_H_ */
