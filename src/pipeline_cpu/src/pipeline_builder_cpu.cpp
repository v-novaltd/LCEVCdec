/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>

namespace lcevc_dec::pipeline_cpu {
using namespace common;

// PipelineBuilderCPU
//
static const ConfigMemberMap<PipelineConfigCPU> kConfigMemberMap = {
    {"allow_dithering", makeBinding(&PipelineConfigCPU::ditherEnabled)},
    {"default_max_reorder", makeBinding(&PipelineConfigCPU::defaultMaxReorder)},
    {"dither_seed", makeBinding(&PipelineConfigCPU::setDitherSeed)},
    {"dither_strength", makeBinding(&PipelineConfigCPU::ditherOverrideStrength)},
    {"enhancement_delay", makeBinding(&PipelineConfigCPU::enhancementDelay)},
    {"force_bitstream_version", makeBinding(&PipelineConfigCPU::forceBitstreamVersion)},
    {"force_scalar", makeBinding(&PipelineConfigCPU::forceScalar)},
    {"highlight_residuals", makeBinding(&PipelineConfigCPU::highlightResiduals)},
    {"log_tasks", makeBinding(&PipelineConfigCPU::showTasks)},
    {"max_latency", makeBinding(&PipelineConfigCPU::maxLatency)},
    {"min_latency", makeBinding(&PipelineConfigCPU::minLatency)},
    {"temporal_buffers", makeBinding(&PipelineConfigCPU::numTemporalBuffers)},
    {"passthrough_mode", makeBinding(&PipelineConfigCPU::setPassthroughMode)},
    {"s_filter_strength", makeBinding(&PipelineConfigCPU::sharpeningOverrideStrength)},
    {"threads", makeBinding(&PipelineConfigCPU::numThreads)},
};

PipelineBuilderCPU::PipelineBuilderCPU(LdcMemoryAllocator* allocator)
    : m_allocator(allocator)
    , m_configurableMembers(kConfigMemberMap, m_configuration)
{
    // Set default thread count - number of platform cores, plus 1 for main thread
    m_configuration.numThreads = threadNumCores() + 1;
}

PipelineBuilderCPU::~PipelineBuilderCPU() {}

std::unique_ptr<pipeline::Pipeline> PipelineBuilderCPU::finish(pipeline::EventSink* eventSink) const
{
    std::unique_ptr<pipeline::Pipeline> pipeline = std::make_unique<PipelineCPU>(*this, eventSink);

    return pipeline;
}

// Forward configuration to default config mapping mechanism.
//
bool PipelineBuilderCPU::configure(std::string_view name, bool val)
{
    return m_configurableMembers.configure(name, val);
}

bool PipelineBuilderCPU::configure(std::string_view name, int32_t val)
{
    return m_configurableMembers.configure(name, val);
}
bool PipelineBuilderCPU::configure(std::string_view name, float val)
{
    return m_configurableMembers.configure(name, val);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::string& val)
{
    return m_configurableMembers.configure(name, val);
}

bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<bool>& arr)
{
    return m_configurableMembers.configure(name, arr);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<int32_t>& arr)
{
    return m_configurableMembers.configure(name, arr);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<float>& arr)
{
    return m_configurableMembers.configure(name, arr);
}
bool PipelineBuilderCPU::configure(std::string_view name, const std::vector<std::string>& arr)
{
    return m_configurableMembers.configure(name, arr);
}

} // namespace lcevc_dec::pipeline_cpu

VN_LCEVC_PIPELINE_API lcevc_dec::pipeline::PipelineBuilder*
createPipelineBuilderCPU(void* diagnosticState, void* accelerationState)
{
    // Connect this shared libraries diagnostics to parent
    ldcDiagnosticsInitialize(diagnosticState);
    ldcAccelerationSet(static_cast<const LdcAcceleration*>(accelerationState));

    return new lcevc_dec::pipeline_cpu::PipelineBuilderCPU(ldcMemoryAllocatorMalloc()); // NOLINT(cppcoreguidelines-owning-memory)
}
