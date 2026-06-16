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

#include "timestamp_vulkan.h"

#include <LCEVC/common/log.h>
#include <LCEVC/common/printf_macros.h>

#include <cassert>
#include <cstdio>
#include <sstream>

namespace lcevc_dec::pipeline_vulkan {

TimestampVulkan::TimestampVulkan(VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkInstance instance, size_t count, VkTimeDomainEXT timeDomain)
    : m_device(device)
    , m_maxCount(count)
{
    if (count == 0) {
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    m_timestampPeriod = props.limits.timestampPeriod;

    if (props.limits.timestampComputeAndGraphics == VK_FALSE) {
        VNLogWarning("TimestampVulkan: device does not support timestamps, disabling");
        m_maxCount = 0;
        return;
    }

    // Set up calibrated timestamps if a non-device time domain is requested.
    if (timeDomain != VK_TIME_DOMAIN_DEVICE_EXT) {
        auto getTimeDomains = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
        m_getCalibratedTimestamps = reinterpret_cast<PFN_vkGetCalibratedTimestampsEXT>(
            vkGetDeviceProcAddr(device, "vkGetCalibratedTimestampsEXT"));

        if (!getTimeDomains || !m_getCalibratedTimestamps) {
            VNLogWarning("TimestampVulkan: VK_EXT_calibrated_timestamps not available, "
                         "falling back to device domain");
            m_getCalibratedTimestamps = nullptr;
        } else {
            // Verify both the device domain and the requested domain are supported.
            uint32_t domainCount = 0;
            getTimeDomains(physicalDevice, &domainCount, nullptr);
            std::vector<VkTimeDomainEXT> domains(domainCount);
            getTimeDomains(physicalDevice, &domainCount, domains.data());

            bool deviceSupported = false;
            bool requestedSupported = false;
            for (auto d : domains) {
                if (d == VK_TIME_DOMAIN_DEVICE_EXT) {
                    deviceSupported = true;
                }
                if (d == timeDomain) {
                    requestedSupported = true;
                }
            }

            if (!deviceSupported || !requestedSupported) {
                VNLogWarning("TimestampVulkan: requested time domain not supported, "
                             "falling back to device domain");
                m_getCalibratedTimestamps = nullptr;
            } else {
                m_timeDomain = timeDomain;
            }
        }
    }

    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = static_cast<uint32_t>(count);

    if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &m_queryPool) != VK_SUCCESS) {
        VNLogError("TimestampVulkan: failed to create query pool");
        m_maxCount = 0;
    }

    m_events.reserve(count);
}

TimestampVulkan::~TimestampVulkan()
{
    if (m_queryPool && m_device) {
        vkDestroyQueryPool(m_device, m_queryPool, nullptr);
    }
}

TimestampVulkan::TimestampVulkan(TimestampVulkan&& other) noexcept
    : m_device(other.m_device)
    , m_queryPool(other.m_queryPool)
    , m_maxCount(other.m_maxCount)
    , m_nextQuery(other.m_nextQuery)
    , m_timestampPeriod(other.m_timestampPeriod)
    , m_exhausted(other.m_exhausted)
    , m_events(std::move(other.m_events))
    , m_timeDomain(other.m_timeDomain)
    , m_getCalibratedTimestamps(other.m_getCalibratedTimestamps)
{
    other.m_queryPool = VK_NULL_HANDLE;
    other.m_maxCount = 0;
    other.m_getCalibratedTimestamps = nullptr;
}

TimestampVulkan& TimestampVulkan::operator=(TimestampVulkan&& other) noexcept
{
    if (this != &other) {
        if (m_queryPool && m_device) {
            vkDestroyQueryPool(m_device, m_queryPool, nullptr);
        }
        m_device = other.m_device;
        m_queryPool = other.m_queryPool;
        m_maxCount = other.m_maxCount;
        m_nextQuery = other.m_nextQuery;
        m_timestampPeriod = other.m_timestampPeriod;
        m_exhausted = other.m_exhausted;
        m_events = std::move(other.m_events);
        m_timeDomain = other.m_timeDomain;
        m_getCalibratedTimestamps = other.m_getCalibratedTimestamps;
        other.m_queryPool = VK_NULL_HANDLE;
        other.m_maxCount = 0;
        other.m_getCalibratedTimestamps = nullptr;
    }
    return *this;
}

void TimestampVulkan::reset(VkCommandBuffer commandBuffer)
{
    if (m_maxCount == 0) {
        return;
    }

    vkCmdResetQueryPool(commandBuffer, m_queryPool, 0, static_cast<uint32_t>(m_maxCount));
    m_nextQuery = 0;
    m_exhausted = false;
    m_events.clear();
}

uint32_t TimestampVulkan::allocateQuery()
{
    if (m_nextQuery >= m_maxCount) {
        if (!m_exhausted) {
            VNLogWarning(
                "TimestampVulkan: query pool exhausted, ignoring further events until next reset");
            m_exhausted = true;
        }
        return UINT32_MAX;
    }
    return m_nextQuery++;
}

