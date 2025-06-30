/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <LCEVC/build_config.h>
#include <LCEVC/common/acceleration.h>
//
#include <assert.h>

static LdcAcceleration defaultAcceleration = {0};
static const LdcAcceleration* currentAcceleration = &defaultAcceleration;

void ldcAccelerationInitialize(bool enable)
{
    if (enable) {
#if VN_SDK_FEATURE(SSE)
        defaultAcceleration.SSE = true;
#else
        defaultAcceleration.SSE = false;
#endif
#if VN_SDK_FEATURE(AVX2)
        defaultAcceleration.AVX2 = true;
#else
        defaultAcceleration.AVX2 = false;
#endif
#if VN_SDK_FEATURE(NEON)
        defaultAcceleration.NEON = true;
#else
        defaultAcceleration.NEON = false;
#endif
    } else {
        defaultAcceleration.SSE = false;
        defaultAcceleration.AVX2 = false;
        defaultAcceleration.NEON = false;
    }

    currentAcceleration = &defaultAcceleration;
}

void ldcAccelerationSet(const LdcAcceleration* acceleration)
{
    assert(acceleration);
    currentAcceleration = acceleration;
}

const LdcAcceleration* ldcAccelerationGet(void) { return currentAcceleration; }
