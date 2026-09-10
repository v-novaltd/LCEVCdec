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

#ifndef VN_LCEVC_PIPELINE_VULKAN_RENDER_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_RENDER_VULKAN_H

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/log.h>

#if defined(VN_SDK_EXECUTABLES)
#define USE_GLFW
#endif

#if VN_OS(ANDROID)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <android/native_window.h>
#include <jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#elif defined(USE_GLFW)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#else
#include <vulkan/vulkan.h>
#endif

#include <vk_mem_alloc.h>

#include <vector>

struct LdppDitherGlobal;
struct LdppDitherFrame;
struct LdpPictureLayout;

namespace lcevc_dec::pipeline_vulkan {

class BackendVulkan;
class ComputeVulkan;
class RenderVulkanDitherTestAccess;

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class RenderVulkan
{
public:
    static constexpr int NUM_PLANES = 3;

    RenderVulkan(BackendVulkan& backend)
        : m_backend(backend)
    {}
    ~RenderVulkan() = default;

    bool init();
    bool initWindow(bool create = true);
    void destroy();
    bool isInitialised() const { return m_isInitialised; }
    void setRotation(uint32_t rotation) { m_rotation = rotation; }

    std::vector<const char*> getRenderingInstanceExtensions() const;
    std::vector<const char*> getRenderingDeviceExtensions() const;

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    bool createSurface();
    bool resetSurfaceAndSwapChain();
    void releaseSurfaceAndSwapChain();

    VkSurfaceKHR& getSurface() { return m_surface; }

    void drawFrame();
    void closeWindow();
    bool ensureTextureResources(uint32_t width, uint32_t height);
    bool mapTexturePlanes(const uint8_t* source, const LdpPictureLayout& layout, uint32_t width,
                          uint32_t height);
    bool waitForTextureUpload();

    bool windowReady() const
    {
#if VN_OS(ANDROID) || defined(USE_GLFW)
        return m_window != nullptr;
#else
        return false;
#endif
    }

    void setAndroidWindow(void* newWindow)
    {
#if VN_OS(ANDROID)
        if (m_window == newWindow) {
            return;
        }
        if (newWindow == nullptr) {
            m_window = nullptr;
            m_pendingSurfaceRecreate = true;
            m_framebufferResized = true;
            return;
        }
        m_window = (ANativeWindow*)(newWindow);
        m_pendingSurfaceRecreate = true;
        m_framebufferResized = true;
        VNLogDebug("got window! ptr=%p", (void*)m_window);
#endif
    };

    bool needsSurfaceRecreate() const { return m_pendingSurfaceRecreate; }
    void clearSurfaceRecreateFlag() { m_pendingSurfaceRecreate = false; }

#if VN_OS(ANDROID)
    bool getWindowSize(int& width, int& height) const
    {
        if (m_window == nullptr) {
            width = 0;
            height = 0;
            return false;
        }
        width = ANativeWindow_getWidth(m_window);
        height = ANativeWindow_getHeight(m_window);
        return true;
    }

    ANativeWindow* getWindow() { return m_window; }
#elif defined(USE_GLFW)
    GLFWwindow* getWindow() { return m_window; }
#endif

    void setRenderFormat(bool bit8, bool nv12)
    {
        m_renderBit8 = bit8 ? 1 : 0;
        m_renderNv12 = nv12 ? 1 : 0;
    }

    void setLimitedRange(bool limited) { m_limitedRange = limited ? 1 : 0; }

    void updateDitherState(const LdppDitherGlobal* global, const LdppDitherFrame* frame);

    VNNoCopyNoMove(RenderVulkan);

private:
    friend class RenderVulkanDitherTestAccess;

    // Render
    bool createSwapChainAndGetImages();
    bool createSwapChainImageViews();
    bool createRenderPass();
    bool createGraphicsDescriptorSetLayout();
    bool createGraphicsPipeline();
    bool createFramebuffers();
    bool createGraphicsCommandPoolAndBuffer();
    bool createVertexBuffer();
    bool createIndexBuffer();
    bool createUniformBuffers();
    bool createSyncObjects();
    bool createTextureImages(int w, int h);
    bool createTextureImageViews();
    bool createTextureSamplers();
    void destroyTextureResources();
    bool createGraphicsDescriptorPool();
    bool createGraphicsDescriptorSets();
    bool recordRenderCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    bool createDitherBuffers();
    void destroyDitherBuffers();
    bool ensureDitherRowOffsetCapacity(uint32_t height);
    static bool populateDitherBuffers(const LdppDitherGlobal* global, const LdppDitherFrame* frame,
                                      uint32_t width, uint32_t height, uint32_t ditherBufferSize,
                                      uint32_t* entropyDst, uint32_t* rowOffsets);

    VkShaderModule createShaderModule(const unsigned char* code, size_t size);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

#if defined(USE_GLFW)
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
#endif

    // Helpers - updating
    bool copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size,
                    VkCommandBuffer& commandBuffer);
    bool copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
    void recreateSwapChain();
    void cleanupSwapChain();
    void updateUniformBuffer(uint32_t currentImage);
    VkResult createBuffer(VkDeviceSize bufferSize, VkBufferUsageFlagBits usage,
                          VkMemoryPropertyFlagBits memoryType, VkBuffer& buffer,
                          VmaAllocation& vmaAllocation);

