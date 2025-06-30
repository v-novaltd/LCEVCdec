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

#ifndef VN_LCEVC_PIPELINE_LEGACY_ENUMS_H
#define VN_LCEVC_PIPELINE_LEGACY_ENUMS_H

#include <cstdint>

namespace lcevc_dec::decoder {

enum class PassthroughPolicy : int32_t
{
    Disable = -1, // base can never pass through. No decode occurs if lcevc is absent/inapplicable
    Allow = 0,    // base can pass through if lcevc is not found or not applied
    Force = 1,    // base must pass through, regardless of lcevc being present or applicable
};

// - PredictedAverageMethod -----------------------------------------------------------------------
enum class PredictedAverageMethod : uint8_t
{
    None = 0,
    Standard = 1,
    BakedIntoKernel = 2, // Or "approximate PA", this method bakes PA into the upscaling kernel.

    COUNT,
};

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_PIPELINE_LEGACY_ENUMS_H
