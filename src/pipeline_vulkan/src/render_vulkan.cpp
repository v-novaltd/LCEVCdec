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

#include "render_vulkan.h"

#include "backend_vulkan.h"
#include "buffer_vulkan.h"
#include "compute_vulkan.h"
#include "frame_vulkan.h"
#include "picture_vulkan.h"
#include "render_fragment.h"
#include "render_vertex.h"

#include <LCEVC/build_config.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
#include <LCEVC/pixel_processing/dither.h>
#if VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
#include <zlib.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <type_traits>

namespace lcevc_dec::pipeline_vulkan {

namespace {
    constexpr uint32_t kDitherBufferSize = 16384;
}

struct Vertex
{
    float pos[3];
    float color[3];
    float texCoord[2];

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

struct UniformBufferObject
{
    float modelViewProjection[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
                                       {0.0f, 1.0f, 0.0f, 0.0f},
                                       {0.0f, 0.0f, 1.0f, 0.0f},
                                       {0.0f, 0.0f, 0.0f, 1.0f}};
    int bit8;
    int nv12;
    int srcWidth;
    int srcHeight;
    int ditherStrength;
    int ditherEnabled;
    int ditherShift;
    int ditherBufferSize;
    int limitedRange;
};

const std::vector<Vertex> g_vertices = {{{-1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                        {{1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                        {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                        {{-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

const std::vector<uint16_t> g_indices = {0, 1, 2, 2, 3, 0};

bool RenderVulkan::init()
{
    VNLogDebug("render init start");

    // Render startup
    if (!createSwapChainAndGetImages()) {
        return false;
    }
    if (!createSwapChainImageViews()) {
        return false;
    }
    if (!createRenderPass()) {
        return false;
    }
    if (!createGraphicsDescriptorSetLayout()) {
        return false;
    }
    if (!createGraphicsPipeline()) {
        return false;
    }
    if (!createFramebuffers()) {
        return false;
    }
    if (!createGraphicsCommandPoolAndBuffer()) {
        return false;
    }
    if (!createVertexBuffer()) {
        return false;
    }
    if (!createIndexBuffer()) {
        return false;
    }
    if (!createUniformBuffers()) {
        return false;
    }
    if (!createDitherBuffers()) {
        return false;
    }
    if (!createSyncObjects()) {
        return false;
    }

    // render frame
    if (!createTextureImages(2, 2)) {
        return false;
    }
    if (!createTextureImageViews()) {
        return false;
    }
    if (!createTextureSamplers()) {
        return false;
    }
    if (!createGraphicsDescriptorPool()) {
        return false;
    }
    if (!createGraphicsDescriptorSets()) {
        return false;
    }

    for (int i = 0; i < NUM_PLANES; ++i) {
        if (!transitionImageLayout(textureImage[i], (VkFormat)0, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
            return false;
        }
    }

    VNLogDebug("render init complete");

    m_isInitialised = true;
    m_pendingSurfaceRecreate = false;

    return true;
}

std::vector<const char*> RenderVulkan::getRenderingExtensions()
{
    std::vector<const char*> extensions;
#if defined(USE_GLFW)
    uint32_t glfwExtensionCount{};
    const char** glfwExtensions{};
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    extensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
#else
    extensions.push_back("VK_KHR_surface");
    extensions.push_back("VK_KHR_android_surface");
#endif

    return extensions;
}

bool RenderVulkan::initWindow(bool create)
{
#if defined(USE_GLFW)
    if (!m_glfwInit) {
        glfwInit();
        m_glfwInit = true;
    }
    if (!create) {
        return true; // TODO
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan DEC", nullptr, nullptr);
    if (!m_window) {
        return false;
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
#endif
    return true;
}

#if defined(USE_GLFW)
void RenderVulkan::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<RenderVulkan*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}
#endif

void RenderVulkan::destroy()
{
    vkDeviceWaitIdle(m_backend.getDevice());

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_backend.getDevice(), m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_backend.getDevice(), m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_backend.getDevice(), m_inFlightFences[i], nullptr);
    }

    vkFreeCommandBuffers(m_backend.getDevice(), m_graphicsCommandPool,
                         static_cast<uint32_t>(m_graphicsCommandBuffers.size()),
                         m_graphicsCommandBuffers.data());
    vkDestroyCommandPool(m_backend.getDevice(), m_graphicsCommandPool, nullptr);

    cleanupSwapChain();

    for (int i = 0; i < NUM_PLANES; i++) {
        vkDestroySampler(m_backend.getDevice(), m_textureSampler[i], nullptr);
    }
    destroyTextureResources();

    vkDestroyDescriptorSetLayout(m_backend.getDevice(), m_graphicsDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_backend.getDevice(), m_graphicsDescriptorPool, nullptr);

    vkDestroyPipeline(m_backend.getDevice(), m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_backend.getDevice(), m_graphicsPipelineLayout, nullptr);
    vkDestroyRenderPass(m_backend.getDevice(), m_renderPass, nullptr);

    vmaDestroyBuffer(m_backend.getVmaAllocator(), m_indexBuffer, m_indexBufferAllocation);
    vmaDestroyBuffer(m_backend.getVmaAllocator(), m_vertexBuffer, m_vertexBufferAllocation);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaUnmapMemory(m_backend.getVmaAllocator(), m_uniformBuffersAllocations[i]);
        vmaDestroyBuffer(m_backend.getVmaAllocator(), m_uniformBuffers[i], m_uniformBuffersAllocations[i]);
    }
    destroyDitherBuffers();

    vkDestroySurfaceKHR(m_backend.getInstance(), m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
    m_swapChain = VK_NULL_HANDLE;
    m_isInitialised = false;
    m_pendingSurfaceRecreate = false;
}

bool RenderVulkan::createSurface()
{
#if VN_OS(ANDROID)
    if (m_window == nullptr) {
        VNLogError("failed to create Android surface: window is null");
        return false;
    }
    if (m_surface != VK_NULL_HANDLE) {
        return true;
    }
    const VkAndroidSurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                                                       nullptr, 0, m_window};

    if (vkCreateAndroidSurfaceKHR(m_backend.getInstance(), &create_info, nullptr, &m_surface) != VK_SUCCESS) {
        VNLogError("failed to create Android surface!");
        return false;
    }
    return true;
#elif defined(USE_GLFW)
    if (glfwCreateWindowSurface(m_backend.getInstance(), m_window, nullptr, &m_surface) != VK_SUCCESS) {
        VNLogError("failed to create glfw surface!");
        return false;
    }
    return true;
#endif
    return false;
}

SwapChainSupportDetails RenderVulkan::querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount,
                                                  details.presentModes.data());
    }

    return details;
}

bool RenderVulkan::createSwapChainAndGetImages()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_backend.getPhysicalDevice());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
#if VN_OS(ANDROID)
    int winWidth = 0;
    int winHeight = 0;
    getWindowSize(winWidth, winHeight);
    VNLogDebug("swapchain extent=%ux%u window=%dx%d", extent.width, extent.height, winWidth, winHeight);
#endif

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = m_backend.getQueueFamilies();
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR preferredAlphas[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};

    for (auto alpha : preferredAlphas) {
        if (swapChainSupport.capabilities.supportedCompositeAlpha & alpha) {
            compositeAlpha = alpha;
            break;
        }
    }
    createInfo.compositeAlpha = compositeAlpha;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_backend.getDevice(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        VNLogError("failed to create swap chain!");
        return false;
    }

    vkGetSwapchainImagesKHR(m_backend.getDevice(), m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_backend.getDevice(), m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
    m_lastWindowWidth = static_cast<int>(extent.width);
    m_lastWindowHeight = static_cast<int>(extent.height);

    return true;
}

void RenderVulkan::recreateSwapChain()
{
#if !VN_OS(ANDROID) && defined(USE_GLFW)
    int width{};
    int height{};
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }
#endif
    vkDeviceWaitIdle(m_backend.getDevice());
    cleanupSwapChain();
    createSwapChainAndGetImages();
    createSwapChainImageViews();
    createFramebuffers();
}

void RenderVulkan::cleanupSwapChain()
{
    if (m_swapChain == VK_NULL_HANDLE) {
        m_swapChainImages.clear();
        m_swapChainImageViews.clear();
        m_swapChainFramebuffers.clear();
        return;
    }
    for (auto& swapChainFramebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_backend.getDevice(), swapChainFramebuffer, nullptr);
    }

    for (auto& swapChainImageView : m_swapChainImageViews) {
        vkDestroyImageView(m_backend.getDevice(), swapChainImageView, nullptr);
    }

    vkDestroySwapchainKHR(m_backend.getDevice(), m_swapChain, nullptr);
    m_swapChain = VK_NULL_HANDLE;
}

bool RenderVulkan::createSwapChainImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());
    int i{};
    for (auto& swapChainImage : m_swapChainImages) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImage;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_backend.getDevice(), &createInfo, nullptr, &m_swapChainImageViews[i]) !=
            VK_SUCCESS) {
            VNLogError("failed to create image views!");
            return false;
        }
        ++i;
    }

    return true;
}

