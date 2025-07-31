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

#include "pipeline_vulkan.h"
//
#include "apply.h"
#include "blit.h"
#include "conversion.h"
#include "frame_vulkan.h"
#include "picture_vulkan.h"
#include "pipeline_builder_vulkan.h"
#include "upscale_horizontal.h"
#include "upscale_vertical.h"
//
#include <LCEVC/common/check.h>
#include <LCEVC/common/constants.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/printf_macros.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/pixel_processing/blit.h> // TODO - remove
//
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <type_traits>

namespace lcevc_dec::pipeline_vulkan {

struct PushConstants
{
    int kernel[4];
    int srcWidth;
    int srcHeight;
    int pa;
    int containerStrideIn;
    int containerOffsetIn;
    int containerStrideOut;
    int containerOffsetOut;
    int containerStrideBase;
    int containerOffsetBase;
};

struct PushConstantsApply
{
    int srcWidth;
    int srcHeight;
    int residualOffset;
    int stride;
    int saturate;
    int testVal;
    int layerCount;
    int tuRasterOrder;
};

struct PushConstantsConversion
{
    int width;
    int containerStrideIn;
    int containerOffsetIn;
    int containerStrideOut;
    int containerOffsetOut;
    int containerStrideV; // used to check for nv12 and as input or output stride for V-plane
    int containerOffsetV; // used with nv12 as input or output offset for V-plane
    int bit8;
    int toInternal;
    int shift; // 5 for 10bit, 3 for 12bit, and 1 for 14bit
};

struct PushConstantsBlit
{
    int width;
    int height;
    int batchSize; // number of elements to process in each shader invocation
};

// Utility functions for finding things in Arrays
//
namespace {
    // Compare 'close' timestamps - allows wrapping around end of uint64_t range
    // (Unlikely when starting at zero - but allows timestamps to start 'before' zero)
    inline int compareTimestamps(uint64_t lhs, uint64_t rhs)
    {
        const int64_t delta = (int64_t)(lhs - rhs);
        if (delta < 0) {
            return -1;
        }
        if (delta > 0) {
            return 1;
        }
        return 0;
    }

    inline int findFrameTimestamp(const void* element, const void* ptr)
    {
        const auto* alloc{static_cast<const LdcMemoryAllocation*>(element)};
        assert(VNIsAllocated(*alloc));
        const uint64_t ets{VNAllocationPtr(*alloc, FrameVulkan)->timestamp};
        const uint64_t ts{*static_cast<const uint64_t*>(ptr)};

        return compareTimestamps(ets, ts);
    }

    inline int sortFramePtrTimestamp(const void* lhs, const void* rhs)
    {
        const auto* frameLhs{*static_cast<const FrameVulkan* const *>(lhs)};
        const auto* frameRhs{*static_cast<const FrameVulkan* const *>(rhs)};

        return compareTimestamps(frameLhs->timestamp, frameRhs->timestamp);
    }

    inline int findBasePictureTimestamp(const void* element, const void* ptr)
    {
        const auto* alloc{static_cast<const LdcMemoryAllocation*>(element)};
        assert(VNIsAllocated(*alloc));
        const uint64_t ets{VNAllocationPtr(*alloc, BasePicture)->timestamp};
        const uint64_t ts{*static_cast<const uint64_t*>(ptr)};

        return compareTimestamps(ets, ts);
    }

    inline void getSubsamplingShifts(LdeChroma chroma, int& widthShift, int& heightShift)
    {
        widthShift = 0;
        heightShift = 0;
        switch (chroma) {
            case LdeChroma::CT420: heightShift = 1; [[fallthrough]];
            case LdeChroma::CT422: widthShift = 1; break;
            default: break;
        }
    }

    template <typename, typename = std::void_t<>>
    struct HasContainerBase : std::false_type
    {};

    template <typename T>
    struct HasContainerBase<T, std::void_t<decltype(std::declval<T>().containerStrideBase),
                                           decltype(std::declval<T>().containerOffsetBase)>> : std::true_type
    {};

    template <typename T>
    inline void setContainerStrides(T& constants, int index, PictureVulkan* srcPicture,
                                    PictureVulkan* dstPicture, PictureVulkan* basePicture)
    {
        constants.containerStrideIn = srcPicture->layout.rowStrides[index] >> 2;
        constants.containerOffsetIn = srcPicture->layout.planeOffsets[index] >> 2;
        constants.containerStrideOut = dstPicture->layout.rowStrides[index] >> 2;
        constants.containerOffsetOut = dstPicture->layout.planeOffsets[index] >> 2;
        if constexpr (HasContainerBase<T>::value) {
            if (basePicture) {
                constants.containerStrideBase = basePicture->layout.rowStrides[index] >> 2;
                constants.containerOffsetBase = basePicture->layout.planeOffsets[index] >> 2;
            }
        }
    }
} // namespace

VKAPI_ATTR VkBool32 VKAPI_CALL
PipelineVulkan::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
{
    COUT_STR(std::string("validation layer: ").append(std::string(pCallbackData->pMessage)));

    return VK_FALSE;
}

bool PipelineVulkan::checkValidationLayerSupport()
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

std::vector<const char*> PipelineVulkan::getRequiredExtensions()
{
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void PipelineVulkan::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
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

bool PipelineVulkan::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        VNLogError("validation layers requested, but not available!");
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Upscale";
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
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

VkResult PipelineVulkan::CreateDebugUtilsMessengerEXT(VkInstance instance,
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

void PipelineVulkan::DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                                   VkDebugUtilsMessengerEXT debugMessenger,
                                                   const VkAllocationCallbacks* pAllocator)
{
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

bool PipelineVulkan::setupDebugMessenger()
{
    if (!enableValidationLayers) {
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

bool PipelineVulkan::isDeviceSuitable(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties gpuProperties;
    vkGetPhysicalDeviceProperties(device, &gpuProperties);

    const auto realGpu = gpuProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                         gpuProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

    if (!realGpu) {
        return false;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            m_queueFamilyIndex = i;
            return true;
        }
        i++;
    }

    return false;
}

VkPhysicalDevice PipelineVulkan::pickBestDevice(const std::vector<VkPhysicalDevice>& devices)
{
    if (devices.empty()) {
        return VK_NULL_HANDLE;
    }

    std::vector<std::pair<VkPhysicalDevice, uint32_t>> scoredList;
    for (const auto& device : devices) {
        uint32_t score = 0; // TODO - needs to be tuned

        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        VNLogDebug("Suitable compute device: %s", deviceProperties.deviceName);

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

        VkPhysicalDeviceVulkan13Properties vulkan13properties{};
        vulkan13properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
        vulkan13properties.pNext = nullptr;

        VkPhysicalDeviceProperties2 deviceProperties2{};
        deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties2.pNext = &vulkan13properties;
        vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

        score += vulkan13properties.maxSubgroupSize;
        VNLogDebug("subgroupSize: %u", vulkan13properties.maxSubgroupSize);

        score += deviceProperties2.properties.limits.maxMemoryAllocationCount;
        VNLogDebug("maxMemoryAllocationCount: %u",
                   deviceProperties2.properties.limits.maxMemoryAllocationCount);

        scoredList.emplace_back(device, score);
    }

    std::sort(scoredList.begin(), scoredList.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    return scoredList.front().first;
}

bool PipelineVulkan::pickPhysicalDevice()
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
        if (isDeviceSuitable(device)) {
            suitableDevices.push_back(device);
        }
    }

    m_physicalDevice = pickBestDevice(suitableDevices);

    if (m_physicalDevice == VK_NULL_HANDLE) {
        VNLogError("failed to find a suitable GPU from %d devices", deviceCount);
        return false;
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    VNLogDebug("Using device: %s", deviceProperties.deviceName);

    return true;
}

bool PipelineVulkan::createLogicalDeviceAndQueue()
{
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    const auto queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    if (enableValidationLayers) {
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        deviceCreateInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) {
        VNLogError("failed to create logical device!");
        return false;
    }

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queueIntermediate);
    return true;
}

bool PipelineVulkan::createBindingsAndPipelineLayout(uint32_t numBuffers, uint32_t pushConstantsSize,
                                                     VkDescriptorSetLayout& setLayoutFunc,
                                                     VkPipelineLayout& pipelineLayoutFunc)
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

    if (vkCreateDescriptorSetLayout(m_device, &setLayoutCreateInfo, nullptr, &setLayoutFunc) != VK_SUCCESS) {
        VNLogError("failed to create descriptor set layout!");
        return false;
    }

    VkPushConstantRange range = {};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = pushConstantsSize;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &setLayoutFunc;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &range;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayoutFunc)) {
        VNLogError("failed to create pipeline layout");
        return false;
    }

    return true;
}

std::vector<char> PipelineVulkan::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        VNLogError("failed to open file!");
        return {};
    }

    const auto fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

bool PipelineVulkan::createComputePipeline(const unsigned char* shaderName, size_t shaderSize,
                                           VkPipelineLayout& layout, VkPipeline& pipe, int wgSize)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderName);
    shaderModuleCreateInfo.codeSize = shaderSize;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        VNLogError("failed to create shader module");
        return false;
    }

    VkSpecializationMapEntry specializationMapEntry[3];
    specializationMapEntry[0].constantID = 0;
    specializationMapEntry[0].offset = 0;
    specializationMapEntry[0].size = 4;
    specializationMapEntry[1].constantID = 1;
    specializationMapEntry[1].offset = 4;
    specializationMapEntry[1].size = 4;
    specializationMapEntry[2].constantID = 2;
    specializationMapEntry[2].offset = 8;
    specializationMapEntry[2].size = 4;

    int32_t specializationData[] = {wgSize, wgSize, 1};

    VkSpecializationInfo specializationInfo;
    specializationInfo.mapEntryCount = 3;
    specializationInfo.pMapEntries = specializationMapEntry;
    specializationInfo.dataSize = 12;
    specializationInfo.pData = specializationData;

    VkComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module = shaderModule;
    pipelineCreateInfo.stage.pSpecializationInfo = &specializationInfo;

    pipelineCreateInfo.stage.pName = "main";
    pipelineCreateInfo.layout = layout;

    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipe) !=
        VK_SUCCESS) {
        VNLogError("failed to create compute pipeline");
        return false;
    }
    vkDestroyShaderModule(m_device, shaderModule, nullptr);

    return true;
}

uint32_t PipelineVulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    VNLogError("failed to find suitable memory type!");
    return 0;
}

bool PipelineVulkan::allocateDescriptorSets()
{
    constexpr int descriptorSetCount = 5; // This is how many VkDescriptorSet data members total
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = descriptorSetCount;

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    /*
    This is how many descriptors (buffers) total
    Vertical shader has 2 (input and output)
    Horizontal has 3 (input, output, and base)
    Apply has 2 (command buffer and output plane)
    Conversion has 2 (input buffer and output buffer)
    Blit has 2 (input buffer and output buffer)
    */
    poolSize.descriptorCount = 11;

    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        VNLogError("failed to create descriptor pool");
        return false;
    }

    VkDescriptorSetLayout setLayouts[descriptorSetCount];
    setLayouts[0] = m_setLayoutVertical;
    setLayouts[1] = m_setLayoutHorizontal;
    setLayouts[2] = m_setLayoutApply;
    setLayouts[3] = m_setLayoutConversion;
    setLayouts[4] = m_setLayoutBlit;

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
    descriptorSetAllocateInfo.pSetLayouts = setLayouts;

    VkDescriptorSet descriptorSets[descriptorSetCount];
    if (vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, descriptorSets) != VK_SUCCESS) {
        VNLogError("failed to allocate descriptor sets");
        return false;
    }

    m_descriptorSetSrcMid = descriptorSets[0];
    m_descriptorSetMidDst = descriptorSets[1];
    m_descriptorSetApply = descriptorSets[2];
    m_descriptorSetConversion = descriptorSets[3];
    m_descriptorSetBlit = descriptorSets[4];

    return true;
}

void PipelineVulkan::updateComputeDescriptorSets(VkBuffer src, VkBuffer dst, VkDescriptorSet descriptorSet)
{
    std::vector<VkWriteDescriptorSet> descriptorSetWrites(2);
    std::vector<VkDescriptorBufferInfo> bufferInfos(2);

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    VkDescriptorBufferInfo buffInfo{};
    buffInfo.offset = 0;
    buffInfo.range = VK_WHOLE_SIZE;

    buffInfo.buffer = src;
    bufferInfos[0] = buffInfo;
    buffInfo.buffer = dst;
    bufferInfos[1] = buffInfo;

    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.pBufferInfo = &bufferInfos[0];
    descriptorSetWrites[0] = writeDescriptorSet;
    writeDescriptorSet.dstBinding = 1;
    writeDescriptorSet.pBufferInfo = &bufferInfos[1];
    descriptorSetWrites[1] = writeDescriptorSet;

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorSetWrites.size()),
                           descriptorSetWrites.data(), 0, nullptr);
}

