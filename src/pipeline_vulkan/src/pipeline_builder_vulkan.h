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

#ifndef VN_LCEVC_PIPELINE_VULKAN_PIPELINE_BUILDER_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_PIPELINE_BUILDER_VULKAN_H

#include "pipeline_config_vulkan.h"
//
#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/configure_members.hpp>
#include <LCEVC/common/memory.h>
#include <LCEVC/pipeline/pipeline.h>

namespace lcevc_dec::pipeline_vulkan {

class PipelineBuilderVulkan : public pipeline::PipelineBuilder
{
public:
    explicit PipelineBuilderVulkan(LdcMemoryAllocator* allocator);
    ~PipelineBuilderVulkan();

    // Configurable
    bool configure(std::string_view name, bool val) override;
    bool configure(std::string_view name, int32_t val) override;
    bool configure(std::string_view name, float val) override;
    bool configure(std::string_view name, const std::string& val) override;

    bool configure(std::string_view name, const std::vector<bool>& arr) override;
    bool configure(std::string_view name, const std::vector<int32_t>& arr) override;
    bool configure(std::string_view name, const std::vector<float>& arr) override;
    bool configure(std::string_view name, const std::vector<std::string>& arr) override;

    // PipelineBuilder
    std::unique_ptr<pipeline::Pipeline> finish(pipeline::EventSink* eventSink) const override;

    LdcMemoryAllocator* allocator() const { return m_allocator; }
    const PipelineConfigVulkan& configuration() const { return m_configuration; }

    VNNoCopyNoMove(PipelineBuilderVulkan);

private:
    friend std::unique_ptr<PipelineBuilder> createPipelineBuilder();

    LdcMemoryAllocator* m_allocator = nullptr;

    PipelineConfigVulkan m_configuration;

    common::ConfigurableMembers<PipelineConfigVulkan> m_configurableMembers;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_PIPELINE_BUILDER_VULKAN_H
