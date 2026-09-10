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

#include "compute_vulkan.h"

#include "add.h"
#include "apply.h"
#include "buffer_vulkan.h"
#include "convert_in.h"
#include "convert_out.h"
#include "convert_out_image_nv12.h"
#include "convert_out_image_p010.h"
#include "convert_out_image_rgba10.h"
#include "convert_out_image_rgba8.h"
#include "frame_context_vulkan.h"
#include "frame_vulkan.h"
#include "from_base.h"
#include "picture_vulkan.h"
#include "pipeline_vulkan.h"
#include "shaders/src/specialization_ids.h"
#include "upscale_horizontal.h"
#include "upscale_vertical.h"

#include <LCEVC/build_config.h> // NOLINT(misc-include-cleaner) - needed for VN_SDK_FEATURE in #if
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/pipeline/detail/picture_layout.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
#include <vulkan/vulkan_core.h>
#if VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
#include <zlib.h> // NOLINT(misc-include-cleaner)
#endif

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace lcevc_dec::pipeline_vulkan {

struct PushConstantsUpscale
{
    int32_t kernel[4];
    uint32_t srcWidth;
    uint32_t srcHeight;
    uint32_t containerStrideIn;
    uint32_t containerOffsetIn;
    uint32_t containerStrideOut;
    uint32_t containerOffsetOut;
    uint32_t containerStrideBase;
    uint32_t containerOffsetBase;
};

struct PushConstantsApply
{
    uint32_t srcWidth;
    uint32_t srcHeight;
    uint32_t residualOffset;
    uint32_t containerStride;
    uint32_t containerOffset;
    uint32_t saturate;
    uint32_t tileX;
    uint32_t tileY;
    uint32_t tileWidth;
    uint32_t commandOffset; // offset into merged command buffer for this tile in uint32s
    uint32_t commandCount;  // number of commands in this tile
    // Reciprocal of blocksInRow as ceil(2^32 / blocksInRow). The shader recovers
    // block / blocksInRow via umulExtended, avoiding a per-command integer divide.
    // Zero is a sentinel meaning blocksInRow == 1 (single column of blocks).
    uint32_t blocksInRowMagic;
    uint32_t dds;
    uint32_t rasterOrder;
};

struct PushConstantsConversion
{
    uint32_t width;
    uint32_t height;
    uint32_t containerStrideIn;
    uint32_t containerOffsetIn;
    uint32_t containerStrideOut;
    uint32_t containerOffsetOut;
    uint32_t containerStrideV; // used to check for nv12 and as input or output stride for V-plane
    uint32_t containerOffsetV; // used with nv12 as input or output offset for V-plane
    uint32_t shift;            // 5 for 10bit, 3 for 12bit, and 1 for 14bit
    uint32_t sharpenStrength;  // 0 disables sharpening, otherwise U16 strength
    uint32_t nv21Swap;         // non-zero -> swap U/V order in 8-bit semi-planar chroma write
    // Note: convert_out_image_*.comp shaders alias `shift` and `sharpenStrength`
    // as chromaWidthShift / chromaHeightShift and do not read `nv21Swap`.
    //
    // Strides of eash planes' temporal buffer - buffer is present if stride != 0
    uint32_t containerStrideTemporal0;
    uint32_t containerOffsetTemporal0;
    uint32_t containerStrideTemporal1;
    uint32_t containerOffsetTemporal1;
    uint32_t containerStrideTemporal2;
    uint32_t containerOffsetTemporal2;
};

struct PushConstantsAdd
{
    uint32_t width;
    uint32_t height;
    uint32_t containerStride;
    uint32_t containerOffset;
};

namespace {
    // Convert a byte offset or stride to uint32 (SSBO) units. The GPU shader
    // addresses the buffer as uint32[], so byte values must be a multiple of 4.
    inline int toUint32Units(uint32_t bytes)
    {
        // Must be 4-byte aligned for buffer addressing
        assert((bytes & 3) == 0);
        return static_cast<int>(bytes >> 2);
    }

    template <typename, typename = std::void_t<>>
    struct HasContainerBase : std::false_type
    {};

    template <typename T>
    struct HasContainerBase<T, std::void_t<decltype(std::declval<T>().containerStrideBase),
                                           decltype(std::declval<T>().containerOffsetBase)>> : std::true_type
    {};

    template <typename T>
    inline void setContainerStrides(T& constants, uint32_t plane, const PictureVulkan* srcPicture,
                                    const PictureVulkan* dstPicture, const PictureVulkan* basePicture)
    {
        constants.containerStrideIn =
            toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, plane));
        constants.containerOffsetIn =
            toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, plane));
        constants.containerStrideOut =
            toUint32Units(ldpPictureLayoutRowStride(&dstPicture->layout, plane));
        constants.containerOffsetOut =
            toUint32Units(ldpPictureLayoutPlaneOffset(&dstPicture->layout, plane));
        if constexpr (HasContainerBase<T>::value) {
            if (basePicture) {
                constants.containerStrideBase =
                    toUint32Units(ldpPictureLayoutRowStride(&basePicture->layout, plane));
                constants.containerOffsetBase =
                    toUint32Units(ldpPictureLayoutPlaneOffset(&basePicture->layout, plane));
            }
        }
    }

    inline void setTemporalStrides(PushConstantsConversion& constants, uint32_t slot, uint32_t plane,
                                   bool doAdd, uint8_t numEnhancedPlanes, const PictureVulkan* temporal)
    {
        uint32_t stride = 0;
        uint32_t offset = 0;
        if (doAdd && temporal && plane < numEnhancedPlanes) {
            stride = static_cast<uint32_t>(
                toUint32Units(ldpPictureLayoutRowStride(&temporal->layout, plane)));
            offset = static_cast<uint32_t>(
                toUint32Units(ldpPictureLayoutPlaneOffset(&temporal->layout, plane)));
        }
        switch (slot) {
            case 0:
                constants.containerStrideTemporal0 = stride;
                constants.containerOffsetTemporal0 = offset;
                break;
            case 1:
                constants.containerStrideTemporal1 = stride;
                constants.containerOffsetTemporal1 = offset;
                break;
            default:
                constants.containerStrideTemporal2 = stride;
                constants.containerOffsetTemporal2 = offset;
                break;
        }
    }

    inline void getSubsamplingShifts(LdeChroma chroma, int& widthShift, int& heightShift)
    {
        switch (chroma) {
            case LdeChroma::CT420:
                widthShift = 1;
                heightShift = 1;
                return;
            case LdeChroma::CT422:
                widthShift = 1;
                heightShift = 0;
                return;
            default:
                widthShift = 0;
                heightShift = 0;
                return;
        }
    }

    inline LdpColorFormat chromaToColorFormat(LdeChroma chroma)
    {
        switch (chroma) {
            case LdeChroma::CTMonochrome: return LdpColorFormatGRAY_16_LE;
            case LdeChroma::CT420: return LdpColorFormatI420_16_LE;
            case LdeChroma::CT422: return LdpColorFormatI422_16_LE;
            case LdeChroma::CT444: return LdpColorFormatI444_16_LE;
            default: return LdpColorFormatUnknown;
        }
    }
} // namespace

//// ComputeVulkan
//
ComputeVulkan::ComputeVulkan(BackendVulkan& backend, uint32_t contextCount)
    : m_backend(backend)
    , m_frameContexts(contextCount)
    , m_freeContexts(nextPowerOfTwoU32(contextCount + 1))
{}

VulkanFrameContext* ComputeVulkan::acquireFrameContext(FrameVulkan* frame)
{
    VkDevice& device = m_backend.getDevice();

    uint32_t idx{};
    m_freeContexts.pop(idx);
    assert(idx < m_frameContexts.size());

    VulkanFrameContext* context = &m_frameContexts[idx];

    vkResetFences(device, 1, &context->m_fence);
    context->m_frame = frame;

    return &m_frameContexts[idx];
}