void PipelineVulkan::updateComputeDescriptorSets(VkBuffer src, VkBuffer dst1, VkBuffer dst2,
                                                 VkDescriptorSet descriptorSet)
{
    std::vector<VkWriteDescriptorSet> descriptorSetWrites(3);
    std::vector<VkDescriptorBufferInfo> bufferInfos(3);

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    VkDescriptorBufferInfo buffInfo{};
    buffInfo.offset = 0;
    buffInfo.range = VK_WHOLE_SIZE;

    buffInfo.buffer = src;
    bufferInfos[0] = buffInfo;
    buffInfo.buffer = dst1;
    bufferInfos[1] = buffInfo;
    buffInfo.buffer = dst2;
    bufferInfos[2] = buffInfo;

    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.pBufferInfo = &bufferInfos[0];
    descriptorSetWrites[0] = writeDescriptorSet;
    writeDescriptorSet.dstBinding = 1;
    writeDescriptorSet.pBufferInfo = &bufferInfos[1];
    descriptorSetWrites[1] = writeDescriptorSet;
    writeDescriptorSet.dstBinding = 2;
    writeDescriptorSet.pBufferInfo = &bufferInfos[2];
    descriptorSetWrites[2] = writeDescriptorSet;

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorSetWrites.size()),
                           descriptorSetWrites.data(), 0, nullptr);
}

bool PipelineVulkan::createCommandPoolAndBuffer()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;

    if (vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        VNLogError("failed to create command pool");
        return false;
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to allocate command buffer");
        return false;
    }

    if (vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_commandBufferIntermediate) !=
        VK_SUCCESS) {
        VNLogError("failed to allocate command buffer");
        return false;
    }

    return true;
}

bool PipelineVulkan::init()
{
    if (!createInstance()) {
        return false;
    }

    if (!setupDebugMessenger()) {
        return false;
    }

    if (!pickPhysicalDevice()) {
        return false;
    }

    if (!createLogicalDeviceAndQueue()) {
        return false;
    }

    if (!createBindingsAndPipelineLayout(2, sizeof(PushConstants), m_setLayoutVertical,
                                         m_pipelineLayoutVertical)) {
        return false;
    }

    if (!createBindingsAndPipelineLayout(3, sizeof(PushConstants), m_setLayoutHorizontal,
                                         m_pipelineLayoutHorizontal)) {
        return false;
    }

    if (!createBindingsAndPipelineLayout(2, sizeof(PushConstantsApply), m_setLayoutApply,
                                         m_pipelineLayoutApply)) {
        return false;
    }

    if (!createBindingsAndPipelineLayout(2, sizeof(PushConstantsConversion), m_setLayoutConversion,
                                         m_pipelineLayoutConversion)) {
        return false;
    }

    if (!createBindingsAndPipelineLayout(2, sizeof(PushConstantsBlit), m_setLayoutBlit,
                                         m_pipelineLayoutBlit)) {
        return false;
    }

    if (!createComputePipeline(upscale_vertical_spv, sizeof(upscale_vertical_spv),
                               m_pipelineLayoutVertical, m_pipelineVertical, workGroupSize)) {
        return false;
    }

    if (!createComputePipeline(upscale_horizontal_spv, sizeof(upscale_horizontal_spv),
                               m_pipelineLayoutHorizontal, m_pipelineHorizontal, workGroupSize)) {
        return false;
    }

    if (!createComputePipeline(apply_spv, sizeof(apply_spv), m_pipelineLayoutApply, m_pipelineApply,
                               workGroupSizeDebug)) {
        return false; // TODO - check this
    }

    if (!createComputePipeline(conversion_spv, sizeof(conversion_spv), m_pipelineLayoutConversion,
                               m_pipelineConversion, workGroupSizeDebug)) {
        return false; // TODO - check this
    }

    if (!createComputePipeline(blit_spv, sizeof(blit_spv), m_pipelineLayoutBlit, m_pipelineBlit,
                               workGroupSizeDebug)) {
        return false; // TODO - check this
    }

    if (!allocateDescriptorSets()) {
        return false;
    }

    if (!createCommandPoolAndBuffer()) {
        return false;
    }

    return true;
}

void PipelineVulkan::dispatchCompute(int width, int height, VkCommandBuffer& cmdBuf, int wgSize, int packDensity)
{
    const int modX = width % (packDensity * wgSize);
    const int divX = width / (packDensity * wgSize);
    const int numGroupsX = modX ? divX + 1 : divX;
    const int modY = height % wgSize;
    const int divY = height / wgSize;
    const int numGroupsY = modY ? divY + 1 : divY;
    vkCmdDispatch(cmdBuf, numGroupsX, numGroupsY, 1);
}

bool PipelineVulkan::blit(VulkanBlitArgs* params)
{
    auto* srcPicture = params->src;
    auto* srcBuffer = static_cast<BufferVulkan*>(srcPicture->buffer);
    VkBuffer& srcVkBuffer = srcBuffer->getVkBuffer();

    auto* dstPicture = params->dst;
    auto* dstBuffer = static_cast<BufferVulkan*>(dstPicture->buffer);
    VkBuffer& dstVkBuffer = dstBuffer->getVkBuffer();

    LdpPictureDesc srcDesc{};
    srcPicture->getDesc(srcDesc);

    PushConstantsBlit constants{};
    constants.width = srcDesc.width;
    constants.height = srcDesc.height;
    constants.batchSize = 16;

    updateComputeDescriptorSets(srcVkBuffer, dstVkBuffer, m_descriptorSetBlit);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineBlit);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayoutBlit,
                            0, 1, &m_descriptorSetBlit, 0, nullptr);
    vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutBlit, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstantsBlit), &constants);

    dispatchCompute(constants.width, constants.height, m_commandBuffer, workGroupSizeDebug);

    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to end command buffer");
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    auto result = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        VNLogError("Failed to submit compute queue!");
        return false;
    }

    vkDeviceWaitIdle(m_device);

    return true;
}

bool PipelineVulkan::conversion(VulkanConversionArgs* params)
{
    auto* srcPicture = params->src;
    auto* srcBuffer = static_cast<BufferVulkan*>(srcPicture->buffer);
    VkBuffer& srcVkBuffer = srcBuffer->getVkBuffer();

    auto* dstPicture = params->dst;
    auto* dstBuffer = static_cast<BufferVulkan*>(dstPicture->buffer);
    VkBuffer& dstVkBuffer = dstBuffer->getVkBuffer();

    PushConstantsConversion constants{};
    LdpPictureDesc srcDesc{}, dstDesc{};
    srcPicture->getDesc(srcDesc);
    dstPicture->getDesc(dstDesc);

    auto isBit8 = [](LdpColorFormat colorFormat) {
        return colorFormat == LdpColorFormatI444_8 || colorFormat == LdpColorFormatI422_8 ||
               colorFormat == LdpColorFormatI420_8 || colorFormat == LdpColorFormatGRAY_8 ||
               colorFormat == LdpColorFormatNV12_8 || colorFormat == LdpColorFormatNV21_8;
    };
    const auto srcBit8 = isBit8(srcDesc.colorFormat);
    const auto dstBit8 = isBit8(dstDesc.colorFormat);

    constants.width = srcPicture->layout.width;
    auto height = srcPicture->layout.height;
    constants.bit8 = params->toInternal ? static_cast<int>(srcBit8) : static_cast<int>(dstBit8);
    constants.toInternal = params->toInternal ? 1 : 0;
    constants.shift = m_shift;

    setContainerStrides(constants, 0, srcPicture, dstPicture, nullptr);

    constants.containerStrideV = 0; // used to signal no nv12
    constants.containerOffsetV = 0;

    updateComputeDescriptorSets(srcVkBuffer, dstVkBuffer, m_descriptorSetConversion);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineConversion);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_pipelineLayoutConversion, 0, 1, &m_descriptorSetConversion, 0, nullptr);
    vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutConversion, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstantsConversion), &constants);

    const int packDensity = (srcBit8 || dstBit8) ? 4 : 2;
    dispatchCompute(constants.width, height, m_commandBuffer, workGroupSize, packDensity); // Y

    if (m_chroma != LdeChroma::CTMonochrome) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(m_chroma, widthShift, heightShift);

        const auto srcNv12 = srcDesc.colorFormat == LdpColorFormatNV12_8;
        const auto dstNv12 = dstDesc.colorFormat == LdpColorFormatNV12_8;
        const auto srcNv21 = srcDesc.colorFormat == LdpColorFormatNV21_8;
        const auto dstNv21 = dstDesc.colorFormat == LdpColorFormatNV21_8;
        const auto nv12 = srcNv12 || srcNv21 || dstNv12 || dstNv21;
        if (!nv12) {
            constants.width = srcPicture->layout.width >> widthShift;
        } else {
            // for nv12 src, these will be used as V-plane outputs, otherwise V-plane inputs
            constants.containerStrideV = srcNv12 ? dstPicture->layout.rowStrides[2] >> 2
                                                 : srcPicture->layout.rowStrides[2] >> 2;
            constants.containerOffsetV = srcNv12 ? dstPicture->layout.planeOffsets[2] >> 2
                                                 : srcPicture->layout.planeOffsets[2] >> 2;
        }
        height = srcPicture->layout.height >> heightShift;
        setContainerStrides(constants, 1, srcPicture, dstPicture, nullptr);

        vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutConversion, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(PushConstantsConversion), &constants);
        dispatchCompute(constants.width, height, m_commandBuffer, workGroupSize, packDensity); // U

        if (!nv12) {
            setContainerStrides(constants, 2, srcPicture, dstPicture, nullptr);
            vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutConversion, VK_SHADER_STAGE_COMPUTE_BIT,
                               0, sizeof(PushConstantsConversion), &constants);
            dispatchCompute(constants.width, height, m_commandBuffer, workGroupSize, packDensity); // V
        }
    }

    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to end command buffer");
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    auto result = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        VNLogError("Failed to submit compute queue!");
        return false;
    }

    vkDeviceWaitIdle(m_device);

    return true;
}

bool PipelineVulkan::upscaleVertical(const LdeKernel* kernel, VulkanUpscaleArgs* params)
{
    PictureVulkan* srcPicture = params->src;
    PictureVulkan* dstPicture = params->dst;

    LdpPictureDesc srcDesc;
    srcPicture->getDesc(srcDesc);

    // Get VkBuffers to update descriptor sets (attach buffers to shaders)
    auto* srcBuffer = static_cast<BufferVulkan*>(srcPicture->buffer);
    VkBuffer& srcVkBuffer = srcBuffer->getVkBuffer();

    auto* dstBuffer = static_cast<BufferVulkan*>(dstPicture->buffer);
    VkBuffer& dstVkBuffer = dstBuffer->getVkBuffer();

    // Vertical set for upscaling from base and writing to intermediate
    updateComputeDescriptorSets(srcVkBuffer, dstVkBuffer, m_descriptorSetSrcMid);

    PushConstants constants{};
    if (kernel->length == 2) {
        constants.kernel[0] = 0;
        constants.kernel[1] = kernel->coeffs[0][0];
        constants.kernel[2] = kernel->coeffs[0][1];
        constants.kernel[3] = 0;
    } else {
        for (int i = 0; i < 4; ++i) {
            constants.kernel[i] = kernel->coeffs[0][i];
        }
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_commandBufferIntermediate, &beginInfo);

    VkMemoryBarrier memoryBarrierIntermediate{};
    memoryBarrierIntermediate.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrierIntermediate.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    memoryBarrierIntermediate.dstAccessMask = VK_ACCESS_NONE;
    vkCmdPipelineBarrier(m_commandBufferIntermediate,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1, &memoryBarrierIntermediate, 0,
                         nullptr, 0, nullptr);

    constants.srcWidth = srcDesc.width;
    constants.srcHeight = srcDesc.height;
    constants.pa = 0;
    setContainerStrides(constants, 0, srcPicture, dstPicture, nullptr);

    vkCmdBindPipeline(m_commandBufferIntermediate, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineVertical);
    vkCmdBindDescriptorSets(m_commandBufferIntermediate, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_pipelineLayoutVertical, 0, 1, &m_descriptorSetSrcMid, 0, nullptr);
    vkCmdPushConstants(m_commandBufferIntermediate, m_pipelineLayoutVertical,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &constants);
    dispatchCompute(constants.srcWidth, constants.srcHeight, m_commandBufferIntermediate,
                    workGroupSize); // Y

    if (m_chroma != LdeChroma::CTMonochrome) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(m_chroma, widthShift, heightShift);

        constants.srcWidth = srcDesc.width >> widthShift;
        constants.srcHeight = srcDesc.height >> heightShift;
        setContainerStrides(constants, 1, srcPicture, dstPicture, nullptr);

        vkCmdPushConstants(m_commandBufferIntermediate, m_pipelineLayoutVertical,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &constants);
        dispatchCompute(constants.srcWidth, constants.srcHeight, m_commandBufferIntermediate,
                        workGroupSize); // U

        setContainerStrides(constants, 2, srcPicture, dstPicture, nullptr);

        vkCmdPushConstants(m_commandBufferIntermediate, m_pipelineLayoutVertical,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &constants);
        dispatchCompute(constants.srcWidth, constants.srcHeight, m_commandBufferIntermediate,
                        workGroupSize); // V
    }

    if (vkEndCommandBuffer(m_commandBufferIntermediate) != VK_SUCCESS) {
        VNLogError("failed to end command buffer");
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBufferIntermediate;

    auto result = vkQueueSubmit(m_queueIntermediate, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        VNLogError("Failed to submit compute queue!");
        return false;
    }
    vkDeviceWaitIdle(m_device);

    return true;
}

