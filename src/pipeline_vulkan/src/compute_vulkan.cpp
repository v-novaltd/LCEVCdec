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

#include "apply.h"
#include "buffer_vulkan.h"
#include "convert_in.h"
#include "convert_out_image_nv12.h"
#include "convert_out_image_p010.h"
#include "convert_out_image_rgba10.h"
#include "convert_out_image_rgba10_generic.h"
#include "convert_out_image_rgba8.h"
#include "convert_out_image_rgba8_generic.h"
#include "convert_out_nv12.h"
#include "convert_out_p010.h"
#include "convert_out_planar16.h"
#include "convert_out_planar8.h"
#include "dequant_transform.h"
#include "frame_context_vulkan.h"
#include "frame_vulkan.h"
#include "from_base.h"
#include "picture_vulkan.h"
#include "pipeline_vulkan.h"
#include "shaders/src/specialization_ids.h"
#include "upscale_1d.h"
#include "upscale_2d.h"

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

#include <cassert>
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
    uint32_t containerStrideV;
    uint32_t containerOffsetV;
    uint32_t shift;           // 5 for 10bit, 3 for 12bit, and 1 for 14bit
    uint32_t sharpenStrength; // 0 disables sharpening, otherwise U16 strength
    uint32_t nv21Swap;        // non-zero -> swap U/V order during semiplanar input conversion
    // Note: generic RGBA convert_out_image_*.comp shaders alias `shift` and
    // `sharpenStrength` as chromaWidthShift / chromaHeightShift. Fixed-4:2:0 image shaders
    // do not read those fields, and no image-output shader reads `nv21Swap`.
    //
    // Strides of each plane's temporal buffer - buffer is present if stride != 0.
    uint32_t containerStrideTemporal0;
    uint32_t containerOffsetTemporal0;
    uint32_t containerStrideTemporal1;
    uint32_t containerOffsetTemporal1;
    uint32_t containerStrideTemporal2;
    uint32_t containerOffsetTemporal2;
    // Additional metadata used only by fused semiplanar buffer-output shaders.
    uint32_t containerStrideU;
    uint32_t containerOffsetU;
    uint32_t containerStrideUVOut;
    uint32_t containerOffsetUVOut;
};

static_assert(sizeof(PushConstantsConversion) == 21 * sizeof(uint32_t));
static_assert(offsetof(PushConstantsConversion, containerStrideTemporal0) == 11 * sizeof(uint32_t));
static_assert(offsetof(PushConstantsConversion, containerStrideU) == 17 * sizeof(uint32_t));

struct PushConstantsDequantTransform
{
    uint32_t transformUnitCount;
    uint32_t layerCount;
    uint32_t temporalSignal;
    uint32_t scalingMode;
    uint32_t deblockEnabled;
    uint32_t deblockCorner;
    uint32_t deblockSide;
};

namespace {
    constexpr Dimension kConversionWorkgroup{32, 8};