bool RenderVulkan::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_backend.getDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        VNLogError("failed to create render pass!");
        return false;
    }

    return true;
}

VkShaderModule RenderVulkan::createShaderModule(const unsigned char* code, size_t size)
{
#if VN_SDK_FEATURE(MINIMUM_LIB_SIZE)
    std::vector<uint8_t> decompressed;
    if (size >= 4) {
        uint32_t firstWord;
        memcpy(&firstWord, code, 4);
        if (firstWord != 0x07230203u) {
            decompressed.resize(firstWord);
            uLongf len = firstWord;
            if (uncompress(decompressed.data(), &len, code + 4, static_cast<uLong>(size - 4)) != Z_OK) {
                VNLogError("failed to decompress shader module!");
                return VK_NULL_HANDLE;
            }
            code = reinterpret_cast<const unsigned char*>(decompressed.data());
            size = static_cast<size_t>(len);
        }
    }
#endif

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code);

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_backend.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        VNLogError("failed to create shader module!");
    }

    return shaderModule;
}

bool RenderVulkan::createGraphicsDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBindingU{};
    samplerLayoutBindingU.binding = 2;
    samplerLayoutBindingU.descriptorCount = 1;
    samplerLayoutBindingU.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBindingU.pImmutableSamplers = nullptr;
    samplerLayoutBindingU.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBindingV{};
    samplerLayoutBindingV.binding = 3;
    samplerLayoutBindingV.descriptorCount = 1;
    samplerLayoutBindingV.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBindingV.pImmutableSamplers = nullptr;
    samplerLayoutBindingV.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding ditherEntropyBinding{};
    ditherEntropyBinding.binding = 4;
    ditherEntropyBinding.descriptorCount = 1;
    ditherEntropyBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ditherEntropyBinding.pImmutableSamplers = nullptr;
    ditherEntropyBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding ditherRowOffsetBinding{};
    ditherRowOffsetBinding.binding = 5;
    ditherRowOffsetBinding.descriptorCount = 1;
    ditherRowOffsetBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ditherRowOffsetBinding.pImmutableSamplers = nullptr;
    ditherRowOffsetBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 6> bindings = {
        uboLayoutBinding,      samplerLayoutBinding, samplerLayoutBindingU,
        samplerLayoutBindingV, ditherEntropyBinding, ditherRowOffsetBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_backend.getDevice(), &layoutInfo, nullptr,
                                    &m_graphicsDescriptorSetLayout) != VK_SUCCESS) {
        VNLogError("failed to create descriptor set layout!");
        return false;
    }

    return true;
}

