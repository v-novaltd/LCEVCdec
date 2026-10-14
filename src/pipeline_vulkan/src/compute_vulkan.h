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

#ifndef VN_LCEVC_PIPELINE_VULKAN_COMPUTE_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_COMPUTE_VULKAN_H

#include "frame_context_vulkan.h"

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/ring_buffer.hpp>
#include <LCEVC/common/threads.hpp>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/pipeline/types.h>
//
#include <vulkan/vulkan.h>
//
#include <array>
#include <vector>

namespace lcevc_dec::pipeline_vulkan {

class BackendVulkan;
class RenderVulkan;
class FrameVulkan;
class BufferVulkan;
class PictureVulkan;
class VulkanFrameContext;

struct VulkanApplyCommonArgs;
struct VulkanApplyTileArgs;

struct VulkanConvertInArgs;
struct PushConstantsConversion;
struct VulkanConvertOutArgs;
struct VulkanUpscaleArgs;

struct VulkanDequantTransformArgs
{
    BufferVulkan* coefficients{};
    BufferVulkan* dequantParameters{};
    BufferVulkan* residuals{};
    VulkanFrameContext* context{};
    uint32_t transformUnitCount{};
    uint32_t layerCount{};
    uint32_t temporalSignal{};
    LdeScalingMode scalingMode{};
    bool deblockEnabled{};
    uint32_t deblockCorner{};
    uint32_t deblockSide{};
};

// Arguments for creating a compute pipeline - workgroup dimensions and shader specialization
struct Dimension
{
    uint32_t w;
    uint32_t h;
};

struct SpecValue
{
    uint32_t id;
    uint32_t value;
};

// State for Compute Pipeline Layout - may be shared between several specialized pipelines
struct ComputePipelineLayout
{
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
};

// State for a Compute Pipeline (possibly specialized)
struct ComputePipeline
{
    VkPipeline pipeline;
    Dimension workgroup; // width and height of workgroup
    Dimension packing;   // How many samples per dimension of workgroup
    const char* name;    // debug name
};

struct DescriptorSetLayouts
{
    VkDescriptorSetLayout upscale{};
    VkDescriptorSetLayout apply{};
    VkDescriptorSetLayout conversion{};
    VkDescriptorSetLayout convertOutImage{};
    VkDescriptorSetLayout dequantTransform{};
};

class ComputeVulkan
{
public:
    ComputeVulkan(BackendVulkan& backend, uint32_t contextCount);
    ~ComputeVulkan() = default;

    // API
    bool applyCommon(VulkanApplyCommonArgs* params);
    bool applyTile(VulkanApplyTileArgs* params);

    bool convertIn(VulkanConvertInArgs* params);
    bool convertOut(VulkanConvertOutArgs* params);
    bool dequantTransform(VulkanDequantTransformArgs* params);

private:
    // Per-flavour helpers for convertOut(). The common preamble in
    // convertOut() resolves source/destination buffers and primes a
    // PushConstantsConversion; these methods finish off the dispatch for
    // their respective destination flavour.
    bool convertOutToImage(VulkanConvertOutArgs* params, PushConstantsConversion& constants);
    bool convertOutToBuffer(VulkanConvertOutArgs* params, PushConstantsConversion& constants);

public:
    bool upscale1D(const LdeKernel* kernel, VulkanUpscaleArgs* params);
    bool upscale2D(const LdeKernel* kernel, VulkanUpscaleArgs* params);
    bool upscaleFrame(const LdeKernel* kernel, VulkanUpscaleArgs* params);

    bool beginCompute(VulkanFrameContext* context);
    bool endCompute(VulkanFrameContext* context);

    bool updateConfig(uint8_t loq, const LdpPictureDesc& desc);
    uint32_t frameContextCount() const { return static_cast<uint32_t>(m_frameContexts.size()); }

    VulkanFrameContext* acquireFrameContext(FrameVulkan* frame);
    void releaseFrameContext(VulkanFrameContext* context);

    bool init();
    void destroy();
    void destroyComputePipelineLayout(ComputePipelineLayout& computePipelineLayout);
    void destroyComputePipeline(ComputePipeline& computePipeline);

    VNNoCopyNoMove(ComputeVulkan);

private:
    // Compute
    bool createComputeBindingsAndPipelineLayout(ComputePipelineLayout& computePipelineLayout,
                                                const std::vector<VkDescriptorType>& descriptorTypes,
                                                uint32_t pushConstantsSize);
    bool createConvertOutImageBindingsAndPipelineLayout(ComputePipelineLayout& computePipelineLayout,
                                                        uint32_t pushConstantsSize);
    bool createComputePipeline(ComputePipeline& computePipeline,
                               const ComputePipelineLayout& computePipelineLayout,
                               const unsigned char* shader, size_t shaderSize, const char* name,
                               Dimension workgroup, Dimension packing,
                               const std::vector<SpecValue>& specValues);