    bool transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout);

    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t m_currentFrame = 0;
    bool m_framebufferResized = false;
    bool m_isInitialised = false;
    bool m_pendingSurfaceRecreate = false;
    int m_lastWindowWidth = 0;
    int m_lastWindowHeight = 0;
    uint32_t m_rotation = 0;
    uint32_t m_textureWidth = 0;
    uint32_t m_textureHeight = 0;
    bool m_textureBit8 = true;

    VkImage m_textureImage[NUM_PLANES] = {};
    VkDeviceMemory m_textureImageMemory[NUM_PLANES] = {};

#if VN_OS(ANDROID)
    ANativeWindow* m_window = nullptr;
#elif defined(USE_GLFW)
    const uint32_t WIDTH = 1280; // default window size for glfw
    const uint32_t HEIGHT = 720;
    bool m_glfwInit = false;
    GLFWwindow* m_window = nullptr;
#endif

    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapChain{VK_NULL_HANDLE};
    std::vector<VkImage> m_swapChainImages;
    VkFormat m_swapChainImageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapChainExtent{};
    std::vector<VkImageView> m_swapChainImageViews;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_graphicsDescriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout m_graphicsPipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_graphicsPipeline{VK_NULL_HANDLE};
    VkCommandPool m_graphicsCommandPool{VK_NULL_HANDLE};
    VkDescriptorPool m_graphicsDescriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> m_graphicsDescriptorSets;
    std::vector<VkCommandBuffer> m_graphicsCommandBuffers;
    VkBuffer m_vertexBuffer{VK_NULL_HANDLE};
    VmaAllocation m_vertexBufferAllocation{VK_NULL_HANDLE};
    VkBuffer m_indexBuffer{VK_NULL_HANDLE};
    VmaAllocation m_indexBufferAllocation{VK_NULL_HANDLE};
    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VmaAllocation> m_uniformBuffersAllocations;
    std::vector<void*> m_uniformBuffersMapped;
    VkBuffer m_ditherEntropyBuffer = VK_NULL_HANDLE;
    VmaAllocation m_ditherEntropyAllocation = VK_NULL_HANDLE;
    void* m_ditherEntropyMapped = nullptr;
    VkBuffer m_ditherRowOffsetBuffer = VK_NULL_HANDLE;
    VmaAllocation m_ditherRowOffsetAllocation = VK_NULL_HANDLE;
    void* m_ditherRowOffsetMapped = nullptr;
    uint32_t m_ditherRowOffsetCapacity = 0;
    int m_renderBit8 = 1;
    int m_renderNv12 = 0;
    int m_limitedRange = 0;
    int m_ditherStrength = 0;
    int m_ditherEnabled = 0;
    int m_ditherShift = 0;
    int m_ditherBufferSize = 0;

    VkImageView m_textureImageView[NUM_PLANES] = {};
    VkSampler m_textureSampler[NUM_PLANES] = {};
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    VkFence m_textureInFlightFence{VK_NULL_HANDLE};

    BackendVulkan& m_backend;
};
} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_RENDER_VULKAN_H
