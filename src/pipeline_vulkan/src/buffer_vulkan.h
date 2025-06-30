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

#ifndef VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_H

#include "pipeline_vulkan.h"

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/memory.h>
#include <LCEVC/pipeline/buffer.h>

namespace lcevc_dec::pipeline_vulkan {

class PictureVulkan;
class PipelineVulkan;

class BufferVulkan : public LdpBuffer
{
public:
    BufferVulkan(PipelineVulkan& pipeline, uint32_t size);
    ~BufferVulkan();

    bool map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access);
    void unmap(const LdpBufferMapping* mapping);

    void clear();

    uint8_t* ptr() const;
    uint32_t size() const;

    bool resize(uint32_t size);

    VkBuffer& getVkBuffer() { return m_buffer; }
    void* getBuffer() { return m_allocation.ptr; }

    VNNoCopyNoMove(BufferVulkan);

private:
    bool createBufferAndMemory(uint32_t size);
    void destroy();

    VkBuffer m_buffer;
    VkDeviceMemory m_memory;

    PipelineVulkan& m_pipeline;
    LdcMemoryAllocation m_allocation = {0};

    bool m_mapped = false;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_H