    static bool unbindInternalPictureBuffer(PictureVulkan* picture);
    static bool prepareInternalPictureBuffer(PictureVulkan* picture, const LdpPictureDesc& desc);

    static bool prepareDescriptorBufferInfo(BufferVulkan* buffer, VkDescriptorBufferInfo& info);
    static bool validateDescriptorSet(VkDescriptorSet descriptorSet);
    bool updateComputeDescriptorSets(BufferVulkan* src, BufferVulkan* dst, VkDescriptorSet descriptorSet);
    bool updateComputeImageDescriptorSets(BufferVulkan* src, BufferVulkan* dst,
                                          BufferVulkan* temporal, VkDescriptorSet descriptorSet);
    bool updateComputeDescriptorSets(BufferVulkan* src, BufferVulkan* dst1, BufferVulkan* dst2,
                                     VkDescriptorSet descriptorSet,
                                     const std::vector<VkDescriptorType>& descriptorTypes);

    BackendVulkan& m_backend;

    std::array<LdpPictureDesc, LOQMaxCount> m_previousDesc{};

    // Pool of compute decoding contexts
    std::vector<VulkanFrameContext> m_frameContexts;

    // Buffer of available context indexes - thread safe
    common::RingBuffer<uint32_t> m_freeContexts;

    // Shared pipeline layouts for various decode steps
    ComputePipelineLayout m_pipelineLayoutUpscale{};
    ComputePipelineLayout m_pipelineLayoutApply{};
    ComputePipelineLayout m_pipelineLayoutConversion{};
    ComputePipelineLayout m_pipelineLayoutConvertOutImage{};
    ComputePipelineLayout m_pipelineLayoutDequantTransform{};

    // THe various compute pipelines with per step specializations for
    // various global options
    ComputePipeline m_pipelineUpscale1DPA0{};
    ComputePipeline m_pipelineUpscale1DPA1{};
    ComputePipeline m_pipelineUpscale2DPA0{};
    ComputePipeline m_pipelineUpscale2DPA2{};
    ComputePipeline m_pipelineApplyDd{};
    ComputePipeline m_pipelineApplyDds{};
    ComputePipeline m_pipelineApplyDdRaster{};
    ComputePipeline m_pipelineApplyDdsRaster{};
    ComputePipeline m_pipelineConversionToInternal8Bit{};
    ComputePipeline m_pipelineConversionToInternal8BitNV12{};
    ComputePipeline m_pipelineConversionToInternal16Bit{};
    ComputePipeline m_pipelineConversionFromInternalPlanar8{};
    ComputePipeline m_pipelineConversionFromInternalPlanar8Sharpen{};
    ComputePipeline m_pipelineConversionFromInternalPlanar16{};
    ComputePipeline m_pipelineConversionFromInternalPlanar16Sharpen{};
    ComputePipeline m_pipelineConversionFromInternalNv12{};
    ComputePipeline m_pipelineConversionFromInternalNv12Sharpen{};
    ComputePipeline m_pipelineConversionFromInternalP010{};
    ComputePipeline m_pipelineConversionFromInternalP010Sharpen{};
    ComputePipeline m_pipelineConversionFromInternalRgba8Image{};
    ComputePipeline m_pipelineConversionFromInternalRgba8ImageGeneric{};
    ComputePipeline m_pipelineConversionFromInternalRgba10Image{};
    ComputePipeline m_pipelineConversionFromInternalRgba10ImageGeneric{};
    ComputePipeline m_pipelineConversionFromInternalP010Image{};
    ComputePipeline m_pipelineConversionFromInternalNv12Image{};
    ComputePipeline m_pipelineConversionFromInternalPlanar8AddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalPlanar8SharpenAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalPlanar16AddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalPlanar16SharpenAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalNv12AddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalNv12SharpenAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalP010AddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalP010SharpenAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalRgba8ImageAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalRgba8ImageGenericAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalRgba10ImageAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalRgba10ImageGenericAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalP010ImageAddTemporal{};
    ComputePipeline m_pipelineConversionFromInternalNv12ImageAddTemporal{};
    ComputePipeline m_pipelineDequantTransform{};

    DescriptorSetLayouts m_descriptorSetLayouts{};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_COMPUTE_VULKAN_H