    constexpr std::array<VkDescriptorType, 2> kApplyDescriptorTypes = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
    constexpr std::array<VkDescriptorType, 3> kThreeStorageBufferDescriptorTypes = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
    constexpr std::array<VkDescriptorType, 4> kConvertOutImageDescriptorTypes = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
    constexpr std::array<VkDescriptorType, 3> kDequantTransformDescriptorTypes = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};

    struct ComputeLayoutSpec
    {
        ComputeLayoutId id;
        const VkDescriptorType* descriptorTypes;
        size_t descriptorTypeCount;
        uint32_t pushConstantsSize;
    };

    template <size_t N>
    constexpr ComputeLayoutSpec makeLayoutSpec(ComputeLayoutId id,
                                               const std::array<VkDescriptorType, N>& descriptorTypes,
                                               uint32_t pushConstantsSize)
    {
        return {id, descriptorTypes.data(), descriptorTypes.size(), pushConstantsSize};
    }

    constexpr std::array kComputeLayoutSpecs = {
        makeLayoutSpec(ComputeLayoutId::Upscale, kThreeStorageBufferDescriptorTypes,
                       sizeof(PushConstantsUpscale)),
        makeLayoutSpec(ComputeLayoutId::Apply, kApplyDescriptorTypes, sizeof(PushConstantsApply)),
        makeLayoutSpec(ComputeLayoutId::Conversion, kThreeStorageBufferDescriptorTypes,
                       sizeof(PushConstantsConversion)),
        makeLayoutSpec(ComputeLayoutId::ConvertOutImage, kConvertOutImageDescriptorTypes,
                       sizeof(PushConstantsConversion)),
        makeLayoutSpec(ComputeLayoutId::DequantTransform, kDequantTransformDescriptorTypes,
                       sizeof(PushConstantsDequantTransform)),
    };
    static_assert(kComputeLayoutSpecs.size() == toIndex(ComputeLayoutId::Count));

    struct SpecializationValues
    {
        std::array<SpecValue, 2> values{};
        size_t count{};
    };

    constexpr SpecializationValues specialization(SpecValue first) { return {{{first, {}}}, 1}; }
    constexpr SpecializationValues specialization(SpecValue first, SpecValue second)
    {
        return {{{first, second}}, 2};
    }

    struct ComputePipelineSpec
    {
        ComputePipelineId id;
        ComputeLayoutId layout;
        const unsigned char* shader;
        size_t shaderSize;
        const char* name;
        Dimension workgroup;
        Dimension packing;
        SpecializationValues specialization;
    };

    constexpr ComputePipelineSpec makePipelineSpec(ComputePipelineId id, ComputeLayoutId layout,
                                                   const unsigned char* shader, size_t shaderSize,
                                                   const char* name, Dimension workgroup, Dimension packing,
                                                   SpecializationValues specValues = {})
    {
        return {id, layout, shader, shaderSize, name, workgroup, packing, specValues};
    }

    const std::array kComputePipelineSpecs = {
        makePipelineSpec(ComputePipelineId::Upscale1DPA0, ComputeLayoutId::Upscale, upscale_1d_spv,
                         sizeof(upscale_1d_spv), "upscale1DPA0", {64, 1}, {2, 1},
                         specialization({SPECID_PA, 0})),
        makePipelineSpec(ComputePipelineId::Upscale1DPA1, ComputeLayoutId::Upscale, upscale_1d_spv,
                         sizeof(upscale_1d_spv), "upscale1DPA1", {64, 1}, {2, 1},
                         specialization({SPECID_PA, 1})),
        makePipelineSpec(ComputePipelineId::Upscale2DPA0, ComputeLayoutId::Upscale, upscale_2d_spv,
                         sizeof(upscale_2d_spv), "upscale2DPA0", {16, 8}, {2, 1},
                         specialization({SPECID_PA, 0})),
        makePipelineSpec(ComputePipelineId::Upscale2DPA2, ComputeLayoutId::Upscale, upscale_2d_spv,
                         sizeof(upscale_2d_spv), "upscale2DPA2", {16, 8}, {2, 1},
                         specialization({SPECID_PA, 2})),
        makePipelineSpec(ComputePipelineId::ApplyDd, ComputeLayoutId::Apply, apply_spv,
                         sizeof(apply_spv), "applyDd", {64, 1}, {1, 1}),
        makePipelineSpec(ComputePipelineId::ApplyDds, ComputeLayoutId::Apply, apply_spv,
                         sizeof(apply_spv), "applyDds", {64, 1}, {1, 1}),
        makePipelineSpec(ComputePipelineId::ApplyDdRaster, ComputeLayoutId::Apply, apply_spv,
                         sizeof(apply_spv), "applyDdRaster", {64, 1}, {1, 1}),
        makePipelineSpec(ComputePipelineId::ApplyDdsRaster, ComputeLayoutId::Apply, apply_spv,
                         sizeof(apply_spv), "applyDdsRaster", {64, 1}, {1, 1}),
        makePipelineSpec(ComputePipelineId::ConversionToInternal8Bit, ComputeLayoutId::Conversion,
                         convert_in_spv, sizeof(convert_in_spv), "conversionIn8", kConversionWorkgroup,
                         {4, 1}, specialization({SPECID_8_BIT, 1}, {SPECID_NV12, 0})),
        makePipelineSpec(ComputePipelineId::ConversionToInternal8BitNV12, ComputeLayoutId::Conversion,
                         convert_in_spv, sizeof(convert_in_spv), "conversionIn8Nv12", kConversionWorkgroup,
                         {4, 1}, specialization({SPECID_8_BIT, 1}, {SPECID_NV12, 1})),
        makePipelineSpec(ComputePipelineId::ConversionToInternal16Bit, ComputeLayoutId::Conversion,
                         convert_in_spv, sizeof(convert_in_spv), "conversionIn16", kConversionWorkgroup,
                         {2, 1}, specialization({SPECID_8_BIT, 0}, {SPECID_NV12, 0})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar8, ComputeLayoutId::Conversion,
                         convert_out_planar8_spv, sizeof(convert_out_planar8_spv),
                         "conversionOutPlanar8", kConversionWorkgroup, {4, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar8Sharpen,
                         ComputeLayoutId::Conversion, convert_out_planar8_spv,
                         sizeof(convert_out_planar8_spv), "conversionOutPlanar8Sharpen",
                         kConversionWorkgroup, {4, 2}, specialization({SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar16, ComputeLayoutId::Conversion,
                         convert_out_planar16_spv, sizeof(convert_out_planar16_spv),
                         "conversionOutPlanar16", kConversionWorkgroup, {2, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar16Sharpen,
                         ComputeLayoutId::Conversion, convert_out_planar16_spv,
                         sizeof(convert_out_planar16_spv), "conversionOutPlanar16Sharpen",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalNv12, ComputeLayoutId::Conversion,
                         convert_out_nv12_spv, sizeof(convert_out_nv12_spv), "conversionOutNv12",
                         kConversionWorkgroup, {4, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalNv12Sharpen, ComputeLayoutId::Conversion,
                         convert_out_nv12_spv, sizeof(convert_out_nv12_spv), "conversionOutNv12Sharpen",
                         kConversionWorkgroup, {4, 2}, specialization({SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalP010, ComputeLayoutId::Conversion,
                         convert_out_p010_spv, sizeof(convert_out_p010_spv), "conversionOutP010",
                         kConversionWorkgroup, {2, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalP010Sharpen, ComputeLayoutId::Conversion,
                         convert_out_p010_spv, sizeof(convert_out_p010_spv), "conversionOutP010Sharpen",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba8Image, ComputeLayoutId::ConvertOutImage,
                         convert_out_image_rgba8_spv, sizeof(convert_out_image_rgba8_spv),
                         "conversionOutRgba8Image", kConversionWorkgroup, {2, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba8ImageGeneric,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_rgba8_generic_spv,
                         sizeof(convert_out_image_rgba8_generic_spv),
                         "conversionOutRgba8ImageGeneric", kConversionWorkgroup, {1, 1}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba10Image, ComputeLayoutId::ConvertOutImage,
                         convert_out_image_rgba10_spv, sizeof(convert_out_image_rgba10_spv),
                         "conversionOutRgba10Image", kConversionWorkgroup, {2, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba10ImageGeneric,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_rgba10_generic_spv,
                         sizeof(convert_out_image_rgba10_generic_spv),
                         "conversionOutRgba10ImageGeneric", kConversionWorkgroup, {1, 1}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalP010Image, ComputeLayoutId::ConvertOutImage,
                         convert_out_image_p010_spv, sizeof(convert_out_image_p010_spv),
                         "conversionOutP010Image", kConversionWorkgroup, {2, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalNv12Image, ComputeLayoutId::ConvertOutImage,
                         convert_out_image_nv12_spv, sizeof(convert_out_image_nv12_spv),
                         "conversionOutNv12Image", kConversionWorkgroup, {2, 2}),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar8AddTemporal,
                         ComputeLayoutId::Conversion, convert_out_planar8_spv,
                         sizeof(convert_out_planar8_spv), "conversionOutPlanar8AddTemporal",
                         kConversionWorkgroup, {4, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar8SharpenAddTemporal,
                         ComputeLayoutId::Conversion, convert_out_planar8_spv, sizeof(convert_out_planar8_spv),
                         "conversionOutPlanar8SharpenAddTemporal", kConversionWorkgroup, {4, 2},
                         specialization({SPECID_ADD_TEMPORAL, 1}, {SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar16AddTemporal,
                         ComputeLayoutId::Conversion, convert_out_planar16_spv,
                         sizeof(convert_out_planar16_spv), "conversionOutPlanar16AddTemporal",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalPlanar16SharpenAddTemporal,
                         ComputeLayoutId::Conversion, convert_out_planar16_spv,
                         sizeof(convert_out_planar16_spv),
                         "conversionOutPlanar16SharpenAddTemporal", kConversionWorkgroup, {2, 2},
                         specialization({SPECID_ADD_TEMPORAL, 1}, {SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalNv12AddTemporal,
                         ComputeLayoutId::Conversion, convert_out_nv12_spv,
                         sizeof(convert_out_nv12_spv), "conversionOutNv12AddTemporal",
                         kConversionWorkgroup, {4, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalNv12SharpenAddTemporal,
                         ComputeLayoutId::Conversion, convert_out_nv12_spv, sizeof(convert_out_nv12_spv),
                         "conversionOutNv12SharpenAddTemporal", kConversionWorkgroup, {4, 2},
                         specialization({SPECID_ADD_TEMPORAL, 1}, {SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalP010AddTemporal,
                         ComputeLayoutId::Conversion, convert_out_p010_spv,
                         sizeof(convert_out_p010_spv), "conversionOutP010AddTemporal",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalP010SharpenAddTemporal,
                         ComputeLayoutId::Conversion, convert_out_p010_spv, sizeof(convert_out_p010_spv),
                         "conversionOutP010SharpenAddTemporal", kConversionWorkgroup, {2, 2},
                         specialization({SPECID_ADD_TEMPORAL, 1}, {SPECID_SHARPEN, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba8ImageAddTemporal,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_rgba8_spv,
                         sizeof(convert_out_image_rgba8_spv), "conversionOutRgba8ImageAddTemporal",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba8ImageGenericAddTemporal,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_rgba8_generic_spv,
                         sizeof(convert_out_image_rgba8_generic_spv), "conversionOutRgba8ImageGenericAddTemporal",
                         kConversionWorkgroup, {1, 1}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba10ImageAddTemporal,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_rgba10_spv,
                         sizeof(convert_out_image_rgba10_spv), "conversionOutRgba10ImageAddTemporal",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalRgba10ImageGenericAddTemporal,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_rgba10_generic_spv,
                         sizeof(convert_out_image_rgba10_generic_spv),
                         "conversionOutRgba10ImageGenericAddTemporal", kConversionWorkgroup, {1, 1},
                         specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalP010ImageAddTemporal,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_p010_spv,
                         sizeof(convert_out_image_p010_spv), "conversionOutP010ImageAddTemporal",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::ConversionFromInternalNv12ImageAddTemporal,
                         ComputeLayoutId::ConvertOutImage, convert_out_image_nv12_spv,
                         sizeof(convert_out_image_nv12_spv), "conversionOutNv12ImageAddTemporal",
                         kConversionWorkgroup, {2, 2}, specialization({SPECID_ADD_TEMPORAL, 1})),
        makePipelineSpec(ComputePipelineId::DequantTransform, ComputeLayoutId::DequantTransform,
                         dequant_transform_spv, sizeof(dequant_transform_spv), "dequantTransform",
                         {64, 1}, {1, 1}),
    };
    static_assert(kComputePipelineSpecs.size() == toIndex(ComputePipelineId::Count));

    class DescriptorWriter
    {
    public:
        DescriptorWriter(VkDevice device, VkDescriptorSet descriptorSet)
            : m_device(device)
            , m_descriptorSet(descriptorSet)
        {
            if (m_descriptorSet == VK_NULL_HANDLE) {
                VNLogError("null descriptor set");
            }
        }

        ~DescriptorWriter() = default;

        DescriptorWriter(const DescriptorWriter&) = delete;
        DescriptorWriter& operator=(const DescriptorWriter&) = delete;
        DescriptorWriter(DescriptorWriter&&) = delete;
        DescriptorWriter& operator=(DescriptorWriter&&) = delete;

        explicit operator bool() const { return m_descriptorSet != VK_NULL_HANDLE; }

        bool addBuffer(uint32_t binding, BufferVulkan* buffer,
                       VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            if (!hasCapacity(m_bufferInfoCount) || buffer == nullptr) {
                if (buffer == nullptr) {
                    VNLogError("null descriptor buffer");
                }
                return false;
            }

            VkDescriptorBufferInfo& info = m_bufferInfos[m_bufferInfoCount++];
            info.buffer = buffer->getVkBuffer();
            info.offset = 0;
            info.range = buffer->size();
            if (info.buffer == VK_NULL_HANDLE || info.range == 0) {
                VNLogError("invalid descriptor buffer: object=%p vkBufferNull=%d size=%u",
                           static_cast<void*>(buffer), info.buffer == VK_NULL_HANDLE, buffer->size());
                return false;
            }

            VkWriteDescriptorSet& write = m_writes[m_writeCount++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptorSet;
            write.dstBinding = binding;
            write.descriptorCount = 1;
            write.descriptorType = type;
            write.pBufferInfo = &info;
            return true;
        }

        bool addStorageImage(uint32_t binding, VkImageView imageView)
        {
            if (!hasCapacity(m_imageInfoCount) || imageView == VK_NULL_HANDLE) {
                if (imageView == VK_NULL_HANDLE) {
                    VNLogError("null descriptor image view");
                }
                return false;
            }

            VkDescriptorImageInfo& info = m_imageInfos[m_imageInfoCount++];
            info.imageView = imageView;
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet& write = m_writes[m_writeCount++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptorSet;
            write.dstBinding = binding;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.pImageInfo = &info;
            return true;
        }

        bool update() const
        {
            if (m_descriptorSet == VK_NULL_HANDLE) {
                return false;
            }
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(m_writeCount), m_writes.data(),
                                   0, nullptr);
            return true;
        }

    private:
        bool hasCapacity(size_t infoCount) const
        {
            if (m_writeCount < m_writes.size() && infoCount < kMaxComputeDescriptorBindings) {
                return true;
            }
            VNLogError("too many descriptor writes");
            return false;
        }

        VkDevice m_device{};
        VkDescriptorSet m_descriptorSet{};
        std::array<VkDescriptorBufferInfo, kMaxComputeDescriptorBindings> m_bufferInfos{};
        std::array<VkDescriptorImageInfo, kMaxComputeDescriptorBindings> m_imageInfos{};
        std::array<VkWriteDescriptorSet, kMaxComputeDescriptorBindings> m_writes{};
        size_t m_bufferInfoCount{};
        size_t m_imageInfoCount{};
        size_t m_writeCount{};
    };

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
                                                           const VkDescriptorType* descriptorTypes,
                                                           size_t descriptorTypeCount,
                                                           uint32_t pushConstantsSize)
{
    VkDevice device = m_backend.getDevice();
    ComputePipelineLayout candidate{};
    if (descriptorTypeCount > candidate.descriptorTypes.size()) {
        VNLogError("too many compute descriptor bindings: %zu", descriptorTypeCount);
        return false;
    }

    candidate.descriptorTypeCount = descriptorTypeCount;
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    layoutBindings.reserve(descriptorTypeCount);

    for (size_t i = 0; i < descriptorTypeCount; ++i) {
        candidate.descriptorTypes[i] = descriptorTypes[i];

        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = static_cast<uint32_t>(i);
        layoutBinding.descriptorType = descriptorTypes[i];
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings.push_back(layoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
    setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    setLayoutCreateInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr,
                                    candidate.descriptorSetLayout.put(device)) != VK_SUCCESS) {
        VNLogError("failed to create descriptor set layout!");
        return false;
    }

    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = pushConstantsSize;

    VkDescriptorSetLayout descriptorSetLayout = candidate.descriptorSetLayout.get();
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &range;

    if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr,
                               candidate.pipelineLayout.put(device)) != VK_SUCCESS) {
        VNLogError("failed to create pipeline layout");
        return false;
    }

    computePipelineLayout = std::move(candidate);
    return true;
}

bool ComputeVulkan::createComputePipeline(ComputePipeline& computePipeline,
                                          const ComputePipelineLayout& computePipelineLayout,
                                          const unsigned char* shader, size_t shaderSize,
                                          const char* name, Dimension workgroup, Dimension packing,
                                          const SpecValue* extraSpecValues, size_t extraSpecValueCount)
{
    VkDevice device = m_backend.getDevice();
    ComputePipeline candidate{};
    candidate.name = name;
    candidate.workgroup = workgroup;
    candidate.packing = packing;

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

    UniqueShaderModule shaderModule;
    if (VkResult r =
            vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, shaderModule.put(device));
        r != VK_SUCCESS) {
        VNLogError("failed to create shader module for %s: %d", name, static_cast<int32_t>(r));
        return false;
    }

    // Set up specialization data
    std::vector<SpecValue> specializationData = {
        {SPECID_LOCAL_SIZE_X, candidate.workgroup.w},
        {SPECID_LOCAL_SIZE_Y, candidate.workgroup.h},
        {SPECID_LOCAL_SIZE_Z, 1},
    };
    if (extraSpecValueCount != 0) {
        specializationData.insert(specializationData.end(), extraSpecValues,
                                  extraSpecValues + extraSpecValueCount);
    }

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
    pipelineCreateInfo.stage.module = shaderModule.get();
    pipelineCreateInfo.stage.pSpecializationInfo = &specializationInfo;

    pipelineCreateInfo.stage.pName = "main";
    pipelineCreateInfo.layout = computePipelineLayout.pipelineLayout.get();

    if (VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
                                              nullptr, candidate.pipeline.put(device));
        r != VK_SUCCESS) {
        VNLogError("failed to create compute pipeline %s: %d", name, static_cast<int32_t>(r));
        return false;
    }

    computePipeline = std::move(candidate);
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

bool ComputeVulkan::updateComputeDescriptorSets(VkDescriptorSet descriptorSet,
                                                BufferVulkan* buffer0, BufferVulkan* buffer1)
{
    DescriptorWriter writer(m_backend.getDevice(), descriptorSet);
    return writer && writer.addBuffer(0, buffer0) && writer.addBuffer(1, buffer1) && writer.update();
}

bool ComputeVulkan::updateComputeDescriptorSets(VkDescriptorSet descriptorSet, BufferVulkan* buffer0,
                                                BufferVulkan* buffer1, BufferVulkan* buffer2)
{
    DescriptorWriter writer(m_backend.getDevice(), descriptorSet);
    return writer && writer.addBuffer(0, buffer0) && writer.addBuffer(1, buffer1) &&
           writer.addBuffer(2, buffer2) && writer.update();
}

bool ComputeVulkan::updateComputeImageDescriptorSets(VkDescriptorSet descriptorSet, BufferVulkan* src,
                                                     BufferVulkan* dst, BufferVulkan* temporal)
{
    DescriptorWriter writer(m_backend.getDevice(), descriptorSet);
    if (!writer || !writer.addBuffer(0, src)) {
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

    return writer.addStorageImage(1, plane0View) && writer.addStorageImage(2, plane1View) &&
           writer.addBuffer(3, temporal) && writer.update();
}

bool ComputeVulkan::updateDequantDescriptorSet(BufferVulkan* coefficients, BufferVulkan* dequantParameters,
                                               BufferVulkan* residuals, VkDescriptorSet descriptorSet)
{
    DescriptorWriter writer(m_backend.getDevice(), descriptorSet);
    return writer && writer.addBuffer(0, coefficients) &&
           writer.addBuffer(1, dequantParameters, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) &&
           writer.addBuffer(2, residuals) && writer.update();
}

bool ComputeVulkan::updateConfig(uint8_t loq, const LdpPictureDesc& desc)
{
    if (desc != m_previousDesc[loq]) {
        m_previousDesc[loq] = desc;
        return true;
    }
    return false;
}

bool ComputeVulkan::beginCompute(VulkanFrameContext* context)
{
    return context->begin(m_descriptorSetLayouts);
}

bool ComputeVulkan::endCompute(VulkanFrameContext* context)
{
    return context->end(m_backend.getComputeQueue());
}

bool ComputeVulkan::init()
{
    for (const ComputeLayoutSpec& spec : kComputeLayoutSpecs) {
        if (!createComputeBindingsAndPipelineLayout(pipelineLayout(spec.id), spec.descriptorTypes,
                                                    spec.descriptorTypeCount, spec.pushConstantsSize)) {
            return false;
        }
        m_descriptorSetLayouts[toIndex(spec.id)] = pipelineLayout(spec.id).descriptorSetLayoutInfo();
    }

    for (const ComputePipelineSpec& spec : kComputePipelineSpecs) {
        if (!createComputePipeline(pipeline(spec.id), pipelineLayout(spec.layout), spec.shader,
                                   spec.shaderSize, spec.name, spec.workgroup, spec.packing,
                                   spec.specialization.values.data(), spec.specialization.count)) {
            return false;
        }
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
            params->temporalPicture->temporalBufferEmpty = true;
        }
    }

    VulkanFrameContext* context = params->context;
    const uint8_t loq = params->loq;

    // Select per-LOQ command buffer and descriptor set
    BufferVulkan*& gpuCommandBuffer = context->m_gpuCommandBuffer[loq];
    VkDescriptorSet& descriptorSetApply = context->m_descriptorSets.apply[loq];

    BufferVulkan* applyPlaneBuffer = applyTemporal ? fromPipeline(params->temporalPicture->buffer)
                                                   : fromPipeline(params->picture->buffer);

    // Update the per-LOQ apply descriptor set. The output plane may change between tiles
    // (different planes/pictures), so always update.
    if (!updateComputeDescriptorSets(descriptorSetApply, gpuCommandBuffer, applyPlaneBuffer)) {
        VNLogError("apply: failed to update descriptor set");
        return false;
    }

    // Select the specialised apply pipeline based on DDS and raster order
    ComputePipelineId applyPipelineId{};
    if (params->transform == TransformDDS) {
        applyPipelineId =
            params->tuRasterOrder ? ComputePipelineId::ApplyDdsRaster : ComputePipelineId::ApplyDds;
    } else {
        applyPipelineId =
            params->tuRasterOrder ? ComputePipelineId::ApplyDdRaster : ComputePipelineId::ApplyDd;
    }

    context->bind(&pipelineLayout(ComputeLayoutId::Apply), &pipeline(applyPipelineId), descriptorSetApply);

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
    constants.dds = params->transform == TransformDDS ? 1 : 0;
    constants.rasterOrder = params->tuRasterOrder ? 1 : 0;

    vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Apply),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsApply), &constants);

    // Dispatch shader
    context->dispatch(buffer.commandCount, 1, "apply", 1);

    return true;
}

bool ComputeVulkan::dequantTransform(VulkanDequantTransformArgs* params)
{
    if (params == nullptr || params->context == nullptr || params->coefficients == nullptr ||
        params->dequantParameters == nullptr || params->residuals == nullptr ||
        params->transformUnitCount == 0 ||
        (params->layerCount != RCLayerCountDD && params->layerCount != RCLayerCountDDS)) {
        VNLogError("dequant_transform: invalid arguments");
        return false;
    }

    VulkanFrameContext* context = params->context;
    if (!updateDequantDescriptorSet(params->coefficients, params->dequantParameters,
                                    params->residuals, context->m_descriptorSets.dequantTransform)) {
        VNLogError("dequant_transform: failed to update descriptor set");
        return false;
    }

    context->bind(&pipelineLayout(ComputeLayoutId::DequantTransform),
                  &pipeline(ComputePipelineId::DequantTransform),
                  context->m_descriptorSets.dequantTransform);

    const PushConstantsDequantTransform constants = {params->transformUnitCount,
                                                     params->layerCount,
                                                     params->temporalSignal,
                                                     static_cast<uint32_t>(params->scalingMode),
                                                     params->deblockEnabled ? 1u : 0u,
                                                     params->deblockCorner,
                                                     params->deblockSide};
    vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::DequantTransform),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants), &constants);
    context->dispatch(params->transformUnitCount, 1, "dequant_transform", 1);
    context->insertComputeBarrier("barrier_after_dequant_transform");
    return true;
}

bool ComputeVulkan::prepareConvertIn(const VulkanConvertInArgs* params)
{
    if (params == nullptr || params->src == nullptr || params->dst == nullptr || params->context == nullptr) {
        VNLogError("prepareConvertIn: invalid arguments");
        return false;
    }

    if (!updateComputeDescriptorSets(params->context->m_descriptorSets.conversionTo,
                                     fromPipeline(params->src->buffer),
                                     fromPipeline(params->dst->buffer))) {
        VNLogError("prepareConvertIn: failed to update descriptor set");
        return false;
    }
    return true;
}

bool ComputeVulkan::convertIn(const VulkanConvertInArgs* params, uint8_t plane)
{
    if (params == nullptr || params->src == nullptr || params->dst == nullptr ||
        params->context == nullptr || plane >= ldpPictureLayoutPlanes(&params->src->layout)) {
        VNLogError("convert_in: invalid arguments");
        return false;
    }

    PictureVulkan* srcPicture = params->src;
    const LdpColorFormat srcFormat = ldpPictureLayoutFormat(&srcPicture->layout);
    const bool is8bit = ldpPictureLayoutSampleSize(&srcPicture->layout) == 1;

    PictureVulkan* dstPicture = params->dst;

    const bool isSemiPlanarNV = srcFormat == LdpColorFormatNV12_8 || srcFormat == LdpColorFormatNV21_8;

    PushConstantsConversion constants{};

    constants.width = srcPicture->layout.width;
    constants.height = srcPicture->layout.height;
    constants.shift = 15 - params->bitDepth;
    constants.sharpenStrength = 0;
    constants.nv21Swap = (srcFormat == LdpColorFormatNV21_8) ? 1u : 0u;

    VulkanFrameContext* context = params->context;

    constants.containerStrideV = 0; // No interleaved chroma for Y or planar dispatches.
    constants.containerOffsetV = 0;

    const ComputePipelineId pipelineId =
        is8bit ? (isSemiPlanarNV ? ComputePipelineId::ConversionToInternal8BitNV12
                                 : ComputePipelineId::ConversionToInternal8Bit)
               : ComputePipelineId::ConversionToInternal16Bit;
    context->bind(&pipelineLayout(ComputeLayoutId::Conversion), &pipeline(pipelineId),
                  context->m_descriptorSets.conversionTo);

    const auto componentMasks = physicalPlaneComponentMasks(srcPicture->layout);
    const LdpColorComponentMask components = componentMasks[plane];
    if ((components & LdpColorComponentMaskC0) != 0) {
        setContainerStrides(constants, 0, srcPicture, dstPicture, nullptr);
        vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Conversion),
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
        context->dispatch(constants.width, constants.height, "convert_in_y", 1);
    }

    if (params->chroma == LdeChroma::CTMonochrome ||
        (components & (LdpColorComponentMaskC1 | LdpColorComponentMaskC2)) == 0) {
        return true;
    }

    int widthShift{};
    int heightShift{};
    getSubsamplingShifts(params->chroma, widthShift, heightShift);
    constants.width = isSemiPlanarNV ? srcPicture->layout.width : srcPicture->layout.width >> widthShift;
    constants.height = srcPicture->layout.height >> heightShift;
    if (isSemiPlanarNV) {
        constants.containerStrideV = toUint32Units(ldpPictureLayoutRowStride(&dstPicture->layout, 2));
        constants.containerOffsetV = toUint32Units(ldpPictureLayoutPlaneOffset(&dstPicture->layout, 2));
    }

    if ((components & LdpColorComponentMaskC1) != 0) {
        setContainerStrides(constants, 1, srcPicture, dstPicture, nullptr);
        vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Conversion),
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
        context->dispatch(constants.width, constants.height, "convert_in_u", 1);
    }
    if ((components & LdpColorComponentMaskC2) != 0 && !isSemiPlanarNV) {
        setContainerStrides(constants, 2, srcPicture, dstPicture, nullptr);
        vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Conversion),
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
        context->dispatch(constants.width, constants.height, "convert_in_v", 1);
    }

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
        dstFormat == LdpColorFormatP010_10 || dstFormat == LdpColorFormatNV12_8 ||
        dstFormat == LdpColorFormatNV21_8;
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
    const bool dstIsNv8Image = dstFormat == LdpColorFormatNV12_8 || dstFormat == LdpColorFormatNV21_8;
    const bool dstIsRgba10Image = dstFormat == LdpColorFormatRGBA_10_2_LE;

    if (params->chroma == LdeChroma::CTMonochrome) {
        VNLogError("convertOut: VkImage output requires chroma planes");
        return false;
    }
    const bool dstIs420Image = dstIsP010Image || dstIsNv8Image;
    const bool use420BlockShader = dstIs420Image || params->chroma == LdeChroma::CT420;
    if (dstIs420Image && params->chroma != LdeChroma::CT420) {
        VNLogError("convertOut: NV12/NV21/P010 VkImage output requires 4:2:0 chroma");
        return false;
    }
    if (use420BlockShader && ((constants.width | constants.height) & 1) != 0) {
        VNLogError("convertOut: 4:2:0 VkImage output requires even storage dimensions");
        return false;
    }

    // Image-output shaders consume all three source plane stride/offset slots directly.
    // Generic RGBA shaders additionally alias `shift`/`sharpenStrength` as chroma subsampling
    // shifts; fixed-4:2:0 image shaders do not read those fields.
    constants.containerStrideIn = toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, 0));
    constants.containerOffsetIn = toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, 0));
    const bool dstIsNv21Image = dstFormat == LdpColorFormatNV21_8;
    const uint32_t firstChromaPlane = dstIsNv21Image ? 2u : 1u;
    const uint32_t secondChromaPlane = dstIsNv21Image ? 1u : 2u;
    constants.containerStrideOut =
        toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, firstChromaPlane));
    constants.containerOffsetOut =
        toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, firstChromaPlane));
    constants.containerStrideV =
        toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, secondChromaPlane));
    constants.containerOffsetV =
        toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, secondChromaPlane));
    if (!use420BlockShader) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(params->chroma, widthShift, heightShift);
        constants.shift = static_cast<uint32_t>(widthShift);
        constants.sharpenStrength = static_cast<uint32_t>(heightShift);
    }

    // Temporal-add. One dispatch handles Y/U/V, so the per-plane temporal stride/offset (pairs
    // 0/1/2) are all set here; Non-zero stride enables the add for a plane.
    const bool doAdd = params->addTemporal && params->temporal && params->temporal->buffer;
    setTemporalStrides(constants, 0, 0, doAdd, params->numEnhancedPlanes, params->temporal);
    setTemporalStrides(constants, 1, firstChromaPlane, doAdd, params->numEnhancedPlanes, params->temporal);
    setTemporalStrides(constants, 2, secondChromaPlane, doAdd, params->numEnhancedPlanes, params->temporal);
    BufferVulkan* temporalBuffer = doAdd ? fromPipeline(params->temporal->buffer) : srcBuffer;

    if (!updateComputeImageDescriptorSets(context->m_descriptorSets.conversionFromImage, srcBuffer,
                                          dstBuffer, temporalBuffer)) {
        VNLogError("convert_out_image: failed to update descriptor set");
        return false;
    }

    // Transition the (possibly imported) destination image into GENERAL
    // layout so the convert-out compute shader can write to it.
    BufferVulkan::imageLayoutTransition(
        context->m_commandBuffer, dstBuffer->getVkImage(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL, /*srcAccess=*/0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    const bool useGenericRgbaShader = params->chroma != LdeChroma::CT420;
    ComputePipelineId convertOutPipelineId{};
    const char* convertOutLabel{};
    if (useGenericRgbaShader) {
        convertOutPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalRgba8ImageGenericAddTemporal
                                     : ComputePipelineId::ConversionFromInternalRgba8ImageGeneric;
        convertOutLabel = "convert_out_image_rgba8_generic";
    } else {
        convertOutPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalRgba8ImageAddTemporal
                                     : ComputePipelineId::ConversionFromInternalRgba8Image;
        convertOutLabel = "convert_out_image_rgba8_420";
    }
    if (dstIsP010Image) {
        convertOutPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalP010ImageAddTemporal
                                     : ComputePipelineId::ConversionFromInternalP010Image;
        convertOutLabel = "convert_out_image_p010";
    } else if (dstIsNv8Image) {
        convertOutPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalNv12ImageAddTemporal
                                     : ComputePipelineId::ConversionFromInternalNv12Image;
        convertOutLabel =
            dstFormat == LdpColorFormatNV21_8 ? "convert_out_image_nv21" : "convert_out_image_nv12";
    } else if (dstIsRgba10Image) {
        if (useGenericRgbaShader) {
            convertOutPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalRgba10ImageGenericAddTemporal
                                         : ComputePipelineId::ConversionFromInternalRgba10ImageGeneric;
            convertOutLabel = "convert_out_image_rgba10_generic";
        } else {
            convertOutPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalRgba10ImageAddTemporal
                                         : ComputePipelineId::ConversionFromInternalRgba10Image;
            convertOutLabel = "convert_out_image_rgba10_420";
        }
    }
    context->bind(&pipelineLayout(ComputeLayoutId::ConvertOutImage),
                  &pipeline(convertOutPipelineId), context->m_descriptorSets.conversionFromImage);

    vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::ConvertOutImage),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
    context->dispatch(constants.width, constants.height, convertOutLabel, 1);
    context->insertComputeBarrier("barrier_after_convert_out_image");
    return true;
}

