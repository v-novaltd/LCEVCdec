/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "decoder_config.h"

#include "uEnums.h"
#include "uPlatform.h"

#include <LCEVC/PerseusDecoder.h>
#include <LCEVC/lcevc_dec.h>

// ------------------------------------------------------------------------------------------------

using namespace lcevc_dec::utility;

namespace lcevc_dec::decoder {

// This callback allows us to accept logs from the Core decoder and turn them into VNLog-style
// logs. It duplicates "decoderLogCallback" in the core_test_harness.
static void coreDecLogCallback(void* userData, perseus_decoder_log_type type, const char* msg, size_t msgLength)
{
    VNUnused(userData);

    static constexpr LogType kTable[] = {LogType_Error, LogType_Info, LogType_Warning,
                                         LogType_Debug, LogType_Debug};
    std::string strToPrint(msg, msgLength);
    log_print(kTable[type], "Core Decoder", 0, "%s", strToPrint.c_str());
}

// - DecoderConfig --------------------------------------------------------------------------------

const ConfigMap<DecoderConfig> DecoderConfig::kConfigMap({
    {"highlight_residuals", makeBinding(&DecoderConfig::m_highlightResiduals)},
    {"log_stdout", makeBinding(&DecoderConfig::m_logToStdOut)},
    {"use_loq0", makeBinding(&DecoderConfig::m_useLoq0)},
    {"use_loq1", makeBinding(&DecoderConfig::m_useLoq1)},
    {"s_filter_strength", makeBinding(&DecoderConfig::m_sFilterStrength)},
    {"dither_strength", makeBinding(&DecoderConfig::m_ditherStrength)},
    {"dpi_pipeline_mode", makeBinding(&DecoderConfig::m_coreDecoderPipelineMode)},
    {"dpi_threads", makeBinding(&DecoderConfig::m_coreDecoderNumThreads)},
    {"log_level", makeBinding(&DecoderConfig::m_logLevel)},
    {"results_queue_cap", makeBinding(&DecoderConfig::m_resultsQueueCap)},
    {"loq_unprocessed_cap", makeBinding(&DecoderConfig::m_loqUnprocessedCap)},
    {"passthrough_mode", makeBinding(&DecoderConfig::m_passthroughMode)},
    {"predicted_average_method", makeBinding(&DecoderConfig::m_predictedAverageMethod)},
    {"pss_surface_fp_setting", makeBinding(&DecoderConfig::m_residualSurfaceFPSetting)},
    {"events", makeBinding(&DecoderConfig::m_events)},
});

bool DecoderConfig::validate() const
{
    bool valid = true;

    if (m_loqUnprocessedCap < -1) {
        VNLogError("Invalid config: loq_unprocessed_cap should not be less than -1, but it's %d\n",
                   m_loqUnprocessedCap);
        valid = false;
    }

    if (m_resultsQueueCap < -1) {
        VNLogError("Invalid config: results_queue_cap should not be less than -1, but it's %d\n",
                   m_resultsQueueCap);
        valid = false;
    }

    if (m_predictedAverageMethod < static_cast<int32_t>(PredictedAverageMethod::None) ||
        m_predictedAverageMethod >= static_cast<int32_t>(PredictedAverageMethod::COUNT)) {
        VNLogError("Invalid config: predicted_average_method should be between %d and %d "
                   "(inclusive), but it's %d\n",
                   static_cast<int32_t>(PredictedAverageMethod::None),
                   static_cast<int32_t>(PredictedAverageMethod::COUNT) - 1, m_predictedAverageMethod);
        valid = false;
    }

    for (int32_t eventType : m_events) {
        if (eventType >= LCEVC_EventCount || eventType < 0) {
            VNLogError(
                "Invalid config: event types must be between 0 and %d, but %d was supplied.\n",
                LCEVC_EventCount, eventType);
            valid = false;
        }
    }

    return valid;
}

void DecoderConfig::initialiseLogs() const
{
    log_set_enable_stdout(m_logToStdOut);
    log_set_verbosity(static_cast<LogType>(m_logLevel));
}

void DecoderConfig::initialiseCoreConfig(perseus_decoder_config& cfgOut) const
{
    perseus_decoder_config_init(&cfgOut);

    if (m_coreDecoderNumThreads != -1) {
        cfgOut.num_worker_threads = m_coreDecoderNumThreads;
    }
    if (m_coreDecoderPipelineMode != -1) {
        cfgOut.pipeline_mode = static_cast<perseus_pipeline_mode>(m_coreDecoderPipelineMode);
    }
    if (m_sFilterStrength >= 0.0f) {
        cfgOut.s_strength = m_sFilterStrength;
    }
    cfgOut.use_approximate_pa =
        (m_predictedAverageMethod == static_cast<int32_t>(PredictedAverageMethod::BakedIntoKernel));

    cfgOut.log_callback = &coreDecLogCallback;

    VNLogVerbose("num_worker_threads   : %d\n", cfgOut.num_worker_threads);
    VNLogVerbose("pipeline_mode        : %d\n", cfgOut.pipeline_mode);
    VNLogVerbose("s_strength           : %d\n", cfgOut.s_strength);
    VNLogVerbose("use_approximate_pa   : %d\n", cfgOut.use_approximate_pa);
}

} // namespace lcevc_dec::decoder
