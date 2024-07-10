/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "decoder_config.h"

#include "enums.h"
#include "lcevc_config.h"
#include "log.h"

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/PerseusDecoder.h>

#include <array>
#include <cinttypes>
#include <cstddef>
#include <limits>
#include <string>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

static const LogComponent kComp = LogComponent::DecoderConfig;

// This callback allows us to accept logs from the Core decoder and turn them into VNLog-style
// logs. It duplicates "decoderLogCallback" in the core_test_harness.
static void coreDecLogCallback(void* userData, perseus_decoder_log_type type, const char* msg, size_t msgLength)
{
    VN_UNUSED(userData);

    // Note double-debug. That's the core's "unknown" log level, which we default to Debug.
    static constexpr std::array<LogLevel, PSS_LT_UNKNOWN + 1> kTable = {
        LogLevel::Error, LogLevel::Info, LogLevel::Warning, LogLevel::Debug, LogLevel::Debug};
    std::string strToPrint(msg, msgLength);
    sLog.print(LogComponent::CoreDecoder, kTable[type], "?", 0, "%s", strToPrint.c_str());
}

// - DecoderConfig --------------------------------------------------------------------------------

// This is sorted alphabetically to make it easier to compare against the documentation.
const ConfigMap<DecoderConfig> DecoderConfig::kConfigMap({
    {"allow_dithering", makeBinding(&DecoderConfig::m_allowDithering)},
    {"core_threads", makeBinding(&DecoderConfig::m_coreDecoderNumThreads)},
    {"disable_simd", makeBinding(&DecoderConfig::m_disableSIMD)},
    {"dither_seed", makeBinding(&DecoderConfig::m_ditherSeed)},
    {"dither_strength", makeBinding(&DecoderConfig::m_ditherStrength)},
    {"enable_logo_overlay", makeBinding(&DecoderConfig::m_enableLogoOverlay)},
    {"events", makeBinding(&DecoderConfig::m_events)},
    {"generate_cmdbuffers", makeBinding(&DecoderConfig::m_generateCmdbuffers)},
    {"highlight_residuals", makeBinding(&DecoderConfig::m_highlightResiduals)},
    {"high_precision", makeBinding(&DecoderConfig::m_highPrecision)},
    {"log_level", makeBinding(&DecoderConfig::m_logLevelGlobal)},
    {"log_level_api",
     makeBindingArrElement<static_cast<size_t>(LogComponent::API)>(&DecoderConfig::m_logLevels)},
    {"log_level_buffer_manager",
     makeBindingArrElement<static_cast<size_t>(LogComponent::BufferManager)>(&DecoderConfig::m_logLevels)},
    {"log_level_core_decoder",
     makeBindingArrElement<static_cast<size_t>(LogComponent::CoreDecoder)>(&DecoderConfig::m_logLevels)},
    {"log_level_decoder",
     makeBindingArrElement<static_cast<size_t>(LogComponent::Decoder)>(&DecoderConfig::m_logLevels)},
    {"log_level_decoder_config",
     makeBindingArrElement<static_cast<size_t>(LogComponent::DecoderConfig)>(&DecoderConfig::m_logLevels)},
    {"log_level_interface",
     makeBindingArrElement<static_cast<size_t>(LogComponent::Interface)>(&DecoderConfig::m_logLevels)},
    {"log_level_lcevc_processor",
     makeBindingArrElement<static_cast<size_t>(LogComponent::LCEVCProcessor)>(&DecoderConfig::m_logLevels)},
    {"log_level_log",
     makeBindingArrElement<static_cast<size_t>(LogComponent::Log)>(&DecoderConfig::m_logLevels)},
    {"log_level_picture",
     makeBindingArrElement<static_cast<size_t>(LogComponent::Picture)>(&DecoderConfig::m_logLevels)},
    {"log_stdout", makeBinding(&DecoderConfig::m_logToStdOut)},
    {"log_timestamp_precision", makeBinding(&DecoderConfig::m_logTimestampPrecision)},
    {"logo_overlay_delay_frames", makeBinding(&DecoderConfig::m_logoOverlayDelayFrames)},
    {"logo_overlay_position_x", makeBinding(&DecoderConfig::m_logoOverlayPositionX)},
    {"logo_overlay_position_y", makeBinding(&DecoderConfig::m_logoOverlayPositionY)},
    {"loq_unprocessed_cap", makeBinding(&DecoderConfig::m_loqUnprocessedCap)},
    {"parallel_decode", makeBinding(&DecoderConfig::m_coreParallelDecode)},
    {"passthrough_mode", makeBinding(&DecoderConfig::m_passthroughMode)},
    {"predicted_average_method", makeBinding(&DecoderConfig::m_predictedAverageMethod)},
    {"pss_surface_fp_setting", makeBinding(&DecoderConfig::m_residualSurfaceFPSetting)},
    {"results_queue_cap", makeBinding(&DecoderConfig::m_resultsQueueCap)},
    {"s_filter_strength", makeBinding(&DecoderConfig::m_sFilterStrength)},
});