void ComputeVulkan::releaseFrameContext(VulkanFrameContext* context)
{
    const uint32_t idx = static_cast<uint32_t>(context - m_frameContexts.data());

    assert(context >= m_frameContexts.data() &&
           context <= (m_frameContexts.data() + m_frameContexts.size()));

    context->m_frame = nullptr;

    m_freeContexts.push(idx);
}

bool ComputeVulkan::createComputeBindingsAndPipelineLayout(ComputePipelineLayout& computePipelineLayout,
                                                           uint32_t numBuffers, uint32_t pushConstantsSize)
{
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

    for (uint32_t i = 0; i < numBuffers; i++) {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = i;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings.push_back(layoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
    setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    setLayoutCreateInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(m_backend.getDevice(), &setLayoutCreateInfo, nullptr,
                                    &computePipelineLayout.descriptorSetLayout) != VK_SUCCESS) {
        VNLogError("failed to create descriptor set layout!");
        return false;
    }

    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = pushConstantsSize;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &computePipelineLayout.descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &range;

    if (vkCreatePipelineLayout(m_backend.getDevice(), &pipelineLayoutCreateInfo, nullptr,
                               &computePipelineLayout.pipelineLayout) != VK_SUCCESS) {
        VNLogError("failed to create pipeline layout");
        return false;
    }

    return true;
}

bool ComputeVulkan::createConvertOutImageBindingsAndPipelineLayout(ComputePipelineLayout& computePipelineLayout,
                                                                   uint32_t pushConstantsSize)
{
    // Bindings:
    //  0:   src (prediction) buffer,
    //  1,2: dst image plane(s),
    //  3:   temporal buffer.
    std::array<VkDescriptorSetLayoutBinding, 4> layoutBindings{};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[3].binding = 3;
    layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[3].descriptorCount = 1;
    layoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
    setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    setLayoutCreateInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(m_backend.getDevice(), &setLayoutCreateInfo, nullptr,
                                    &computePipelineLayout.descriptorSetLayout) != VK_SUCCESS) {
        VNLogError("failed to create image conversion descriptor set layout!");
        return false;
    }

    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = pushConstantsSize;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &computePipelineLayout.descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &range;

    if (vkCreatePipelineLayout(m_backend.getDevice(), &pipelineLayoutCreateInfo, nullptr,
                               &computePipelineLayout.pipelineLayout) != VK_SUCCESS) {
        VNLogError("failed to create image conversion pipeline layout");
        return false;
    }

    return true;
}

bool ComputeVulkan::createComputePipeline(ComputePipeline& computePipeline,
                                          const ComputePipelineLayout& computePipelineLayout,
                                          const unsigned char* shader, size_t shaderSize,
                                          const char* name, Dimension workgroup, Dimension packing,
                                          const std::vector<SpecValue>& extraSpecValues)
{
    computePipeline.name = name;
    computePipeline.workgroup = workgroup;
    computePipeline.packing = packing;

#if VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
    // Detect size-prefixed zlib compression: compressed blobs start with a LE uint32 uncompressed
    // size rather than the SPIR-V magic 0x07230203.
    std::vector<uint8_t> decompressed;
    if (shaderSize >= 4) {
        uint32_t firstWord;
        memcpy(&firstWord, shader, 4);
        if (firstWord != 0x07230203u) {
            decompressed.resize(firstWord);
            uLongf len = firstWord;
            if (uncompress(decompressed.data(), &len, shader + 4, static_cast<uLong>(shaderSize - 4)) != Z_OK) {
                VNLogError("Failed to decompress shader SPIR-V for %s", name);
                return false;
            }
            shader = reinterpret_cast<const unsigned char*>(decompressed.data());
            shaderSize = static_cast<size_t>(len);
        }
    }
#endif

    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shader);
    shaderModuleCreateInfo.codeSize = shaderSize;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (VkResult r = vkCreateShaderModule(m_backend.getDevice(), &shaderModuleCreateInfo, nullptr,
                                          &shaderModule);
        r != VK_SUCCESS) {
        VNLogError("failed to create shader module for %s: %d", name, static_cast<int32_t>(r));
        return false;
    }

    // Set up specialization data
    std::vector<SpecValue> specializationData = {
        {SPECID_LOCAL_SIZE_X, computePipeline.workgroup.w},
        {SPECID_LOCAL_SIZE_Y, computePipeline.workgroup.h},
        {SPECID_LOCAL_SIZE_Z, 1},
    };
    specializationData.insert(specializationData.end(), extraSpecValues.begin(), extraSpecValues.end());

    // Convert to map for Vulkan
    std::vector<VkSpecializationMapEntry> specializationMap;

    specializationMap.reserve(specializationData.size());
    uint32_t offset{offsetof(SpecValue, value)};
    for (const auto& d : specializationData) {
        specializationMap.push_back(VkSpecializationMapEntry{d.id, offset, sizeof(d.value)});
        offset += sizeof(SpecValue);
    }

    VkSpecializationInfo specializationInfo{};
    specializationInfo.mapEntryCount = static_cast<uint32_t>(specializationMap.size());
    specializationInfo.pMapEntries = specializationMap.data();
    specializationInfo.dataSize = specializationData.size() * sizeof(specializationData[0]);
    specializationInfo.pData = specializationData.data();

    VkComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module = shaderModule;
    pipelineCreateInfo.stage.pSpecializationInfo = &specializationInfo;

    pipelineCreateInfo.stage.pName = "main";
    pipelineCreateInfo.layout = computePipelineLayout.pipelineLayout;

    if (VkResult r = vkCreateComputePipelines(m_backend.getDevice(), VK_NULL_HANDLE, 1,
                                              &pipelineCreateInfo, nullptr, &computePipeline.pipeline);
        r != VK_SUCCESS) {
        VNLogError("failed to create compute pipeline %s: %d", name, static_cast<int32_t>(r));
        vkDestroyShaderModule(m_backend.getDevice(), shaderModule, nullptr);
        return false;
    }
    vkDestroyShaderModule(m_backend.getDevice(), shaderModule, nullptr);

    return true;
}

bool ComputeVulkan::unbindInternalPictureBuffer(PictureVulkan* picture)
{
    if (picture->buffer == nullptr) {
        return true;
    }

    BufferVulkan* buffer = fromPipeline(picture->buffer);
    if (buffer != nullptr && buffer->getVkBuffer() != VK_NULL_HANDLE && buffer->size() != 0) {
        return true;
    }

    if (picture->isExternalBuffer() || !picture->unbindBuffer()) {
        VNLogError("failed to discard invalid internal picture buffer");
        return false;
    }

    return true;
}

bool ComputeVulkan::prepareInternalPictureBuffer(PictureVulkan* picture, const LdpPictureDesc& desc)
{
    if (picture == nullptr) {
        VNLogError("null internal picture");
        return false;
    }

    if (!unbindInternalPictureBuffer(picture)) {
        return false;
    }

    if (!picture->setDescAndBind(desc, pipeline::BufferUsageInternal)) {
        unbindInternalPictureBuffer(picture);
        VNLogError("failed to bind internal picture buffer: width=%u height=%u", desc.width, desc.height);
        return false;
    }

    BufferVulkan* buffer = fromPipeline(picture->buffer);
    if (buffer == nullptr || buffer->getVkBuffer() == VK_NULL_HANDLE || buffer->size() == 0) {
        unbindInternalPictureBuffer(picture);
        VNLogError("binding produced an invalid internal picture buffer: width=%u height=%u",
                   desc.width, desc.height);
        return false;
    }
    return true;
}