bool RenderVulkan::createGraphicsPipeline()
{
    VkShaderModule vertShaderModule = createShaderModule(render_vertex_spv, sizeof(render_vertex_spv));
    VkShaderModule fragShaderModule =
        createShaderModule(render_fragment_spv, sizeof(render_fragment_spv));

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_graphicsDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(m_backend.getDevice(), &pipelineLayoutInfo, nullptr,
                               &m_graphicsPipelineLayout) != VK_SUCCESS) {
        VNLogError("failed to create pipeline layout!");
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_graphicsPipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_backend.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_graphicsPipeline) != VK_SUCCESS) {
        VNLogError("failed to create graphics pipeline!");
        return false;
    }

    vkDestroyShaderModule(m_backend.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_backend.getDevice(), vertShaderModule, nullptr);

    return true;
}

bool RenderVulkan::createFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {m_swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_backend.getDevice(), &framebufferInfo, nullptr,
                                &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            VNLogError("failed to create framebuffer!");
            return false;
        }
    }

    return true;
}

bool RenderVulkan::createGraphicsCommandPoolAndBuffer()
{
    QueueFamilyIndices queueFamilyIndices = m_backend.getQueueFamilies();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_backend.getDevice(), &poolInfo, nullptr, &m_graphicsCommandPool) != VK_SUCCESS) {
        VNLogError("failed to create command pool!");
        return false;
    }

    m_graphicsCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_graphicsCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_graphicsCommandBuffers.size();

    if (vkAllocateCommandBuffers(m_backend.getDevice(), &allocInfo, m_graphicsCommandBuffers.data()) !=
        VK_SUCCESS) {
        VNLogError("failed to allocate command buffers!");
        return false;
    }

    return true;
}

