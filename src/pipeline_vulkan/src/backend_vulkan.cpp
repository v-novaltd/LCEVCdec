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

#include <LCEVC/build_config.h>
#include <LCEVC/common/log.h>
#include <vulkan/vulkan_core.h>

#include <string>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#if VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
#define VMA_STATS_STRING_ENABLED 0
#endif

// The VulkanMemoryAllocator implementation collides with some of the more paranoid GCC warnings.
//
#if !VN_COMPILER(MSVC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#if VN_COMPILER(CLANG)
#pragma GCC diagnostic ignored "-Wunused-private-field"
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#endif
#endif

#include "backend_vulkan.h"

#if !VN_COMPILER(MSVC)
#pragma GCC diagnostic pop
#endif

//
#include <set>

namespace lcevc_dec::pipeline_vulkan {

const std::vector<const char*> BackendVulkan::validationLayers = {"VK_LAYER_KHRONOS_validation"};

BackendVulkan::BackendVulkan(bool renderingEnabled, int32_t deviceOverride,
                             bool enableValidationLayers, int32_t timestampsLimit, int32_t contextCount)
    : m_renderingEnabled(renderingEnabled)
    , m_deviceOverride(deviceOverride)
    , m_enableValidationLayers(enableValidationLayers)
    , m_timestampsLimit(timestampsLimit)
    , m_compute(*this, contextCount)
    , m_render(*this)
{}

void BackendVulkan::destroy()
{
    waitDeviceIdle();
    m_compute.destroy();
    if (m_renderingEnabled) {
        m_render.destroy();
    }

    // Tear down allocator
    vmaDestroyAllocator(m_allocator);

    // this needs to defer until dec is finished with all vulkan buffers
    {
        common::ScopedLock lock(m_queueMutex);
        vkDestroyDevice(m_device, nullptr);
    }

    if (m_enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }

    vkDestroyInstance(m_instance, nullptr);
}

VkResult BackendVulkan::submitQueue(VkQueue queue, uint32_t submitCount,
                                    const VkSubmitInfo* submits, VkFence fence)
{
    common::ScopedLock lock(m_queueMutex);
    return vkQueueSubmit(queue, submitCount, submits, fence);
}

VkResult BackendVulkan::presentQueue(VkQueue queue, const VkPresentInfoKHR* presentInfo)
{
    common::ScopedLock lock(m_queueMutex);
    return vkQueuePresentKHR(queue, presentInfo);
}

VkResult BackendVulkan::waitQueueIdle(VkQueue queue)
{
    common::ScopedLock lock(m_queueMutex);
    return vkQueueWaitIdle(queue);
}

VkResult BackendVulkan::waitDeviceIdle()
{
    common::ScopedLock lock(m_queueMutex);
    return vkDeviceWaitIdle(m_device);
}
VKAPI_ATTR VkBool32 VKAPI_CALL BackendVulkan::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        VNLogErrorF("validation layer: %s", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        VNLogWarningF("validation layer: %s", pCallbackData->pMessage);
    } else {
        VNLogDebugF("validation layer: %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

void BackendVulkan::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VkResult BackendVulkan::CreateDebugUtilsMessengerEXT(VkInstance instance,
                                                     const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                     const VkAllocationCallbacks* pAllocator,
                                                     VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void BackendVulkan::DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                                  VkDebugUtilsMessengerEXT debugMessenger,
                                                  const VkAllocationCallbacks* pAllocator)
{
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

bool BackendVulkan::setupDebugMessenger()
{
    if (!m_enableValidationLayers) {
        return true;
    }
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        VNLogError("failed to set up debug messenger!");
        return false;
    }
    return true;
}

std::vector<const char*> BackendVulkan::getRequiredInstanceExtensions() const
{
    std::vector<const char*> extensions;
    if (m_renderingEnabled) {
        extensions = m_render.getRenderingInstanceExtensions();
    }
    extensions.push_back("VK_KHR_get_physical_device_properties2");
    extensions.push_back("VK_KHR_external_memory_capabilities");
    if (m_enableValidationLayers) {
        extensions.push_back("VK_EXT_debug_utils");
    }
    return extensions;
}

std::vector<const char*> BackendVulkan::getRequiredDeviceExtensions() const
{
    std::vector<const char*> extensions;

    if (m_renderingEnabled) {
        extensions = m_render.getRenderingDeviceExtensions();
    }

#if VN_OS(ANDROID)
    extensions.push_back("VK_ANDROID_external_memory_android_hardware_buffer");
#endif

    return extensions;
}

bool BackendVulkan::checkValidationLayerSupport() const
{
    uint32_t layerCount{};
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);

    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool found = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }
    return true;
}

bool BackendVulkan::createInstance()
{
    if (m_enableValidationLayers && !checkValidationLayerSupport()) {
        VNLogError("validation layers requested, but not available!");
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan DEC";
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredInstanceExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        VNLogError("failed to create instance!");
        return false;
    }
    return true;
}

bool BackendVulkan::checkDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    const auto rde = getRequiredDeviceExtensions();
    std::set<std::string, std::less<>> requiredDeviceExtensions(rde.begin(), rde.end());

