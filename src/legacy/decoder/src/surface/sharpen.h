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

#ifndef VN_LCEVC_LEGACY_SHARPEN_H
#define VN_LCEVC_LEGACY_SHARPEN_H

#include "common/log.h"
#include "common/types.h"
#include "context.h"

/*------------------------------------------------------------------------------*/

typedef struct Context Context_t;
typedef struct Dither* Dither_t;
typedef struct Surface Surface_t;
typedef struct ThreadManager ThreadManager_t;

/*! Opaque handle to the sharpen module. */
typedef struct Sharpen* Sharpen_t;

/*------------------------------------------------------------------------------*/

/*! TODO
 */
bool sharpenInitialize(ThreadManager_t* threadManager, Memory_t memory, Logger_t log,
                       Sharpen_t* sharpenOut, float globalStrength);

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

#endif // VN_LCEVC_LEGACY_SHARPEN_H
