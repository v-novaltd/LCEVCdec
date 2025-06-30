/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#include "decoder_config.h"
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/printf_macros.h>
#include <LCEVC/legacy/PerseusDecoder.h>
#include <LCEVC/pipeline/event_sink.h>
//
#include <array>
#include <limits>
#include <string>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// This callback allows us to accept logs from the Core decoder and turn them into VNLog-style
// logs. It duplicates "decoderLogCallback" in the core_test_harness.
namespace {
    void coreDecLogCallback(void*, perseus_decoder_log_type type, const char* msg, size_t msgLength)
    {
        // Core doesn't have a "fatal", so that's absent here, but otherwise this is one-to-one.
        static constexpr std::array<LdcLogLevel, PSS_LT_UNKNOWN> kTable = {
            LdcLogLevel::LdcLogLevelError, LdcLogLevel::LdcLogLevelWarning, LdcLogLevel::LdcLogLevelInfo,
            LdcLogLevel::LdcLogLevelDebug, LdcLogLevel::LdcLogLevelVerbose};
        const std::string strToPrint(msg, msgLength);
        VNLogF(kTable[type], "%s", strToPrint.c_str());
    }
} // namespace

// - DecoderConfig --------------------------------------------------------------------------------

// This is sorted alphabetically to make it easier to compare against the documentation.
const ConfigMap<DecoderConfig> DecoderConfig::kConfigMap({
    {"threads", makeBinding(&DecoderConfig::m_coreDecoderNumThreads)},
    {"allow_dithering", makeBinding(&DecoderConfig::m_allowDithering)},
    {"dither_seed", makeBinding(&DecoderConfig::m_ditherSeed)},
    {"dither_strength", makeBinding(&DecoderConfig::m_ditherStrength)},
    {"enable_logo_overlay", makeBinding(&DecoderConfig::m_enableLogoOverlay)},
    {"force_bitstream_version", makeBinding(&DecoderConfig::m_forceBitstreamVersion)},
    {"generate_cmdbuffers", makeBinding(&DecoderConfig::m_generateCmdbuffers)},
    {"high_precision", makeBinding(&DecoderConfig::m_highPrecision)},
    {"highlight_residuals", makeBinding(&DecoderConfig::m_highlightResiduals)},
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
    {"events", makeBinding(&DecoderConfig::m_events)},
});

// Helper to print iterable objects. Use sparingly (printing should be cheap).
template <typename T>
static inline std::string iterableToString(const T& iterable)
{
    std::string out = "{";
    for (const auto& item : iterable) {
        out += std::to_string(item);
        out += ", ";
    }
    out += "}";
    return out;
}