bool ComputeVulkan::prepareDescriptorBufferInfo(BufferVulkan* buffer, VkDescriptorBufferInfo& info)
{
    if (buffer == nullptr) {
        VNLogError("null descriptor buffer");
        return false;
    }

    const VkBuffer vkBuffer = buffer->getVkBuffer();
    const uint32_t size = buffer->size();
    if (vkBuffer == VK_NULL_HANDLE || size == 0) {
        VNLogError("invalid descriptor buffer: object=%p vkBufferNull=%d size=%u",
                   static_cast<void*>(buffer), vkBuffer == VK_NULL_HANDLE, size);
        return false;
    }

    info.buffer = vkBuffer;
    info.offset = 0;
    info.range = size;
    return true;
}

bool ComputeVulkan::validateDescriptorSet(VkDescriptorSet descriptorSet)
{
    if (descriptorSet == VK_NULL_HANDLE) {
        VNLogError("null descriptor set");
        return false;
    }
    return true;
}

bool ComputeVulkan::updateComputeDescriptorSets(BufferVulkan* src, BufferVulkan* dst,
                                                VkDescriptorSet descriptorSet)
{
    if (!validateDescriptorSet(descriptorSet)) {
        return false;
    }

    std::array<VkDescriptorBufferInfo, 2> bufferInfos{};
    if (!prepareDescriptorBufferInfo(src, bufferInfos[0]) ||
        !prepareDescriptorBufferInfo(dst, bufferInfos[1])) {
        return false;
    }

    std::array<VkWriteDescriptorSet, 2> writes{};
    for (uint32_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptorSet;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(m_backend.getDevice(), static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    return true;
}

bool ComputeVulkan::updateComputeImageDescriptorSets(BufferVulkan* src, BufferVulkan* dst,
                                                     BufferVulkan* temporal, VkDescriptorSet descriptorSet)
{
    if (!validateDescriptorSet(descriptorSet)) {
        return false;
    }

    VkDescriptorBufferInfo bufferInfo{};
    VkDescriptorBufferInfo temporalInfo{};
    if (!prepareDescriptorBufferInfo(src, bufferInfo) ||
        !prepareDescriptorBufferInfo(temporal, temporalInfo)) {
        return false;
    }

    if (dst == nullptr) {
        VNLogError("null destination descriptor image");
        return false;
    }

    const VkImage image = dst->getVkImage();
    const VkImageView plane0View = dst->getVkImageView(0);
    const VkImageView plane1View = dst->getVkImageView(1);
    if (image == VK_NULL_HANDLE || plane0View == VK_NULL_HANDLE || plane1View == VK_NULL_HANDLE) {
        VNLogError("invalid destination descriptor image: object=%p imageNull=%d "
                   "plane0ViewNull=%d plane1ViewNull=%d",
                   static_cast<void*>(dst), image == VK_NULL_HANDLE, plane0View == VK_NULL_HANDLE,
                   plane1View == VK_NULL_HANDLE);
        return false;
    }

    VkDescriptorImageInfo imageInfoPlane0{};
    imageInfoPlane0.imageView = plane0View;
    imageInfoPlane0.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo imageInfoPlane1{};
    imageInfoPlane1.imageView = plane1View;
    imageInfoPlane1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array<VkWriteDescriptorSet, 4> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &bufferInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &imageInfoPlane0;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = descriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].pImageInfo = &imageInfoPlane1;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = descriptorSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo = &temporalInfo;

    vkUpdateDescriptorSets(m_backend.getDevice(), static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    return true;
}

bool ComputeVulkan::updateComputeDescriptorSets(BufferVulkan* src, BufferVulkan* dst1,
                                                BufferVulkan* dst2, VkDescriptorSet descriptorSet)
{
    if (!validateDescriptorSet(descriptorSet)) {
        return false;
    }

    std::array<VkDescriptorBufferInfo, 3> bufferInfos{};
    if (!prepareDescriptorBufferInfo(src, bufferInfos[0]) ||
        !prepareDescriptorBufferInfo(dst1, bufferInfos[1]) ||
        !prepareDescriptorBufferInfo(dst2, bufferInfos[2])) {
        return false;
    }

    std::array<VkWriteDescriptorSet, 3> writes{};
    for (uint32_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptorSet;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(m_backend.getDevice(), static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    return true;
}

bool ComputeVulkan::updateConfig(uint8_t loq, const LdpPictureDesc& desc)
{
    if (desc != m_previousDesc[loq]) {
        m_previousDesc[loq] = desc;
        return true;
    }
    return false;
}

bool ComputeVulkan::beginCompute(VulkanFrameContext* context, uint32_t numTiles)
{
    return context->begin(m_descriptorSetLayouts, numTiles);
}

bool ComputeVulkan::endCompute(VulkanFrameContext* context)
{
    return context->end(m_backend.getComputeQueue());
}

bool ComputeVulkan::init()
{
    // Maybe tune based on device?
    uint32_t workgroupWidth = 32;
    uint32_t workgroupHeight = 8;

    if (!createComputeBindingsAndPipelineLayout(m_pipelineLayoutVertical, 2, sizeof(PushConstantsUpscale))) {
        return false;
    }

    if (!createComputeBindingsAndPipelineLayout(m_pipelineLayoutHorizontal, 3,
                                                sizeof(PushConstantsUpscale))) {
        return false;
    }

    if (!createComputeBindingsAndPipelineLayout(m_pipelineLayoutApply, 2, sizeof(PushConstantsApply))) {
        return false;
    }

    if (!createComputeBindingsAndPipelineLayout(m_pipelineLayoutConversion, 3,
                                                sizeof(PushConstantsConversion))) {
        return false;
    }

    if (!createConvertOutImageBindingsAndPipelineLayout(m_pipelineLayoutConvertOutImage,
                                                        sizeof(PushConstantsConversion))) {
        return false;
    }

    if (!createComputeBindingsAndPipelineLayout(m_pipelineLayoutAdd, 2, sizeof(PushConstantsAdd))) {
        return false;
    }

    if (!createComputePipeline(m_pipelineVertical, m_pipelineLayoutVertical, upscale_vertical_spv,
                               sizeof(upscale_vertical_spv), "upscaleVertical", {16, 16}, {2, 1}, {})) {
        return false;
    }

    if (!createComputePipeline(m_pipelineHorizontalPA0, m_pipelineLayoutHorizontal,
                               upscale_horizontal_spv, sizeof(upscale_horizontal_spv),
                               "upscaleHorizontalPA0", {64, 1}, {2, 1}, {{SPECID_PA, 0}})) {
        return false;
    }

    if (!createComputePipeline(m_pipelineHorizontalPA1, m_pipelineLayoutHorizontal,
                               upscale_horizontal_spv, sizeof(upscale_horizontal_spv),
                               "upscaleHorizontalPA1", {64, 1}, {2, 1}, {{SPECID_PA, 1}})) {
        return false;
    }

    if (!createComputePipeline(m_pipelineHorizontalPA2, m_pipelineLayoutHorizontal,
                               upscale_horizontal_spv, sizeof(upscale_horizontal_spv),
                               "upscaleHorizontalPA2", {64, 1}, {2, 1}, {{SPECID_PA, 2}})) {
        return false;
    }

    if (!createComputePipeline(m_pipelineApplyDd, m_pipelineLayoutApply, apply_spv,
                               sizeof(apply_spv), "applyDd", {64, 1}, {1, 1}, {})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineApplyDds, m_pipelineLayoutApply, apply_spv,
                               sizeof(apply_spv), "applyDds", {64, 1}, {1, 1}, {})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineApplyDdRaster, m_pipelineLayoutApply, apply_spv,
                               sizeof(apply_spv), "applyDdRaster", {64, 1}, {1, 1}, {})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineApplyDdsRaster, m_pipelineLayoutApply, apply_spv,
                               sizeof(apply_spv), "applyDdsRaster", {64, 1}, {1, 1}, {})) {
        return false;
    }

    if (!createComputePipeline(m_pipelineConversionToInternal8Bit, m_pipelineLayoutConversion,
                               convert_in_spv, sizeof(convert_in_spv), "conversionIn8",
                               {workgroupWidth, workgroupHeight}, {4, 1},
                               {{SPECID_8_BIT, 1}, {SPECID_NV12, 0}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionToInternal8BitNV12, m_pipelineLayoutConversion,
                               convert_in_spv, sizeof(convert_in_spv), "conversionIn8Nv12",
                               {workgroupWidth, workgroupHeight}, {4, 1},
                               {{SPECID_8_BIT, 1}, {SPECID_NV12, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternal8Bit, m_pipelineLayoutConversion,
                               convert_out_spv, sizeof(convert_out_spv), "conversionOut8",
                               {workgroupWidth, workgroupHeight}, {4, 1},
                               {{SPECID_8_BIT, 1}, {SPECID_NV12, 0}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternal8BitNV12, m_pipelineLayoutConversion,
                               convert_out_spv, sizeof(convert_out_spv), "conversionOut8Nv12",
                               {workgroupWidth, workgroupHeight}, {4, 1},
                               {{SPECID_8_BIT, 1}, {SPECID_NV12, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionToInternal16Bit, m_pipelineLayoutConversion,
                               convert_in_spv, sizeof(convert_in_spv), "conversionIn16",
                               {workgroupWidth, workgroupHeight}, {2, 1},
                               {{SPECID_8_BIT, 0}, {SPECID_NV12, 0}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternal16Bit, m_pipelineLayoutConversion,
                               convert_out_spv, sizeof(convert_out_spv), "conversionOut16",
                               {workgroupWidth, workgroupHeight}, {2, 1},
                               {{SPECID_8_BIT, 0}, {SPECID_NV12, 0}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternal16BitP010,
                               m_pipelineLayoutConversion, convert_out_spv, sizeof(convert_out_spv),
                               "conversionOut16P010", {workgroupWidth, workgroupHeight}, {2, 1},
                               {{SPECID_8_BIT, 0}, {SPECID_NV12, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalRgba8Image,
                               m_pipelineLayoutConvertOutImage, convert_out_image_rgba8_spv,
                               sizeof(convert_out_image_rgba8_spv), "conversionOutRgba8Image",
                               {workgroupWidth, workgroupHeight}, {1, 1}, {})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalP010Image, m_pipelineLayoutConvertOutImage,
                               convert_out_image_p010_spv, sizeof(convert_out_image_p010_spv),
                               "conversionOutP010Image", {workgroupWidth, workgroupHeight}, {1, 1}, {})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalNv12Image, m_pipelineLayoutConvertOutImage,
                               convert_out_image_nv12_spv, sizeof(convert_out_image_nv12_spv),
                               "conversionOutNv12Image", {workgroupWidth, workgroupHeight}, {1, 1}, {})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalRgba10Image,
                               m_pipelineLayoutConvertOutImage, convert_out_image_rgba10_spv,
                               sizeof(convert_out_image_rgba10_spv), "conversionOutRgba10Image",
                               {workgroupWidth, workgroupHeight}, {1, 1}, {})) {
        return false;
    }

    if (!createComputePipeline(m_pipelineConversionFromInternal8BitAddTemporal,
                               m_pipelineLayoutConversion, convert_out_spv, sizeof(convert_out_spv),
                               "conversionOut8AddTemporal", {workgroupWidth, workgroupHeight}, {4, 1},
                               {{SPECID_8_BIT, 1}, {SPECID_NV12, 0}, {SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternal8BitNV12AddTemporal,
                               m_pipelineLayoutConversion, convert_out_spv, sizeof(convert_out_spv),
                               "conversionOut8Nv12AddTemporal", {workgroupWidth, workgroupHeight},
                               {4, 1}, {{SPECID_8_BIT, 1}, {SPECID_NV12, 1}, {SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternal16BitAddTemporal,
                               m_pipelineLayoutConversion, convert_out_spv, sizeof(convert_out_spv),
                               "conversionOut16AddTemporal", {workgroupWidth, workgroupHeight}, {2, 1},
                               {{SPECID_8_BIT, 0}, {SPECID_NV12, 0}, {SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternal16BitP010AddTemporal,
                               m_pipelineLayoutConversion, convert_out_spv, sizeof(convert_out_spv),
                               "conversionOut16P010AddTemporal", {workgroupWidth, workgroupHeight},
                               {2, 1}, {{SPECID_8_BIT, 0}, {SPECID_NV12, 1}, {SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalRgba8ImageAddTemporal,
                               m_pipelineLayoutConvertOutImage, convert_out_image_rgba8_spv,
                               sizeof(convert_out_image_rgba8_spv), "conversionOutRgba8ImageAddTemporal",
                               {workgroupWidth, workgroupHeight}, {1, 1}, {{SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalP010ImageAddTemporal,
                               m_pipelineLayoutConvertOutImage, convert_out_image_p010_spv,
                               sizeof(convert_out_image_p010_spv), "conversionOutP010ImageAddTemporal",
                               {workgroupWidth, workgroupHeight}, {1, 1}, {{SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalNv12ImageAddTemporal,
                               m_pipelineLayoutConvertOutImage, convert_out_image_nv12_spv,
                               sizeof(convert_out_image_nv12_spv), "conversionOutNv12ImageAddTemporal",
                               {workgroupWidth, workgroupHeight}, {1, 1}, {{SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }
    if (!createComputePipeline(m_pipelineConversionFromInternalRgba10ImageAddTemporal,
                               m_pipelineLayoutConvertOutImage, convert_out_image_rgba10_spv,
                               sizeof(convert_out_image_rgba10_spv), "conversionOutRgba10ImageAddTemporal",
                               {workgroupWidth, workgroupHeight}, {1, 1}, {{SPECID_ADD_TEMPORAL, 1}})) {
        return false;
    }

    if (!createComputePipeline(m_pipelineAdd, m_pipelineLayoutAdd, add_spv, sizeof(add_spv), "add",
                               {workgroupWidth, workgroupHeight}, {2, 1}, {})) {
        return false;
    }

    // Build free list of frame contexts
    for (auto& context : m_frameContexts) {
        context.m_backend = &m_backend;
        if (!context.init()) {
            return false;
        }

        const uint32_t idx = static_cast<uint32_t>(&context - m_frameContexts.data());
        m_freeContexts.push(idx);
    }

    // Recreate for previous code
    m_descriptorSetLayouts.vertical = m_pipelineLayoutVertical.descriptorSetLayout;
    m_descriptorSetLayouts.horizontal = m_pipelineLayoutHorizontal.descriptorSetLayout;
    m_descriptorSetLayouts.apply = m_pipelineLayoutApply.descriptorSetLayout;
    m_descriptorSetLayouts.conversion = m_pipelineLayoutConversion.descriptorSetLayout;
    m_descriptorSetLayouts.convertOutImage = m_pipelineLayoutConvertOutImage.descriptorSetLayout;
    m_descriptorSetLayouts.add = m_pipelineLayoutAdd.descriptorSetLayout;

    return true;
}

bool ComputeVulkan::applyCommon(VulkanApplyCommonArgs* params)
{
    if (params == nullptr || params->context == nullptr || params->loq >= LOQEnhancedCount) {
        VNLogError("apply: invalid arguments");
        return false;
    }

    const bool applyTemporal = (params->picture == nullptr);
    if (applyTemporal && params->temporalPicture == nullptr) {
        VNLogError("apply: null temporal picture");
        return false;
    }

    if (applyTemporal && params->plane == 0) {
        LdpPictureDesc desc{};
        ldpDefaultPictureDesc(&desc, chromaToColorFormat(params->chroma), params->planeWidth,
                              params->planeHeight);
        BufferVulkan* previousBuffer = fromPipeline(params->temporalPicture->buffer);
        const bool needsClear = updateConfig(LOQEnhancedCount, desc) || previousBuffer == nullptr ||
                                previousBuffer->getVkBuffer() == VK_NULL_HANDLE ||
                                previousBuffer->size() == 0;
        if (!prepareInternalPictureBuffer(params->temporalPicture, desc)) {
            VNLogError("apply: failed to prepare temporal picture buffer");
            return false;
        }
        if (needsClear) {
            BufferVulkan* temporalBuffer = fromPipeline(params->temporalPicture->buffer);
            temporalBuffer->clearCmd(params->context->m_commandBuffer);
        }
    }

    VulkanFrameContext* context = params->context;
    const uint8_t loq = params->loq;

    // Select per-LOQ command buffer and descriptor set
    BufferVulkan*& gpuCommandBuffer = context->m_gpuCommandBuffer[loq];
    VkDescriptorSet& descriptorSetApply = context->m_descriptorSetApply[loq];

    BufferVulkan* applyPlaneBuffer = applyTemporal ? fromPipeline(params->temporalPicture->buffer)
                                                   : fromPipeline(params->picture->buffer);

    // Update the per-LOQ apply descriptor set. The output plane may change between tiles
    // (different planes/pictures), so always update.
    if (!updateComputeDescriptorSets(gpuCommandBuffer, applyPlaneBuffer, descriptorSetApply)) {
        VNLogError("apply: failed to update descriptor set");
        return false;
    }

    // Select the specialised apply pipeline based on DDS and raster order
    const ComputePipeline* applyPipeline{};
    if (params->dds) {
        if (params->tuRasterOrder) {
            applyPipeline = &m_pipelineApplyDdsRaster;
        } else {
            applyPipeline = &m_pipelineApplyDds;
        }
    } else {
        if (params->tuRasterOrder) {
            applyPipeline = &m_pipelineApplyDdRaster;
        } else {
            applyPipeline = &m_pipelineApplyDd;
        }
    }

    context->bind(&m_pipelineLayoutApply, applyPipeline, descriptorSetApply);

    return true;
}

bool ComputeVulkan::applyTile(VulkanApplyTileArgs* params)
{
    const LdeCmdBufferGpu buffer = params->bufferGpu;

    if (buffer.commandCount == 0) {
        // Nothing to do
        return true;
    }

    VulkanFrameContext* context = params->context;
    const uint8_t loq = params->loq;
    assert(loq < LOQEnhancedCount);

    // Select per-LOQ command buffer state
    BufferVulkan* gpuCommandBuffer = context->m_gpuCommandBuffer[loq];
    const uint32_t offset = context->m_gpuCommandBufferOffset[loq];
    assert((offset & 3) == 0);

    // Figure out sizes
    const uint32_t residualOffset = sizeof(LdeCmdBufferGpuCmd) * buffer.commandCount;
    const uint32_t residualSize = buffer.residualCount * sizeof(uint16_t);
    const uint32_t tileSize = residualOffset + residualSize;

    // Copy this tile's data into the pre-allocated merged buffer at the aligned offset
    gpuCommandBuffer->copyIn(offset, buffer.commands, residualOffset);
    gpuCommandBuffer->copyIn(offset + residualOffset, buffer.residuals,
                             buffer.residualCount * sizeof(uint16_t));

    // Advance the write offset past this tile's data
    context->m_gpuCommandBufferOffset[loq] = alignU32(offset + tileSize, 4);

    // Build push constants
    PushConstantsApply constants{};

    constants.srcWidth = params->planeWidth;
    constants.srcHeight = params->planeHeight;
    const PictureVulkan* applyPicture = params->picture ? params->picture : params->temporalPicture;
    constants.containerStride =
        toUint32Units(ldpPictureLayoutRowStride(&applyPicture->layout, params->plane));
    constants.containerOffset =
        toUint32Units(ldpPictureLayoutPlaneOffset(&applyPicture->layout, params->plane));

    constants.residualOffset = toUint32Units(residualOffset);
    constants.saturate = params->highlightResiduals ? 1 : 0;
    constants.tileX = params->tileX;
    constants.tileY = params->tileY;
    constants.tileWidth = params->tileWidth;
    constants.commandOffset = toUint32Units(offset);
    constants.commandCount = buffer.commandCount;
    const uint32_t applyWidth = constants.tileWidth != 0 ? constants.tileWidth : constants.srcWidth;
    const uint32_t blocksInRow = (applyWidth + 31u) / 32u;
    constants.blocksInRowMagic =
        blocksInRow <= 1u ? 0u : static_cast<uint32_t>(((uint64_t{1} << 32) + blocksInRow - 1u) / blocksInRow);
    constants.dds = params->dds ? 1 : 0;
    constants.rasterOrder = params->tuRasterOrder ? 1 : 0;

    vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutApply.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsApply), &constants);

    // Dispatch shader
    context->dispatch(buffer.commandCount, 1, "apply");

    return true;
}

bool ComputeVulkan::add(VulkanAddArgs* params)
{
    PictureVulkan* srcPicture = params->src;
    BufferVulkan* srcBuffer = fromPipeline(srcPicture->buffer);

    PictureVulkan* dstPicture = params->dst;
    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);

    VulkanFrameContext* context = params->context;

    if (!updateComputeDescriptorSets(srcBuffer, dstBuffer, context->m_descriptorSetAdd)) {
        VNLogError("add: failed to update descriptor set");
        return false;
    }

    context->bind(&m_pipelineLayoutAdd, &m_pipelineAdd, context->m_descriptorSetAdd);

    PushConstantsAdd constants{};
    for (uint8_t plane = 0; plane < params->numEnhancedPlanes; ++plane) {
        int widthShift = 0;
        int heightShift = 0;
        if (plane != 0) {
            getSubsamplingShifts(params->chroma, widthShift, heightShift);
        }
        constants.width = srcPicture->getActiveWidth() >> widthShift;
        constants.height = srcPicture->getActiveHeight() >> heightShift;
        constants.containerStride = toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, plane));
        constants.containerOffset =
            toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, plane));

        vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutAdd.pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsAdd), &constants);

        context->dispatch(constants.width, constants.height, "add");
    }

    context->insertComputeBarrier();

    return true;
}

bool ComputeVulkan::convertIn(VulkanConvertInArgs* params)
{
    PictureVulkan* srcPicture = params->src;
    BufferVulkan* srcBuffer = fromPipeline(srcPicture->buffer);
    const LdpColorFormat srcFormat = ldpPictureLayoutFormat(&srcPicture->layout);
    const bool is8bit = ldpPictureLayoutSampleSize(&srcPicture->layout) == 1;

    PictureVulkan* dstPicture = params->dst;
    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);

    const bool isNv12 = srcFormat == LdpColorFormatNV12_8 || srcFormat == LdpColorFormatNV21_8;

    PushConstantsConversion constants{};

    constants.width = srcPicture->layout.width;
    constants.height = srcPicture->layout.height;
    constants.shift = 15 - params->bitDepth;
    constants.sharpenStrength = 0;

    setContainerStrides(constants, 0, srcPicture, dstPicture, nullptr);

    VulkanFrameContext* context = params->context;

    constants.containerStrideV = 0; // used to signal no nv12
    constants.containerOffsetV = 0;

    // Alias srcBuffer intop temporal
    if (!updateComputeDescriptorSets(srcBuffer, dstBuffer, srcBuffer, context->m_descriptorSetConversionTo)) {
        VNLogError("convert_in: failed to update descriptor set");
        return false;
    }

    if (is8bit) {
        context->bind(&m_pipelineLayoutConversion,
                      isNv12 ? &m_pipelineConversionToInternal8BitNV12 : &m_pipelineConversionToInternal8Bit,
                      context->m_descriptorSetConversionTo);
    } else {
        context->bind(&m_pipelineLayoutConversion, &m_pipelineConversionToInternal16Bit,
                      context->m_descriptorSetConversionTo);
    }

    vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutConversion.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);

    context->dispatch(constants.width, constants.height, "convert_in_y"); // Y

    if (params->chroma != LdeChroma::CTMonochrome) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(params->chroma, widthShift, heightShift);

        if (!isNv12) {
            constants.width = srcPicture->layout.width >> widthShift;
        } else {
            constants.containerStrideV =
                toUint32Units(ldpPictureLayoutRowStride(&dstPicture->layout, 2));
            constants.containerOffsetV =
                toUint32Units(ldpPictureLayoutPlaneOffset(&dstPicture->layout, 2));
        }
        const uint32_t chromaHeight = srcPicture->layout.height >> heightShift;
        constants.height = chromaHeight;
        constants.sharpenStrength = 0;
        setContainerStrides(constants, 1, srcPicture, dstPicture, nullptr);

        vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutConversion.pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
        context->dispatch(constants.width, chromaHeight, "convert_in_u"); // U

        if (!isNv12) {
            setContainerStrides(constants, 2, srcPicture, dstPicture, nullptr);
            vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutConversion.pipelineLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion),
                               &constants);
            context->dispatch(constants.width, chromaHeight, "convert_in_v"); // V
        }
    }

    context->insertComputeBarrier();

    return true;
}

bool ComputeVulkan::convertOut(VulkanConvertOutArgs* params)
{
    PictureVulkan* srcPicture = params->src;
    PictureVulkan* dstPicture = params->dst;
    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);
    if (dstBuffer == nullptr) {
        VNLogError("convert_out: null destination buffer");
        return false;
    }
    const LdpColorFormat dstFormat = ldpPictureLayoutFormat(&dstPicture->layout);

    PushConstantsConversion constants{};
    constants.width = srcPicture->layout.width;
    constants.height = srcPicture->layout.height;
    constants.shift = 15 - params->bitDepth;
    constants.sharpenStrength =
        static_cast<uint32_t>(clampF32(params->sharpenStrength, 0.0f, 1.0f) * 65535.0f);
    setContainerStrides(constants, 0, srcPicture, dstPicture, nullptr);

    // Figure if writing to image or buffer ...
    const bool dstHasImage = dstBuffer->getVkImage() != VK_NULL_HANDLE;
    const bool dstIsImageFormat =
        dstFormat == LdpColorFormatRGBA_8 || dstFormat == LdpColorFormatRGBA_10_2_LE ||
        dstFormat == LdpColorFormatP010_10 || dstFormat == LdpColorFormatNV12_8;
    if (dstHasImage && dstIsImageFormat) {
        return convertOutToImage(params, constants);
    }
    return convertOutToBuffer(params, constants);
}

bool ComputeVulkan::convertOutToImage(VulkanConvertOutArgs* params, PushConstantsConversion& constants)
{
    PictureVulkan* srcPicture = params->src;
    BufferVulkan* srcBuffer = fromPipeline(srcPicture->buffer);
    PictureVulkan* dstPicture = params->dst;
    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);
    const LdpColorFormat dstFormat = ldpPictureLayoutFormat(&dstPicture->layout);
    VulkanFrameContext* context = params->context;

    const bool dstIsP010Image = dstFormat == LdpColorFormatP010_10;
    const bool dstIsNv12Image = dstFormat == LdpColorFormatNV12_8;
    const bool dstIsRgba10Image = dstFormat == LdpColorFormatRGBA_10_2_LE;

    if (params->chroma == LdeChroma::CTMonochrome) {
        VNLogError("convertOut: VkImage output requires chroma planes");
        return false;
    }
    int widthShift{};
    int heightShift{};
    getSubsamplingShifts(params->chroma, widthShift, heightShift);

    // The image-output shaders alias `shift`/`sharpenStrength` as the chroma
    // sub-sampling shifts, and consume all three plane stride/offset slots
    // straight from the src layout.
    constants.containerStrideIn = toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, 0));
    constants.containerOffsetIn = toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, 0));
    constants.containerStrideOut = toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, 1));
    constants.containerOffsetOut = toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, 1));
    constants.containerStrideV = toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, 2));
    constants.containerOffsetV = toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, 2));
    constants.shift = static_cast<uint32_t>(widthShift);
    constants.sharpenStrength = static_cast<uint32_t>(heightShift);

    // Temporal-add. One dispatch handles Y/U/V, so the per-plane temporal stride/offset (pairs
    // 0/1/2) are all set here; Non-zero stride enables the add for a plane.
    const bool doAdd = params->addTemporal && params->temporal && params->temporal->buffer;
    setTemporalStrides(constants, 0, 0, doAdd, params->numEnhancedPlanes, params->temporal);
    setTemporalStrides(constants, 1, 1, doAdd, params->numEnhancedPlanes, params->temporal);
    setTemporalStrides(constants, 2, 2, doAdd, params->numEnhancedPlanes, params->temporal);
    BufferVulkan* temporalBuffer = doAdd ? fromPipeline(params->temporal->buffer) : srcBuffer;

    if (!updateComputeImageDescriptorSets(srcBuffer, dstBuffer, temporalBuffer,
                                          context->m_descriptorSetConversionFromImage)) {
        VNLogError("convert_out_image: failed to update descriptor set");
        return false;
    }

    // Transition the (possibly imported) destination image into GENERAL
    // layout so the convert-out compute shader can write to it.
    BufferVulkan::imageLayoutTransition(
        context->m_commandBuffer, dstBuffer->getVkImage(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL, /*srcAccess=*/0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    const ComputePipeline* convertOutPipeline = doAdd ? &m_pipelineConversionFromInternalRgba8ImageAddTemporal
                                                      : &m_pipelineConversionFromInternalRgba8Image;
    const char* convertOutLabel = "convert_out_image_rgba8";
    if (dstIsP010Image) {
        convertOutPipeline = doAdd ? &m_pipelineConversionFromInternalP010ImageAddTemporal
                                   : &m_pipelineConversionFromInternalP010Image;
        convertOutLabel = "convert_out_image_p010";
    } else if (dstIsNv12Image) {
        convertOutPipeline = doAdd ? &m_pipelineConversionFromInternalNv12ImageAddTemporal
                                   : &m_pipelineConversionFromInternalNv12Image;
        convertOutLabel = "convert_out_image_nv12";
    } else if (dstIsRgba10Image) {
        convertOutPipeline = doAdd ? &m_pipelineConversionFromInternalRgba10ImageAddTemporal
                                   : &m_pipelineConversionFromInternalRgba10Image;
        convertOutLabel = "convert_out_image_rgba10";
    }
    context->bind(&m_pipelineLayoutConvertOutImage, convertOutPipeline,
                  context->m_descriptorSetConversionFromImage);

    vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutConvertOutImage.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
    context->dispatch(constants.width, constants.height, convertOutLabel);
    context->insertComputeBarrier();
    return true;
}

bool ComputeVulkan::convertOutToBuffer(VulkanConvertOutArgs* params, PushConstantsConversion& constants)
{
    PictureVulkan* srcPicture = params->src;
    BufferVulkan* srcBuffer = fromPipeline(srcPicture->buffer);
    PictureVulkan* dstPicture = params->dst;
    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);
    const LdpColorFormat dstFormat = ldpPictureLayoutFormat(&dstPicture->layout);
    const bool is8bit = ldpPictureLayoutSampleSize(&dstPicture->layout) == 1;
    const bool nv12 = dstFormat == LdpColorFormatNV12_8 || dstFormat == LdpColorFormatNV21_8;
    const bool p010 = dstFormat == LdpColorFormatP010_10;
    const bool semiPlanar = nv12 || p010;
    VulkanFrameContext* context = params->context;

    const bool doAdd = params->addTemporal && params->temporal && params->temporal->buffer;
    BufferVulkan* temporalBuffer = doAdd ? fromPipeline(params->temporal->buffer) : srcBuffer;

    constants.containerStrideV = 0; // used to signal no nv12
    constants.containerOffsetV = 0;
    // Y plane (plane 0) -> slot 0; slot 2 unused for Y.
    setTemporalStrides(constants, 0, 0, doAdd, params->numEnhancedPlanes, params->temporal);
    setTemporalStrides(constants, 2, 0, false, 0, nullptr);
    if (!updateComputeDescriptorSets(srcBuffer, dstBuffer, temporalBuffer,
                                     context->m_descriptorSetConversionFrom)) {
        VNLogError("convert_out_buffer: failed to update descriptor set");
        return false;
    }

    const ComputePipeline* pipeline = nullptr;
    if (is8bit) {
        if (nv12) {
            pipeline = doAdd ? &m_pipelineConversionFromInternal8BitNV12AddTemporal
                             : &m_pipelineConversionFromInternal8BitNV12;
        } else {
            pipeline = doAdd ? &m_pipelineConversionFromInternal8BitAddTemporal
                             : &m_pipelineConversionFromInternal8Bit;
        }
    } else {
        if (p010) {
            pipeline = doAdd ? &m_pipelineConversionFromInternal16BitP010AddTemporal
                             : &m_pipelineConversionFromInternal16BitP010;
        } else {
            pipeline = doAdd ? &m_pipelineConversionFromInternal16BitAddTemporal
                             : &m_pipelineConversionFromInternal16Bit;
        }
    }
    context->bind(&m_pipelineLayoutConversion, pipeline, context->m_descriptorSetConversionFrom);

    vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutConversion.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);

    context->dispatch(constants.width, constants.height, "convert_out_y"); // Y

    if (params->chroma != LdeChroma::CTMonochrome) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(params->chroma, widthShift, heightShift);

        if (!semiPlanar) {
            constants.width = srcPicture->layout.width >> widthShift;
        } else {
            constants.containerStrideV =
                toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, 2));
            constants.containerOffsetV =
                toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, 2));
        }
        const uint32_t chromaHeight = srcPicture->layout.height >> heightShift;
        constants.height = chromaHeight;
        constants.sharpenStrength = 0;
        setContainerStrides(constants, 1, srcPicture, dstPicture, nullptr);
        constants.nv21Swap = (is8bit && dstFormat == LdpColorFormatNV21_8) ? 1u : 0u;
        // U plane (plane 1) -> slot 0; for semi-planar the same dispatch reads V
        // (plane 2) -> slot 2, otherwise slot 2 is unused.
        setTemporalStrides(constants, 0, 1, doAdd, params->numEnhancedPlanes, params->temporal);
        if (semiPlanar) {
            setTemporalStrides(constants, 2, 2, doAdd, params->numEnhancedPlanes, params->temporal);
        } else {
            setTemporalStrides(constants, 2, 0, false, 0, nullptr);
        }

        vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutConversion.pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
        context->dispatch(constants.width, chromaHeight, "convert_out_u"); // U

        if (!semiPlanar) {
            setContainerStrides(constants, 2, srcPicture, dstPicture, nullptr);
            // V plane (plane 2) -> slot 0 for this separate dispatch.
            setTemporalStrides(constants, 0, 2, doAdd, params->numEnhancedPlanes, params->temporal);
            vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutConversion.pipelineLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion),
                               &constants);
            context->dispatch(constants.width, chromaHeight, "convert_out_v"); // V
        }
    }

    context->insertComputeBarrier();

    return true;
}

