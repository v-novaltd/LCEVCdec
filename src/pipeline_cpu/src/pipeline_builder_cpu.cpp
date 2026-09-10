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

#include "pipeline_builder_cpu.h"
//
#include "pipeline_cpu.h"

#include <LCEVC/common/acceleration.h>
#include <LCEVC/pipeline_cpu/create_pipeline.h>

namespace lcevc_dec::pipeline_cpu {
using namespace common;

// PipelineBuilderCPU
//
static const ConfigMemberMap<PipelineConfigCPU> kConfigMemberMap = {
    {"use_system_allocator", makeBinding(&PipelineConfigCPU::useSystemAllocator)},
    {"will_render", makeBinding(&PipelineConfigCPU::willRender)},
};

PipelineBuilderCPU::PipelineBuilderCPU(LdcMemoryAllocator* allocator)
    : pipeline::PipelineBuilderBase(allocator, m_configuration)
    , m_allocator(allocator)
    , m_configurableMembersCPU(kConfigMemberMap, m_configuration)
{
    setDefaultThreadCount(m_configuration);
}

PipelineBuilderCPU::~PipelineBuilderCPU() {}

std::unique_ptr<pipeline::Pipeline> PipelineBuilderCPU::finish(pipeline::EventSink* eventSink) const
{
    std::unique_ptr<pipeline::Pipeline> pipeline = std::make_unique<PipelineCPU>(*this, eventSink);

    return pipeline;
}

// Forward configuration to default config mapping mechanism, then base
//
bool PipelineBuilderCPU::configure(std::string_view name, bool val)
{
    return m_configurableMembersCPU.configure(name, val) || PipelineBuilderBase::configure(name, val);
}
bool PipelineBuilderCPU::configure(std::string_view name, int32_t val)
{
    return m_configurableMembersCPU.configure(name, val) || PipelineBuilderBase::configure(name, val);
}
bool PipelineBuilderCPU::configure(std::string_view name, float val)
{
    return m_configurableMembersCPU.configure(name, val) || PipelineBuilderBase::configure(name, val);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::string& val)
{
    return m_configurableMembersCPU.configure(name, val) || PipelineBuilderBase::configure(name, val);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<bool>& arr)
{
    return m_configurableMembersCPU.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<int32_t>& arr)
{
    return m_configurableMembersCPU.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<float>& arr)
{
    return m_configurableMembersCPU.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<std::string>& arr)
{
    return m_configurableMembersCPU.configure(name, arr) || PipelineBuilderBase::configure(name, arr);
}

} // namespace lcevc_dec::pipeline_cpu

VN_LCEVC_PIPELINE_API lcevc_dec::pipeline::PipelineBuilder*
CREATE_PIPELINE_CPU_BUILDER_NAME(void* diagnosticState, void* accelerationState)
{
    // Connect this shared libraries diagnostics to parent
    ldcDiagnosticsInitialize(diagnosticState);
    ldcAccelerationSet(static_cast<const LdcAcceleration*>(accelerationState));

    return new lcevc_dec::pipeline_cpu::PipelineBuilderCPU(ldcMemoryAllocatorMalloc()); // NOLINT(cppcoreguidelines-owning-memory)
}
