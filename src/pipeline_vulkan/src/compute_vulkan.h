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
#include "vulkan_handle.h"

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/ring_buffer.hpp>
#include <LCEVC/common/threads.hpp>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/pipeline/types.h>
//
#include <vulkan/vulkan.h>
//
#include <array>
#include <cstddef>
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

enum class ComputeLayoutId : uint8_t
{
    Upscale,
    Apply,
    Conversion,
    ConvertOutImage,
    DequantTransform,
    Count
};

enum class ComputePipelineId : uint8_t
{
    Upscale1DPA0,
    Upscale1DPA1,
    Upscale2DPA0,
    Upscale2DPA2,
    ApplyDd,
    ApplyDds,
    ApplyDdRaster,
    ApplyDdsRaster,
    ConversionToInternal8Bit,
    ConversionToInternal8BitNV12,
    ConversionToInternal16Bit,
    ConversionFromInternalPlanar8,
    ConversionFromInternalPlanar8Sharpen,
    ConversionFromInternalPlanar16,
    ConversionFromInternalPlanar16Sharpen,
    ConversionFromInternalNv12,
    ConversionFromInternalNv12Sharpen,
    ConversionFromInternalP010,
    ConversionFromInternalP010Sharpen,
    ConversionFromInternalRgba8Image,
    ConversionFromInternalRgba8ImageGeneric,
    ConversionFromInternalRgba10Image,
    ConversionFromInternalRgba10ImageGeneric,
    ConversionFromInternalP010Image,
    ConversionFromInternalNv12Image,
    ConversionFromInternalPlanar8AddTemporal,
    ConversionFromInternalPlanar8SharpenAddTemporal,
    ConversionFromInternalPlanar16AddTemporal,
    ConversionFromInternalPlanar16SharpenAddTemporal,
    ConversionFromInternalNv12AddTemporal,
    ConversionFromInternalNv12SharpenAddTemporal,
    ConversionFromInternalP010AddTemporal,
    ConversionFromInternalP010SharpenAddTemporal,
    ConversionFromInternalRgba8ImageAddTemporal,
    ConversionFromInternalRgba8ImageGenericAddTemporal,
    ConversionFromInternalRgba10ImageAddTemporal,
    ConversionFromInternalRgba10ImageGenericAddTemporal,
    ConversionFromInternalP010ImageAddTemporal,
    ConversionFromInternalNv12ImageAddTemporal,
    DequantTransform,
    Count
};

constexpr size_t toIndex(ComputeLayoutId id) { return static_cast<size_t>(id); }
constexpr size_t toIndex(ComputePipelineId id) { return static_cast<size_t>(id); }

constexpr size_t kMaxComputeDescriptorBindings = 4;

struct DescriptorSetLayoutInfo
{
    VkDescriptorSetLayout handle{};
    std::array<VkDescriptorType, kMaxComputeDescriptorBindings> descriptorTypes{};
    size_t descriptorTypeCount{};
};

struct DescriptorSetLayouts
{
    DescriptorSetLayoutInfo& operator[](size_t index) { return values[index]; }
    const DescriptorSetLayoutInfo& operator[](size_t index) const { return values[index]; }

    std::array<DescriptorSetLayoutInfo, toIndex(ComputeLayoutId::Count)> values{};
};

// State for Compute Pipeline Layout - may be shared between several specialized pipelines
struct ComputePipelineLayout
{
    DescriptorSetLayoutInfo descriptorSetLayoutInfo() const
    {
        return {descriptorSetLayout.get(), descriptorTypes, descriptorTypeCount};
    }

    UniqueDescriptorSetLayout descriptorSetLayout;
    UniquePipelineLayout pipelineLayout;
    std::array<VkDescriptorType, kMaxComputeDescriptorBindings> descriptorTypes{};
    size_t descriptorTypeCount{};
};

// State for a Compute Pipeline (possibly specialized)
struct ComputePipeline
{
    UniquePipeline pipeline;
    Dimension workgroup; // width and height of workgroup
    Dimension packing;   // How many samples per dimension of workgroup
    const char* name;    // debug name
};

class ComputeVulkan
{
public:
    ComputeVulkan(BackendVulkan& backend, uint32_t contextCount);
    ~ComputeVulkan() = default;

    bool init();

    bool beginCompute(VulkanFrameContext* context);
    bool endCompute(VulkanFrameContext* context);

    // Prepare buffer descriptors once before dispatching planes.
    bool prepareConvertIn(const VulkanConvertInArgs* params);
    // Convert one physical input plane. Interleaved UV writes both internal chroma planes.
    bool convertIn(const VulkanConvertInArgs* params, uint8_t plane);

