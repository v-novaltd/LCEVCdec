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

#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <picture_vulkan.h>

namespace lcevc_dec::pipeline_vulkan {

class PipelineVulkan;
class VulkanFrameContext;

struct VulkanConvertInArgs
{
    PictureVulkan* src;
    PictureVulkan* dst;
    uint8_t bitDepth; /**< Bit depth of the source picture */
    LdeChroma chroma;
    VulkanFrameContext* context{};
};

struct VulkanConvertOutArgs
{
    PictureVulkan* src;
    PictureVulkan* dst;
    uint8_t bitDepth; /**< Bit depth of the destination picture */
    LdeChroma chroma;
    float sharpenStrength{0.0f}; /**< Sharpen strength for output conversion (plane 0 only) */
    PictureVulkan* temporal{};   /**< Optional temporal residuals to add before conversion */
    bool addTemporal{false};
    uint8_t numEnhancedPlanes{0};
    VulkanFrameContext* context{};
};

struct VulkanAddArgs
{
    PictureVulkan* src;
    PictureVulkan* dst;
    uint8_t numEnhancedPlanes;
    LdeChroma chroma;
    VulkanFrameContext* context{};
};

struct VulkanUpscaleArgs
{
    PictureVulkan* src;
    PictureVulkan* dst;
    PictureVulkan* base;     /**< base picture only used for PA */
    uint8_t applyPA;         /**< Indicates that predicted-average should be off, 1D, or 2D */
    LdppDitherFrame* dither; /**< Indicates that dithering should be applied  */
    LdeScalingMode mode;     /**< The type of scaling to perform (1D or 2D). */
    bool vertical; /**< Not part of the standard but if the scaling mode is 1D we can optionally do vertical instead of horizontal. Required for unit tests */
    bool loq1;     /**< Allows separate intermediate states for LOQ1/0 */
    uint8_t numImagePlanes;
    LdeChroma chroma;
    PictureVulkan* intermediateUpscalePicture[LOQEnhancedCount] = {};
    PipelineVulkan* pipeline{};
    VulkanFrameContext* context{};
};

struct VulkanApplyCommonArgs
{
    PictureVulkan* picture; // nullptr for temporal apply
    uint32_t planeWidth;
    uint32_t planeHeight;
    uint8_t plane;
    uint8_t loq; // LOQ index (LOQ0 or LOQ1) for selecting the correct command buffer
    LdeChroma chroma;
    bool dds;           // true when layerCount == 16 (DDS), false for DD
    bool tuRasterOrder; // true when tiles are in raster order
    PictureVulkan* temporalPicture;
    VulkanFrameContext* context{};
};

struct VulkanApplyTileArgs
{
    PictureVulkan* picture; // nullptr for temporal apply
    uint32_t planeWidth;
    uint32_t planeHeight;
    LdeCmdBufferGpu bufferGpu;
    uint16_t tileX;
    uint16_t tileY;
    uint16_t tileWidth;
    uint8_t plane;
    uint8_t loq; // LOQ index (LOQ0 or LOQ1) for selecting the correct command buffer
    bool highlightResiduals;
    bool dds; // true when layerCount == 16 (DDS), false for DD
    bool tuRasterOrder;
    PictureVulkan* temporalPicture;
    VulkanFrameContext* context{};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_TYPES_VULKAN_H
