/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_UTIL_ENUMS_H_
#define VN_UTIL_ENUMS_H_

#include "uEnumMap.h"

namespace lcevc_dec::api_utility {

enum class DILPassthroughPolicy : int32_t
{
    Disable = -1, // base can never pass through, i.e. it must always be lcevc enhanced
    Allow = 0,    // base can pass through if lcevc is not found or not applied
    Force = 1,    // base must pass through, regardless of lcevc being present or applicable
};

// - PredictedAverageMethod -----------------------------------------------------------------------
// TODO - 03/02/2023: This has been moved from the Integration layer to here (the integration
// utils). It might be even better to move this to the Core utils, although it does use some c++
// classes so, maybe not yet.
enum class PredictedAverageMethod : uint8_t
{
    None = 0,
    Standard = 1,
    BakedIntoKernel = 2, // Or "approximate PA", this method bakes PA into the upscaling kernel.

    COUNT,
};

static constexpr EnumMapArr<PredictedAverageMethod, static_cast<size_t>(PredictedAverageMethod::COUNT)>
    kPredictedAverageMethodDesc(PredictedAverageMethod::None, "None", PredictedAverageMethod::Standard,
                                "Standard", PredictedAverageMethod::BakedIntoKernel, "Baked into kernel");

static_assert(kPredictedAverageMethodDesc.size == kPredictedAverageMethodDesc.capacity(),
              "kPredictedAverageMethodDesc has too many or too few entries.");

} // namespace lcevc_dec::api_utility

#endif // VN_UTIL_ENUMS_H_