bool PipelineVulkan::upscaleHorizontal(const LdeKernel* kernel, VulkanUpscaleArgs* params)
{
    PictureVulkan* srcPicture = params->src;
    PictureVulkan* dstPicture = params->dst;
    PictureVulkan* basePicture = params->base;

    assert(srcPicture);
    assert(dstPicture);
    assert(basePicture);

    LdpPictureDesc srcDesc;
    srcPicture->getDesc(srcDesc);

    // Get VkBuffers to update descriptor sets (attach buffers to shaders)
    auto* srcBuffer = static_cast<BufferVulkan*>(srcPicture->buffer);
    assert(srcBuffer);
    VkBuffer& srcVkBuffer = srcBuffer->getVkBuffer();

    auto* baseBuffer = static_cast<BufferVulkan*>(basePicture->buffer);
    assert(baseBuffer);
    VkBuffer& baseVkBuffer = baseBuffer->getVkBuffer();

    auto* dstBuffer = static_cast<BufferVulkan*>(dstPicture->buffer);
    assert(dstBuffer);
    VkBuffer& dstVkBuffer = dstBuffer->getVkBuffer();

    // Horizontal set for upscaling from intermediate, applying PA from base, and writing to output
    updateComputeDescriptorSets(srcVkBuffer, dstVkBuffer, baseVkBuffer, m_descriptorSetMidDst);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    PushConstants constants{};
    if (kernel->length == 2) {
        constants.kernel[0] = 0;
        constants.kernel[1] = kernel->coeffs[0][0];
        constants.kernel[2] = kernel->coeffs[0][1];
        constants.kernel[3] = 0;
    } else {
        for (int i = 0; i < 4; ++i) {
            constants.kernel[i] = kernel->coeffs[0][i];
        }
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBufferIntermediate;

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                         nullptr);

    constants.srcWidth = srcDesc.width;
    constants.srcHeight = srcDesc.height;
    constants.pa = params->applyPA;

    setContainerStrides(constants, 0, srcPicture, dstPicture, basePicture);

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineHorizontal);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_pipelineLayoutHorizontal, 0, 1, &m_descriptorSetMidDst, 0, nullptr);
    vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutHorizontal, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstants), &constants);
    dispatchCompute(constants.srcWidth, constants.srcHeight >> 1, m_commandBuffer,
                    workGroupSize); // Y. constants.srcHeight / 2 because we are doing 2 rows at a time for PA

    if (m_chroma != LdeChroma::CTMonochrome) {
        int widthShift{};
        int heightShift{};
        getSubsamplingShifts(m_chroma, widthShift, heightShift);

        constants.srcWidth = srcDesc.width >> widthShift;
        constants.srcHeight = srcDesc.height >> heightShift;

        setContainerStrides(constants, 1, srcPicture, dstPicture, basePicture);

        vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutHorizontal, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(PushConstants), &constants);
        dispatchCompute(constants.srcWidth, constants.srcHeight, m_commandBuffer, workGroupSize); // U

        setContainerStrides(constants, 2, srcPicture, dstPicture, basePicture);

        vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutHorizontal, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(PushConstants), &constants);
        dispatchCompute(constants.srcWidth, constants.srcHeight, m_commandBuffer,
                        workGroupSize); // V
    }

    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to end command buffer");
        return false;
    }

    submitInfo.pCommandBuffers = &m_commandBuffer;
    auto result = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        VNLogError("Failed to submit compute queue!");
        return false;
    }

    vkDeviceWaitIdle(m_device);

    return true;
}

bool PipelineVulkan::upscaleFrame(const LdeKernel* kernel, VulkanUpscaleArgs* params)
{
    params->base = params->src;
    if (params->mode == Scale1D) {
        LdpPictureDesc dstDesc;
        params->src->getDesc(dstDesc);
        params->vertical ? dstDesc.height *= 2 : dstDesc.width *= 2;
        params->dst->functions->setDesc(params->dst, &dstDesc);
        params->applyPA = params->applyPA ? 1 : 0;

        params->vertical ? upscaleVertical(kernel, params) : upscaleHorizontal(kernel, params);
    } else if (params->mode == Scale2D) {
        PictureVulkan* output = params->dst;
        if (params->loq1 && m_firstFrame[LOQ1]) {
            LdpPictureDesc intermediateDesc;
            params->src->getDesc(intermediateDesc);
            intermediateDesc.height *= 2;
            m_intermediateUpscalePicture[LOQ1] = new PictureVulkan(*this);
            m_intermediateUpscalePicture[LOQ1]->setDesc(intermediateDesc);
        }
        if (!params->loq1 && m_firstFrame[LOQ0]) {
            LdpPictureDesc intermediateDesc;
            params->src->getDesc(intermediateDesc);
            intermediateDesc.height *= 2;
            m_intermediateUpscalePicture[LOQ0] = new PictureVulkan(*this);
            m_intermediateUpscalePicture[LOQ0]->setDesc(intermediateDesc);
        }
        params->dst = (params->loq1) ? m_intermediateUpscalePicture[LOQ1]
                                     : m_intermediateUpscalePicture[LOQ0];
        upscaleVertical(kernel, params);

        LdpPictureDesc outputDesc{};
        params->src->getDesc(outputDesc);
        outputDesc.width *= 2;
        outputDesc.height *= 2;
        output->functions->setDesc(output, &outputDesc);

        params->src = params->dst;
        params->dst = output;
        params->applyPA = params->applyPA ? 2 : 0;
        upscaleHorizontal(kernel, params);
    }

    if (params->loq1)
        m_firstFrame[LOQ1] = false;
    else
        m_firstFrame[LOQ0] = false;

    return true;
}

bool PipelineVulkan::apply(VulkanApplyArgs* params)
{
    const LdeCmdBufferGpu buffer = params->bufferGpu;
    const bool applyTemporal = (params->plane == nullptr) ? true : false;

    // TODO - swap firstApply bool for config pool or cache to handle dynamic frame changes
    if (applyTemporal && m_firstApply) {
        m_firstApply = false;

        LdpPictureDesc desc{};
        desc.width = params->planeWidth;
        desc.height = params->planeHeight;
        desc.colorFormat = LdpColorFormatGRAY_16_LE;
        m_temporalPicture = new PictureVulkan(*this);
        m_temporalPicture->setDesc(desc);
        auto* temporalBuffer = static_cast<BufferVulkan*>(m_temporalPicture->buffer);
        std::memset(temporalBuffer->ptr(), 0, temporalBuffer->size());
    }

    // TODO temp copy to Vulkan buffer
    const unsigned residualOffset = sizeof(LdeCmdBufferGpuCmd) * buffer.commandCount;
    uint32_t size = residualOffset + buffer.residualCount * sizeof(uint16_t);
    if (size == 0) { // TODO - check this. No commands but possibly still temporal refresh
        return true;
    }

    auto gpuCommandBuffer = std::make_unique<BufferVulkan>(*this, size);
    std::memcpy(gpuCommandBuffer->ptr(), buffer.commands, residualOffset);
    std::memcpy(gpuCommandBuffer->ptr() + residualOffset, buffer.residuals,
                buffer.residualCount * sizeof(uint16_t));

    auto* applyPlaneBuffer = applyTemporal ? static_cast<BufferVulkan*>(m_temporalPicture->buffer)
                                           : static_cast<BufferVulkan*>(params->plane->buffer);

    updateComputeDescriptorSets(gpuCommandBuffer->getVkBuffer(), applyPlaneBuffer->getVkBuffer(),
                                m_descriptorSetApply);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    PushConstantsApply constants{};

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    constants.srcWidth = applyTemporal ? m_temporalPicture->layout.width : params->plane->layout.width;
    constants.srcHeight = applyTemporal ? m_temporalPicture->layout.height : params->plane->layout.height;
    constants.stride = applyTemporal ? m_temporalPicture->layout.rowStrides[0] >> 1
                                     : params->plane->layout.rowStrides[0] >> 1;
    constants.residualOffset = residualOffset;
    constants.saturate = params->highlightResiduals ? 1 : 0;
    constants.testVal = 0;
    constants.layerCount = buffer.layerCount;
    constants.tuRasterOrder = params->tuRasterOrder ? 1 : 0;
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineApply);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayoutApply,
                            0, 1, &m_descriptorSetApply, 0, nullptr);
    vkCmdPushConstants(m_commandBuffer, m_pipelineLayoutApply, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstantsApply), &constants);
    vkCmdDispatch(m_commandBuffer, buffer.commandCount, 1, 1); // TODO - check this

    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to end command buffer");
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    const VkResult result = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        VNLogError("Failed to submit compute apply queue!");
        return false;
    }

    vkDeviceWaitIdle(m_device);

    return true;
}

void PipelineVulkan::destroy()
{
    for (int i = 0; i < NUM_PLANES; ++i) {
#if defined(ANDROID_BUFFERS)
        AHardwareBuffer_release(srcHardwareBuffer[i]);
        AHardwareBuffer_release(midHardwareBuffer[i]);
        AHardwareBuffer_release(dstHardwareBuffer[i]);
#endif
    }

    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffer);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBufferIntermediate);
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    vkDestroyPipeline(m_device, m_pipelineVertical, nullptr);
    vkDestroyPipeline(m_device, m_pipelineHorizontal, nullptr);
    vkDestroyPipeline(m_device, m_pipelineApply, nullptr);
    vkDestroyPipeline(m_device, m_pipelineConversion, nullptr);
    vkDestroyPipeline(m_device, m_pipelineBlit, nullptr);

    vkDestroyPipelineLayout(m_device, m_pipelineLayoutVertical, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayoutHorizontal, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayoutApply, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayoutConversion, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayoutBlit, nullptr);

    vkDestroyDescriptorSetLayout(m_device, m_setLayoutVertical, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_setLayoutHorizontal, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_setLayoutApply, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_setLayoutConversion, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_setLayoutBlit, nullptr);

    vkDestroyDevice(m_device, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }

    vkDestroyInstance(m_instance, nullptr);

    for (int i = 0; i < LOQEnhancedCount; ++i) {
        if (m_intermediateUpscalePicture[i]) {
            delete m_intermediateUpscalePicture[i];
        }
    }
    delete m_temporalPicture;
}

PipelineVulkan::PipelineVulkan(const PipelineBuilderVulkan& builder, pipeline::EventSink* eventSink)
    : m_physicalDevice(VK_NULL_HANDLE)
    , m_queueFamilyIndex(0)
    , m_device(VK_NULL_HANDLE)
    , m_configuration(builder.configuration())
    , m_eventSink(eventSink ? eventSink : pipeline::EventSink::nullSink())
    , m_allocator(builder.allocator())
    , m_buffers(builder.configuration().maxLatency, builder.allocator())
    , m_pictures(builder.configuration().maxLatency, builder.allocator())
    , m_frames(builder.configuration().maxLatency, builder.allocator())
    , m_reorderIndex(builder.configuration().maxLatency, builder.allocator())
    , m_processingIndex(builder.configuration().maxLatency, builder.allocator())
    , m_maxReorder(m_configuration.defaultMaxReorder)
    , m_temporalBuffers(builder.configuration().numTemporalBuffers * RCMaxPlanes, builder.allocator())
    , m_basePicturePending(nextPowerOfTwoU32(builder.configuration().maxLatency + 1), builder.allocator())
    , m_basePictureOutBuffer(nextPowerOfTwoU32(builder.configuration().maxLatency + 1), builder.allocator())
    , m_outputPictureAvailableBuffer(nextPowerOfTwoU32(builder.configuration().maxLatency + 1),
                                     builder.allocator())

