/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_DECODER_CONFIG_H_
#define VN_API_DECODER_CONFIG_H_

#include "uConfigMap.h"
#include "uEnums.h"
#include "uLog.h"

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

    // Remember, this configuration SPECIFICALLY is "flipped": 0 means infinite, infinite means 0.
    uint32_t getLoqUnprocessedCap() const { return m_loqUnprocessedCap; }

    // This, on the other hand, is normal.
    uint32_t getResultsQueueCap() const { return m_resultsQueueCap; }

    lcevc_dec::api_utility::DILPassthroughPolicy getPassthroughMode() const
    {
        return static_cast<lcevc_dec::api_utility::DILPassthroughPolicy>(m_passthroughMode);
    }
    int32_t getResidualSurfaceFPSetting() const { return m_residualSurfaceFPSetting; }

    // Turn off clang-format to keep these boilerplate functions on one line each.
    // clang-format off
    bool set(const char* name, const bool& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const int32_t& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const float& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const std::string& val) { return kConfigMap.getConfig(name)->set(*this, val); }
    bool set(const char* name, const Vec<bool>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    bool set(const char* name, const Vec<int32_t>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    bool set(const char* name, const Vec<float>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    bool set(const char* name, const Vec<std::string>& arr) { return kConfigMap.getConfig(name)->set(*this, arr); }
    // clang-format on

    const std::vector<int32_t>& getEvents() const { return m_events; }

private:
    bool m_enableLogoOverlay = false;
    bool m_highlightResiduals = false;
    bool m_logToStdOut = false;
    bool m_useLoq0 = true;
    bool m_useLoq1 = true;

    float m_sFilterStrength = -1.0f;

    int32_t m_coreDecoderNumThreads = -1;
    int32_t m_coreDecoderPipelineMode = -1;
    int32_t m_ditherStrength = -1;
    int32_t m_logLevel = static_cast<int32_t>(LogType_Error);
    int32_t m_logoOverlayDelayFrames = -1;
    int32_t m_logoOverlayPositionX = -1;
    int32_t m_logoOverlayPositionY = -1;
    int32_t m_resultsQueueCap = 24;
    int32_t m_loqUnprocessedCap = 100;
    int32_t m_passthroughMode = static_cast<int32_t>(lcevc_dec::api_utility::DILPassthroughPolicy::Allow);
    int32_t m_predictedAverageMethod =
        static_cast<int32_t>(lcevc_dec::api_utility::PredictedAverageMethod::Standard);
    int32_t m_residualSurfaceFPSetting = -1;

    std::vector<int32_t> m_events;

    static const ConfigMap<DecoderConfig> kConfigMap;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_DECODER_CONFIG_H_
