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

#ifndef VN_LCEVC_PIPELINE_VULKAN_PIPELINE_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_PIPELINE_VULKAN_H

// #define DEBUG_LOGGING

#include <vulkan/vulkan.h>

#if defined(ANDROID)
// #define ANDROID_BUFFERS
#include <android/log.h>
#include <vulkan/vulkan_android.h>

#if defined(ANDROID_BUFFERS)
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#endif

#if defined(DEBUG_LOGGING)
#define COUT_STR(x) __android_log_print(ANDROID_LOG_DEBUG, "vulkan_upscale", "%s", (x).c_str());
#else
#define COUT_STR(x)
#endif

#else

#if defined(DEBUG_LOGGING)
#define COUT_STR(x) std::cout << (x) << std::endl;
#else
#define COUT_STR(x)
#endif

#endif

#include "buffer_vulkan.h"
// #include "picture_vulkan.h"
#include "pipeline_builder_vulkan.h"

#include <LCEVC/common/constants.h>
#include <LCEVC/common/threads.h>
//
#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/ring_buffer.hpp>
#include <LCEVC/common/rolling_arena.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/common/threads.hpp>
#include <LCEVC/common/vector.hpp>
#include <LCEVC/enhancement/config_pool.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/frame.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pixel_processing/dither.h>
//
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Set to true to debug Vulkan core
const bool enableValidationLayers = false;

namespace lcevc_dec::pipeline_vulkan {

class BufferVulkan;
class FrameVulkan;
class PictureVulkan;

// Description of a temporal buffer
struct TemporalBufferDesc
{
    uint64_t timestamp;
    bool clear;
    uint32_t plane;
    uint32_t width;
    uint32_t height;
};

// Temporal buffer associated with pipeline
//
struct TemporalBuffer
{
    // Description of this buffer
    TemporalBufferDesc desc;

    // Timestamp upper limit that this buffer could fulfil
    uint64_t timestampLimit;

    // Frame that is using this buffer or null if available
    FrameVulkan* frame;
    // pointer/stride for buffer
    LdpPicturePlaneDesc planeDesc;
    // Buffer allocation
    LdcMemoryAllocation allocation;
};

struct VulkanConversionArgs
{
    PictureVulkan* src;
    PictureVulkan* dst;
    bool toInternal; /**< Indicates whether we are converting to or from internal */
};

struct VulkanBlitArgs
{
    PictureVulkan* src;
    PictureVulkan* dst;
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
};

struct VulkanApplyArgs
{
    PictureVulkan* plane;
    uint32_t planeWidth;
    uint32_t planeHeight;
    LdeCmdBufferGpu bufferGpu;
    bool highlightResiduals;
    bool temporalRefresh;
    bool tuRasterOrder;
};

// A base picture reference and other arguments from sendBase()
//
// Used for pending base pictures, before association with frames.
//
struct BasePicture
{
    uint64_t timestamp;
    LdpPicture* picture;
    uint64_t deadline;
    void* userData;
};

class PipelineVulkan : public pipeline::Pipeline
{
public:
    PipelineVulkan(const PipelineBuilderVulkan& builder, pipeline::EventSink* eventSink);
    ~PipelineVulkan() override;

    // Vulkan control
    bool init();
    bool blit(VulkanBlitArgs* params);
    bool conversion(VulkanConversionArgs* params);
    bool upscaleFrame(const LdeKernel* kernel, VulkanUpscaleArgs* params);
    bool upscaleVertical(const LdeKernel* kernel, VulkanUpscaleArgs* params);
    bool upscaleHorizontal(const LdeKernel* kernel, VulkanUpscaleArgs* params);
    void reset()
    {
        for (int i = 0; i < LOQEnhancedCount; ++i) {
            m_firstFrame[i] = true;
        }
        m_firstApply = true;
    }
    bool apply(VulkanApplyArgs* params);
    bool isInitialised() const { return m_initialized; }
    bool isRealGpu() const { return m_realGpu; }
    void destroy();

    // Vulkan getters/helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkDevice& getDevice() { return m_device; }
    uint32_t getQueueFamilyIndex() const { return m_queueFamilyIndex; }

    // Send/receive
    LdcReturnCode sendBasePicture(uint64_t timestamp, LdpPicture* basePicture, uint32_t timeoutUs,
                                  void* userData) override;
    LdcReturnCode sendEnhancementData(uint64_t timestamp, const uint8_t* data, uint32_t byteSize) override;
    LdcReturnCode sendOutputPicture(LdpPicture* outputPicture) override;

