/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_DEC_CORE_OVERLAY_H_
#define VN_DEC_CORE_OVERLAY_H_

#include "common/types.h"
#include "context.h"

#define VN_OVERLAY_MAX_DELAY() 750

typedef struct OverlayArgs
{
    const Surface_t* dst;
} OverlayArgs_t;

/*! \brief apply overlay to an image */
int32_t overlayApply(Logger_t log, Context_t* ctx, const OverlayArgs_t* params);

/*! \brief determines whether overlay should be applied. */
bool overlayIsEnabled(const Context_t* ctx);

#endif /* VN_DEC_CORE_OVERLAY_H_ */