    VNLogInfo("Required %d device extensions:", requiredDeviceExtensions.size());
    for ([[maybe_unused]] const auto& e : requiredDeviceExtensions) {
        VNLogInfoF(" - %s", e.c_str());
    }

    for (const auto& e : availableExtensions) {
        requiredDeviceExtensions.erase(e.extensionName);
    }

    if (!requiredDeviceExtensions.empty()) {
        VNLogError("Missing %d device extensions:", requiredDeviceExtensions.size());
        for ([[maybe_unused]] const auto& e : requiredDeviceExtensions) {
            VNLogErrorF("  - %s", e.c_str());
        }
    }

    return requiredDeviceExtensions.empty();
}

bool BackendVulkan::checkDeviceExtension(VkPhysicalDevice device, const char* ext) const
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for (const auto& e : availableExtensions) {
        if (!strcmp(ext, e.extensionName)) {
            return true;
        }
    }

    return false;
}

QueueFamilyIndices BackendVulkan::findQueueFamilies(VkPhysicalDevice device, bool renderingRequired,
                                                    VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }
        if (renderingRequired) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            if (surface != VK_NULL_HANDLE) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

                if (presentSupport) {
                    indices.presentFamily = i;
                }
            }
        }
        if (indices.isComplete(renderingRequired, (surface != VK_NULL_HANDLE))) {
            break;
        }

        i++;
    }

    return indices;
}

bool BackendVulkan::isDeviceSuitableWithSurface(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    if (deviceProperties.apiVersion < VK_API_VERSION_1_1) {
        VNLogWarning("Skipping device %s: Vulkan %u.%u.%u < 1.1", deviceProperties.deviceName,
                     VK_VERSION_MAJOR(deviceProperties.apiVersion),
                     VK_VERSION_MINOR(deviceProperties.apiVersion),
                     VK_VERSION_PATCH(deviceProperties.apiVersion));
        return false;
    }

    QueueFamilyIndices indices = findQueueFamilies(device, true, m_render.getSurface());

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    const SwapChainSupportDetails swapChainSupport = m_render.querySwapChainSupport(device);
    bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

    return indices.isComplete(true, true) && swapChainAdequate && extensionsSupported;
}

bool BackendVulkan::isDeviceSuitableHeadless(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties gpuProperties;
    vkGetPhysicalDeviceProperties(device, &gpuProperties);
    if (gpuProperties.apiVersion < VK_API_VERSION_1_1) {
        VNLogWarning("Skipping device %s: Vulkan %u.%u.%u < 1.1", gpuProperties.deviceName,
                     VK_VERSION_MAJOR(gpuProperties.apiVersion),
                     VK_VERSION_MINOR(gpuProperties.apiVersion),
                     VK_VERSION_PATCH(gpuProperties.apiVersion));
        return false;
    }

    const auto realGpu = gpuProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                         gpuProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

    if (!realGpu) {
        return false;
    }

    const auto indices = findQueueFamilies(device, m_renderingEnabled);
    if (!indices.isComplete(m_renderingEnabled, false)) {
        return false;
    }

    const auto extensionsSupported = checkDeviceExtensionSupport(device);
    return extensionsSupported;
}

VkPhysicalDevice BackendVulkan::pickBestDevice(const std::vector<VkPhysicalDevice>& devices)
{
    if (devices.empty()) {
        return VK_NULL_HANDLE;
    }

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    uint32_t bestScore = 0;
    bool haveBest = false;
    for (const auto& device : devices) {
        uint32_t score = 0; // TODO - needs to be tuned

        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        VNLogDebugF("Suitable compute device: %s", deviceProperties.deviceName);

        score += deviceProperties.limits.maxComputeWorkGroupCount[0];
        VNLogDebug("maxComputeWorkGroupCount: %u %u %u",
                   deviceProperties.limits.maxComputeWorkGroupCount[0],
                   deviceProperties.limits.maxComputeWorkGroupCount[1],
                   deviceProperties.limits.maxComputeWorkGroupCount[2]);

        score += deviceProperties.limits.maxComputeWorkGroupSize[0];
        VNLogDebug("maxComputeWorkGroupSize: %u %u %u",
                   deviceProperties.limits.maxComputeWorkGroupSize[0],
                   deviceProperties.limits.maxComputeWorkGroupSize[1],
                   deviceProperties.limits.maxComputeWorkGroupSize[2]);

        score += deviceProperties.limits.maxComputeWorkGroupInvocations;
        VNLogDebug("maxComputeWorkGroupInvocations: %u",
                   deviceProperties.limits.maxComputeWorkGroupInvocations);

        VkPhysicalDeviceSubgroupProperties subgroupProperties{};
        subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        subgroupProperties.pNext = nullptr;

        VkPhysicalDeviceProperties2 deviceProperties2{};
        deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties2.pNext = &subgroupProperties;
        vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

        score += subgroupProperties.subgroupSize;
        VNLogDebug("subgroupSize: %u", subgroupProperties.subgroupSize);

        score += deviceProperties2.properties.limits.maxMemoryAllocationCount;
        VNLogDebug("maxMemoryAllocationCount: %u",
                   deviceProperties2.properties.limits.maxMemoryAllocationCount);

        if (!haveBest || score > bestScore) {
            bestScore = score;
            bestDevice = device;
            haveBest = true;
        }
    }

    // Allow the device choice to be over-ridden from configuration
    if (m_deviceOverride >= 0) {
        if ((size_t)m_deviceOverride >= devices.size()) {
            return VK_NULL_HANDLE;
        }
        return devices[m_deviceOverride];
    }

    return bestDevice;
}