bool RenderVulkan::recordRenderCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        VNLogError("failed to begin recording command buffer!");
        return false;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapChainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    VkBuffer vertexBuffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChainExtent.width);
    viewport.height = static_cast<float>(m_swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout,
                            0, 1, &m_graphicsDescriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(g_indices.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to record command buffer!");
        return false;
    }

    return true;
}

bool RenderVulkan::createSyncObjects()
{
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_backend.getDevice(), &semaphoreInfo, nullptr,
                              &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_backend.getDevice(), &semaphoreInfo, nullptr,
                              &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_backend.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            VNLogError("failed to create synchronization objects for a frame!");
            return false;
        }
    }

    return true;
}

bool RenderVulkan::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(g_vertices[0]) * g_vertices.size();
    if (createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                     m_vertexBuffer, m_vertexBufferAllocation) != VK_SUCCESS) {
        return false;
    }

    if (vmaCopyMemoryToAllocation(m_backend.getVmaAllocator(), g_vertices.data(),
                                  m_vertexBufferAllocation, 0, bufferSize) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool RenderVulkan::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(g_indices[0]) * g_indices.size();

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    if (createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                     stagingBuffer, stagingBufferAllocation) != VK_SUCCESS) {
        return false;
    }

    if (createBuffer(bufferSize,
                     static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer,
                     m_indexBufferAllocation) != VK_SUCCESS) {
        return false;
    }

    if (vmaCopyMemoryToAllocation(m_backend.getVmaAllocator(), g_indices.data(),
                                  stagingBufferAllocation, 0, bufferSize) != VK_SUCCESS) {
        return false;
    }

    if (!copyBuffer(stagingBuffer, m_indexBuffer, bufferSize)) {
        return false;
    }

    vmaDestroyBuffer(m_backend.getVmaAllocator(), stagingBuffer, stagingBufferAllocation);

    return true;
}

VkResult RenderVulkan::createBuffer(VkDeviceSize bufferSize, VkBufferUsageFlagBits usage,
                                    VkMemoryPropertyFlagBits memoryType, VkBuffer& buffer,
                                    VmaAllocation& vmaAllocation)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    allocationCreateInfo.requiredFlags = memoryType;

    return vmaCreateBuffer(m_backend.getVmaAllocator(), &bufferInfo, &allocationCreateInfo, &buffer,
                           &vmaAllocation, nullptr);
}

