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

#ifndef VN_LCEVC_PIPELINE_VULKAN_VULKAN_HANDLE_H
#define VN_LCEVC_PIPELINE_VULKAN_VULKAN_HANDLE_H

#include <vulkan/vulkan.h>

#include <utility>

namespace lcevc_dec::pipeline_vulkan {

template <typename Handle, typename Deleter>
class UniqueDeviceHandle
{
public:
    UniqueDeviceHandle() = default;
    ~UniqueDeviceHandle() { reset(); }

    UniqueDeviceHandle(UniqueDeviceHandle&& other) noexcept
        : m_device(std::exchange(other.m_device, VkDevice{}))
        , m_handle(std::exchange(other.m_handle, Handle{}))
    {}

    UniqueDeviceHandle& operator=(UniqueDeviceHandle&& other) noexcept
    {
        if (this != &other) {
            reset();
            m_device = std::exchange(other.m_device, VkDevice{});
            m_handle = std::exchange(other.m_handle, Handle{});
        }
        return *this;
    }

    UniqueDeviceHandle(const UniqueDeviceHandle&) = delete;
    UniqueDeviceHandle& operator=(const UniqueDeviceHandle&) = delete;

    Handle get() const noexcept { return m_handle; }
    explicit operator bool() const noexcept { return m_handle != Handle{}; }

    // Releases any current object and returns storage suitable for a Vulkan create call.
    Handle* put(VkDevice device) noexcept
    {
        reset();
        m_device = device;
        return &m_handle;
    }

    void reset() noexcept
    {
        if (m_handle != Handle{}) {
            Deleter{}(m_device, m_handle);
        }
        m_device = VkDevice{};
        m_handle = Handle{};
    }

private:
    VkDevice m_device{};
    Handle m_handle{};
};

struct ShaderModuleDeleter
{
    void operator()(VkDevice device, VkShaderModule handle) const noexcept
    {
        vkDestroyShaderModule(device, handle, nullptr);
    }
};

struct DescriptorSetLayoutDeleter
{
    void operator()(VkDevice device, VkDescriptorSetLayout handle) const noexcept
    {
        vkDestroyDescriptorSetLayout(device, handle, nullptr);
    }
};

struct DescriptorPoolDeleter
{
    void operator()(VkDevice device, VkDescriptorPool handle) const noexcept
    {
        vkDestroyDescriptorPool(device, handle, nullptr);
    }
};

struct PipelineLayoutDeleter
{
    void operator()(VkDevice device, VkPipelineLayout handle) const noexcept
    {
        vkDestroyPipelineLayout(device, handle, nullptr);
    }
};

struct PipelineDeleter
{
    void operator()(VkDevice device, VkPipeline handle) const noexcept
    {
        vkDestroyPipeline(device, handle, nullptr);
    }
};

using UniqueShaderModule = UniqueDeviceHandle<VkShaderModule, ShaderModuleDeleter>;
using UniqueDescriptorSetLayout = UniqueDeviceHandle<VkDescriptorSetLayout, DescriptorSetLayoutDeleter>;
using UniqueDescriptorPool = UniqueDeviceHandle<VkDescriptorPool, DescriptorPoolDeleter>;
using UniquePipelineLayout = UniqueDeviceHandle<VkPipelineLayout, PipelineLayoutDeleter>;
using UniquePipeline = UniqueDeviceHandle<VkPipeline, PipelineDeleter>;

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_VULKAN_HANDLE_H
