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

#ifndef VN_API_ENUMS_H_
#define VN_API_ENUMS_H_

#include <LCEVC/api_utility/enum_map.h>

#include <cstddef>
#include <cstdint>

namespace lcevc_dec::decoder {

using lcevc_dec::utility::EnumMapArr;

enum class PassthroughPolicy : int32_t
{
    Disable = -1, // base can never pass through. No decode occurs if lcevc is absent/inapplicable
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

static constexpr EnumMapArr<PredictedAverageMethod, static_cast<size_t>(PredictedAverageMethod::COUNT)> kPredictedAverageMethodDesc{
    PredictedAverageMethod::None,
    "None",
    PredictedAverageMethod::Standard,
    "Standard",
    PredictedAverageMethod::BakedIntoKernel,
    "Baked into kernel",
};

static_assert(!kPredictedAverageMethodDesc.isMissingEnums(),
              "kPredictedAverageMethodDesc is missing an entry for some enum.");

} // namespace lcevc_dec::decoder

#endif // VN_API_ENUMS_H_