bool ComputeVulkan::upscaleVertical(const LdeKernel* kernel, VulkanUpscaleArgs* params)
{
    if (kernel == nullptr || params == nullptr || params->src == nullptr ||
        params->dst == nullptr || params->context == nullptr) {
        VNLogError("upscale_vertical: invalid arguments");
        return false;
    }

    PictureVulkan* srcPicture = params->src;
    PictureVulkan* dstPicture = params->dst;

    // Get VkBuffers to update descriptor sets (attach buffers to shaders)
    BufferVulkan* srcBuffer = fromPipeline(srcPicture->buffer);
    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);

    VulkanFrameContext* context = params->context;

    // Vertical set for upscaling from base and writing to intermediate
    if (params->loq1) {
        if (!updateComputeDescriptorSets(srcBuffer, dstBuffer, context->m_descriptorSetSrcMidLoq1)) {
            VNLogError("upscale_vertical_loq1: failed to update descriptor set");
            return false;
        }
    } else {
        if (!updateComputeDescriptorSets(srcBuffer, dstBuffer, context->m_descriptorSetSrcMidLoq0)) {
            VNLogError("upscale_vertical_loq0: failed to update descriptor set");
            return false;
        }
    }

    const uint32_t srcWidth = srcPicture->getActiveWidth();
    const uint32_t srcHeight = srcPicture->getActiveHeight();

    PushConstantsUpscale constants{};
    for (int i = 0; i < 4; ++i) {
        constants.kernel[i] = ldeKernelGetCoefficient(kernel, i);
    }

    constants.srcWidth = srcWidth;
    constants.srcHeight = srcHeight;
    setContainerStrides(constants, 0, srcPicture, dstPicture, nullptr);

    VkDescriptorSet& verticalSet =
        params->loq1 ? context->m_descriptorSetSrcMidLoq1 : context->m_descriptorSetSrcMidLoq0;

    context->bind(&m_pipelineLayoutVertical, &m_pipelineVertical, verticalSet);

    vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutVertical.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);

    context->dispatch(constants.srcWidth, constants.srcHeight, "upscalev_y"); // Y

    if (params->chroma != LdeChroma::CTMonochrome) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(params->chroma, widthShift, heightShift);

        constants.srcWidth = srcWidth >> widthShift;
        constants.srcHeight = srcHeight >> heightShift;
        setContainerStrides(constants, 1, srcPicture, dstPicture, nullptr);

        vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutVertical.pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);
        context->dispatch(constants.srcWidth, constants.srcHeight, "upscalev_u"); // U

        setContainerStrides(constants, 2, srcPicture, dstPicture, nullptr);

        vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutVertical.pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);
        context->dispatch(constants.srcWidth, constants.srcHeight, "upscalev_v"); // V
    }

    context->insertComputeBarrier();

    return true;
}