bool DecoderConfig::validate() const
{
    bool valid = true;

    // m_ditherSeed
    if (m_ditherSeed != -1 && (m_ditherStrength == 0 || m_allowDithering == false)) {
        VNLogWarning("Setting a custom dither seed, but dithering has been manually disabled. No "
                     "dithering will occur\n");
    }

    // m_ditherStrength
    if (m_ditherStrength > 1 && !m_allowDithering) {
        VNLogError("Forcing dither to non-zero value (%d), while also banning dithering. This is "
                   "incompatible.\n");
        valid = false;
    }

    // m_events
    for (int32_t eventType : m_events) {
        if (eventType < LCEVC_Log || eventType >= LCEVC_EventCount) {
            VNLogError(
                "Invalid config: event types must be between %d and %d (which should be less "
                "than %hhu), inclusive, but %d was supplied.\n",
                LCEVC_Log, LCEVC_EventCount - 1, std::numeric_limits<uint8_t>::max(), eventType);
            valid = false;
        }
    }

    // m_logLevelGlobal
    if (m_logLevelGlobal < LCEVC_LogDisabled || m_logLevelGlobal >= LCEVC_LogLevelCount) {
        VNLogError("Invalid config: log level must be between %d and %d (inclusive), but %d was "
                   "supplied as the global log level.\n",
                   LCEVC_LogDisabled, LCEVC_LogLevelCount - 1, m_logLevelGlobal);
        valid = false;
    }

    // m_logLevels
    for (size_t idx = 0; idx < m_logLevels.size(); idx++) {
        const int32_t logLevel = m_logLevels[idx];
        if (logLevel < LCEVC_LogDisabled || logLevel >= LCEVC_LogLevelCount) {
            VNLogError("Invalid config: log levels must be between %d and %d (inclusive), but %d "
                       "was supplied for component %d.\n",
                       LCEVC_LogDisabled, LCEVC_LogLevelCount - 1, logLevel, idx);
            valid = false;
        }
    }

    // m_logTimestampPrecision
    if (m_logTimestampPrecision < LCEVC_LogNano || m_logTimestampPrecision > LCEVC_LogPrecisionCount) {
        VNLogError("Invalid config: log_timestamp_precision should be between %d and %d, "
                   "inclusive, but it's %d\n",
                   LCEVC_LogNano, LCEVC_LogPrecisionCount, m_logTimestampPrecision);
        valid = false;
    }

    // m_loqUnprocessedCap
    if (m_loqUnprocessedCap < -1) {
        VNLogError("Invalid config: loq_unprocessed_cap should not be less than -1, but it's %d\n",
                   m_loqUnprocessedCap);
        valid = false;
    }

    // m_predictedAverageMethod
    if (m_predictedAverageMethod < static_cast<int32_t>(PredictedAverageMethod::None) ||
        m_predictedAverageMethod >= static_cast<int32_t>(PredictedAverageMethod::COUNT)) {
        VNLogError("Invalid config: predicted_average_method should be between %d and %d "
                   "(inclusive), but it's %d\n",
                   static_cast<int32_t>(PredictedAverageMethod::None),
                   static_cast<int32_t>(PredictedAverageMethod::COUNT) - 1, m_predictedAverageMethod);
        valid = false;
    }

    // m_resultsQueueCap
    if (m_resultsQueueCap < -1) {
        VNLogError("Invalid config: results_queue_cap should not be less than -1, but it's %d\n",
                   m_resultsQueueCap);
        valid = false;
    }

    VNLogInfo("Standard config:\n"
              "\tlog_level  : %d\n"
              "\tevents     : %s\n",
              m_logLevelGlobal, iterableToString(m_events).c_str());

    VNLogDebug("Additional config:\n"
               "\tenable_logo_overlay       : %d\n"
               "\thighlight_residuals       : %d\n"
               "\tlog_stdout                : %d\n"
               "\ts_filter_strength         : %f\n"
               "\tdpi_pipeline_mode         : %d\n"
               "\tdpi_threads               : %d\n"
               "\tdither_strength           : %d\n"
               "\tlog_timestamp_precision   : %d\n"
               "\tlogo_overlay_delay_frames : %d\n"
               "\tlogo_overlay_position_x   : %d\n"
               "\tlogo_overlay_position_y   : %d\n"
               "\tloq_unprocessed_cap       : %d\n"
               "\tpassthrough_mode          : %d\n"
               "\tpredicted_average_method  : %d\n"
               "\tpss_surface_fp_setting    : %d\n"
               "\tresults_queue_cap         : %d\n"
               "\tper-component log levels  : %s\n",
               m_enableLogoOverlay, m_highlightResiduals, m_logToStdOut, m_sFilterStrength,
               m_highPrecision, m_coreDecoderNumThreads, m_ditherStrength, m_logTimestampPrecision,
               m_logoOverlayDelayFrames, m_logoOverlayPositionX, m_logoOverlayPositionY,
               m_loqUnprocessedCap, m_passthroughMode, m_predictedAverageMethod,
               m_residualSurfaceFPSetting, m_resultsQueueCap, iterableToString(m_logLevels).c_str());
    return valid;
}