{
    // Set up dithering
    ldppDitherGlobalInitialize(m_allocator, &m_dither, m_configuration.ditherSeed);

    // Set up an allocator for per frame data
    ldcRollingArenaInitialize(&m_rollingArena, m_allocator, m_configuration.initialArenaCount,
                              m_configuration.initialArenaSize);

    // Configuration pool
    LdeBitstreamVersion bitstreamVersion = BitstreamVersionUnspecified;
    if (m_configuration.forceBitstreamVersion >= BitstreamVersionInitial &&
        m_configuration.forceBitstreamVersion <= BitstreamVersionCurrent) {
        bitstreamVersion = static_cast<LdeBitstreamVersion>(m_configuration.forceBitstreamVersion);
    }
    ldeConfigPoolInitialize(m_allocator, &m_configPool, bitstreamVersion);

    // Start task pool - pool threads is 1 less than configured threads
    VNCheck(m_configuration.numThreads >= 1);
    ldcTaskPoolInitialize(&m_taskPool, m_allocator, m_allocator, m_configuration.numThreads - 1,
                          m_configuration.numReservedTasks);

    // Fill in empty temporal buffer anchors
    TemporalBuffer buf{};
    buf.desc.timestamp = kInvalidTimestamp;
    buf.timestampLimit = kInvalidTimestamp;
    for (uint32_t i = 0; i < m_configuration.numTemporalBuffers * RCMaxPlanes; ++i) {
        m_temporalBuffers.append(buf);
    }

    m_eventSink->generate(pipeline::EventCanSendEnhancement);
    m_eventSink->generate(pipeline::EventCanSendBase);
    m_eventSink->generate(pipeline::EventCanSendPicture);

    // Initialise vulkan state
    m_initialized = init();
}

PipelineVulkan::~PipelineVulkan()
{
    // Release pictures
    for (uint32_t i = 0; i < m_pictures.size(); ++i) {
        PictureVulkan* picture{VNAllocationPtr(m_pictures[i], PictureVulkan)};
        // Call destructor directly, as we are doing in-place construct/destruct
        picture->~PictureVulkan();
        VNFree(m_allocator, &m_pictures[i]);
    }

    // Release frames
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameVulkan* frame{VNAllocationPtr(m_frames[i], FrameVulkan)};
        frame->release();
        // Call destructor directly, as we are doing in-place construct/destruct
        frame->~FrameVulkan();
        VNFree(m_allocator, &m_frames[i]);
    }

    // Release any temporal buffers
    for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
        TemporalBuffer* tb = m_temporalBuffers.at(i);
        if (VNIsAllocated(tb->allocation)) {
            VNFree(m_allocator, &tb->allocation);
        }
    }
    // Release dither
    ldppDitherGlobalRelease(&m_dither);

    ldeConfigPoolRelease(&m_configPool);

    ldcRollingArenaDestroy(&m_rollingArena);

    // Close down task pool
    ldcTaskPoolDestroy(&m_taskPool);

    m_eventSink->generate(pipeline::EventExit);

    // Release vulkan objects
    if (m_initialized) {
        destroy();
    }
}