bool ComputeVulkan::upscaleHorizontal(const LdeKernel* kernel, VulkanUpscaleArgs* params)
{
    if (kernel == nullptr || params == nullptr || params->src == nullptr ||
        params->dst == nullptr || params->base == nullptr || params->context == nullptr) {
        VNLogError("upscale_horizontal: invalid arguments");
        return false;
    }

    PictureVulkan* srcPicture = params->src;
    PictureVulkan* dstPicture = params->dst;
    PictureVulkan* basePicture = params->base;

    // Get VkBuffers to update descriptor sets (attach buffers to shaders)
    BufferVulkan* srcBuffer = fromPipeline(srcPicture->buffer);

    BufferVulkan* baseBuffer = fromPipeline(basePicture->buffer);

    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);

    const uint32_t srcWidth = srcPicture->getActiveWidth();
    const uint32_t srcHeight = srcPicture->getActiveHeight();

    VulkanFrameContext* context = params->context;

    // Horizontal set for upscaling from intermediate, applying PA from base, and writing to output
    if (params->loq1) {
        if (!updateComputeDescriptorSets(srcBuffer, dstBuffer, baseBuffer,
                                         context->m_descriptorSetMidDstLoq1)) {
            VNLogError("upscale_horizontal_loq1: failed to update descriptor set");
            return false;
        }
    } else {
        if (!updateComputeDescriptorSets(srcBuffer, dstBuffer, baseBuffer,
                                         context->m_descriptorSetMidDstLoq0)) {
            VNLogError("upscale_horizontal_loq0: failed to update descriptor set");
            return false;
        }
    }

    PushConstantsUpscale constants{};
    for (uint32_t i = 0; i < 4; ++i) {
        constants.kernel[i] = ldeKernelGetCoefficient(kernel, i);
    }

    constants.srcWidth = srcWidth;
    constants.srcHeight = srcHeight;
    setContainerStrides(constants, 0, srcPicture, dstPicture, basePicture);

    VkDescriptorSet& horizontalSet =
        params->loq1 ? context->m_descriptorSetMidDstLoq1 : context->m_descriptorSetMidDstLoq0;

    const ComputePipeline* horizontalPipeline = &m_pipelineHorizontalPA0;
    if (params->applyPA == 1) {
        horizontalPipeline = &m_pipelineHorizontalPA1;
    } else if (params->applyPA == 2) {
        horizontalPipeline = &m_pipelineHorizontalPA2;
    }

    context->bind(&m_pipelineLayoutHorizontal, horizontalPipeline, horizontalSet);

    vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutHorizontal.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);
    context->dispatch(constants.srcWidth, (constants.srcHeight + 1) >> 1,
                      "upscaleh_y"); // Y. Round up: 2 rows at a time for PA, last invocation may have 1 row

    if (params->chroma != LdeChroma::CTMonochrome) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(params->chroma, widthShift, heightShift);

        constants.srcWidth = srcWidth >> widthShift;
        constants.srcHeight = srcHeight >> heightShift;

        setContainerStrides(constants, 1, srcPicture, dstPicture, basePicture);

        vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutHorizontal.pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);
        context->dispatch(constants.srcWidth, (constants.srcHeight + 1) >> 1,
                          "upscaleh_u"); // U. Round up: 2 rows at a time for PA

        setContainerStrides(constants, 2, srcPicture, dstPicture, basePicture);

        vkCmdPushConstants(context->m_commandBuffer, m_pipelineLayoutHorizontal.pipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);
        context->dispatch(constants.srcWidth, (constants.srcHeight + 1) >> 1,
                          "upscaleh_v"); // V. Round up: 2 rows at a time for PA
    }

    context->insertComputeBarrier();

    return true;
}

