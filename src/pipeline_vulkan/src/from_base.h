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

#ifndef VN_LCEVC_PIPELINE_VULKAN_FROM_BASE_H
#define VN_LCEVC_PIPELINE_VULKAN_FROM_BASE_H

#include "buffer_vulkan.h"
#include "buffer_vulkan_vma.h"
#include "frame_vulkan.h"
#include "picture_lock_vulkan.h"
#include "picture_vulkan.h"
#include "pipeline_builder_vulkan.h"
#include "pipeline_config_vulkan.h"
#include "pipeline_vulkan.h"
#include "temporal_buffer_vulkan.h"

#include <LCEVC/pipeline/temporal_buffer_base.h>

namespace lcevc_dec::pipeline_vulkan {

// Mechanism to convert from xxxBase types  to pipeline's derived type. Uses dynamic_cast<> for
// debug builds.
//
template <typename B>
struct Derived;

template <>
struct Derived<pipeline::BufferBase>
{
    typedef BufferVulkanVma Type;
};
template <>
struct Derived<pipeline::PictureBase>
{
    typedef PictureVulkan Type;
};
template <>
struct Derived<pipeline::FrameBase>
{
    typedef FrameVulkan Type;
};
template <>
struct Derived<pipeline::PipelineBase>
{
    typedef PipelineVulkan Type;
};
template <>
struct Derived<pipeline::PictureLockBase>
{
    typedef PictureLockVulkan Type;
};
template <>
struct Derived<pipeline::PipelineBuilderBase>
{
    typedef PipelineBuilderVulkan Type;
};
template <>
struct Derived<pipeline::PipelineConfigBase>
{
    typedef PipelineConfigVulkan Type;
};
template <>
struct Derived<pipeline::TemporalBufferBase>
{
    typedef TemporalBufferVulkan Type;
};

template <>
struct Derived<LdpPicture>
{
    typedef PictureVulkan Type;
};
template <>
struct Derived<LdpFrame>
{
    typedef FrameVulkan Type;
};
template <>
struct Derived<LdpBuffer>
{
    typedef BufferVulkanVma Type;
};
template <>
struct Derived<LdpPictureLock>
{
    typedef PictureLockVulkan Type;
};

template <typename B>
typename Derived<B>::Type* fromBase(B* bp)
{
#if !defined(NDEBUG)
    auto* dp = dynamic_cast<typename Derived<B>::Type*>(bp);
    assert(dp);
    return dp;
#else
    return static_cast<typename Derived<B>::Type*>(bp);
#endif
}

template <typename B>
typename Derived<B>::Type* fromPipeline(B* bp)
{
    return static_cast<typename Derived<B>::Type*>(bp);
}

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_FROM_BASE_H
