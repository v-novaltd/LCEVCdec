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

#ifndef VN_LCEVC_PIPELINE_VULKAN_TIMESTAMP_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_TIMESTAMP_VULKAN_H

#include <vulkan/vulkan.h>
//
#include <cstddef>
#include <vector>

namespace lcevc_dec::pipeline_vulkan {

class TimestampVulkan
{
public:
    // Default construct as disabled — all methods are no-ops.
    TimestampVulkan() = default;

    // Construct with count of maximum timestamps.
    // If count == 0, no pool is constructed and other methods do nothing.
    // If timeDomain is not VK_TIME_DOMAIN_DEVICE_EXT, VK_EXT_calibrated_timestamps is used
    // to produce timestamps in the given domain. Falls back to device domain if unavailable.
    TimestampVulkan(VkDevice device, VkPhysicalDevice physicalDevice, VkInstance instance,
                    size_t count, VkTimeDomainEXT timeDomain);
    ~TimestampVulkan();

    TimestampVulkan(TimestampVulkan&& other) noexcept;
    TimestampVulkan& operator=(TimestampVulkan&& other) noexcept;
    TimestampVulkan(const TimestampVulkan&) = delete;
    TimestampVulkan& operator=(const TimestampVulkan&) = delete;

    // Reset timestamps for a new frame.
    void reset(VkCommandBuffer commandBuffer);

    // Add a 'begin' timestamp event with given label.
    void begin(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, const char* label);

    // Add an 'end' timestamp event with given label.
    void end(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, const char* label);

    // Add an 'instant' timestamp with given label.
    void event(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, const char* label);

    // Write a report to log that summarises the recorded timestamps.
    void report() const;

private:
    enum class EventType
    {
        Begin,
        End,
        Instant
    };

    struct Event
    {
        EventType type;
        const char* label;
        uint32_t queryIndex;
    };

    uint32_t allocateQuery();

    VkDevice m_device{};
    VkQueryPool m_queryPool{};
    size_t m_maxCount{0};
    uint32_t m_nextQuery{0};

    float m_timestampPeriod{0.0f};
    bool m_exhausted{false};
    std::vector<Event> m_events;

    VkTimeDomainEXT m_timeDomain{VK_TIME_DOMAIN_DEVICE_EXT};
    PFN_vkGetCalibratedTimestampsEXT m_getCalibratedTimestamps{nullptr};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_TIMESTAMP_VULKAN_H