bool RenderVulkan::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersAllocations.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                         m_uniformBuffers[i], m_uniformBuffersAllocations[i]) != VK_SUCCESS) {
            return false;
        }

        if (vmaMapMemory(m_backend.getVmaAllocator(), m_uniformBuffersAllocations[i],
                         &m_uniformBuffersMapped[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

bool RenderVulkan::createDitherBuffers()
{
    m_ditherBufferSize = static_cast<int>(kDitherBufferSize);
    const VkDeviceSize entropySize = static_cast<VkDeviceSize>(m_ditherBufferSize) * sizeof(uint32_t);
    if (createBuffer(entropySize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                     m_ditherEntropyBuffer, m_ditherEntropyAllocation) != VK_SUCCESS) {
        return false;
    }
    if (vmaMapMemory(m_backend.getVmaAllocator(), m_ditherEntropyAllocation, &m_ditherEntropyMapped) !=
        VK_SUCCESS) {
        return false;
    }

    const uint32_t height = m_swapChainExtent.height;
    const VkDeviceSize rowOffsetSize = static_cast<VkDeviceSize>(height) * NUM_PLANES * sizeof(uint32_t);
    if (createBuffer(rowOffsetSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                     m_ditherRowOffsetBuffer, m_ditherRowOffsetAllocation) != VK_SUCCESS) {
        return false;
    }
    if (vmaMapMemory(m_backend.getVmaAllocator(), m_ditherRowOffsetAllocation,
                     &m_ditherRowOffsetMapped) != VK_SUCCESS) {
        return false;
    }
    m_ditherRowOffsetCapacity = height;

    return true;
}

void RenderVulkan::destroyDitherBuffers()
{
    if (m_ditherEntropyBuffer != VK_NULL_HANDLE) {
        if (m_ditherEntropyMapped) {
            vmaUnmapMemory(m_backend.getVmaAllocator(), m_ditherEntropyAllocation);
            m_ditherEntropyMapped = nullptr;
        }
        vmaDestroyBuffer(m_backend.getVmaAllocator(), m_ditherEntropyBuffer, m_ditherEntropyAllocation);
        m_ditherEntropyBuffer = VK_NULL_HANDLE;
        m_ditherEntropyAllocation = VK_NULL_HANDLE;
    }
    if (m_ditherRowOffsetBuffer != VK_NULL_HANDLE) {
        if (m_ditherRowOffsetMapped) {
            vmaUnmapMemory(m_backend.getVmaAllocator(), m_ditherRowOffsetAllocation);
            m_ditherRowOffsetMapped = nullptr;
        }
        vmaDestroyBuffer(m_backend.getVmaAllocator(), m_ditherRowOffsetBuffer, m_ditherRowOffsetAllocation);
        m_ditherRowOffsetBuffer = VK_NULL_HANDLE;
        m_ditherRowOffsetAllocation = VK_NULL_HANDLE;
        m_ditherRowOffsetCapacity = 0;
    }
}

bool RenderVulkan::ensureDitherRowOffsetCapacity(uint32_t height)
{
    if (height <= m_ditherRowOffsetCapacity) {
        return true;
    }

    if (m_ditherRowOffsetMapped) {
        vmaUnmapMemory(m_backend.getVmaAllocator(), m_ditherRowOffsetAllocation);
        m_ditherRowOffsetMapped = nullptr;
    }
    if (m_ditherRowOffsetBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_backend.getVmaAllocator(), m_ditherRowOffsetBuffer, m_ditherRowOffsetAllocation);
    }

    const VkDeviceSize rowOffsetSize = static_cast<VkDeviceSize>(height) * NUM_PLANES * sizeof(uint32_t);
    if (createBuffer(rowOffsetSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
                     m_ditherRowOffsetBuffer, m_ditherRowOffsetAllocation) != VK_SUCCESS) {
        return false;
    }
    if (vmaMapMemory(m_backend.getVmaAllocator(), m_ditherRowOffsetAllocation,
                     &m_ditherRowOffsetMapped) != VK_SUCCESS) {
        return false;
    }
    m_ditherRowOffsetCapacity = height;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo rowOffsetInfo{};
        rowOffsetInfo.buffer = m_ditherRowOffsetBuffer;
        rowOffsetInfo.offset = 0;
        rowOffsetInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_graphicsDescriptorSets[i];
        descriptorWrite.dstBinding = 5;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &rowOffsetInfo;

        vkUpdateDescriptorSets(m_backend.getDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    return true;
}

void RenderVulkan::updateUniformBuffer(uint32_t currentImage)
{
    UniformBufferObject ubo{};
    ubo.bit8 = m_renderBit8;
    ubo.nv12 = m_renderNv12;
    ubo.srcWidth = static_cast<int>(m_textureWidth);
    ubo.srcHeight = static_cast<int>(m_textureHeight);
    ubo.ditherStrength = m_ditherStrength;
    ubo.ditherEnabled = m_ditherEnabled;
    ubo.ditherShift = m_ditherShift;
    ubo.ditherBufferSize = m_ditherBufferSize;
    ubo.limitedRange = m_limitedRange;

#if VN_OS(ANDROID)
    float c = 0.0f;
    float s = 0.0f;

    switch (m_rotation) {
        case 0:
            c = 1.0f;
            s = 0.0f;
            break;
        case 90:
            c = 0.0f;
            s = 1.0f;
            break;
        case 180:
            c = -1.0f;
            s = 0.0f;
            break;
        case 270:
            c = 0.0f;
            s = -1.0f;
            break;
    }

    ubo.modelViewProjection[0][0] = c;
    ubo.modelViewProjection[0][1] = s;
    ubo.modelViewProjection[1][0] = -s;
    ubo.modelViewProjection[1][1] = c;
    ubo.modelViewProjection[2][2] = 1.0f;
    ubo.modelViewProjection[3][3] = 1.0f;
#endif
    memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

bool RenderVulkan::createGraphicsDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 6> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[3].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[4].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[5].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(m_backend.getDevice(), &poolInfo, nullptr, &m_graphicsDescriptorPool) !=
        VK_SUCCESS) {
        VNLogError("failed to create descriptor pool!");
        return false;
    }

    return true;
}

bool RenderVulkan::createGraphicsDescriptorSets()
{
    if (m_graphicsDescriptorSets.empty()) {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_graphicsDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_graphicsDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        m_graphicsDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(m_backend.getDevice(), &allocInfo,
                                     m_graphicsDescriptorSets.data()) != VK_SUCCESS) {
            VNLogError("failed to allocate descriptor sets!");
            return false;
        }
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_textureImageView[0];
        imageInfo.sampler = m_textureSampler[0];

        VkDescriptorImageInfo imageInfoU{};
        imageInfoU.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoU.imageView = m_textureImageView[1];
        imageInfoU.sampler = m_textureSampler[1];

        VkDescriptorImageInfo imageInfoV{};
        imageInfoV.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoV.imageView = m_textureImageView[2];
        imageInfoV.sampler = m_textureSampler[2];

        VkDescriptorBufferInfo ditherEntropyInfo{};
        ditherEntropyInfo.buffer = m_ditherEntropyBuffer;
        ditherEntropyInfo.offset = 0;
        ditherEntropyInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo ditherRowOffsetInfo{};
        ditherRowOffsetInfo.buffer = m_ditherRowOffsetBuffer;
        ditherRowOffsetInfo.offset = 0;
        ditherRowOffsetInfo.range = VK_WHOLE_SIZE;

        std::array<VkWriteDescriptorSet, 6> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_graphicsDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_graphicsDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_graphicsDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &imageInfoU;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = m_graphicsDescriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &imageInfoV;

        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = m_graphicsDescriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &ditherEntropyInfo;

        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = m_graphicsDescriptorSets[i];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pBufferInfo = &ditherRowOffsetInfo;

        vkUpdateDescriptorSets(m_backend.getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }

    return true;
}

void RenderVulkan::updateDitherState(const LdppDitherGlobal* global, const LdppDitherFrame* frame)
{
    if (!global || !global->buffer || !m_ditherEntropyMapped || !m_ditherRowOffsetMapped) {
        m_ditherStrength = 0;
        m_ditherEnabled = 0;
        return;
    }

    const uint32_t width = m_textureWidth;
    const uint32_t height = m_textureHeight;
    if (width == 0 || height == 0) {
        m_ditherStrength = 0;
        m_ditherEnabled = 0;
        return;
    }

    ensureDitherRowOffsetCapacity(height);

    if (!populateDitherBuffers(global, frame, width, height, static_cast<uint32_t>(m_ditherBufferSize),
                               static_cast<uint32_t*>(m_ditherEntropyMapped),
                               static_cast<uint32_t*>(m_ditherRowOffsetMapped))) {
        m_ditherStrength = 0;
        m_ditherEnabled = 0;
        return;
    }

    m_ditherStrength = frame->strength;
    m_ditherEnabled = 1;
    m_ditherShift = 0;
}

bool RenderVulkan::populateDitherBuffers(const LdppDitherGlobal* global, const LdppDitherFrame* frame,
                                         uint32_t width, uint32_t height, uint32_t ditherBufferSize,
                                         uint32_t* entropyDst, uint32_t* rowOffsets)
{
    if (!global || !global->buffer || !entropyDst || !rowOffsets) {
        return false;
    }
    if (!frame || frame->strength == 0) {
        return false;
    }
    if (width == 0 || height == 0 || ditherBufferSize == 0 || ditherBufferSize > kDitherBufferSize) {
        return false;
    }

    const uint32_t requiredLength = width << 2;
    if (requiredLength > kDitherBufferSize) {
        return false;
    }

    for (uint32_t i = 0; i < ditherBufferSize; ++i) {
        entropyDst[i] = static_cast<uint32_t>(global->buffer[i]);
    }

    for (uint32_t plane = 0; plane < NUM_PLANES; ++plane) {
        for (uint32_t y = 0; y < height; ++y) {
            LdppDitherSlice slice{};
            ldppDitherSliceInitialise(&slice, frame, y, plane);
            const uint16_t* buffer = ldppDitherGetBuffer(&slice, requiredLength);
            if (!buffer) {
                return false;
            }
            rowOffsets[plane * height + y] = static_cast<uint32_t>(buffer - global->buffer);
        }
    }

    return true;
}

bool RenderVulkan::createTextureImages(int width, int height)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM; // TODO m_renderBit8 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R16_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(m_backend.getDevice(), &imageInfo, nullptr, &textureImage[0]) != VK_SUCCESS) {
        VNLogError("failed to create Y image!");
        return false;
    }

    imageInfo.extent.width = static_cast<uint32_t>(width / 2);
    imageInfo.extent.height = static_cast<uint32_t>(height / 2);
    if (vkCreateImage(m_backend.getDevice(), &imageInfo, nullptr, &textureImage[1]) != VK_SUCCESS) {
        VNLogError("failed to create U image!");
        return false;
    }

    if (vkCreateImage(m_backend.getDevice(), &imageInfo, nullptr, &textureImage[2]) != VK_SUCCESS) {
        VNLogError("failed to create V image!");
        return false;
    }

    for (int i = 0; i < NUM_PLANES; ++i) {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_backend.getDevice(), textureImage[i], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_backend.findMemoryType(memRequirements.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_backend.getDevice(), &allocInfo, nullptr, &textureImageMemory[i]) !=
            VK_SUCCESS) {
            VNLogError("failed to allocate image memory!");
            return false;
        }

        vkBindImageMemory(m_backend.getDevice(), textureImage[i], textureImageMemory[i], 0);
    }

    m_textureWidth = static_cast<uint32_t>(width);
    m_textureHeight = static_cast<uint32_t>(height);

    return true;
}