// Send/receive
LdcReturnCode PipelineVulkan::sendEnhancementData(uint64_t timestamp, const uint8_t* data, uint32_t byteSize)
{
    VNLogDebug("sendEnhancementData: %" PRIx64 " %d", timestamp, byteSize);
    VNTraceInstant("sendEnhancementData", timestamp);

    // Invalid if this timestamp is already present in decoder.
    //
    // NB: API clients are expected to make distinct timestamps over discontinuities using utility library
    if (findFrame(timestamp)) {
        return LdcReturnCodeInvalidParam;
    }

    if (frameLatency() >= m_configuration.maxLatency) {
        VNLogDebug("sendEnhancementData: %" PRIx64 " AGAIN", timestamp);
        return LdcReturnCodeAgain;
    }

    // New pending frame
    FrameVulkan* const frame{allocateFrame(timestamp)};
    if (!frame) {
        return LdcReturnCodeError;
    }

    LdcMemoryAllocation enhancementDataAllocation{};
    uint8_t* const enhancement{VNAllocateArray(m_allocator, &enhancementDataAllocation, uint8_t, byteSize)};
    memcpy(enhancement, data, byteSize);
    frame->m_enhancementData = enhancementDataAllocation;
    frame->m_state = FrameStateReorder;

    // Add frame to reorder table sorted by timestamp
    m_reorderIndex.insert(sortFramePtrTimestamp, frame);

    // Attach any pending base for matching timestamp
    if (BasePicture* bp = m_basePicturePending.findUnordered(findBasePictureTimestamp, &frame->timestamp);
        bp) {
        frame->setBase(bp->picture, bp->deadline, bp->userData);
        m_basePicturePending.remove(bp);
        m_eventSink->generate(pipeline::EventCanSendBase);
    }

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineVulkan::sendBasePicture(uint64_t timestamp, LdpPicture* basePicture,
                                              uint32_t timeoutUs, void* userData)
{
    VNLogDebug("sendBasePicture: %" PRIx64 " %p", timestamp, (void*)basePicture);
    VNTraceInstant("sendBasePicture", timestamp);

    // Find the frame associated with PTS
    FrameVulkan* frame{findFrame(timestamp)};
    if (frame) {
        // Enhancement exists
        if (LdcReturnCode ret = frame->setBase(
                basePicture, threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData);
            ret != LdcReturnCodeSuccess) {
            return ret;
        }

        // Kick off any frames that are at or before the base timestamp
        startProcessing(timestamp);
        m_eventSink->generate(pipeline::EventCanSendBase);
        return LdcReturnCodeSuccess;
    }

    BasePicture bp = {timestamp, basePicture,
                      threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData};

    if (m_basePicturePending.size() < m_configuration.enhancementDelay) {
        // Room to buffer picture
        m_basePicturePending.append(bp);
        return LdcReturnCodeSuccess;
    }

    // Cannot buffer any more pending bases
    if (m_configuration.passthroughMode == PassthroughMode::Disable) {
        // No pass-through
        return LdcReturnCodeAgain;
    }

    // Base frame is going to go through pipeline as some sort of pass-through ...
    if (!m_basePicturePending.isEmpty()) {
        m_basePicturePending.append(bp);
        bp = m_basePicturePending[0];
        m_basePicturePending.removeIndex(0);
        m_eventSink->generate(pipeline::EventCanSendBase);
    }

    // New pass-through frame - no enhancement
    FrameVulkan* const passFrame{allocateFrame(timestamp)};

    if (!passFrame) {
        return LdcReturnCodeError;
    }

    // Add frame to reorder table sorted by timestamp
    passFrame->m_state = FrameStateReorder;
    passFrame->m_ready = true;
    passFrame->m_passthrough = true;
    passFrame->setBase(basePicture, threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData);

    m_reorderIndex.insert(sortFramePtrTimestamp, passFrame);

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineVulkan::sendOutputPicture(LdpPicture* outputPicture)
{
    VNLogDebug("sendOutputPicture: %p", (void*)outputPicture);
    VNTraceInstant("sendOutputPicture", (void*)outputPicture);

    // Add to available queue
    if (m_outputPictureAvailableBuffer.size() > m_configuration.maxLatency ||
        !m_outputPictureAvailableBuffer.tryPush(outputPicture)) {
        VNLogDebug("sendOutputPicture: AGAIN");
        return LdcReturnCodeAgain;
    }

    connectOutputPictures();

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdpPicture* PipelineVulkan::receiveOutputPicture(LdpDecodeInformation& decodeInfoOut)
{
    FrameVulkan* frame{};

    // Pull any done frame from start (lowest timestamp) of 'processing' frame index.
    while (true) {
        common::ScopedLock lock(m_interTaskMutex);

        if (m_processingIndex.isEmpty()) {
            // No frames in progress
            break;
        }

        if (m_processingIndex[0]->m_state == FrameStateDone) {
            // Earliest frame is finished
            frame = m_processingIndex[0];
            m_processingIndex.removeIndex(0);
            break;
        }

        if (m_processingIndex[0]->canComplete()) {
            // Earliest frame will complete, so hang around and wait for it
            VNLogDebug("receiveOutputPicture waiting for %" PRIx64, m_processingIndex[0]->timestamp);

            if (m_interTaskFrameDone.waitDeadline(lock, m_processingIndex[0]->m_deadline)) {
                continue;
            }
            VNLogWarning("receiveOutputPicture wait timed out");
#ifdef VN_SDK_LOG_ENABLE_DEBUG
            ldcTaskPoolDump(&m_taskPool, nullptr);
#endif
        } else {
            break;
        }
    }

    if (!frame) {
        return nullptr;
    }

    // Copy surviving data from frame
    decodeInfoOut = frame->m_decodeInfo;
    LdpPicture* pictureOut{frame->outputPicture};

    VNLogDebug("receiveOutputPicture: %" PRIx64 " %p hb:%d he:%d sk:%d enh:%d",
               decodeInfoOut.timestamp, (void*)pictureOut, decodeInfoOut.hasBase,
               decodeInfoOut.hasEnhancement, decodeInfoOut.skipped, decodeInfoOut.enhanced);

    VNTraceInstant("receiveOutputPicture", frame->timestamp, (void*)frame->outputPicture);

    // Once an output picture has left the building - we can drop the associated frame
    freeFrame(frame);

    return pictureOut;
}

LdpPicture* PipelineVulkan::receiveFinishedBasePicture()
{
    // Is there anything in finished base FIFO?
    LdpPicture* basePicture{};
    if (!m_basePictureOutBuffer.tryPop(basePicture)) {
        return nullptr;
    }

    VNLogDebug("receiveFinishedBasePicture: %" PRIx64 " %p", (void*)basePicture);
    VNTraceInstant("receiveFinishedBasePicture", (void*)basePicture);

    return basePicture;
}

// Dig out info about a current timestamp
LdcReturnCode PipelineVulkan::peek(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut)
{
    // Flush everything up to given timestamp
    startProcessing(timestamp);

    // Find the frame associated with PTS
    const FrameVulkan* frame{findFrame(timestamp)};
    if (!frame) {
        return LdcReturnCodeNotFound;
    }
    if (!frame->globalConfig) {
        if (m_configuration.passthroughMode == PassthroughMode::Disable) {
            return LdcReturnCodeNotFound;
        } else {
            return LdcReturnCodeAgain;
        }
    }

    widthOut = frame->globalConfig->width;
    heightOut = frame->globalConfig->height;
    return LdcReturnCodeSuccess;
}

// Move any reorder frames at or before timestamp into processing state
void PipelineVulkan::startProcessing(uint64_t timestamp)
{
    // Mark any frames in reorder buffer as 'flush'
    for (uint32_t i = 0; i < m_reorderIndex.size(); ++i) {
        FrameVulkan* const frame = m_reorderIndex[i];
        if (frame->m_state == FrameStateReorder && compareTimestamps(frame->timestamp, timestamp) <= 0) {
            frame->m_ready = true;
        }
    }

    startReadyFrames();
}

// Make pending frames get decoded
LdcReturnCode PipelineVulkan::flush(uint64_t timestamp)
{
    VNLogDebug("flush: %" PRIx64 " %p", timestamp);
    VNTraceInstant("flush", timestamp);

    // Mark any frames in reorder buffer as 'flush'
    for (uint32_t i = 0; i < m_reorderIndex.size(); ++i) {
        FrameVulkan* const frame = m_reorderIndex[i];
        if (compareTimestamps(frame->timestamp, timestamp) <= 0) {
            // Mark frame as flushable
            frame->m_ready = true;
        }
    }

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

// Mark everything before timestamp as not needing decoding
LdcReturnCode PipelineVulkan::skip(uint64_t timestamp)
{
    VNLogDebug("skip: %" PRIx64 " %p", timestamp);
    VNTraceInstant("skip", timestamp);

    // Look at all frames
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameVulkan* const frame{VNAllocationPtr(m_frames[i], FrameVulkan)};
        if (compareTimestamps(frame->timestamp, timestamp) <= 0) {
            // Mark frame as skippable and flushable
            frame->m_skip = true;
            frame->m_ready = true;
        }
    }

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

// Wait for all work to be finished - optionally stopping anything in progress
LdcReturnCode PipelineVulkan::synchronize(bool dropPending)
{
    VNLogDebug("synchronize: %d", dropPending);
    VNTraceInstant("synchronize", dropPending);

    // Mark current frames as skippable
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameVulkan* const frame{VNAllocationPtr(m_frames[i], FrameVulkan)};
        frame->m_skip = dropPending;
    }
    startReadyFrames();

    // For all pending frames that are not blocked on input - wait in timestamp order
    for (uint32_t i = 0; i < m_processingIndex.size(); ++i) {
        FrameVulkan* frame = m_processingIndex[i];
        if (!frame->canComplete()) {
            continue;
        }
        ldcTaskGroupWait(&frame->m_taskGroup);
    }

    return LdcReturnCodeSuccess;
}

// Buffers
//
BufferVulkan* PipelineVulkan::allocateBuffer(uint32_t requiredSize)
{
    // Allocate buffer structure
    LdcMemoryAllocation allocation;
    BufferVulkan* const buffer{VNAllocateZero(m_allocator, &allocation, BufferVulkan)};
    if (!buffer) {
        return nullptr;
    }
    // Insert into table
    m_buffers.append(allocation);

    // In place construction
    return new (buffer) BufferVulkan(*this, requiredSize); // NOLINT(cppcoreguidelines-owning-memory)
}

void PipelineVulkan::releaseBuffer(BufferVulkan* buffer)
{
    assert(buffer);

    // Release buffer structure
    LdcMemoryAllocation* const pAlloc{m_buffers.findUnordered(ldcVectorCompareAllocationPtr, buffer)};

    if (!pAlloc) {
        // Could not find picture!
        VNLogWarning("Could not find buffer to release: %p", (void*)buffer);
        return;
    }

    // Call destructor directly, as we are doing in-place construct/destruct
    buffer->~BufferVulkan();

    // Release memory
    VNFree(m_allocator, pAlloc);

    m_buffers.removeReorder(pAlloc);
}

// Picture-handling
// Internal allocation

PictureVulkan* PipelineVulkan::allocatePicture()
{
    // Allocate picture
    LdcMemoryAllocation pictureAllocation;
    PictureVulkan* picture{VNAllocateZero(m_allocator, &pictureAllocation, PictureVulkan)};
    if (!picture) {
        return nullptr;
    }
    // Insert into table
    m_pictures.append(pictureAllocation);

    // In place construction
    return new (picture) PictureVulkan(*this); // NOLINT(cppcoreguidelines-owning-memory)
}

void PipelineVulkan::releasePicture(PictureVulkan* picture)
{
    // Find slot
    LdcMemoryAllocation* pAlloc{m_pictures.findUnordered(ldcVectorCompareAllocationPtr, picture)};

    if (!pAlloc) {
        // Could not find picture!
        VNLogWarning("Could not find picture to release: %p", (void*)picture);
        return;
    }

    // Call destructor directly, as we are doing in-place construct/destruct
    picture->~PictureVulkan();

    // Release memory
    VNFree(m_allocator, pAlloc);

    m_pictures.removeReorder(pAlloc);
}

LdpPicture* PipelineVulkan::allocPictureManaged(const LdpPictureDesc& desc)
{
    PictureVulkan* picture{allocatePicture()};
    if (picture) {
        picture->setDesc(desc);
        return picture;
    }
    return nullptr;
}

LdpPicture* PipelineVulkan::allocPictureExternal(const LdpPictureDesc& desc,
                                                 const LdpPicturePlaneDesc* planeDescArr,
                                                 const LdpPictureBufferDesc* buffer)
{
    PictureVulkan* picture{allocatePicture()};
    if (picture) {
        picture->setDesc(desc);
        picture->setExternal(planeDescArr, buffer);
        return picture;
    }
    return nullptr;
}

void PipelineVulkan::freePicture(LdpPicture* ldpPicture)
{
    // Get back to derived Picture class
    PictureVulkan* picture{static_cast<PictureVulkan*>(ldpPicture)};
    assert(ldpPicture);

    releasePicture(picture);
}

// Frames
//

// Allocate or find working data for a timestamp
//
// Given that there is going to be in the order of 100 or less frames, stick
// with an array and linear searches.
//
// Returns nullptr if there is no capacity for another frame.
//
FrameVulkan* PipelineVulkan::allocateFrame(uint64_t timestamp)
{
    assert(findFrame(timestamp) == nullptr);
    assert(m_frames.size() < m_configuration.maxLatency);

    // Allocate frame with in place construction
    LdcMemoryAllocation frameAllocation = {};
    FrameVulkan* const frame{VNAllocateZero(m_allocator, &frameAllocation, FrameVulkan)};
    if (!frame) {
        return nullptr;
    }

    // Append allocation into table
    m_frames.append(frameAllocation);

    // In place construction
    return new (frame) FrameVulkan(this, timestamp); // NOLINT(cppcoreguidelines-owning-memory)
}

// Find existing Frame for a timestamp, or return nullptr if it does not exist.
//
FrameVulkan* PipelineVulkan::findFrame(uint64_t timestamp)
{
    // Look for frame
    if (const LdcMemoryAllocation * pAlloc{m_frames.findUnordered(findFrameTimestamp, &timestamp)}) {
        return VNAllocationPtr(*pAlloc, FrameVulkan);
    }

    return nullptr;
}

// Release frame back to pool
//
void PipelineVulkan::freeFrame(FrameVulkan* frame)
{
    // Release task group and allocations
    frame->release();

    // Find slot
    LdcMemoryAllocation* frameAlloc{m_frames.findUnordered(ldcVectorCompareAllocationPtr, frame)};

    if (!frameAlloc) {
        // Could not find picture!
        VNLogWarning("Could not find frame to release: %p", (void*)frame);
        return;
    }

    // Call destructor directly, as we are doing in-place construct/destruct
    frame->~FrameVulkan();

    // Release memory
    VNFree(m_allocator, frameAlloc);

    m_frames.removeReorder(frameAlloc);
}

// Number of outstanding frames
uint32_t PipelineVulkan::frameLatency() const
{
    return m_reorderIndex.size() + m_processingIndex.size();
}

//// Frame start
//
// Get the next frame, if any, in timestamp order - taking into account reorder and flushing.
//
FrameVulkan* PipelineVulkan::getNextReordered()
{
    // Are there any frames at all?
    if (m_reorderIndex.isEmpty()) {
        return nullptr;
    }

    // If exceeded reorder limit, or flushing
    if (m_reorderIndex.size() >= m_maxReorder || m_reorderIndex[0]->m_ready) {
        FrameVulkan* const frame{m_reorderIndex[0]};
        m_reorderIndex.removeIndex(0);
        // Tell API there is enhancement space
        m_eventSink->generate(pipeline::EventCanSendEnhancement);
        return frame;
    }

    return nullptr;
}

// Resolve ready frame configurations in timestamp order, and generate tasks for each one.
//
// Once we are handling frames here, the frame is in flight - async to the API, so no error returns.
//
void PipelineVulkan::startReadyFrames()
{
    // Pull ready frames from reorder table
    while (FrameVulkan* frame = getNextReordered()) {
        const uint64_t timestamp{frame->timestamp};
        bool goodConfig = false;

        if (m_previousTimestamp != kInvalidTimestamp &&
            compareTimestamps(m_previousTimestamp, timestamp) > 0) {
            // Frame has been flushed out of reorder queue too late - mark as pass- through
            VNLogDebug("startReadyFrames: out of order: ts:%" PRIx64 " prev: %" PRIx64);
            frame->m_passthrough = true;
        }

        if (!frame->m_passthrough) {
            // Parse the LCEVC configuration into distinct per-frame data
            // Switch to pass-through if configuration parse failed.
            goodConfig = ldeConfigPoolFrameInsert(&m_configPool, timestamp,
                                                  VNAllocationPtr(frame->m_enhancementData, uint8_t),
                                                  VNAllocationSize(frame->m_enhancementData, uint8_t),
                                                  &frame->globalConfig, &frame->config);

            if (!goodConfig) {
                frame->m_passthrough = true;
            }
        }

        if (frame->m_passthrough) {
            // Set up enough frame configuration to support pass-through
            ldeConfigPoolFramePassthrough(&m_configPool, &frame->globalConfig, &frame->config);
        }

        VNLogDebug("Start Frame: %" PRIx64 " goodConfig:%d temporalEnabled:%d, temporalPresent:%d "
                   "temporalRefresh:%d loqEnabled[0]:%d loqEnabled[1]:%d passthrough:%d",
                   timestamp, goodConfig, frame->globalConfig->temporalEnabled,
                   frame->config.temporalSignallingPresent, frame->config.temporalRefresh,
                   frame->config.loqEnabled[0], frame->config.loqEnabled[1], frame->m_passthrough);

        // Once we have per frame configuration, we can properly initialize and figure out tasks for the frame
        if (!frame->initialize()) {
            VNLogError("Could not allocate frame buffers: %" PRIx64, frame->timestamp);
            // Could not allocate buffers - switch to pass-through
            frame->m_passthrough = true;
        }

        // All good - make tasks, and add to processing index
        // with frame it should get temporal from if it needs it
        frame->generateTasks(m_lastGoodTimestamp);

        {
            common::ScopedLock lock(m_interTaskMutex);
            frame->m_state = FrameStateProcessing;
            m_processingIndex.append(frame);
        }

        // Remember timestamps for next time
        m_previousTimestamp = timestamp;
        if (goodConfig) {
            m_lastGoodTimestamp = timestamp;
        }
    }

    // Connect available output pictures to started pictures
    connectOutputPictures();
}

// Connect any available output pictures to frames that can use them
//
void PipelineVulkan::connectOutputPictures()
{
    // While there are available output pictures and pending frames,
    // go through frames in timestamp order, assigning next output picture
    while (true) {
        FrameVulkan* frame{};

        if (m_outputPictureAvailableBuffer.isEmpty()) {
            // No output pictures left
            break;
        }

        // Find next in process frame with base data, and without an assigned output picture
        {
            common::ScopedLock lock(m_interTaskMutex);

            for (uint32_t idx = 0; idx < m_processingIndex.size(); ++idx) {
                if (!m_processingIndex[idx]->outputPicture && m_processingIndex[idx]->baseDataValid()) {
                    frame = m_processingIndex[idx];
                    break;
                }
            }
        }

        if (!frame) {
            // No frames without output pictures left
            break;
        }

        //  Get the picture
        LdpPicture* ldpPicture{};
        m_outputPictureAvailableBuffer.pop(ldpPicture);
        assert(ldpPicture);

        // Set the output layout
        const LdpPictureDesc desc{frame->getOutputPictureDesc()};
        ldpPictureSetDesc(ldpPicture, &desc);
        if (frame->globalConfig->cropEnabled) {
            ldpPicture->margins.left = frame->globalConfig->crop.left;
            ldpPicture->margins.right = frame->globalConfig->crop.right;
            ldpPicture->margins.top = frame->globalConfig->crop.top;
            ldpPicture->margins.bottom = frame->globalConfig->crop.bottom;
        }

        // Poke it into the frame's task group
        frame->outputPicture = ldpPicture;

        VNLogDebug("connectOutputPicture: %" PRIx64 " %p %ux%u (r:%d p:%d o:%d)", frame->timestamp,
                   (void*)ldpPicture, desc.width, desc.height, m_reorderIndex.size(),
                   m_processingIndex.size(), m_outputPictureAvailableBuffer.size());
        ldcTaskDependencyMet(&frame->m_taskGroup, frame->m_depOutputPicture, ldpPicture);

        // Tell API there is output picture space

        m_eventSink->generate(pipeline::EventCanSendPicture);
    }
}

//// Temporal
//
// Mark a frame as needing a temporal buffer of given timestamp and dimensions
//
// This might be resolved immediately if the previous frame is done already.
//
LdcTaskDependency PipelineVulkan::requireTemporalBuffer(FrameVulkan* frame, uint64_t timestamp, uint32_t plane)
{
    LdcTaskDependency dep{ldcTaskDependencyAdd(&frame->m_taskGroup)};
    TemporalBuffer* found{};

    uint32_t width = frame->globalConfig->width;
    uint32_t height = frame->globalConfig->height;
    width >>= ldpColorFormatPlaneWidthShift(frame->baseFormat, plane);
    height >>= ldpColorFormatPlaneHeightShift(frame->baseFormat, plane);

    // TODO - do this in shader
    // TODO - possibly maintain multiple buffers like the CPU dec
    if (!m_firstApply && frame->config.temporalRefresh) {
        const auto* temporalBuffer = static_cast<BufferVulkan*>(m_temporalPicture->buffer);
        std::memset(temporalBuffer->ptr(), 0, temporalBuffer->size());
    }

    // Fill in requirements
    frame->m_temporalBufferDesc[plane].timestamp = timestamp;
    frame->m_temporalBufferDesc[plane].clear =
        frame->config.nalType == NTIDR || frame->config.temporalRefresh;
    frame->m_temporalBufferDesc[plane].width = width;
    frame->m_temporalBufferDesc[plane].height = height;
    frame->m_temporalBufferDesc[plane].plane = plane;

    VNLogDebug("requireTemporalBuffer: %" PRIx64 " wants %" PRIx64 " plane %" PRIu32 " (%d %dx%d)",
               frame->timestamp, timestamp, plane, frame->m_temporalBufferDesc[plane].clear, width,
               height);

    {
        common::ScopedLock lock(m_interTaskMutex);

        // Do any of the available temporal buffers meet the requirements?
        for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
            TemporalBuffer* tb{m_temporalBuffers.at(i)};
            if (tb->frame) {
                // In use
                continue;
            }

            if (tb->desc.plane == plane && tb->desc.timestamp == timestamp) {
                // Exact plane index and timestamp match
                found = tb;
                break;
            }

            if (frame->m_temporalBufferDesc[plane].clear && tb->desc.timestamp == kInvalidTimestamp) {
                // An existing unused buffer
                found = tb;
                break;
            }
        }

        // Got one - mark it as in use
        if (found) {
            frame->m_temporalBuffer[plane] = found;
            found->frame = frame;

            if (frame->m_temporalBufferDesc[plane].clear) {
                // Update limit on any other prior buffers
                for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
                    TemporalBuffer* tb = m_temporalBuffers.at(i);
                    if (tb == found) {
                        continue;
                    }
                }
            }
        }

        frame->m_depTemporalBuffer[plane] = dep;
    }

    if (!found) {
        // Not found - will get resolved later by prior frame
        return dep;
    }

    VNLogDebug("  requireTemporalBuffer found: plane=%" PRIu32 " frame=%" PRIx64 " prev=%" PRIx64,
               plane, frame->timestamp, found->desc.timestamp);

    // Make sure found buffer meets requirements
    updateTemporalBufferDesc(found, frame->m_temporalBufferDesc[plane]);

    ldcTaskDependencyMet(&frame->m_taskGroup, dep, found);
    return dep;
}

// Mark the frame as having finished with its temporal buffer, and possibly
// hand buffer on to another frame
//
void PipelineVulkan::releaseTemporalBuffer(FrameVulkan* prevFrame, uint32_t plane)
{
    FrameVulkan* foundFrame{nullptr};
    TemporalBuffer* tb{nullptr};

    VNLogDebug("releaseTemporalBuffer: %" PRIx64 " plane: %" PRIu32, prevFrame->timestamp, plane);

    {
        common::ScopedLock lock(m_interTaskMutex);

        tb = prevFrame->m_temporalBuffer[plane];

        if (!tb) {
            // No temporal buffer to be released
            return;
        }

        tb->desc.timestamp = prevFrame->timestamp;
        // Do any of the pending frames want this buffer?
        for (uint32_t idx = 0; idx < m_processingIndex.size(); ++idx) {
            FrameVulkan* frame{m_processingIndex[idx]};

            if (compareTimestamps(frame->timestamp, prevFrame->timestamp) <= 0) {
                continue;
            }

            if (frame->m_depTemporalBuffer[plane] == kTaskDependencyInvalid ||
                frame->m_temporalBuffer[plane]) {
                // Does not need a buffer
                continue;
            }

            if (tb->desc.timestamp == frame->m_temporalBufferDesc[plane].timestamp) {
                // Matches this frame
                foundFrame = frame;
                break;
            }
            if (tb->desc.timestamp == kInvalidTimestamp) {
                // Unused buffer
                foundFrame = frame;
                break;
            }
        }

        // Detach from previous frame
        prevFrame->m_temporalBuffer[plane] = nullptr;
        tb->frame = nullptr;

        if (foundFrame) {
            foundFrame->m_temporalBuffer[plane] = tb;
            tb->frame = foundFrame;
        }
    }

    if (foundFrame) {
        VNLogDebug("  Vulkan::releaseTemporalBuffer found: plane=%" PRIu32 " frame=%" PRIx64
                   " prev=%" PRIx64,
                   plane, foundFrame->timestamp, prevFrame->timestamp);
        updateTemporalBufferDesc(tb, foundFrame->m_temporalBufferDesc[plane]);
        ldcTaskDependencyMet(&foundFrame->m_taskGroup, foundFrame->m_depTemporalBuffer[plane], tb);
    }
}

// Make a temporal buffer match the given description
void PipelineVulkan::updateTemporalBufferDesc(TemporalBuffer* buffer, const TemporalBufferDesc& desc) const
{
    const size_t byteStride{static_cast<size_t>(desc.width) * sizeof(uint16_t)};
    const size_t bufferSize{byteStride * static_cast<size_t>(desc.height)};

    if (!VNIsAllocated(buffer->allocation) || buffer->desc.width != desc.width ||
        buffer->desc.height != desc.height) {
        // Reallocate buffer
        if (!desc.clear && desc.timestamp != kInvalidTimestamp) {
            // Frame was expecting prior residuals - but dimensions are wrong!?
            VNLogWarning("Temporal buffer does not match: %08d Got %dx%d, Wanted %dx%d", desc.timestamp,
                         buffer->desc.width, buffer->desc.height, desc.width, desc.height);
        }
        buffer->planeDesc.firstSample =
            VNReallocateArray(allocator(), &buffer->allocation, uint8_t, bufferSize);
        buffer->planeDesc.rowByteStride = static_cast<uint32_t>(byteStride);
        memset(buffer->planeDesc.firstSample, 0, bufferSize);
    } else if (desc.clear) {
        memset(buffer->planeDesc.firstSample, 0, bufferSize);
    }

    // Update description
    buffer->desc = desc;
    buffer->desc.clear = false;
}

//// ConvertToInternal
//
// Copy incoming picture plane to internal fixed point surface format
//
// NB: There is likely a good templated C++ class that wraps these tasks up neatly,
// Worth figuring out once this has stabilised.
//
struct TaskConvertToInternalData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    unsigned baseDepth;
    unsigned enhancementDepth;
};