    LdpPicture* receiveOutputPicture(LdpDecodeInformation& decodeInfoOut) override;
    LdpPicture* receiveFinishedBasePicture() override;

    // "Trick-play"
    LdcReturnCode flush(uint64_t timestamp) override;
    LdcReturnCode peek(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut) override;
    LdcReturnCode skip(uint64_t timestamp) override;
    LdcReturnCode synchronize(bool dropPending) override;

    // Picture-handling
    LdpPicture* allocPictureManaged(const LdpPictureDesc& desc) override;
    LdpPicture* allocPictureExternal(const LdpPictureDesc& desc, const LdpPicturePlaneDesc* planeDescArr,
                                     const LdpPictureBufferDesc* buffer) override;

    void freePicture(LdpPicture* ldpPicture) override;

    // Accessors for use by frames
    const PipelineConfigVulkan& configuration() const { return m_configuration; }
    LdcMemoryAllocator* allocator() const { return m_allocator; }
    LdcTaskPool* taskPool() { return &m_taskPool; }
    LdppDitherGlobal* globalDitherBuffer() { return &m_dither; }

    // Buffer allocation
    BufferVulkan* allocateBuffer(uint32_t requiredSize);
    void releaseBuffer(BufferVulkan* buffer);

    // Picture allocation
    PictureVulkan* allocatePicture();
    void releasePicture(PictureVulkan* picture);

    //// Temporal buffer management

    // Mark a frame as needing a temporal buffer, given possible previous timestamp
    LdcTaskDependency requireTemporalBuffer(FrameVulkan* frame, uint64_t timestamp, uint32_t plane);

    // Mark the frame as having finished with it's temporal buffer
    void releaseTemporalBuffer(FrameVulkan* prevFrame, uint32_t plane);

    // Create task group for this frame
    void generateTasksEnhancement(FrameVulkan* frame, uint64_t previousTimestamp);

    void generateTasksPassthrough(FrameVulkan* frame);

    void updateTemporalBufferDesc(TemporalBuffer* buffer, const TemporalBufferDesc& desc) const;

    PictureVulkan* getTemporalPicture() { return m_temporalPicture; }

#ifdef VN_SDK_LOG_ENABLE_DEBUG
    // Write Debug log of current frame state
    void logFrames() const;
#endif

    VNNoCopyNoMove(PipelineVulkan);

private:
    // Vulkan core
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    bool createInstance();
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator);
    bool setupDebugMessenger();
    bool isDeviceSuitable(VkPhysicalDevice device);
    static VkPhysicalDevice pickBestDevice(const std::vector<VkPhysicalDevice>& devices);
    bool pickPhysicalDevice();
    bool createLogicalDeviceAndQueue();
    bool createBindingsAndPipelineLayout(uint32_t numBuffers, uint32_t pushConstantsSize,
                                         VkDescriptorSetLayout& setLayout,
                                         VkPipelineLayout& pipelineLayout);
    std::vector<char> readFile(const std::string& filename);
    bool createComputePipeline(const unsigned char* shaderName, size_t shaderSize,
                               VkPipelineLayout& layout, VkPipeline& pipe, int wgSize);

