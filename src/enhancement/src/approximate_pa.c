/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#include <LCEVC/common/log.h>
#include <LCEVC/common/stdlib.h>
#include <LCEVC/enhancement/approximate_pa.h>
#include <LCEVC/enhancement/bitstream_types.h>

static const uint16_t kUnity = 1 << 14;

static bool approximatePA4Tap(LdeKernel* kernel)
{
    assert(kernel->length == 4);

    const uint16_t halfBDDiff = (uint16_t)(((uint32_t)kernel->coeffs[0] + kernel->coeffs[2]) / 2);

    kernel->coeffs[0] = halfBDDiff;
    kernel->coeffs[1] = kUnity;
    kernel->coeffs[2] = halfBDDiff;
    kernel->coeffs[3] = 0;

    kernel->approximatedPA = true;

    return true;
}

static bool approximatePA2TapZeroPad(LdeKernel* kernel)
{
    assert(kernel->length == 2);

    assert(kernel->coeffs[0] == 0);
    assert(kernel->coeffs[3] == 0);

    kernel->length = 4;

    return approximatePA4Tap(kernel);
}

bool ldeApproximatePA(LdeGlobalConfig* globalConfig)
{
    if (!globalConfig->initialized) {
        VNLogError("Global config was not initialized before attempting to approximate PA");
        return false;
    }
    if (!globalConfig->predictedAverageEnabled) {
        return true; // No need to modify the kernel if we're not using PA
    }
    if (globalConfig->kernel.approximatedPA) {
        return true; // Already approximated
    }

    switch (globalConfig->upscale) {
        case USLinear: return approximatePA2TapZeroPad(&globalConfig->kernel);
        case USCubic:
        case USModifiedCubic:
        case USAdaptiveCubic: return approximatePA4Tap(&globalConfig->kernel);
        case USNearest:
        case USCubicPrediction:
        case USMishus:
        case USReserved1:
        case USReserved2:
        case USUnspecified:
        case USLanczos: return true; // No modification to kernel required
    }
    return true;
}