bool RenderVulkan::createTextureImageViews()
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = textureImage[0];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM; // TODO m_renderBit8 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R16_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_backend.getDevice(), &viewInfo, nullptr, &m_textureImageView[0]) != VK_SUCCESS) {
        VNLogError("failed to create Y texture image view!");
        return false;
    }

    viewInfo.image = textureImage[1];
    if (vkCreateImageView(m_backend.getDevice(), &viewInfo, nullptr, &m_textureImageView[1]) != VK_SUCCESS) {
        VNLogError("failed to create U texture image view!");
        return false;
    }

    viewInfo.image = textureImage[2];
    if (vkCreateImageView(m_backend.getDevice(), &viewInfo, nullptr, &m_textureImageView[2]) != VK_SUCCESS) {
        VNLogError("failed to create V texture image view!");
        return false;
    }

    return true;
}

void RenderVulkan::destroyTextureResources()
{
    for (int i = 0; i < NUM_PLANES; ++i) {
        if (m_textureImageView[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(m_backend.getDevice(), m_textureImageView[i], nullptr);
            m_textureImageView[i] = VK_NULL_HANDLE;
        }
        if (textureImage[i] != VK_NULL_HANDLE) {
            vkDestroyImage(m_backend.getDevice(), textureImage[i], nullptr);
            textureImage[i] = VK_NULL_HANDLE;
        }
        if (textureImageMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(m_backend.getDevice(), textureImageMemory[i], nullptr);
            textureImageMemory[i] = VK_NULL_HANDLE;
        }
    }

    m_textureWidth = 0;
    m_textureHeight = 0;
}

bool RenderVulkan::createTextureSamplers()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_backend.getPhysicalDevice(), &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_backend.getDevice(), &samplerInfo, nullptr, &m_textureSampler[0]) != VK_SUCCESS) {
        VNLogError("failed to create Y texture sampler!");
        return false;
    }

    if (vkCreateSampler(m_backend.getDevice(), &samplerInfo, nullptr, &m_textureSampler[1]) != VK_SUCCESS) {
        VNLogError("failed to create U texture sampler!");
        return false;
    }

    if (vkCreateSampler(m_backend.getDevice(), &samplerInfo, nullptr, &m_textureSampler[2]) != VK_SUCCESS) {
        VNLogError("failed to create V texture sampler!");
        return false;
    }

    return true;
}