bool ComputeVulkan::upscaleFrame(const LdeKernel* kernel, VulkanUpscaleArgs* params)
{
    if (kernel == nullptr || params == nullptr || params->src == nullptr ||
        params->dst == nullptr || params->context == nullptr) {
        VNLogError("upscale: invalid arguments");
        return false;
    }

    params->base = params->src;
    LdpPictureDesc desc{};
    params->src->getDesc(desc);
    if (params->mode == Scale1D) {
        params->vertical ? desc.height *= 2 : desc.width *= 2;
        if (!prepareInternalPictureBuffer(params->dst, desc)) {
            VNLogError("upscale_1d: failed to prepare destination picture buffer");
            return false;
        }

        params->applyPA = params->applyPA ? 1 : 0;
        return params->vertical ? upscaleVertical(kernel, params) : upscaleHorizontal(kernel, params);
    }

    if (params->mode == Scale2D) {
        PictureVulkan* output = params->dst;
        const LdeLOQIndex loq = params->loq1 ? LOQ1 : LOQ0;
        PictureVulkan* intermediate = params->intermediateUpscalePicture[loq];

        LdpPictureDesc intermediateDesc{desc};
        intermediateDesc.height *= 2;
        updateConfig(static_cast<uint8_t>(loq), desc);
        if (!prepareInternalPictureBuffer(intermediate, intermediateDesc)) {
            VNLogError("upscale_2d: failed to prepare intermediate picture buffer");
            return false;
        }
        params->dst = intermediate;
        if (!upscaleVertical(kernel, params)) {
            return false;
        }

        desc.width *= 2;
        desc.height *= 2;
        if (!prepareInternalPictureBuffer(output, desc)) {
            VNLogError("upscale_2d: failed to prepare destination picture buffer");
            return false;
        }
        params->src = params->dst;
        params->dst = output;
        params->applyPA = params->applyPA ? 2 : 0;
        return upscaleHorizontal(kernel, params);
    }

    VNLogError("upscale: unsupported scaling mode %d", static_cast<int32_t>(params->mode));
    return false;
}