bool DecoderConfig::validate() const
{
    bool valid = true;

    // m_ditherSeed
    if (m_ditherSeed != -1 && (m_ditherStrength == 0 || m_allowDithering == false)) {
        VNLogWarning("Setting a custom dither seed, but dithering has been manually disabled. No "
                     "dithering will occur");
    }
    if (m_ditherSeed != -1 && m_coreDecoderNumThreads != 1) {
        VNLogWarning("threads must be 1 to give deterministic dithering with a dither seed");
    }

    // m_ditherStrength
    if (m_ditherStrength > 1 && !m_allowDithering) {
        VNLogError("Forcing dither to non-zero value (%d), while also banning dithering. This is "
                   "incompatible.");
        valid = false;
    }

    // m_loqUnprocessedCap
    if (m_loqUnprocessedCap < -1) {
        VNLogError("Invalid config: loq_unprocessed_cap should not be less than -1, but it's %d",
                   m_loqUnprocessedCap);
        valid = false;
    }

    // m_predictedAverageMethod
    if (m_predictedAverageMethod < static_cast<int32_t>(PredictedAverageMethod::None) ||
        m_predictedAverageMethod >= static_cast<int32_t>(PredictedAverageMethod::COUNT)) {
        VNLogError("Invalid config: predicted_average_method should be between %d and %d "
                   "(inclusive), but it's %d",
                   static_cast<int32_t>(PredictedAverageMethod::None),
                   static_cast<int32_t>(PredictedAverageMethod::COUNT) - 1, m_predictedAverageMethod);
        valid = false;
    }

    // m_resultsQueueCap
    if (m_resultsQueueCap < -1) {
        VNLogError("Invalid config: results_queue_cap should not be less than -1, but it's %d",
                   m_resultsQueueCap);
        valid = false;
    }

    // m_events
    for (auto e : m_events) {
        if (e >= lcevc_dec::pipeline::Event_Count) {
            valid = false;
        }
    }

    VNLogDebugF("Additional config:\n"
                "\tallow_dithering           : %d\n"
                "\tcore_parallel_decode      : %d\n"
                "\tenable_logo_overlay       : %d\n"
                "\tgenerate_cmdbuffers       : %d\n"
                "\thighlight_residuals       : %d\n"
                "\thigh_precision            : %d\n"
                "\ts_filter_strength         : %f\n"
                "\tthreads              : %d\n"
                "\tdither_seed               : %d\n"
                "\tdither_strength           : %d\n"
                "\tforce_bitstream_version   : %d\n"
                "\tlogo_overlay_delay_frames : %d\n"
                "\tlogo_overlay_position_x   : %d\n"
                "\tlogo_overlay_position_y   : %d\n"
                "\tloq_unprocessed_cap       : %d\n"
                "\tpassthrough_mode          : %d\n"
                "\tpredicted_average_method  : %d\n"
                "\tpss_surface_fp_setting    : %d\n"
                "\tresults_queue_cap         : %d\n",
                m_allowDithering, m_coreParallelDecode, m_enableLogoOverlay, m_generateCmdbuffers,
                m_highlightResiduals, m_highPrecision, m_sFilterStrength, m_coreDecoderNumThreads,
                m_ditherSeed, m_ditherStrength, m_forceBitstreamVersion, m_logoOverlayDelayFrames,
                m_logoOverlayPositionX, m_logoOverlayPositionY, m_loqUnprocessedCap, m_passthroughMode,
                m_predictedAverageMethod, m_residualSurfaceFPSetting, m_resultsQueueCap);

    return valid;
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
#if VN_SDK_FEATURE(SSE)
    cfgOut.simd_type = ldcAccelerationGet()->SSE ? PSS_SIMD_AUTO : PSS_SIMD_DISABLED;
#elif VN_SDK_FEATURE(NEON)
    cfgOut.simd_type = ldcAccelerationGet()->NEON ? PSS_SIMD_AUTO : PSS_SIMD_DISABLED;
#else
    cfgOut.simd_type = PSS_SIMD_AUTO;
#endif
    cfgOut.generate_cmdbuffers = m_generateCmdbuffers;
    cfgOut.apply_cmdbuffers_internal = m_generateCmdbuffers;
    cfgOut.use_parallel_decode = m_coreParallelDecode;
    cfgOut.pipeline_mode = (m_highPrecision ? PPM_PRECISION : PPM_SPEED);

    // Settings where negative means "don't set/auto"
    if (m_coreDecoderNumThreads != -1) {
        cfgOut.num_worker_threads = m_coreDecoderNumThreads;
        cfgOut.apply_cmdbuffers_threads = static_cast<int16_t>(m_coreDecoderNumThreads);
    }
    if (m_forceBitstreamVersion >= 0) {
        cfgOut.force_bitstream_version = static_cast<uint8_t>(m_forceBitstreamVersion);
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

    VNLogVerboseF("Core decoder config:\n"
                  "\tdisable_dithering         : %" PRIu8 "\n"
                  "\tdither_override_strength  : %d\n"
                  "\fforce_bitstream_version   : %" PRIu8 "\n"
                  "\tgenerate_cmdbuffers       : %d\n"
                  "\tlogo_overlay_delay        : %" PRIu16 "\n"
                  "\tlogo_overlay_enable       : %" PRIu8 "\n"
                  "\tlogo_overlay_position_x,y : %" PRIu16 ",%" PRIu16 "\n"
                  "\tnum_worker_threads        : %d\n"
                  "\tparallel_decode           : %d\n"
                  "\tpipeline_mode             : %d\n"
                  "\ts_strength                : %f\n"
                  "\tsimd_enabled              : %d\n"
                  "\tuse_approximate_pa        : %" PRIu8 "",
                  cfgOut.disable_dithering, cfgOut.dither_override_strength, cfgOut.force_bitstream_version,
                  cfgOut.generate_cmdbuffers, cfgOut.logo_overlay_delay, cfgOut.logo_overlay_enable,
                  cfgOut.logo_overlay_position_x, cfgOut.logo_overlay_position_y,
                  cfgOut.num_worker_threads, cfgOut.pipeline_mode, cfgOut.use_parallel_decode,
                  cfgOut.s_strength, cfgOut.simd_type, cfgOut.use_approximate_pa);
}

} // namespace lcevc_dec::decoder
