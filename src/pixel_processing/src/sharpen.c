/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#include "sharpen_common.h"

#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/log.h>
#include <LCEVC/pixel_processing/dither.h>
#include <LCEVC/pixel_processing/sharpen.h>
#include <stdbool.h>
#include <stdint.h>

/*------------------------------------------------------------------------------*/

SharpenFunction sharpenGetFunction(LdpFixedPoint fixedPoint)
{
    const LdcAcceleration* acceleration = ldcAccelerationGet();

    SharpenFunction res = NULL;

    if (acceleration->hasSSE) {
        res = sharpenGetFunctionSSE(fixedPoint);
    }

    if (acceleration->hasNeon) {
        res = sharpenGetFunctionNEON(fixedPoint);
    }

#if !VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
    if (!res) {
        res = sharpenGetFunctionScalar(fixedPoint);
    }
#endif

    return res;
}

/*------------------------------------------------------------------------------*/

bool ldppSharpen(const LdpPictureLayout* layout, LdpPicturePlaneDesc* plane, uint32_t planeIndex,
                 float strength, const LdppDitherFrame* frameDither, const LdpPipelineDiagInfo* diagInfo)
{
    const uint32_t width = layout->width >> layout->layoutInfo->planeWidthShift[planeIndex];
    const uint32_t height = layout->height >> layout->layoutInfo->planeHeightShift[planeIndex];
    const LdpFixedPoint fixedPoint = layout->layoutInfo->fixedPoint;
    assert(strength >= 0.0f && strength <= 1.0f);

    SharpenFunction function = sharpenGetFunction(fixedPoint);

    if (!function) {
        VNLogError("Failed to find sharpening function");
        return false;
    }

    VNTraceScopedBeginArgs("task", diagInfo->task, "timestamp", diagInfo->timestamp, "plane",
                           diagInfo->plane, "count", height);

    LdppDitherSlice sliceDither;
    LdppDitherSlice* dither = NULL;
    if (frameDither != NULL) {
        ldppDitherSliceInitialise(&sliceDither, frameDither, 0, planeIndex);
        dither = &sliceDither;
    }

    function(plane->firstSample, plane->rowByteStride, width, height, strength, dither);

    VNTraceScopedEnd();
    return true;
}

/*------------------------------------------------------------------------------*/