    bool allocateDescriptorSets();
    void updateComputeDescriptorSets(VkBuffer src, VkBuffer dst, VkDescriptorSet descriptorSet);
    void updateComputeDescriptorSets(VkBuffer src, VkBuffer dst1, VkBuffer dst2,
                                     VkDescriptorSet descriptorSet);
    bool createCommandPoolAndBuffer();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void* pUserData);

    void dispatchCompute(int width, int height, VkCommandBuffer& cmdBuf, int wgSize, int packDensity = 2);

    bool mapMemory(VkDeviceMemory& memory, uint8_t* mappedPtr);
    bool unmapMemory(VkDeviceMemory& memory, uint8_t* mappedPtr);

    const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

    const std::vector<const char*> deviceExtensions = {
#if defined(ANDROID)
    //"VK_ANDROID_external_memory_android_hardware_buffer",
    //"VK_EXT_queue_family_foreign"
#endif
    };
    static const int NUM_PLANES = 3;
    const int workGroupSize = 1;
    const int workGroupSizeDebug = 1;

    bool m_firstFrame[LOQEnhancedCount] = {true, true};
    bool m_firstApply = true;
    bool m_initialized = false;
    bool m_realGpu = false;
    uint8_t m_shift = 5;
    LdeChroma m_chroma = LdeChroma::CT420;

    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkPhysicalDevice m_physicalDevice;
    uint32_t m_queueFamilyIndex;
    VkDevice m_device;
    VkQueue m_queue;
    VkQueue m_queueIntermediate;
    VkDescriptorSetLayout m_setLayoutVertical;
    VkDescriptorSetLayout m_setLayoutHorizontal;
    VkDescriptorSetLayout m_setLayoutApply;
    VkDescriptorSetLayout m_setLayoutConversion;
    VkDescriptorSetLayout m_setLayoutBlit;
    VkPipelineLayout m_pipelineLayoutVertical;
    VkPipelineLayout m_pipelineLayoutHorizontal;
    VkPipelineLayout m_pipelineLayoutApply;
    VkPipelineLayout m_pipelineLayoutConversion;
    VkPipelineLayout m_pipelineLayoutBlit;
    VkPipeline m_pipelineVertical;
    VkPipeline m_pipelineHorizontal;
    VkPipeline m_pipelineApply;
    VkPipeline m_pipelineConversion;
    VkPipeline m_pipelineBlit;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSetSrcMid; // 2 buffers: 1.base -> VERTICAL -> 2.output
    VkDescriptorSet m_descriptorSetMidDst; // 3 buffers: 1.vertical + 2.base -> HORIZONTAL -> 3.output
    VkDescriptorSet m_descriptorSetApply;      // 2 buffers: 1.commands -> APPLY -> 2.output plane
    VkDescriptorSet m_descriptorSetConversion; // 2 buffers: 1.input -> CONVERSION -> 2.output plane
    VkDescriptorSet m_descriptorSetBlit;       // 2 buffers: 1.input -> BLIT -> 2.output plane
    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffer;
    VkCommandBuffer m_commandBufferIntermediate;

#if defined(ANDROID_BUFFERS)
    AHardwareBuffer* srcHardwareBuffer[NUM_PLANES];
    AHardwareBuffer* midHardwareBuffer[NUM_PLANES];
    AHardwareBuffer* dstHardwareBuffer[NUM_PLANES];