bool ComputeVulkan::convertOutToBuffer(VulkanConvertOutArgs* params, PushConstantsConversion& constants)
{
    PictureVulkan* srcPicture = params->src;
    BufferVulkan* srcBuffer = fromPipeline(srcPicture->buffer);
    PictureVulkan* dstPicture = params->dst;
    BufferVulkan* dstBuffer = fromPipeline(dstPicture->buffer);
    const LdpColorFormat dstFormat = ldpPictureLayoutFormat(&dstPicture->layout);
    VulkanFrameContext* context = params->context;

    enum class BufferOutputType : uint8_t
    {
        Planar8,
        Planar16,
        Nv12,
        P010,
    };

    BufferOutputType outputType{};
    switch (dstFormat) {
        case LdpColorFormatI420_8:
        case LdpColorFormatI422_8:
        case LdpColorFormatI444_8:
        case LdpColorFormatGRAY_8: outputType = BufferOutputType::Planar8; break;
        case LdpColorFormatI420_10_LE:
        case LdpColorFormatI420_12_LE:
        case LdpColorFormatI420_14_LE:
        case LdpColorFormatI420_16_LE:
        case LdpColorFormatI422_10_LE:
        case LdpColorFormatI422_12_LE:
        case LdpColorFormatI422_14_LE:
        case LdpColorFormatI422_16_LE:
        case LdpColorFormatI444_10_LE:
        case LdpColorFormatI444_12_LE:
        case LdpColorFormatI444_14_LE:
        case LdpColorFormatI444_16_LE:
        case LdpColorFormatGRAY_10_LE:
        case LdpColorFormatGRAY_12_LE:
        case LdpColorFormatGRAY_14_LE:
        case LdpColorFormatGRAY_16_LE: outputType = BufferOutputType::Planar16; break;
        case LdpColorFormatNV12_8:
        case LdpColorFormatNV21_8: outputType = BufferOutputType::Nv12; break;
        case LdpColorFormatP010_10: outputType = BufferOutputType::P010; break;
        default:
            VNLogError("convertOut: unsupported VkBuffer destination format %d",
                       static_cast<int32_t>(dstFormat));
            return false;
    }

    if (ldpPictureLayoutChroma(&dstPicture->layout) != params->chroma) {
        VNLogError("convertOut: source and VkBuffer destination chroma formats do not match");
        return false;
    }

    const bool isSemiplanar = outputType == BufferOutputType::Nv12 || outputType == BufferOutputType::P010;
    if (isSemiplanar && ((constants.width | constants.height) & 1) != 0) {
        VNLogError("convertOut: semiplanar VkBuffer output requires even storage dimensions");
        return false;
    }
    const bool validPlanar16Depth =
        params->bitDepth == 10 || params->bitDepth == 12 || params->bitDepth == 14;
    if ((outputType == BufferOutputType::Planar8 && params->bitDepth != 8) ||
        (outputType == BufferOutputType::Planar16 && !validPlanar16Depth) ||
        (outputType == BufferOutputType::Nv12 && params->bitDepth != 8) ||
        (outputType == BufferOutputType::P010 && params->bitDepth != 10)) {
        VNLogError("convertOut: VkBuffer destination format and bit depth do not match");
        return false;
    }

    const bool doAdd = params->addTemporal && params->temporal && params->temporal->buffer;
    const bool doSharpen = constants.sharpenStrength != 0;
    BufferVulkan* temporalBuffer = doAdd ? fromPipeline(params->temporal->buffer) : srcBuffer;
    if (!updateComputeDescriptorSets(context->m_descriptorSets.conversionFrom, srcBuffer, dstBuffer,
                                     temporalBuffer)) {
        VNLogError("convert_out_buffer: failed to update descriptor set");
        return false;
    }

    ComputePipelineId regularPipelineId{};
    ComputePipelineId sharpenPipelineId{};
    switch (outputType) {
        case BufferOutputType::Planar8:
            regularPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalPlanar8AddTemporal
                                      : ComputePipelineId::ConversionFromInternalPlanar8;
            sharpenPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalPlanar8SharpenAddTemporal
                                      : ComputePipelineId::ConversionFromInternalPlanar8Sharpen;
            break;
        case BufferOutputType::Planar16:
            regularPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalPlanar16AddTemporal
                                      : ComputePipelineId::ConversionFromInternalPlanar16;
            sharpenPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalPlanar16SharpenAddTemporal
                                      : ComputePipelineId::ConversionFromInternalPlanar16Sharpen;
            break;
        case BufferOutputType::Nv12:
            regularPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalNv12AddTemporal
                                      : ComputePipelineId::ConversionFromInternalNv12;
            sharpenPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalNv12SharpenAddTemporal
                                      : ComputePipelineId::ConversionFromInternalNv12Sharpen;
            break;
        case BufferOutputType::P010:
            regularPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalP010AddTemporal
                                      : ComputePipelineId::ConversionFromInternalP010;
            sharpenPipelineId = doAdd ? ComputePipelineId::ConversionFromInternalP010SharpenAddTemporal
                                      : ComputePipelineId::ConversionFromInternalP010Sharpen;
            break;
    }

    if (isSemiplanar) {
        context->bind(&pipelineLayout(ComputeLayoutId::Conversion),
                      &pipeline(doSharpen ? sharpenPipelineId : regularPipelineId),
                      context->m_descriptorSets.conversionFrom);
        const bool swapChroma = dstFormat == LdpColorFormatNV21_8;
        const uint32_t firstChromaPlane = swapChroma ? 2u : 1u;
        const uint32_t secondChromaPlane = swapChroma ? 1u : 2u;
        constants.containerStrideU =
            toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, firstChromaPlane));
        constants.containerOffsetU =
            toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, firstChromaPlane));
        constants.containerStrideV =
            toUint32Units(ldpPictureLayoutRowStride(&srcPicture->layout, secondChromaPlane));
        constants.containerOffsetV =
            toUint32Units(ldpPictureLayoutPlaneOffset(&srcPicture->layout, secondChromaPlane));
        constants.containerStrideUVOut =
            toUint32Units(ldpPictureLayoutRowStride(&dstPicture->layout, 1));
        constants.containerOffsetUVOut =
            toUint32Units(ldpPictureLayoutPlaneOffset(&dstPicture->layout, 1));
        setTemporalStrides(constants, 0, 0, doAdd, params->numEnhancedPlanes, params->temporal);
        setTemporalStrides(constants, 1, firstChromaPlane, doAdd, params->numEnhancedPlanes,
                           params->temporal);
        setTemporalStrides(constants, 2, secondChromaPlane, doAdd, params->numEnhancedPlanes,
                           params->temporal);

        vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Conversion),
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
        const char* label = "convert_out_nv12";
        if (outputType == BufferOutputType::P010) {
            label = "convert_out_p010";
        } else if (swapChroma) {
            label = "convert_out_nv21";
        }
        context->dispatch(constants.width, constants.height, label, 1);
    } else {
        const uint8_t planeCount = ldpPictureLayoutPlanes(&srcPicture->layout);
        static constexpr const char* kLabels[] = {"convert_out_y", "convert_out_u", "convert_out_v"};
        setTemporalStrides(constants, 1, 0, false, 0, nullptr);
        setTemporalStrides(constants, 2, 0, false, 0, nullptr);
        context->bind(&pipelineLayout(ComputeLayoutId::Conversion),
                      &pipeline(doSharpen ? sharpenPipelineId : regularPipelineId),
                      context->m_descriptorSets.conversionFrom);
        for (uint8_t plane = 0; plane < planeCount; ++plane) {
            if (plane == 1 && doSharpen) {
                context->bind(&pipelineLayout(ComputeLayoutId::Conversion),
                              &pipeline(regularPipelineId), context->m_descriptorSets.conversionFrom);
            }
            constants.width = ldpPictureLayoutPlaneWidth(&srcPicture->layout, plane);
            constants.height = ldpPictureLayoutPlaneHeight(&srcPicture->layout, plane);
            setContainerStrides(constants, plane, srcPicture, dstPicture, nullptr);
            setTemporalStrides(constants, 0, plane, doAdd, params->numEnhancedPlanes, params->temporal);
            vkCmdPushConstants(
                context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Conversion),
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsConversion), &constants);
            context->dispatch(constants.width, constants.height, kLabels[plane], 1);
        }
    }

    context->insertComputeBarrier("barrier_after_convert_out_buffer");
    return true;
}

