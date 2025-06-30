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

// PipelineConfigCPU
//
// Configurable pipeline parameters - set up by the 'Builder' and then
// passed as a const structure into the initialized Pipeline.
//
#ifndef VN_LCEVC_PIPELINE_CPU_PIPELINE_CONFIG_CPU_H
#define VN_LCEVC_PIPELINE_CPU_PIPELINE_CONFIG_CPU_H

#include <cstdint>

namespace lcevc_dec::pipeline_cpu {

enum class PassthroughMode : int32_t
{
    Disable = -1, // base can never pass through. No decode occurs if lcevc is absent/inapplicable
    Allow = 0,    // base can pass through if lcevc is not found or not applied
    Force = 1,    // base must pass through, regardless of lcevc being present or applicable
    Scale = 2, // base can pass through if lcevc is not found or not applied - will be scaled by previous confguration
};

// The configurable values that get passed from the builder to the initialized pipeline
//`
struct PipelineConfigCPU
{
    // Memory arena defaults
    uint32_t initialArenaCount = 1024;
    uint32_t initialArenaSize = 65536;

    // Maximum number of frames to buffer
    uint32_t maxLatency = 32;

    // Minimum frames that can be held for batching
    uint32_t minLatency = 0;

    // Number of threads - thread pool plus main thread - defaults is filled in from number of platform cores plus 1
    uint32_t numThreads = 1;

    // Initial Number of slots reserved in task pool
    uint32_t numReservedTasks = 32;

    // Default maximum reorder
    uint32_t defaultMaxReorder = 16;

    // Number of frames late that enhancement can arrive late (non-standard)
    uint32_t enhancementDelay = 0;

    // Force scalar pixel operations
    bool forceScalar = false;

    // Show residuals for debugging
    bool highlightResiduals = false;

    // Number of temporal buffers per channel
    uint32_t numTemporalBuffers = 1;

    // How passthrough is handled by pipeline
    PassthroughMode passthroughMode = PassthroughMode::Scale;

    // Dither settings
    bool ditherEnabled = true;
    int32_t ditherOverrideStrength = -1;
    uint64_t ditherSeed = 0;

    // Override s-filter strength for testing
    float sharpeningOverrideStrength = -1;

    // Force the bitstream version
    int32_t forceBitstreamVersion = -1;

    // Describe generated frame tasks in log
    bool showTasks = false;

    // 'set' methods to adapt config types to internal values
    //
    bool setDitherSeed(const int32_t& val)
    {
        ditherSeed = val;
        return true;
    }

    bool setPassthroughMode(const int32_t& val)
    {
        if (val < static_cast<int32_t>(PassthroughMode::Disable) ||
            val > static_cast<int32_t>(PassthroughMode::Scale)) {
            return false;
        }
        passthroughMode = static_cast<PassthroughMode>(val);
        return true;
    }
};

} // namespace lcevc_dec::pipeline_cpu

#endif // VN_LCEVC_PIPELINE_CPU_PIPELINE_CONFIG_CPU_H