void* PipelineVulkan::taskConvertToInternal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskConvertToInternalData));
    const TaskConvertToInternalData& data{VNTaskData(task, TaskConvertToInternalData)};
    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};

    if (frame->m_skip) {
        return nullptr;
    }

    pipeline->m_shift = 15 - frame->baseBitdepth;

    auto* srcPicture = static_cast<PictureVulkan*>(frame->basePicture);
    LdpPictureDesc srcDesc;
    srcPicture->getDesc(srcDesc);

    // external base check
    if (LdpPictureBufferDesc exDesc{}; srcPicture->getBufferDesc(exDesc)) {
        auto managedBuffer = static_cast<BufferVulkan*>(srcPicture->buffer);
        if (managedBuffer->size() != exDesc.byteSize) { // padded base
            const bool nv12 = (srcPicture->layout.layoutInfo->format == LdpColorFormatNV12_8 ||
                               srcPicture->layout.layoutInfo->format == LdpColorFormatNV21_8)
                                  ? true
                                  : false;

            const uint32_t byteWidth = (frame->baseBitdepth == 8) ? srcDesc.width : 2 * srcDesc.width;
            const uint32_t planeWidth = nv12 ? byteWidth : byteWidth >> 1;

            auto removePadding = [&](uint32_t width, uint32_t height, uint32_t pixelWidth,
                                     uint8_t planeIndex, uint32_t internalOffset,
                                     uint32_t externalOffsetU, uint32_t externalOffsetV) {
                for (uint32_t y = 0; y < height; ++y) {
                    const auto internalIndex = internalOffset + y * pixelWidth;
                    auto externalIndex = y * srcPicture->layout.rowStrides[planeIndex];
                    if (planeIndex > 0) {
                        externalIndex += externalOffsetU * srcPicture->layout.rowStrides[planeIndex - 1];
                    }
                    if (planeIndex > 1) {
                        externalIndex += externalOffsetV * srcPicture->layout.rowStrides[planeIndex - 2];
                    }
                    std::memcpy(managedBuffer->ptr() + internalIndex, exDesc.data + externalIndex, width);
                }
            };

            removePadding(byteWidth, srcDesc.height, byteWidth, 0, 0, 0, 0); // Y

            if (pipeline->m_chroma != LdeChroma::CTMonochrome) {
                removePadding(planeWidth, srcDesc.height >> 1, planeWidth, 1,
                              byteWidth * srcDesc.height, srcDesc.height, 0); // U

                if (!nv12) {
                    removePadding(byteWidth >> 1, srcDesc.height >> 1, planeWidth, 2,
                                  5 * byteWidth * srcDesc.height >> 2, srcDesc.height >> 1,
                                  srcDesc.height); // V

                    srcPicture->layout.rowStrides[2] = planeWidth;
                    srcPicture->layout.planeOffsets[2] = 5 * byteWidth * srcDesc.height >> 2;
                }
                srcPicture->layout.rowStrides[1] = planeWidth;
                srcPicture->layout.planeOffsets[1] = byteWidth * srcDesc.height;
            }
            srcPicture->layout.rowStrides[0] = byteWidth;
            srcPicture->layout.planeOffsets[0] = 0;
        } else {
            std::memcpy(managedBuffer->ptr(), exDesc.data, exDesc.byteSize);
        }
    }

    switch (pipeline->m_chroma) {
        case LdeChroma::CTMonochrome: srcDesc.colorFormat = LdpColorFormatGRAY_16_LE; break;
        case LdeChroma::CT420: srcDesc.colorFormat = LdpColorFormatI420_16_LE; break;
        case LdeChroma::CT422: srcDesc.colorFormat = LdpColorFormatI422_16_LE; break;
        case LdeChroma::CT444: srcDesc.colorFormat = LdpColorFormatI444_16_LE; break;
        default: srcDesc.colorFormat = LdpColorFormatUnknown; break;
    }
    auto* dstPicture = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(srcDesc));

    VulkanConversionArgs args{};
    args.src = srcPicture;
    args.dst = dstPicture;
    args.toInternal = true;

    if (!pipeline->conversion(&args)) {
        VNLogError("Conversion to internal failed");
    }

    frame->m_intermediatePicture[LOQ2] = dstPicture;

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskConvertToInternal(FrameVulkan* frame, unsigned baseDepth,
                                                           unsigned enhancementDepth,
                                                           LdcTaskDependency inputDep)
{
    const TaskConvertToInternalData data{this, frame, baseDepth, enhancementDepth};
    const LdcTaskDependency inputs[] = {inputDep};
    const LdcTaskDependency outputDep{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), outputDep,
                    taskConvertToInternal, nullptr, 1, 1, sizeof(data), &data, "ConvertToInternal");

    return outputDep;
}

//// ConvertFromInternal
//
// Concert a picture plane from internal fixed point to output picture pixel format.
//
struct TaskConvertFromInternalData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    unsigned baseDepth;
    unsigned enhancementDepth;
    uint8_t intermediatePtr;
};

void* PipelineVulkan::taskConvertFromInternal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskConvertFromInternalData));
    const TaskConvertFromInternalData& data{VNTaskData(task, TaskConvertFromInternalData)};
    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};
    const uint8_t intermediatePtr{data.intermediatePtr};

    if (frame->m_skip) {
        return nullptr;
    }

    auto* srcPicture = frame->m_intermediatePicture[intermediatePtr];

    LdpPictureDesc dstDesc;
    srcPicture->getDesc(dstDesc);
    dstDesc.colorFormat = frame->outputPicture->layout.layoutInfo->format;
    frame->outputPicture->functions->setDesc(frame->outputPicture, &dstDesc);

    VulkanConversionArgs args{};
    args.src = srcPicture;
    args.dst = static_cast<PictureVulkan*>(frame->outputPicture);
    args.toInternal = false;

    if (!pipeline->conversion(&args)) {
        VNLogError("Conversion from internal failed");
    }

    if (frame->globalConfig->cropEnabled) {
        args.dst->margins.left = frame->globalConfig->crop.left;
        args.dst->margins.right = frame->globalConfig->crop.right;
        args.dst->margins.top = frame->globalConfig->crop.top;
        args.dst->margins.bottom = frame->globalConfig->crop.bottom;
    }

    // external output check
    if (LdpPictureBufferDesc exDesc{};
        static_cast<PictureVulkan*>(frame->outputPicture)->getBufferDesc(exDesc)) {
        auto managedBuffer =
            static_cast<BufferVulkan*>(static_cast<PictureVulkan*>(frame->outputPicture)->buffer);
        std::memcpy(exDesc.data, managedBuffer->ptr(), exDesc.byteSize);
    }

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskConvertFromInternal(FrameVulkan* frame, unsigned baseDepth,
                                                             unsigned enhancementDepth,
                                                             LdcTaskDependency dstDep,
                                                             LdcTaskDependency srcDep,
                                                             uint8_t intermediatePtr)
{
    const TaskConvertFromInternalData data{this, frame, baseDepth, enhancementDepth, intermediatePtr};
    const LdcTaskDependency inputs[] = {dstDep, srcDep};
    const LdcTaskDependency output = ldcTaskDependencyAdd(&frame->m_taskGroup);

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskConvertFromInternal,
                    nullptr, 1, 1, sizeof(data), &data, "ConvertFromInternal");

    return output;
}

//// Upsample
//
// Upscale (1D or 2D) for one plane of picture.
//
// Inputs and outputs may be fixed point or 'external' format if no residuals are being applied.
//
struct TaskUpsampleData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    LdeLOQIndex loq;
    uint8_t intermediatePtr;
    LdeKernel kernel;
};