bool RenderVulkan::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                                         VkImageLayout newLayout)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_graphicsCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(m_backend.getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to allocate command buffer!");
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage{};
    VkPipelineStageFlags destinationStage{};

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        VNLogError("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkCreateFence(m_backend.getDevice(), &fenceInfo, nullptr, &fence);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_backend.getGraphicsQueue(), 1, &submitInfo, fence);
    vkWaitForFences(m_backend.getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);

    vkFreeCommandBuffers(m_backend.getDevice(), m_graphicsCommandPool, 1, &commandBuffer);
    vkDestroyFence(m_backend.getDevice(), fence, nullptr);

    return true;
}

bool RenderVulkan::copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size,
                              VkCommandBuffer& commandBuffer)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        VNLogError("failed to begin command buffer");
        return false;
    }

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to end command buffer");
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(m_backend.getComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        VNLogError("failed to submit command buffer to queue");
        return false;
    }
    vkQueueWaitIdle(m_backend.getComputeQueue());

    vkResetCommandBuffer(commandBuffer, 0);

    return true;
}

bool RenderVulkan::copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_graphicsCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(m_backend.getDevice(), &commandBufferAllocateInfo, &commandBuffer) !=
        VK_SUCCESS) {
        VNLogError("failed to allocate command buffer");
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        VNLogError("failed to begin command buffer");
        return false;
    }

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to end command buffer");
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(m_backend.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        VNLogError("failed to submit command buffer to queue");
        return false;
    }
    vkQueueWaitIdle(m_backend.getGraphicsQueue());

    vkFreeCommandBuffers(m_backend.getDevice(), m_graphicsCommandPool, 1, &commandBuffer);

    return true;
}

