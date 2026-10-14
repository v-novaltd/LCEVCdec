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

// PipelineConfigVulkan
//
// Configurable pipeline parameters - set up by the 'Builder' and then
// passed as a const structure into the initialized Pipeline.
//
#ifndef VN_LCEVC_PIPELINE_VULKAN_PIPELINE_CONFIG_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_PIPELINE_CONFIG_VULKAN_H

#include <LCEVC/pipeline/pipeline_config_base.h>

namespace lcevc_dec::pipeline_vulkan {

// The configurable values that get passed from the builder to the initialized pipeline
//
struct PipelineConfigVulkan : public pipeline::PipelineConfigBase
{
    bool useSystemAllocator = false;

    bool willRender = false;

    int32_t contexts = 1; // Number of available Vulkan frame contexts

    int32_t device = -1;         // If >=0 - choose given Vulkan device
    int32_t timestampsLimit = 0; // Maximum number of Vulkan shader timestamps to record (0 disables)

    bool validation = // Enable Vulkan validation layers
#if defined(NDEBUG)
        false
#else
        true
#endif
        ;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_PIPELINE_CONFIG_VULKAN_H
