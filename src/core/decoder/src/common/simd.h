/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_SIMD_H_
#define VN_DEC_CORE_SIMD_H_

#include "common/neon.h"
#include "common/platform.h"
#include "common/sse.h"
#include "common/types.h"

CPUAccelerationFeatures_t detectSupportedSIMDFeatures(void);

#endif /* VN_DEC_CORE_SIMD_H_ */