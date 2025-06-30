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

#ifndef VN_LCEVC_LEGACY_SHARPEN_COMMON_H
#define VN_LCEVC_LEGACY_SHARPEN_COMMON_H

#include <stdint.h>

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

#endif // VN_LCEVC_LEGACY_SHARPEN_COMMON_H