    // Prepare upscale descriptors once before dispatching planes.
    bool prepareUpscale(const VulkanUpscaleArgs* params);
    // Upscale one internal planar i16 plane into a prepared destination.
    bool upscale(const LdeKernel* kernel, const VulkanUpscaleArgs* params, uint8_t plane);

    bool dequantTransform(VulkanDequantTransformArgs* params);

    bool applyCommon(VulkanApplyCommonArgs* params);
    bool applyTile(VulkanApplyTileArgs* params);

    bool convertOut(VulkanConvertOutArgs* params);

    bool updateConfig(uint8_t loq, const LdpPictureDesc& desc);

    uint32_t frameContextCount() const { return static_cast<uint32_t>(m_frameContexts.size()); }
    VulkanFrameContext* acquireFrameContext(FrameVulkan* frame);
    void releaseFrameContext(VulkanFrameContext* context);

    VNNoCopyNoMove(ComputeVulkan);

private:
    // Per-flavour helpers for convertOut(). The common preamble in
    // convertOut() resolves source/destination buffers and primes a
    // PushConstantsConversion; these methods finish off the dispatch for
    // their respective destination flavour.
    bool convertOutToImage(VulkanConvertOutArgs* params, PushConstantsConversion& constants);
    bool convertOutToBuffer(VulkanConvertOutArgs* params, PushConstantsConversion& constants);

    bool upscale1D(const LdeKernel* kernel, const VulkanUpscaleArgs* params, uint8_t plane);
    bool upscale2D(const LdeKernel* kernel, const VulkanUpscaleArgs* params, uint8_t plane);

    // Compute
    bool createComputeBindingsAndPipelineLayout(ComputePipelineLayout& computePipelineLayout,
                                                const VkDescriptorType* descriptorTypes,
                                                size_t descriptorTypeCount, uint32_t pushConstantsSize);
    bool createComputePipeline(ComputePipeline& computePipeline,
                               const ComputePipelineLayout& computePipelineLayout,
                               const unsigned char* shader, size_t shaderSize, const char* name,
                               Dimension workgroup, Dimension packing, const SpecValue* specValues,
                               size_t specValueCount);

    ComputePipelineLayout& pipelineLayout(ComputeLayoutId id)
    {
        return m_pipelineLayouts[toIndex(id)];
    }
    const ComputePipelineLayout& pipelineLayout(ComputeLayoutId id) const
    {
        return m_pipelineLayouts[toIndex(id)];
    }
    VkPipelineLayout pipelineLayoutHandle(ComputeLayoutId id) const
    {
        return pipelineLayout(id).pipelineLayout.get();
    }
    ComputePipeline& pipeline(ComputePipelineId id) { return m_pipelines[toIndex(id)]; }
    const ComputePipeline& pipeline(ComputePipelineId id) const { return m_pipelines[toIndex(id)]; }

    static bool unbindInternalPictureBuffer(PictureVulkan* picture);
    static bool prepareInternalPictureBuffer(PictureVulkan* picture, const LdpPictureDesc& desc);

    bool updateComputeDescriptorSets(VkDescriptorSet descriptorSet, BufferVulkan* buffer0,
                                     BufferVulkan* buffer1);
    bool updateComputeDescriptorSets(VkDescriptorSet descriptorSet, BufferVulkan* buffer0,
                                     BufferVulkan* buffer1, BufferVulkan* buffer2);

    bool updateComputeImageDescriptorSets(VkDescriptorSet descriptorSet, BufferVulkan* src,
                                          BufferVulkan* dst, BufferVulkan* temporal);
    bool updateDequantDescriptorSet(BufferVulkan* coefficients, BufferVulkan* dequantParameters,
                                    BufferVulkan* residuals, VkDescriptorSet descriptorSet);

    BackendVulkan& m_backend;

    std::array<LdpPictureDesc, LOQMaxCount> m_previousDesc{};

    // Layouts precede pipelines and frame contexts so C++ destroys their dependants first.
    std::array<ComputePipelineLayout, toIndex(ComputeLayoutId::Count)> m_pipelineLayouts{};
    std::array<ComputePipeline, toIndex(ComputePipelineId::Count)> m_pipelines{};

    DescriptorSetLayouts m_descriptorSetLayouts{};

    // Pool of compute decoding contexts
    std::vector<VulkanFrameContext> m_frameContexts;

    // Buffer of available context indexes - thread safe
    common::RingBuffer<uint32_t> m_freeContexts;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_COMPUTE_VULKAN_H
