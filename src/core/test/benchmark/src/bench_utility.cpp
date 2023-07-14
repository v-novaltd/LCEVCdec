/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "bench_utility.h"

CPUAccelerationFeatures_t simdFlag(CPUAccelerationFeatures_t x86Flag)
{
    if (x86Flag != CAFNone) {
#if VN_CORE_FEATURE(SSE) || VN_CORE_FEATURE(AVX2)
        return x86Flag;
#endif
#if VN_CORE_FEATURE(NEON)

        return CAFNEON;
#endif
    }

    return CAFNone;
}