void* PipelineVulkan::taskUpsample(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskUpsampleData));
    const TaskUpsampleData& data{VNTaskData(task, TaskUpsampleData)};

    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};
    const uint8_t intermediatePtr{data.intermediatePtr};
    const LdeLOQIndex loq{data.loq};

    if (frame->m_skip) {
        return nullptr;
    }

    VulkanUpscaleArgs upscaleArgs{};
    upscaleArgs.src = frame->m_intermediatePicture[intermediatePtr];
    const LdpPictureDesc desc{2, 2, LdpColorFormatI420_8};
    frame->m_intermediatePicture[intermediatePtr - 1] =
        static_cast<PictureVulkan*>(pipeline->allocPictureManaged(desc));
    upscaleArgs.dst = frame->m_intermediatePicture[intermediatePtr - 1];

    upscaleArgs.applyPA = static_cast<uint8_t>(frame->globalConfig->predictedAverageEnabled);
    upscaleArgs.dither = nullptr; // TODO pipeline->m_dither;
    upscaleArgs.mode = frame->globalConfig->scalingModes[data.loq - 1];
    upscaleArgs.vertical = false;
    upscaleArgs.loq1 = (loq == 2) ? true : false;

    assert(upscaleArgs.mode != Scale0D);
    VNLogDebug("taskUpsample timestamp:%" PRIx64 " loq:%d", frame->timestamp, (uint32_t)data.loq);

    if (!pipeline->upscaleFrame(&frame->globalConfig->kernel, &upscaleArgs)) {
        VNLogError("Upsample failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskUpsample(FrameVulkan* frame, LdeLOQIndex loq,
                                                  LdcTaskDependency inputDep, uint8_t intermediatePtr)
{
    const TaskUpsampleData data{this, frame, loq, intermediatePtr};

    assert(frame->globalConfig->scalingModes[loq - 1] != Scale0D);

    const LdcTaskDependency inputs[] = {inputDep};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskUpsample, nullptr,
                    1, 1, sizeof(data), &data, "Upsample");

    return output;
}

//// GenerateCmdBuffer
//
// Convert un-encapsulated chunks into a single command buffer.
//
struct TaskGenerateCmdBufferData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    LdpEnhancementTile* enhancementTile;
};

void* PipelineVulkan::taskGenerateCmdBuffer(LdcTask* task, const LdcTaskPart* /*part*/)
{
    assert(task->dataSize == sizeof(TaskGenerateCmdBufferData));
    const TaskGenerateCmdBufferData& data{VNTaskData(task, TaskGenerateCmdBufferData)};
    FrameVulkan* const frame{data.frame};

    VNLogDebug("taskGenerateCmdBuffer timestamp:%" PRIx64 " tile:%d loq:%d plane:%d",
               data.frame->timestamp, data.enhancementTile->tile,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    if (!ldeDecodeEnhancement(frame->globalConfig, &frame->config, data.enhancementTile->loq,
                              data.enhancementTile->plane, data.enhancementTile->tile, nullptr,
                              &data.enhancementTile->bufferGpu, &data.enhancementTile->bufferGpuBuilder)) {
        VNLogError("ldeDecodeEnhancement failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskGenerateCmdBuffer(FrameVulkan* frame,
                                                           LdpEnhancementTile* enhancementTile)
{
    const TaskGenerateCmdBufferData data{this, frame, enhancementTile};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, nullptr, 0, output, taskGenerateCmdBuffer, nullptr, 1, 1,
                    sizeof(data), &data, "GenerateCmdBuffer");

    return output;
}

//// ApplyCmdBufferDirect
//
// Apply a generated GPU command buffer to directly to output plane. (No Temporal)
//
// NB: The output plane will be in 'internal' fixed' point format
//
struct TaskApplyCmdBufferDirectData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    LdpEnhancementTile* enhancementTile;
    uint8_t intermediatePtr;
};

void* PipelineVulkan::taskApplyCmdBufferDirect(LdcTask* task, const LdcTaskPart* part)
{
    VNTraceScoped();
    VNUnused(part);

    assert(task->dataSize == sizeof(TaskApplyCmdBufferDirectData));
    const TaskApplyCmdBufferDirectData& data{VNTaskData(task, TaskApplyCmdBufferDirectData)};
    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};
    const uint8_t intermediatePtr{data.intermediatePtr};

    if (frame->m_skip) {
        return nullptr;
    }

    VNLogDebug("taskApplyCmdBufferDirect timestamp:%" PRIx64 " loq:%d plane:%d", data.frame->timestamp,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    auto* picture = frame->m_intermediatePicture[intermediatePtr];
    LdpPictureDesc desc{};
    picture->getDesc(desc);

    VulkanApplyArgs args{};
    args.plane = picture;
    args.planeWidth = desc.width;
    args.planeHeight = desc.height;
    args.bufferGpu = data.enhancementTile->bufferGpu;
    args.temporalRefresh = false;
    args.highlightResiduals = pipeline->m_configuration.highlightResiduals;
    args.tuRasterOrder =
        !frame->globalConfig->temporalEnabled && frame->globalConfig->tileDimensions == TDTNone;

    if (!pipeline->apply(&args)) {
        VNLogError("Vulkan apply direct failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskApplyCmdBufferDirect(FrameVulkan* frame,
                                                              LdpEnhancementTile* enhancementTile,
                                                              LdcTaskDependency inputDep,
                                                              LdcTaskDependency cmdBufferDep,
                                                              uint8_t intermediatePtr)
{
    const TaskApplyCmdBufferDirectData data{this, frame, enhancementTile, intermediatePtr};
    const LdcTaskDependency inputs[] = {inputDep, cmdBufferDep};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskApplyCmdBufferDirect,
                    nullptr, 1, 1, sizeof(data), &data, "ApplyCmdBufferDirect");

    return output;
}

//// ApplyCmdBufferTemporal
//
// Apply a generated GPU command buffer to a temporal buffer.
//
struct TaskApplyCmdBufferTemporalData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    LdpEnhancementTile* enhancementTile;
    uint8_t intermediatePtr;
};

void* PipelineVulkan::taskApplyCmdBufferTemporal(LdcTask* task, const LdcTaskPart* part)
{
    VNTraceScoped();
    VNUnused(part);

    assert(task->dataSize == sizeof(TaskApplyCmdBufferTemporalData));
    const TaskApplyCmdBufferTemporalData& data{VNTaskData(task, TaskApplyCmdBufferTemporalData)};
    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};
    const uint8_t intermediatePtr{data.intermediatePtr};

    VNLogDebug("taskApplyCmdBufferTemporal timestamp:%" PRIx64 " tile:%d loq:%d plane:%d",
               data.frame->timestamp, data.enhancementTile->tile,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    const auto* picture = frame->m_intermediatePicture[intermediatePtr];
    LdpPictureDesc desc{};
    picture->getDesc(desc);

    VulkanApplyArgs args{};
    args.plane = nullptr;
    args.planeWidth = desc.width;
    args.planeHeight = desc.height;
    args.bufferGpu = data.enhancementTile->bufferGpu;
    args.temporalRefresh = frame->config.temporalRefresh;
    args.highlightResiduals = pipeline->m_configuration.highlightResiduals;

    if (!pipeline->apply(&args)) {
        VNLogError("Vulkan apply temporal failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskApplyCmdBufferTemporal(FrameVulkan* frame,
                                                                LdpEnhancementTile* enhancementTile,
                                                                LdcTaskDependency temporalBufferDep,
                                                                LdcTaskDependency cmdBufferDep,
                                                                uint8_t intermediatePtr)
{
    const TaskApplyCmdBufferTemporalData data{this, frame, enhancementTile, intermediatePtr};
    const LdcTaskDependency inputs[] = {temporalBufferDep, cmdBufferDep};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskApplyCmdBufferTemporal,
                    nullptr, 1, 1, sizeof(data), &data, "ApplyCmdBufferTemporal");

    return output;
}

//// ApplyAddTemporal
//
// Add a temporal buffer to a picture plane.
//
struct TaskApplyAddTemporalData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    uint8_t intermediatePtr;
};

void* PipelineVulkan::taskApplyAddTemporal(LdcTask* task, const LdcTaskPart* part)
{
    VNTraceScoped();
    VNUnused(part);

    assert(task->dataSize == sizeof(TaskApplyAddTemporalData));
    const TaskApplyAddTemporalData& data{VNTaskData(task, TaskApplyAddTemporalData)};
    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};
    const uint8_t intermediatePtr{data.intermediatePtr};

    if (frame->m_skip) {
        return nullptr;
    }

    VNLogDebug("taskApplyAddTemporal timestamp:%" PRIx64 "", data.frame->timestamp);

    VulkanBlitArgs args{};
    args.src = pipeline->m_temporalPicture;
    args.dst = frame->m_intermediatePicture[intermediatePtr];

    if (!pipeline->blit(&args)) {
        VNLogError("Vulkan blit failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskApplyAddTemporal(FrameVulkan* frame, LdcTaskDependency temporalDep,
                                                          LdcTaskDependency sourceDep,
                                                          uint8_t intermediatePtr)
{
    const TaskApplyAddTemporalData data{this, frame, intermediatePtr};
    const LdcTaskDependency inputs[] = {temporalDep, sourceDep};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskApplyAddTemporal,
                    nullptr, 1, 1, sizeof(data), &data, "ApplyAddTemporal");

    return output;
}

//// Passthrough
//
// Copy incoming picture plane to output picture
//
struct TaskPassthroughData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    uint32_t planeIndex;
};

void* PipelineVulkan::taskPassthrough(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskPassthroughData));

    const TaskPassthroughData& data{VNTaskData(task, TaskPassthroughData)};
    PipelineVulkan* const pipeline{data.pipeline};
    const FrameVulkan* const frame{data.frame};

    if (frame->m_skip) {
        return nullptr;
    }

    LdpPicturePlaneDesc srcPlane;
    frame->getBasePlaneDesc(data.planeIndex, srcPlane);

    LdpPicturePlaneDesc dstPlane;
    frame->getOutputPlaneDesc(data.planeIndex, dstPlane);

    VNLogDebug("taskPassthrough timestamp:%" PRIx64 " plane:%d", data.frame->timestamp, data.planeIndex);

    if (!ldppPlaneBlit(&pipeline->m_taskPool, task, pipeline->m_configuration.forceScalar,
                       data.planeIndex, &frame->basePicture->layout, &frame->outputPicture->layout,
                       &srcPlane, &dstPlane, BMCopy)) {
        VNLogError("ldppPlaneBlit In failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskPassthrough(FrameVulkan* frame, uint32_t planeIndex,
                                                     LdcTaskDependency dest, LdcTaskDependency src)
{
    const TaskPassthroughData data{this, frame, planeIndex};
    const LdcTaskDependency inputs[] = {dest, src};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskPassthrough,
                    nullptr, 1, 1, sizeof(data), &data, "Passthrough");

    return output;
}

//// WaitForMany
//
// Wait for several input dependencies to be met.
//
// NB: If this appears to be a bottleneck, it could be integrated better into the task pool.
//
struct TaskWaitForManyData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
};

void* PipelineVulkan::taskWaitForMany(LdcTask* task, const LdcTaskPart* part)
{
    VNUnused(part);

    assert(task->dataSize == sizeof(TaskWaitForManyData));
    const TaskWaitForManyData& data{VNTaskData(task, TaskWaitForManyData)};

    VNLogDebug("taskWaitForMany timestamp:%" PRIx64 "", data.frame->timestamp);
    return nullptr;
}

LdcTaskDependency PipelineVulkan::addTaskWaitForMany(FrameVulkan* frame, const LdcTaskDependency* inputDeps,
                                                     uint32_t inputDepsCount)
{
    const TaskWaitForManyData data{this, frame};
    const LdcTaskDependency outputDep{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputDeps, inputDepsCount, outputDep, taskWaitForMany,
                    nullptr, 1, 1, sizeof(data), &data, "WaitForMany");

    return outputDep;
}

//// BaseSend
//
// Wait for base picture planes to be used, then send base picture back to client
//
struct TaskBaseDoneData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
};

void* PipelineVulkan::taskBaseDone(LdcTask* task, const LdcTaskPart* part)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskBaseDoneData));

    const TaskBaseDoneData& data{VNTaskData(task, TaskBaseDoneData)};

    VNLogDebug("taskBaseDone timestamp:%" PRIx64, data.frame->timestamp);

    assert(data.frame->basePicture);

    // Generate event
    data.pipeline->m_eventSink->generate(pipeline::EventBasePictureDone, data.frame->basePicture);

    // Send base picture back to API
    data.pipeline->m_basePictureOutBuffer.push(data.frame->basePicture);

    // Frame no longer has access to base picture
    data.frame->basePicture = nullptr;
    return nullptr;
}

void PipelineVulkan::addTaskBaseDone(FrameVulkan* frame, const LdcTaskDependency* inputs, uint32_t inputsCount)
{
    const TaskBaseDoneData data{this, frame};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, inputsCount, kTaskDependencyInvalid, taskBaseDone,
                    nullptr, 1, 1, sizeof(data), &data, "BaseDone");
}

//// OutputSend
//
// Wait for a bunch of input dependencies to be met, then:
//
// - Send output picture to output queue
// - Release frame
//
struct TaskOutputDoneData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
};

