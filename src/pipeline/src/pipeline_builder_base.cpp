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

#include <LCEVC/pipeline/pipeline_builder_base.h>
#include <LCEVC/pipeline/pipeline_config_base.h>
//
#include <LCEVC/common/threads.h>

namespace lcevc_dec::pipeline {
using namespace common;

// PipelineBuilderBase
//
static const ConfigMemberMap<PipelineConfigBase> kConfigMemberMap = {
    {"allow_dithering", makeBinding(&PipelineConfigBase::ditherEnabled)},
    {"default_max_reorder", makeBinding(&PipelineConfigBase::defaultMaxReorder)},
    {"dither_seed", makeBinding(&PipelineConfigBase::setDitherSeed)},
    {"dither_strength", makeBinding(&PipelineConfigBase::ditherOverrideStrength)},
    {"enhancement_delay", makeBinding(&PipelineConfigBase::enhancementDelay)},
    {"force_bitstream_version", makeBinding(&PipelineConfigBase::forceBitstreamVersion)},
    {"highlight_residuals", makeBinding(&PipelineConfigBase::highlightResiduals)},
    {"log_tasks", makeBinding(&PipelineConfigBase::showTasks)},
    {"max_latency", makeBinding(&PipelineConfigBase::maxLatency)},
    {"min_latency", makeBinding(&PipelineConfigBase::minLatency)},
    {"temporal_buffers", makeBinding(&PipelineConfigBase::numTemporalBuffers)},
    {"passthrough_mode", makeBinding(&PipelineConfigBase::setPassthroughMode)},
    {"s_filter_strength", makeBinding(&PipelineConfigBase::sharpeningOverrideStrength)},
    {"threads", makeBinding(&PipelineConfigBase::numThreads)},
};

PipelineBuilderBase::PipelineBuilderBase(LdcMemoryAllocator* allocator, PipelineConfigBase& configuration)
    : m_configurableMembersBase(kConfigMemberMap, configuration)
{}

void PipelineBuilderBase::setDefaultThreadCount(PipelineConfigBase& configuration)
{
#if VN_OS(ANDROID) || !VN_SDK_FEATURE(THREADING)
    // Special case for Android, single threaded operation often gives better performance on mobile
    configuration.numThreads = 1;
#else
    // Set default thread count - number of platform cores, plus 1 for main thread
    configuration.numThreads = threadNumCores();
#endif
}

PipelineBuilderBase::~PipelineBuilderBase() {}

std::unique_ptr<pipeline::Pipeline> PipelineBuilderBase::finish(pipeline::EventSink* eventSink) const
{
    return nullptr;
}

// Forward configuration to default config mapping mechanism.
//
bool PipelineBuilderBase::configure(std::string_view name, bool val)
{
    return m_configurableMembersBase.configure(name, val);
}
bool PipelineBuilderBase::configure(std::string_view name, int32_t val)
{
    return m_configurableMembersBase.configure(name, val);
}
bool PipelineBuilderBase::configure(std::string_view name, float val)
{
    return m_configurableMembersBase.configure(name, val);
}
bool PipelineBuilderBase::configure(std::string_view name, const std::string& val)
{
    return m_configurableMembersBase.configure(name, val);
}
bool PipelineBuilderBase::configure(std::string_view name, const std::vector<bool>& arr)
{
    return m_configurableMembersBase.configure(name, arr);
}
bool PipelineBuilderBase::configure(std::string_view name, const std::vector<int32_t>& arr)
{
    return m_configurableMembersBase.configure(name, arr);
}
bool PipelineBuilderBase::configure(std::string_view name, const std::vector<float>& arr)
{
    return m_configurableMembersBase.configure(name, arr);
}
bool PipelineBuilderBase::configure(std::string_view name, const std::vector<std::string>& arr)
{
    return m_configurableMembersBase.configure(name, arr);
}

} // namespace lcevc_dec::pipeline