bool ComputeVulkan::prepareUpscale(const VulkanUpscaleArgs* params)
{
    if (params == nullptr || params->src == nullptr || params->dst == nullptr || params->context == nullptr ||
        params->base == nullptr || (params->loq != LOQ0 && params->loq != LOQ1)) {
        VNLogError("prepareUpscale: invalid arguments");
        return false;
    }

    if (!updateComputeDescriptorSets(params->context->m_descriptorSets.upscale[params->loq],
                                     fromPipeline(params->src->buffer), fromPipeline(params->dst->buffer),
                                     fromPipeline(params->base->buffer))) {
        VNLogError("prepareUpscale: failed to update descriptor set");
        return false;
    }
    return true;
}

bool ComputeVulkan::upscale(const LdeKernel* kernel, const VulkanUpscaleArgs* params, uint8_t plane)
{
    assert(kernel && params);
    assert(params->src && params->dst && params->base && params->context);
    assert(params->loq == LOQ0 || params->loq == LOQ1);
    assert(plane < ldpPictureLayoutPlanes(&params->src->layout));
    assert(plane < ldpPictureLayoutPlanes(&params->dst->layout));
    assert(plane < ldpPictureLayoutPlanes(&params->base->layout));

    switch (params->mode) {
        case Scale1D: return upscale1D(kernel, params, plane);
        case Scale2D: return upscale2D(kernel, params, plane);
        default: {
            VNLogError("upscale: unsupported scaling mode %d", static_cast<int32_t>(params->mode));
            return false;
        }
    }
}