#endif

    // Plane for intermediate vertical upscale
    PictureVulkan* m_intermediateUpscalePicture[LOQEnhancedCount] = {};

    // Planes for apply
    PictureVulkan* m_temporalPicture = nullptr;

    friend PipelineBuilderVulkan;

    // Given a timestamp, either find existing frame, or create a new one
    FrameVulkan* allocateFrame(uint64_t timestamp);

    // Find the Frame associated with a timestamp, or NULL if none.
    FrameVulkan* findFrame(uint64_t timestamp);

    // Frame for given timestamp is finished - release resources
    void releaseFrame(uint64_t timestamp);
    void freeFrame(FrameVulkan* frame);

    // Get next frame reference following reorder or flushing rules
    FrameVulkan* getNextReordered();

    // Move frames from reorder table to generated tasks
    void startReadyFrames();

    // Assign incoming output pictures to Frames
    void connectOutputPictures();

    // Number of outstanding frames
    uint32_t frameLatency() const;

    // Move any frames before `timestamp` into processing queue
    void startProcessing(uint64_t timestamp);

    // Create new tasks
    LdcTaskDependency addTaskGenerateCmdBuffer(FrameVulkan* frame, LdpEnhancementTile* enhancementTile);
    LdcTaskDependency addTaskConvertToInternal(FrameVulkan* frame, unsigned baseDepth,
                                               unsigned enhancementDepth, LdcTaskDependency input);
    LdcTaskDependency addTaskConvertFromInternal(FrameVulkan* frame, unsigned baseDepth,
                                                 unsigned enhancementDepth, LdcTaskDependency dst,
                                                 LdcTaskDependency src, uint8_t intermediatePtr);
    LdcTaskDependency addTaskUpsample(FrameVulkan* frame, LdeLOQIndex fromLoq,
                                      LdcTaskDependency input, uint8_t intermediatePtr);

    LdcTaskDependency addTaskApplyCmdBufferDirect(FrameVulkan* frame, LdpEnhancementTile* enhancementTile,
                                                  LdcTaskDependency inputDep, LdcTaskDependency CmdBufferDep,
                                                  uint8_t intermediatePtr);
    LdcTaskDependency addTaskApplyCmdBufferTemporal(FrameVulkan* frame, LdpEnhancementTile* enhancementTile,
                                                    LdcTaskDependency temporalBufferDep,
                                                    LdcTaskDependency CmdBufferDep,
                                                    uint8_t intermediatePtr);

    LdcTaskDependency addTaskApplyAddTemporal(FrameVulkan* frame, LdcTaskDependency temporalDep,
                                              LdcTaskDependency sourceDep, uint8_t intermediatePtr);

    LdcTaskDependency addTaskWaitForMany(FrameVulkan* frame, const LdcTaskDependency* deps, uint32_t numDeps);

    void addTaskBaseDone(FrameVulkan* frame, const LdcTaskDependency* inputs, uint32_t inputsCount);
    void addTaskOutputDone(FrameVulkan* frame, const LdcTaskDependency* inputs, uint32_t inputsCount);
    LdcTaskDependency addTaskPassthrough(FrameVulkan* frame, uint32_t planeIndex,
                                         LdcTaskDependency destDep, LdcTaskDependency srcDep);

    void addTaskTemporalRelease(FrameVulkan* frame, const LdcTaskDependency* deps);
    // // Task bodies
    static void* taskConvertToInternal(LdcTask* task, const LdcTaskPart* part);
    static void* taskConvertFromInternal(LdcTask* task, const LdcTaskPart* part);
    static void* taskGenerateCmdBuffer(LdcTask* task, const LdcTaskPart* part);
    static void* taskUpsample(LdcTask* task, const LdcTaskPart* part);
    static void* taskApplyCmdBufferDirect(LdcTask* task, const LdcTaskPart* part);
    static void* taskApplyCmdBufferTemporal(LdcTask* task, const LdcTaskPart* part);
    static void* taskApplyAddTemporal(LdcTask* task, const LdcTaskPart* part);
    static void* taskWaitForMany(LdcTask* task, const LdcTaskPart* part);
    static void* taskOutputDone(LdcTask* task, const LdcTaskPart* part);
    static void* taskBaseDone(LdcTask* task, const LdcTaskPart* part);
    static void* taskPassthrough(LdcTask* task, const LdcTaskPart* part);
    static void* taskTemporalRelease(LdcTask* task, const LdcTaskPart* part);

    static uint32_t chromaToNumPlanes(LdeChroma chroma);

    // Configuration from builder
    const PipelineConfigVulkan m_configuration;

    // Interface to event mechanism
    pipeline::EventSink* m_eventSink = nullptr;

    // The system allocator to use
    LdcMemoryAllocator* m_allocator = nullptr;

    // A rolling memory allocator for per-frame blocks
    LdcMemoryAllocatorRollingArena m_rollingArena = {};

    // Enhancement configuration pool
    LdeConfigPool m_configPool = {};

    // Task pool
    LdcTaskPool m_taskPool = {};

    // Vector of Buffer allocations
    lcevc_dec::common::Vector<LdcMemoryAllocation> m_buffers;

    // Vector of Picture allocations
    lcevc_dec::common::Vector<LdcMemoryAllocation> m_pictures;

    // Vector of Frames allocations
    // These frames are NOT in timestamp order.
    // The `reorderIndex` and `processingIndex` vectors contain timestamp-order pointers to the
    // FrameCPU structures.
    lcevc_dec::common::Vector<LdcMemoryAllocation> m_frames;

    // Vector of pending frames pointers during reorder - sorted by timestamp
    lcevc_dec::common::Vector<FrameVulkan*> m_reorderIndex;

    // Vector of pending frames pointers whilst in progress - sorted by timestamp
    lcevc_dec::common::Vector<FrameVulkan*> m_processingIndex;

    // Limit for frame reordering - can be dynamically updated as enhancement data comes in
    uint32_t m_maxReorder = 0;

    // Vector of temporal buffers
    lcevc_dec::common::Vector<TemporalBuffer> m_temporalBuffers;

    // The prior frame during initial in-order config parsing - used to negotiate temporal buffers
    uint64_t m_previousTimestamp = kInvalidTimestamp;

    // The timestamp of the last frame to have its config parsed successfully
    uint64_t m_lastGoodTimestamp = kInvalidTimestamp;

    // Pending base pictures
    lcevc_dec::common::Vector<BasePicture> m_basePicturePending;

    // Base pictures Out - thread safe FIFO
    lcevc_dec::common::RingBuffer<LdpPicture*> m_basePictureOutBuffer;

    // Output Pictures available for rendering - thread safe FIFO
    lcevc_dec::common::RingBuffer<LdpPicture*> m_outputPictureAvailableBuffer;

    // Global dither module
    LdppDitherGlobal m_dither;

    // Lock for interaction between frame tasks and pipeline - when temporal buffers
    // are handed over / negotiated.
    //
    // Protects m_temporalBuffers and m_processingIndex
    common::Mutex m_interTaskMutex;

    // Signalled when frames are done, whilst holding m_interTaskMutex
    common::CondVar m_interTaskFrameDone;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_PIPELINE_VULKAN_H