bool BackendVulkan::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        VNLogError("failed to find GPUs with Vulkan support!");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    std::vector<VkPhysicalDevice> suitableDevices;
    for (const auto& device : devices) {
        if (isDeviceSuitableHeadless(device)) {
            suitableDevices.push_back(device);
        }
    }

    m_physicalDevice = pickBestDevice(suitableDevices);

    if (m_physicalDevice == VK_NULL_HANDLE) {
        VNLogError("failed to find a suitable GPU from %d devices", deviceCount);
        return false;
    }

    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);
    VNLogDebugF("Using device: %s", m_physicalDeviceProperties.deviceName);

    return true;
}

bool BackendVulkan::createLogicalDeviceAndQueues()
{
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice, m_renderingEnabled);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.computeFamily.value()};

    if (m_renderingEnabled) {
        uniqueQueueFamilies.insert(indices.graphicsFamily.value());
    }

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_FALSE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    // Device extensions - add VK_EXT_queue_family_foreign if available
    auto deviceExtensions = getRequiredDeviceExtensions();
    if (checkDeviceExtension(m_physicalDevice, "VK_EXT_queue_family_foreign")) {
        deviceExtensions.push_back("VK_EXT_queue_family_foreign");
    }
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (m_enableValidationLayers) {
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        deviceCreateInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) {
        VNLogError("failed to create logical device!");
        return false;
    }

    // Set up Vulkan memory allocator
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorCreateInfo.physicalDevice = m_physicalDevice;
    allocatorCreateInfo.device = m_device;
    allocatorCreateInfo.instance = m_instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    vmaCreateAllocator(&allocatorCreateInfo, &m_allocator);

    vkGetDeviceQueue(m_device, indices.computeFamily.value(), 0, &m_computeQueue);
    if (m_renderingEnabled) {
        vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    }

    m_indices = indices;

    return true;
}

bool BackendVulkan::finalisePresentQueueAfterSurface()
{
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice, true, m_render.getSurface());

    if (!indices.presentFamily.has_value()) {
        VNLogError("Picked device cannot present to this surface.");
        return false;
    }

    if (indices.presentFamily.value() != indices.graphicsFamily.value()) {
        VNLogError("Present family differs from graphics family; "
                   "device was not created with present family. Need device recreation.");
        return false;
    }

    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);

    m_indices = indices;

    return true;
}

bool BackendVulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties,
                                   uint32_t& memoryTypeIndex)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            return true;
        }
    }
    VNLogError("failed to find suitable memory type!");
    return false;
}

bool BackendVulkan::initRemainingRender()
{
    // debug - window created here now we can continue render setup
    if (m_renderingEnabled) {
        m_render.initWindow();
    }

    if (m_renderingEnabled && m_render.windowReady()) {
#if VN_OS(ANDROID)
        int width = 0;
        int height = 0;
        m_render.getWindowSize(width, height);
        if (width == 0 || height == 0) {
            VNLogDebug("Skipping render init: surface size %dx%d", width, height);
            return false;
        }
#endif
        if (!m_render.isInitialised()) {
            // finish render setup
            if (!m_render.createSurface()) {
                return false;
            }

            if (!finalisePresentQueueAfterSurface()) {
                return false;
            }

            if (!m_render.init()) {
                return false;
            }
        } else if (m_render.needsSurfaceRecreate()) {
            if (!m_render.resetSurfaceAndSwapChain()) {
                return false;
            }
            if (!finalisePresentQueueAfterSurface()) {
                return false;
            }
        }
    }

    return true;
}

bool BackendVulkan::initVulkan()
{
    if (m_renderingEnabled && !m_render.initWindow(false)) {
        return false;
    }

    if (!createInstance()) {
        return false;
    }

    if (!setupDebugMessenger()) {
        return false;
    }

    if (m_renderingEnabled && m_render.windowReady()) {
        if (!m_render.createSurface()) {
            return false;
        }
    }

    if (!pickPhysicalDevice()) {
        return false;
    }

    if (!createLogicalDeviceAndQueues()) {
        return false;
    }

    if (!m_compute.init()) {
        return false;
    }

    return true;
}

} // namespace lcevc_dec::pipeline_vulkan