bool ComputeVulkan::upscale1D(const LdeKernel* kernel, const VulkanUpscaleArgs* params, uint8_t plane)
{
    PictureVulkan* srcPicture = params->src;
    PictureVulkan* dstPicture = params->dst;
    PictureVulkan* basePicture = params->base;

    const uint32_t srcWidth = srcPicture->getActiveWidth();
    const uint32_t srcHeight = srcPicture->getActiveHeight();

    VulkanFrameContext* context = params->context;

    VkDescriptorSet& descriptorSet = context->m_descriptorSets.upscale[params->loq];
    PushConstantsUpscale constants{};
    for (uint32_t i = 0; i < 4; ++i) {
        constants.kernel[i] = ldeKernelGetCoefficient(kernel, i);
    }

    const ComputePipelineId pipelineId =
        params->pa ? ComputePipelineId::Upscale1DPA1 : ComputePipelineId::Upscale1DPA0;
    context->bind(&pipelineLayout(ComputeLayoutId::Upscale), &pipeline(pipelineId), descriptorSet);

    int widthShift{};
    int heightShift{};
    if (params->chroma != LdeChroma::CTMonochrome) {
        getSubsamplingShifts(params->chroma, widthShift, heightShift);
    }

    static constexpr const char* kLabels[] = {"upscale1d_y", "upscale1d_u", "upscale1d_v"};
    constants.srcWidth = plane == 0 ? srcWidth : srcWidth >> widthShift;
    constants.srcHeight = plane == 0 ? srcHeight : srcHeight >> heightShift;
    setContainerStrides(constants, plane, srcPicture, dstPicture, basePicture);
    vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Upscale),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);
    // The 1D shader processes two rows per invocation.
    context->dispatch(constants.srcWidth, (constants.srcHeight + 1) >> 1, kLabels[plane], 1);

    return true;
}

