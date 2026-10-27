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

#ifndef VN_LCEVC_PIPELINE_VULKAN_TYPES_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_TYPES_VULKAN_H

#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <picture_vulkan.h>

namespace lcevc_dec::pipeline_vulkan {

class PipelineVulkan;
class VulkanFrameContext;

struct VulkanConvertInArgs
{
    PictureVulkan* src{};
    PictureVulkan* dst{};
    uint8_t bitDepth{}; // Bit depth of the source picture
    LdeChroma chroma{CTMonochrome};
    VulkanFrameContext* context{};
};

struct VulkanUpscaleArgs
{
    PictureVulkan* src{};
    PictureVulkan* dst{};
    PictureVulkan* base{};        // Base picture used for predicted-average correction
    LdeLOQIndex loq{LOQ0};        // Destination LOQ, either LOQ0 or LOQ1
    LdeScalingMode mode{Scale0D}; // The type of scaling to perform (1D or 2D)
    bool pa{}; // Enables predicted-average correction for the selected scaling mode
    LdeChroma chroma{CTMonochrome};
    VulkanFrameContext* context{};
};

struct VulkanApplyCommonArgs
{
    PictureVulkan* picture{}; // nullptr for temporal apply
    uint32_t planeWidth{};
    uint32_t planeHeight{};
    uint8_t plane{};
    LdeLOQIndex loq{LOQ0}; // LOQ index (LOQ0 or LOQ1) for selecting the correct command buffer
    LdeChroma chroma{CTMonochrome};
    LdeTransformType transform{TransformDD};
    bool tuRasterOrder{}; // true when tiles are in raster order
    PictureVulkan* temporalPicture{};
    VulkanFrameContext* context{};
};

struct VulkanApplyTileArgs
{
    PictureVulkan* picture{}; // nullptr for temporal apply
    uint32_t planeWidth{};
    uint32_t planeHeight{};
    LdeCmdBufferGpu bufferGpu{};
    uint16_t tileX{};
    uint16_t tileY{};
    uint16_t tileWidth{};
    uint8_t plane{};
    LdeLOQIndex loq{LOQ0}; // LOQ index (LOQ0 or LOQ1) for selecting the correct command buffer
    bool highlightResiduals{};
    LdeTransformType transform{TransformDD};
    bool tuRasterOrder{};
    PictureVulkan* temporalPicture{};
    VulkanFrameContext* context{};
};

struct VulkanConvertOutArgs
{
    PictureVulkan* src{};
    PictureVulkan* dst{};
    uint8_t bitDepth{}; // Bit depth of the destination picture
    LdeChroma chroma{CTMonochrome};
    float sharpenStrength{};   // Sharpen strength for output conversion (plane 0 only)
    PictureVulkan* temporal{}; // Optional temporal residuals to add before conversion
    bool addTemporal{false};
    uint8_t numEnhancedPlanes{};
    VulkanFrameContext* context{};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_TYPES_VULKAN_H
