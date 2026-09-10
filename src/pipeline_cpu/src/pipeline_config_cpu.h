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

// PipelineConfigCPU
//
// Configurable pipeline parameters - set up by the 'Builder' and then
// passed as a const structure into the initialized Pipeline.
//
#ifndef VN_LCEVC_PIPELINE_CPU_PIPELINE_CONFIG_CPU_H
#define VN_LCEVC_PIPELINE_CPU_PIPELINE_CONFIG_CPU_H

#include <LCEVC/pipeline/pipeline_config_base.h>

namespace lcevc_dec::pipeline_cpu {

// The configurable values that get passed from the builder to the initialized pipeline
//`
struct PipelineConfigCPU : public pipeline::PipelineConfigBase
{
    // Directly use system allocator (malloc/free) for pipeline allocations
    bool useSystemAllocator = false;

    // Output pictures will be rendered to a window via renderSendPicture()
    bool willRender = false;
};

} // namespace lcevc_dec::pipeline_cpu

#endif // VN_LCEVC_PIPELINE_CPU_PIPELINE_CONFIG_CPU_H
