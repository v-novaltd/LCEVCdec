/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#ifndef VN_API_DECODER_CONFIG_H_
#define VN_API_DECODER_CONFIG_H_

#include "config_map.h"
#include "enums.h"
#include "log.h"

#include <string>
#include <unordered_map>
#include <vector>

// ------------------------------------------------------------------------------------------------

struct perseus_decoder_config;

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

class DecoderConfig
{
    template <typename T>
    using Vec = std::vector<T>;

public:
    bool validate() const;
    void initialiseLogs() const;
    void initialiseCoreConfig(perseus_decoder_config& cfgOut) const;

    bool getHighlightResiduals() const { return m_highlightResiduals; }
    float getSFilterStrength() const { return m_sFilterStrength; }

    // Note that 0 means "there is no queue" (i.e. these things get processed instantly), and -1 is
    // a synonym for "UINT32_MAX" (i.e. the queues are essentially infinite).
    uint32_t getLoqUnprocessedCap() const { return m_loqUnprocessedCap; }
    uint32_t getResultsQueueCap() const { return m_resultsQueueCap; }

    PassthroughPolicy getPassthroughMode() const
    {
        return static_cast<PassthroughPolicy>(m_passthroughMode);
    }
    int32_t getResidualSurfaceFPSetting() const { return m_residualSurfaceFPSetting; }

    // Turn off clang-format to keep these boilerplate functions on one line each.
    // clang-format off
    bool set(const char* name, const bool& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const float& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const int32_t& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const std::string& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const Vec<bool>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    bool set(const char* name, const Vec<float>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    bool set(const char* name, const Vec<int32_t>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    bool set(const char* name, const Vec<std::string>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    // clang-format on

    const std::vector<int32_t>& getEvents() const { return m_events; }

private:
    int32_t getLogLevelInt(LogComponent comp) const
    {
        return std::max(m_logLevelGlobal, m_logLevels[static_cast<size_t>(comp)]);
    }
    LogLevel getLogLevel(LogComponent comp) const
    {
        return static_cast<LogLevel>(getLogLevelInt(comp));
    }
    LogPrecision getLogPrecision() const;

    bool m_allowDithering = true;
    bool m_coreParallelDecode = false;
    bool m_disableSIMD = false;
    bool m_enableLogoOverlay = false;
    bool m_generateCmdbuffers = false;
    bool m_highlightResiduals = false;
    bool m_highPrecision = false;
    bool m_logToStdOut = false;

    float m_sFilterStrength = -1.0f;

    int32_t m_coreDecoderNumThreads = -1;
    int32_t m_ditherSeed = -1;
    int32_t m_ditherStrength = -1;
    int32_t m_forceBitstreamVersion = -1;
    int32_t m_logLevelGlobal = static_cast<int32_t>(LogLevel::Info);
    int32_t m_logoOverlayDelayFrames = -1;
    int32_t m_logoOverlayPositionX = -1;
    int32_t m_logoOverlayPositionY = -1;
    int32_t m_logTimestampPrecision = static_cast<int32_t>(LogPrecision::Count);
    int32_t m_loqUnprocessedCap = 100;
    int32_t m_passthroughMode = static_cast<int32_t>(PassthroughPolicy::Allow);
    int32_t m_predictedAverageMethod = static_cast<int32_t>(PredictedAverageMethod::Standard);
    int32_t m_residualSurfaceFPSetting = -1;
    int32_t m_resultsQueueCap = 24;

    std::vector<int32_t> m_events;
    std::array<int32_t, static_cast<size_t>(LogComponent::Count)> m_logLevels = {};

    static const ConfigMap<DecoderConfig> kConfigMap;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_DECODER_CONFIG_H_
