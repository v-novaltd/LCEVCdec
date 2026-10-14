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

#include "pipeline_builder_vulkan.h"
//
#include "pipeline_config_vulkan.h"
#include "pipeline_vulkan.h"

#include <LCEVC/pipeline_vulkan/create_pipeline.h>
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>

namespace lcevc_dec::pipeline_vulkan {
using namespace common;

// PipelineBuilderVulkan
//
static const ConfigMemberMap<PipelineConfigVulkan> kConfigMemberMap = {
    {"use_system_allocator", makeBinding(&PipelineConfigVulkan::useSystemAllocator)},
    {"will_render", makeBinding(&PipelineConfigVulkan::willRender)},
    {"vulkan_contexts", makeBinding(&PipelineConfigVulkan::contexts)},
    {"vulkan_device", makeBinding(&PipelineConfigVulkan::device)},
    {"vulkan_validation", makeBinding(&PipelineConfigVulkan::validation)},
    {"vulkan_timestamps", makeBinding(&PipelineConfigVulkan::timestampsLimit)},
};

PipelineBuilderVulkan::PipelineBuilderVulkan(LdcMemoryAllocator* allocator)
    : PipelineBuilderBase(allocator, m_configuration)
    , m_allocator(allocator)
    , m_configurableMembers(kConfigMemberMap, m_configuration)
{
    setDefaultThreadCount(m_configuration);
}

PipelineBuilderVulkan::~PipelineBuilderVulkan() {}

std::unique_ptr<pipeline::Pipeline> PipelineBuilderVulkan::finish(pipeline::EventSink* eventSink) const
{
    std::unique_ptr<PipelineVulkan> pipeline = std::make_unique<PipelineVulkan>(*this, eventSink);

    if (!pipeline->isInitialised()) {
        pipeline.reset();
    }

    return pipeline;
}

// Forward configuration to default configuration mapping mechanism.
//
bool PipelineBuilderVulkan::configure(std::string_view name, bool val)
{
    return m_configurableMembers.configure(name, val) || PipelineBuilderBase::configure(name, val);
}

bool PipelineBuilderVulkan::configure(std::string_view name, int32_t val)
{
    if (name == "vulkan_contexts" && val < 1) {
        return false;
    }
    return m_configurableMembers.configure(name, val) || PipelineBuilderBase::configure(name, val);
}
bool PipelineBuilderVulkan::configure(std::string_view name, float val)
{
    return m_configurableMembers.configure(name, val) || PipelineBuilderBase::configure(name, val);
}
bool PipelineBuilderVulkan::configure(std::string_view name, const std::string& val)
{
    return m_configurableMembers.configure(name, val) || PipelineBuilderBase::configure(name, val);
}

bool PipelineBuilderVulkan::configure(std::string_view name, const std::vector<bool>& arr)
{
    return m_configurableMembers.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}
bool PipelineBuilderVulkan::configure(std::string_view name, const std::vector<int32_t>& arr)
{
    return m_configurableMembers.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}
bool PipelineBuilderVulkan::configure(std::string_view name, const std::vector<float>& arr)
{
    return m_configurableMembers.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}
bool PipelineBuilderVulkan::configure(std::string_view name, const std::vector<std::string>& arr)
{
    return m_configurableMembers.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}

} // namespace lcevc_dec::pipeline_vulkan

VN_LCEVC_PIPELINE_API
lcevc_dec::pipeline::PipelineBuilder* CREATE_PIPELINE_VULKAN_BUILDER_NAME(void* diagnosticState,
                                                                          void* accelerationState)
{
    // Connect this shared libraries diagnostics to parent
    ldcDiagnosticsInitialize(diagnosticState);
    ldcAccelerationSet(static_cast<const LdcAcceleration*>(accelerationState));

    return new lcevc_dec::pipeline_vulkan::PipelineBuilderVulkan(ldcMemoryAllocatorMalloc()); // NOLINT(cppcoreguidelines-owning-memory)
}