void TimestampVulkan::begin(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage,
                            const char* label)
{
    if (m_maxCount == 0) {
        return;
    }

    uint32_t idx = allocateQuery();
    if (idx == UINT32_MAX) {
        return;
    }

    vkCmdWriteTimestamp(commandBuffer, pipelineStage, m_queryPool, idx);
    m_events.push_back({EventType::Begin, label, idx});
}

void TimestampVulkan::end(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage,
                          const char* label)
{
    if (m_maxCount == 0) {
        return;
    }

    uint32_t idx = allocateQuery();
    if (idx == UINT32_MAX) {
        return;
    }

    vkCmdWriteTimestamp(commandBuffer, pipelineStage, m_queryPool, idx);
    m_events.push_back({EventType::End, label, idx});
}

void TimestampVulkan::event(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage,
                            const char* label)
{
    if (m_maxCount == 0) {
        return;
    }

    uint32_t idx = allocateQuery();
    if (idx == UINT32_MAX) {
        return;
    }

    vkCmdWriteTimestamp(commandBuffer, pipelineStage, m_queryPool, idx);
    m_events.push_back({EventType::Instant, label, idx});
}

void TimestampVulkan::report() const
{
    if (m_maxCount == 0 || m_events.empty()) {
        return;
    }

    // Fetch results: each query returns a 64-bit timestamp + 64-bit availability
    const uint32_t queryCount = m_nextQuery;
    std::vector<uint64_t> results(queryCount * 2);
    VkResult r = vkGetQueryPoolResults(
        m_device, m_queryPool, 0, queryCount, queryCount * 2 * sizeof(uint64_t), results.data(),
        2 * sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

    if (r != VK_SUCCESS) {
        VNLogError("TimestampVulkan: failed to query results");
    }

    // If using calibrated timestamps, obtain a calibration point to convert device
    // timestamps into the requested time domain.
    uint64_t deviceCalibration = 0;
    uint64_t hostCalibration = 0;
    uint64_t maxDeviation = 0;

    if (m_getCalibratedTimestamps) {
        VkCalibratedTimestampInfoEXT infos[2]{};
        infos[0].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT;
        infos[0].timeDomain = VK_TIME_DOMAIN_DEVICE_EXT;
        infos[1].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT;
        infos[1].timeDomain = m_timeDomain;

        uint64_t calibrationValues[2]{};
        VkResult cr = m_getCalibratedTimestamps(m_device, 2, infos, calibrationValues, &maxDeviation);
        if (cr == VK_SUCCESS) {
            deviceCalibration = calibrationValues[0];
            hostCalibration = calibrationValues[1];
        } else {
            VNLogWarning("TimestampVulkan: vkGetCalibratedTimestampsEXT failed, "
                         "reporting raw device timestamps");
        }
    }

    // Helper to convert a device timestamp to the calibrated domain (nanoseconds).
    [[maybe_unused]] auto toCalibratedNs = [&](uint64_t deviceTs) -> double {
        const auto delta = static_cast<int64_t>(deviceTs - deviceCalibration);
        return static_cast<double>(hostCalibration) +
               static_cast<double>(delta) * static_cast<double>(m_timestampPeriod);
    };

    VNLogInfo("Timestamps: period:%g count:%d %s", m_timestampPeriod, queryCount,
              m_getCalibratedTimestamps ? "Calibrated" : "");

    [[maybe_unused]] float total = 0.0f;
    for (size_t i = 0; i < m_events.size(); ++i) {
        const auto& evt = m_events[i];
        const uint64_t timestamp = results[evt.queryIndex * 2];

        if (results[evt.queryIndex * 2 + 1] == 0) {
            VNLogInfo("%3d    : %32s not available", evt.label);
        }

        // For 'end' events, compute duration from the preceding 'begin'
        if (evt.type == EventType::End && i > 0) {
            // Find matching begin by scanning backwards for a begin with the same label
            for (size_t j = i; j-- > 0;) {
                if (m_events[j].type == EventType::Begin && m_events[j].label == evt.label) {
                    const uint64_t beginTs = results[m_events[j].queryIndex * 2];
                    const float ms = static_cast<float>(timestamp - beginTs) / 1e6f * m_timestampPeriod;

                    VNLogInfo("%3d,%3d: %8.3gms %-32s %" PRIu64 "%s", j, i, ms, evt.label,
                              static_cast<uint64_t>(toCalibratedNs(timestamp)),
                              m_getCalibratedTimestamps ? "ns" : "");
                    total += ms;
                    goto next;
                }
            }
        } else if (evt.type == EventType::Instant) {
            VNLogInfo("%3d    : %32s %" PRIu64 "%s", i, evt.label,
                      static_cast<uint64_t>(toCalibratedNs(timestamp)),
                      m_getCalibratedTimestamps ? "ns" : "");
        }

    next:;
    }

    VNLogInfo("Total: %gms", total);
}

} // namespace lcevc_dec::pipeline_vulkan