void ComputeVulkan::destroyComputePipelineLayout(ComputePipelineLayout& computePipelineLayout)
{
    vkDestroyDescriptorSetLayout(m_backend.getDevice(), computePipelineLayout.descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_backend.getDevice(), computePipelineLayout.pipelineLayout, nullptr);
}

void ComputeVulkan::destroyComputePipeline(ComputePipeline& computePipeline)
{
    vkDestroyPipeline(m_backend.getDevice(), computePipeline.pipeline, nullptr);
}

void ComputeVulkan::destroy()
{
    m_backend.waitDeviceIdle();

    for (auto& context : m_frameContexts) {
        context.destroy();
    }

    destroyComputePipelineLayout(m_pipelineLayoutVertical);
    destroyComputePipelineLayout(m_pipelineLayoutHorizontal);
    destroyComputePipelineLayout(m_pipelineLayoutApply);
    destroyComputePipelineLayout(m_pipelineLayoutConversion);
    destroyComputePipelineLayout(m_pipelineLayoutConvertOutImage);
    destroyComputePipelineLayout(m_pipelineLayoutAdd);

    destroyComputePipeline(m_pipelineVertical);
    destroyComputePipeline(m_pipelineHorizontalPA0);
    destroyComputePipeline(m_pipelineHorizontalPA1);
    destroyComputePipeline(m_pipelineHorizontalPA2);
    destroyComputePipeline(m_pipelineApplyDd);
    destroyComputePipeline(m_pipelineApplyDds);
    destroyComputePipeline(m_pipelineApplyDdRaster);
    destroyComputePipeline(m_pipelineApplyDdsRaster);
    destroyComputePipeline(m_pipelineConversionFromInternal8Bit);
    destroyComputePipeline(m_pipelineConversionToInternal8Bit);
    destroyComputePipeline(m_pipelineConversionFromInternal8BitNV12);
    destroyComputePipeline(m_pipelineConversionToInternal8BitNV12);
    destroyComputePipeline(m_pipelineConversionFromInternal16Bit);
    destroyComputePipeline(m_pipelineConversionToInternal16Bit);
    destroyComputePipeline(m_pipelineConversionFromInternalRgba8Image);
    destroyComputePipeline(m_pipelineConversionFromInternalRgba10Image);
    destroyComputePipeline(m_pipelineConversionFromInternalP010Image);
    destroyComputePipeline(m_pipelineConversionFromInternalNv12Image);
    destroyComputePipeline(m_pipelineConversionFromInternal8BitAddTemporal);
    destroyComputePipeline(m_pipelineConversionFromInternal8BitNV12AddTemporal);
    destroyComputePipeline(m_pipelineConversionFromInternal16BitAddTemporal);
    destroyComputePipeline(m_pipelineConversionFromInternal16BitP010AddTemporal);
    destroyComputePipeline(m_pipelineConversionFromInternalRgba8ImageAddTemporal);
    destroyComputePipeline(m_pipelineConversionFromInternalRgba10ImageAddTemporal);
    destroyComputePipeline(m_pipelineConversionFromInternalP010ImageAddTemporal);
    destroyComputePipeline(m_pipelineConversionFromInternalNv12ImageAddTemporal);
    destroyComputePipeline(m_pipelineAdd);
}

} // namespace lcevc_dec::pipeline_vulkan