bool ComputeVulkan::upscale2D(const LdeKernel* kernel, const VulkanUpscaleArgs* params, uint8_t plane)
{
    PictureVulkan* srcPicture = params->src;
    PictureVulkan* dstPicture = params->dst;
    PictureVulkan* basePicture = params->base;
    VulkanFrameContext* context = params->context;

    VkDescriptorSet& descriptorSet = context->m_descriptorSets.upscale[params->loq];
    PushConstantsUpscale constants{};
    for (uint32_t i = 0; i < 4; ++i) {
        constants.kernel[i] = ldeKernelGetCoefficient(kernel, i);
    }
    const uint32_t srcWidth = srcPicture->getActiveWidth();
    const uint32_t srcHeight = srcPicture->getActiveHeight();

    const ComputePipelineId pipelineId =
        params->pa ? ComputePipelineId::Upscale2DPA2 : ComputePipelineId::Upscale2DPA0;
    context->bind(&pipelineLayout(ComputeLayoutId::Upscale), &pipeline(pipelineId), descriptorSet);

    int widthShift{};
    int heightShift{};
    if (params->chroma != LdeChroma::CTMonochrome) {
        getSubsamplingShifts(params->chroma, widthShift, heightShift);
    }

    static constexpr const char* kLabels[] = {"upscale2d_y", "upscale2d_u", "upscale2d_v"};
    constants.srcWidth = plane == 0 ? srcWidth : srcWidth >> widthShift;
    constants.srcHeight = plane == 0 ? srcHeight : srcHeight >> heightShift;
    setContainerStrides(constants, plane, srcPicture, dstPicture, basePicture);
    vkCmdPushConstants(context->m_commandBuffer, pipelineLayoutHandle(ComputeLayoutId::Upscale),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantsUpscale), &constants);
    context->dispatch(constants.srcWidth, constants.srcHeight, kLabels[plane], 1);
    return true;
}

} // namespace lcevc_dec::pipeline_vulkan