LogPrecision DecoderConfig::getLogPrecision() const
{
    if (m_logTimestampPrecision != static_cast<int32_t>(LogPrecision::Count)) {
        return static_cast<LogPrecision>(m_logTimestampPrecision);
    }
    return (m_logLevelGlobal >= static_cast<int32_t>(LogLevel::Debug) ? LogPrecision::Nano
                                                                      : LogPrecision::Micro);
}

void DecoderConfig::initialiseLogs() const
{
    sLog.setEnableStdout(m_logToStdOut);
    sLog.setTimestampPrecision(getLogPrecision());

    for (size_t idx = 0; idx < m_logLevels.size(); idx++) {
        auto comp = static_cast<LogComponent>(idx);
        sLog.setVerbosity(comp, getLogLevel(comp));
    }
}

void DecoderConfig::initialiseCoreConfig(perseus_decoder_config& cfgOut) const
{
    perseus_decoder_config_init(&cfgOut);

    // Normal settings (passed directly to Core decoder)
    cfgOut.logo_overlay_enable = m_enableLogoOverlay;
    cfgOut.use_approximate_pa =
        (m_predictedAverageMethod == static_cast<int32_t>(PredictedAverageMethod::BakedIntoKernel));
    cfgOut.disable_dithering = !m_allowDithering;
    cfgOut.dither_seed = m_ditherSeed;
    cfgOut.dither_override_strength = m_ditherStrength;
    cfgOut.log_callback = &coreDecLogCallback;
    cfgOut.simd_type = m_disableSIMD ? PSS_SIMD_DISABLED : PSS_SIMD_AUTO;
    cfgOut.generate_cmdbuffers = m_generateCmdbuffers;
    cfgOut.apply_cmdbuffers_internal = m_generateCmdbuffers;
    cfgOut.use_parallel_decode = m_coreParallelDecode;
    cfgOut.pipeline_mode = (m_highPrecision ? PPM_PRECISION : PPM_SPEED);

    // Settings where negative means "don't set/auto"
    if (m_coreDecoderNumThreads != -1) {
        cfgOut.num_worker_threads = m_coreDecoderNumThreads;
    }
    if (m_coreDecoderNumThreads != -1) {
        cfgOut.apply_cmdbuffers_threads = static_cast<uint16_t>(m_coreDecoderNumThreads);
    }
    if (m_logoOverlayDelayFrames > 0) {
        cfgOut.logo_overlay_delay = static_cast<uint16_t>(m_logoOverlayDelayFrames);
    }
    if (m_logoOverlayPositionX > 0) {
        cfgOut.logo_overlay_position_x = static_cast<uint16_t>(m_logoOverlayPositionX);
    }
    if (m_logoOverlayPositionY > 0) {
        cfgOut.logo_overlay_position_y = static_cast<uint16_t>(m_logoOverlayPositionY);
    }
    if (m_sFilterStrength >= 0.0f) {
        cfgOut.s_strength = m_sFilterStrength;
    }

    VNLogTrace("Core decoder config:\n"
               "\tdisable_dithering         : %d\n"
               "\tdither_override_strength  : %d\n"
               "\tgenerate_cmdbuffers       : %d\n"
               "\tlogo_overlay_delay        : %" PRIu16 "\n"
               "\tlogo_overlay_enable       : %" PRIu8 "\n"
               "\tlogo_overlay_position_x,y : %" PRIu16 ",%" PRIu16 "\n"
               "\tnum_worker_threads        : %d\n"
               "\tparallel_decode           : %d\n"
               "\tpipeline_mode             : %d\n"
               "\ts_strength                : %f\n"
               "\tsimd_enabled              : %d\n"
               "\tuse_approximate_pa        : %" PRIu8 "\n",
               cfgOut.dither_override_strength, cfgOut.generate_cmdbuffers, cfgOut.logo_overlay_delay,
               cfgOut.logo_overlay_enable, cfgOut.logo_overlay_position_x, cfgOut.logo_overlay_position_y,
               cfgOut.num_worker_threads, cfgOut.pipeline_mode, cfgOut.use_parallel_decode,
               cfgOut.s_strength, cfgOut.simd_type, cfgOut.use_approximate_pa);
}

} // namespace lcevc_dec::decoder