VkSurfaceFormatKHR RenderVulkan::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR RenderVulkan::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RenderVulkan::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width = 0;
        int height = 0;

#if VN_OS(ANDROID)
        width = ANativeWindow_getWidth(m_window);
        height = ANativeWindow_getHeight(m_window);
#elif defined(USE_GLFW)
        glfwGetFramebufferSize(m_window, &width, &height);
#endif
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void RenderVulkan::drawFrame()
{
    if (!m_isInitialised) {
        if (!m_backend.initRemaningRender()) {
            return;
        }
    }
    if (!windowReady()) {
        releaseSurfaceAndSwapChain();
        return;
    }
#if VN_OS(ANDROID)
    int winWidth = 0;
    int winHeight = 0;
    getWindowSize(winWidth, winHeight);
    if (winWidth == 0 || winHeight == 0) {
        return;
    }
    if (winWidth != m_lastWindowWidth || winHeight != m_lastWindowHeight) {
        m_lastWindowWidth = winWidth;
        m_lastWindowHeight = winHeight;
        framebufferResized = true;
    }
#endif
    vkWaitForFences(m_backend.getDevice(), 1, &m_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result =
        vkAcquireNextImageKHR(m_backend.getDevice(), m_swapChain, UINT64_MAX,
                              m_imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        VNLogError("failed to acquire swap chain image!");
    }

    vkResetFences(m_backend.getDevice(), 1, &m_inFlightFences[currentFrame]);

    updateUniformBuffer(currentFrame);

    vkResetCommandBuffer(m_graphicsCommandBuffers[currentFrame], 0);
    recordRenderCommandBuffer(m_graphicsCommandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_graphicsCommandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    result = vkQueueSubmit(m_backend.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[currentFrame]);

    if (result != VK_SUCCESS) {
        VNLogError("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_backend.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        VNLogError("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool RenderVulkan::resetSurfaceAndSwapChain()
{
    if (!windowReady()) {
        return false;
    }
    vkDeviceWaitIdle(m_backend.getDevice());
    cleanupSwapChain();
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_backend.getInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (!createSurface()) {
        return false;
    }
    recreateSwapChain();
    m_pendingSurfaceRecreate = false;
    return true;
}

void RenderVulkan::closeWindow()
{
#if VN_OS(ANDROID)
    setAndroidWindow(nullptr);
#elif defined(USE_GLFW)
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    m_pendingSurfaceRecreate = true;
    framebufferResized = true;
#endif
}

void RenderVulkan::releaseSurfaceAndSwapChain()
{
    if (m_surface == VK_NULL_HANDLE && m_swapChain == VK_NULL_HANDLE) {
        return;
    }
    vkDeviceWaitIdle(m_backend.getDevice());
    cleanupSwapChain();
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_backend.getInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_swapChain = VK_NULL_HANDLE;
    m_pendingSurfaceRecreate = true;
    framebufferResized = true;
}

void RenderVulkan::ensureTextureResources(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    if (m_textureWidth == width && m_textureHeight == height && textureImage[0] != VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(m_backend.getDevice());

    destroyTextureResources();
    createTextureImages(static_cast<int>(width), static_cast<int>(height));
    createTextureImageViews();

    for (int i = 0; i < NUM_PLANES; ++i) {
        transitionImageLayout(textureImage[i], (VkFormat)0, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    createGraphicsDescriptorSets();
}

} // namespace lcevc_dec::pipeline_vulkan