void* PipelineVulkan::taskOutputDone(LdcTask* task, const LdcTaskPart* part)
{
    VNUnused(part);
    assert(task->dataSize == sizeof(TaskOutputDoneData));
    const TaskOutputDoneData& data{VNTaskData(task, TaskOutputDoneData)};
    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};

    VNLogDebug("taskOutputDone timestamp:%" PRIx64, frame->timestamp);

    // Mark as done, and signal pipeline if it is waiting
    {
        common::ScopedLock lock(pipeline->m_interTaskMutex);
        frame->m_state = FrameStateDone;

        // Build the decode info for the frame
        frame->m_decodeInfo.timestamp = frame->timestamp;
        frame->m_decodeInfo.hasBase = true;
        frame->m_decodeInfo.hasEnhancement =
            frame->config.loqEnabled[LOQ1] || frame->config.loqEnabled[LOQ0];
        frame->m_decodeInfo.skipped = frame->m_skip;
        frame->m_decodeInfo.enhanced = frame->config.loqEnabled[LOQ1] || frame->config.loqEnabled[LOQ0];
        frame->m_decodeInfo.baseWidth = frame->baseWidth;
        frame->m_decodeInfo.baseHeight = frame->baseHeight;
        frame->m_decodeInfo.baseBitdepth = frame->baseBitdepth;
        frame->m_decodeInfo.userData = frame->userData;

        pipeline->m_interTaskFrameDone.signal();

        pipeline->m_eventSink->generate(pipeline::EventOutputPictureDone, frame->outputPicture,
                                        &frame->m_decodeInfo);
        pipeline->m_eventSink->generate(pipeline::EventCanReceive);
    }

    return nullptr;
}

void PipelineVulkan::addTaskOutputDone(FrameVulkan* frame, const LdcTaskDependency* inputs, uint32_t inputsCount)
{
    const TaskOutputDoneData data{this, frame};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, inputsCount, kTaskDependencyInvalid,
                    taskOutputDone, nullptr, 1, 1, sizeof(data), &data, "OutputDone");
}

//// TemporalRelease
//
// Wait for a bunch of input dependencies to be met, then:
//
// - Release temporal buffer to next frame
//
struct TaskTemporalReleaseData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
};

void* PipelineVulkan::taskTemporalRelease(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskTemporalReleaseData));

    const TaskTemporalReleaseData& data{VNTaskData(task, TaskTemporalReleaseData)};
    PipelineVulkan* const pipeline{data.pipeline};
    FrameVulkan* const frame{data.frame};

    VNLogDebug("taskTemporalRelease timestamp:%" PRIx64, frame->timestamp);

    pipeline->releaseTemporalBuffer(frame, 0);

    return nullptr;
}

void PipelineVulkan::addTaskTemporalRelease(FrameVulkan* frame, const LdcTaskDependency* deps)
{
    const TaskTemporalReleaseData data{this, frame};
    const LdcTaskDependency inputs[] = {deps[0]};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), kTaskDependencyInvalid,
                    taskTemporalRelease, nullptr, 1, 1, sizeof(data), &data, "TemporalRelease");
}

// Fill out a task group given a frame configuration
//
void PipelineVulkan::generateTasksEnhancement(FrameVulkan* frame, uint64_t previousTimestamp)
{
    VNTraceScoped();

    // Convenience values for readability
    const LdeFrameConfig& frameConfig{frame->config};
    const LdeGlobalConfig& globalConfig{*frame->globalConfig};
    auto intermediatePtr = static_cast<uint8_t>(LOQ2);

    m_chroma = frame->globalConfig->chroma;

    uint32_t enhancementTileIdx = 0;

    if (frame->config.sharpenType != STDisabled && frame->config.sharpenStrength != 0.0f) {
        VNLogWarning("S-Filter is configured in stream, but not supported by decoder.");
    }

    //// LoQ 1
    // Upsample and residuals

    const bool isEnhanced1 = frame->isEnhanced(LOQ1, 0);

    //// Input conversion
    LdcTaskDependency basePicture{kTaskDependencyInvalid};
    basePicture = addTaskConvertToInternal(frame, globalConfig.baseDepth,
                                           globalConfig.enhancedDepth, frame->m_depBasePicture);

    //// Base + Residuals
    //
    // First upsample
    LdcTaskDependency baseUpsampled{kTaskDependencyInvalid};
    if (globalConfig.scalingModes[LOQ1] != Scale0D) {
        baseUpsampled = addTaskUpsample(frame, LOQ2, basePicture, intermediatePtr);
        intermediatePtr--;
    } else {
        baseUpsampled = basePicture;
    }

    // Enhancement LOQ1 decoding
    if (isEnhanced1 && frameConfig.loqEnabled[LOQ1]) {
        const uint32_t numTiles = globalConfig.numTiles[0][LOQ1];
        if (numTiles > 1) {
            LdcTaskDependency* tiles =
                static_cast<LdcTaskDependency*>(alloca(numTiles * sizeof(LdcTaskDependency)));

            // Generate and apply each tile's command buffer
            for (unsigned tile = 0; tile < numTiles; ++tile) {
                LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                assert(et->loq == LOQ1 && et->tile == tile);

                LdcTaskDependency commands = addTaskGenerateCmdBuffer(frame, et);
                tiles[tile] =
                    addTaskApplyCmdBufferDirect(frame, et, baseUpsampled, commands, intermediatePtr);
            }
            // Wait for all tiles to finish
            basePicture = addTaskWaitForMany(frame, tiles, numTiles);
        } else {
            LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
            assert(et->loq == LOQ1 && et->tile == 0);

            LdcTaskDependency commands = addTaskGenerateCmdBuffer(frame, et);
            basePicture = addTaskApplyCmdBufferDirect(frame, et, baseUpsampled, commands, intermediatePtr);
        }
    } else {
        basePicture = baseUpsampled;
    }

    // Upsample from combined intermediate picture to preliminary output picture
    LdcTaskDependency upsampledPicture{};
    if (globalConfig.scalingModes[LOQ0] != Scale0D) {
        upsampledPicture = addTaskUpsample(frame, LOQ1, basePicture, intermediatePtr);
        intermediatePtr--;
    } else {
        upsampledPicture = basePicture;
    }

    //// LoQ 0
    //
    LdcTaskDependency reconstructedPicture{};
    const bool isEnhanced0 = frame->isEnhanced(LOQ0, 0);
    LdcTaskDependency recon{upsampledPicture};

    if (globalConfig.temporalEnabled && !frame->m_passthrough) {
        LdcTaskDependency temporal{kTaskDependencyInvalid};

        // Still need a temporal buffer, even if the particular frame is not enhanced
        // winds up getting passed through and applied
        temporal = requireTemporalBuffer(frame, previousTimestamp, 0);

        if (isEnhanced0 && frameConfig.loqEnabled[LOQ0]) {
            // Enhancement residuals
            const uint32_t numPlaneTiles = globalConfig.numTiles[0][LOQ0];
            if (numPlaneTiles > 1) {
                LdcTaskDependency* tiles =
                    static_cast<LdcTaskDependency*>(alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                // Generate and apply each tile's command buffer
                for (unsigned tile = 0; tile < numPlaneTiles; ++tile) {
                    LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                    assert(et->loq == LOQ0 && et->tile == tile);
                    LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};

                    tiles[tile] =
                        addTaskApplyCmdBufferTemporal(frame, et, temporal, commands, intermediatePtr);
                }
                // Wait for all tiles to finish
                temporal = addTaskWaitForMany(frame, tiles, numPlaneTiles);
            } else {
                LdpEnhancementTile* et = frame->getEnhancementTile(enhancementTileIdx++);
                assert(et->loq == LOQ0 && et->tile == 0);

                LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};

                temporal = addTaskApplyCmdBufferTemporal(frame, et, temporal, commands, intermediatePtr);
            }
        }

        // Always add temporal buffer, even if no enhancement this frame
        reconstructedPicture = addTaskApplyAddTemporal(frame, temporal, recon, intermediatePtr);
        addTaskTemporalRelease(frame, &reconstructedPicture);
    } else {
        if (isEnhanced0 && frameConfig.loqEnabled[LOQ0]) {
            // Enhancement residuals
            const uint32_t numPlaneTiles = globalConfig.numTiles[0][LOQ0];
            if (numPlaneTiles > 1) {
                LdcTaskDependency* tiles =
                    static_cast<LdcTaskDependency*>(alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                // Generate and apply each tile's command buffer
                for (unsigned tile = 0; tile < numPlaneTiles; ++tile) {
                    LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                    assert(et->loq == LOQ0 && et->tile == tile);
                    LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};
                    tiles[tile] = addTaskApplyCmdBufferDirect(frame, et, recon, commands, intermediatePtr);
                }
                // Wait for all tiles to finish
                recon = addTaskWaitForMany(frame, tiles, numPlaneTiles);
            } else {
                LdpEnhancementTile* et = frame->getEnhancementTile(enhancementTileIdx++);
                assert(et->loq == LOQ0 && et->tile == 0);

                LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};

                recon = addTaskApplyCmdBufferDirect(frame, et, recon, commands, intermediatePtr);
            }
        }

        reconstructedPicture = recon;
    }

    assert(enhancementTileIdx == frame->enhancementTileCount);

    LdcTaskDependency outputPicture{};
    outputPicture =
        addTaskConvertFromInternal(frame, globalConfig.baseDepth, globalConfig.enhancedDepth,
                                   frame->m_depOutputPicture, reconstructedPicture, intermediatePtr);

    // Send output when all planes are ready
    addTaskOutputDone(frame, &outputPicture, 1);

    // Send base when all tasks that use it have completed
    LdcTaskDependency deps[kLdpPictureMaxNumPlanes] = {};
    uint32_t depsCount = 0;
    ldcTaskGroupFindOutputSetFromInput(&frame->m_taskGroup, frame->m_depBasePicture, deps,
                                       kLdpPictureMaxNumPlanes, &depsCount);
    addTaskBaseDone(frame, deps, depsCount);
}

// Fill out a task group for a simple unscaled passthrough configuration
//
void PipelineVulkan::generateTasksPassthrough(FrameVulkan* frame)
{
    VNTraceScoped();

    uint8_t numImagePlanes{kLdpPictureMaxNumPlanes};
    if (frame->basePicture) {
        VNLogDebugF("No base for passthrough: %" PRIx64, frame->timestamp);
        numImagePlanes = ldpPictureLayoutPlanes(&frame->basePicture->layout);
    }

    LdcTaskDependency outputPlanes[kLdpPictureMaxNumPlanes] = {};

    for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
        outputPlanes[plane] =
            addTaskPassthrough(frame, plane, frame->m_depOutputPicture, frame->m_depBasePicture);
    }

    // Send output and base when all planes are ready
    addTaskOutputDone(frame, outputPlanes, numImagePlanes);
    addTaskBaseDone(frame, outputPlanes, numImagePlanes);
}

uint32_t PipelineVulkan::chromaToNumPlanes(LdeChroma chroma)
{
    switch (chroma) {
        case CTMonochrome: return 1;
        case CT420:
        case CT422:
        case CT444: return 3;
        default: assert(0); return 0;
    }
}

#ifdef VN_SDK_LOG_ENABLE_DEBUG
// Dump frame and index state
//
void PipelineVulkan::logFrames() const
{
    char buffer[512];

    VNLogDebugF("Frames: %d", m_frames.size());
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameVulkan* const frame{VNAllocationPtr(m_frames[i], FrameVulkan)};
        frame->longDescription(buffer, sizeof(buffer));
        VNLogDebugF("  %4d: %s", i, buffer);
    }

    VNLogDebugF("Reorder: %d", m_reorderIndex.size());
    for (uint32_t i = 0; i < m_reorderIndex.size(); ++i) {
        FrameVulkan* const frame{m_reorderIndex[i]};
        const LdcMemoryAllocation* const ptr =
            m_frames.findUnordered(ldcVectorCompareAllocationPtr, frame);
        const int32_t idx = static_cast<int32_t>(ptr ? (ptr - &m_frames[0]) : -1);
        VNLogDebugF("  %2d: %4d", i, idx);
    }

    VNLogDebugF("Processing: %d", m_processingIndex.size());
    for (uint32_t i = 0; i < m_processingIndex.size(); ++i) {
        FrameVulkan* const frame{m_processingIndex[i]};
        const LdcMemoryAllocation* const ptr =
            m_frames.findUnordered(ldcVectorCompareAllocationPtr, frame);
        const int32_t idx = static_cast<int32_t>(ptr ? (ptr - &m_frames[0]) : -1);
        VNLogDebugF("  %2d: %4d", i, idx);
    }

    VNLogDebugF("Bases In: %d (%d)", m_basePicturePending.size(), m_basePicturePending.reserved());
    VNLogDebugF("Bases Out: %d (%d)", m_basePictureOutBuffer.size(), m_basePictureOutBuffer.capacity());
    VNLogDebugF("Output: %d (%d)", m_outputPictureAvailableBuffer.size(),
                m_outputPictureAvailableBuffer.capacity());
}
#endif

} // namespace lcevc_dec::pipeline_vulkan
