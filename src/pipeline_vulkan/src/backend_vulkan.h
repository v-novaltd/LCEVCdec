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

#ifndef VN_LCEVC_PIPELINE_VULKAN_BACKEND_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_BACKEND_VULKAN_H

#include "compute_vulkan.h"
#include "render_vulkan.h"

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/log.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <vulkan/vulkan.h>

#if VN_OS(ANDROID)
#include <vulkan/vulkan_android.h>

#if defined(ANDROID_BUFFERS)
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#endif
#endif

#include <vk_mem_alloc.h>

#include <optional>
#include <vector>

namespace lcevc_dec::pipeline_vulkan {

class PipelineVulkan;
struct VulkanApplyCommonArgs;
struct VulkanAddArgs;
struct VulkanConversionArgs;
struct VulkanUpscaleArgs;

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    bool isComplete(bool renderingEnabled, bool requirePresent) const
    {
        bool r = computeFamily.has_value();
        if (renderingEnabled) {
            r = r && graphicsFamily.has_value();
            if (requirePresent) {
                r = r && presentFamily.has_value();
            }
        }
        return r;
    }
};

class BackendVulkan
{
public:
    BackendVulkan(bool renderingEnabled, int32_t deviceOverride, bool enableValidationLayers,
                  int32_t timestampsLimit, int32_t contextCount);
    ~BackendVulkan() = default;

    bool applyCommon(VulkanApplyCommonArgs* params) { return m_compute.applyCommon(params); };
    bool applyTile(VulkanApplyTileArgs* params) { return m_compute.applyTile(params); };
    bool add(VulkanAddArgs* params) { return m_compute.add(params); };
    bool convertIn(VulkanConvertInArgs* params) { return m_compute.convertIn(params); };
    bool convertOut(VulkanConvertOutArgs* params) { return m_compute.convertOut(params); };
    bool upscaleVertical(const LdeKernel* kernel, VulkanUpscaleArgs* params)
    {
        return m_compute.upscaleVertical(kernel, params);
    };
    bool upscaleHorizontal(const LdeKernel* kernel, VulkanUpscaleArgs* params)
    {
        return m_compute.upscaleHorizontal(kernel, params);
    };
    bool upscaleFrame(const LdeKernel* kernel, VulkanUpscaleArgs* params)
    {
        return m_compute.upscaleFrame(kernel, params);
    };

    bool initVulkan();
    bool initRemaningRender();
    void destroy();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkInstance& getInstance() { return m_instance; }
    VkDevice& getDevice() { return m_device; }
    VkPhysicalDevice& getPhysicalDevice() { return m_physicalDevice; }
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, bool renderingRequired,
                                         VkSurfaceKHR surface = VK_NULL_HANDLE);

    bool renderingEnabled() const { return m_renderingEnabled; }
    ComputeVulkan& compute() { return m_compute; }
    RenderVulkan& getRenderer() { return m_render; }
    VkQueue& getComputeQueue() { return m_computeQueue; }
    VkQueue& getGraphicsQueue() { return m_graphicsQueue; }
    VkQueue& getPresentQueue() { return m_presentQueue; }
    const QueueFamilyIndices& getQueueFamilies() { return m_indices; }

    VmaAllocator getVmaAllocator() { return m_allocator; }

    int32_t getTimestampsLimit() { return m_timestampsLimit; }

    const VkPhysicalDeviceProperties& physicalDeviceProperties()
    {
        return m_physicalDeviceProperties;
    }

    VNNoCopyNoMove(BackendVulkan);

private:
    // Vulkan common
    bool createInstance();
    bool setupDebugMessenger();
    bool pickPhysicalDevice();
    bool createLogicalDeviceAndQueues();
    bool finalisePresentQueueAfterSurface();

    // Helpers - building
    bool checkValidationLayerSupport() const;
    std::vector<const char*> getRequiredInstanceExtensions() const;
    std::vector<const char*> getRequiredDeviceExtensions() const;
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator);

    bool isDeviceSuitableHeadless(VkPhysicalDevice device);
    bool isDeviceSuitableWithSurface(VkPhysicalDevice device);
    VkPhysicalDevice pickBestDevice(const std::vector<VkPhysicalDevice>& devices);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    bool checkDeviceExtension(VkPhysicalDevice device, const char* ext) const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void* pUserData);

    static const std::vector<const char*> validationLayers;

    bool m_renderingEnabled{};
    int32_t m_deviceOverride{};
    bool m_enableValidationLayers{};
    int32_t m_timestampsLimit{};
    QueueFamilyIndices m_indices;
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties m_physicalDeviceProperties{};

    VkDevice m_device{};
    VkQueue m_computeQueue{};
    VkQueue m_graphicsQueue{};
    VkQueue m_presentQueue{};
    ComputeVulkan m_compute;
    RenderVulkan m_render;

    VmaAllocator m_allocator{};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_BACKEND_VULKAN_H
