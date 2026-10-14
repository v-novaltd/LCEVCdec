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

#ifndef VN_LCEVC_PIPELINE_TEMPORAL_BUFFER_BASE_H
#define VN_LCEVC_PIPELINE_TEMPORAL_BUFFER_BASE_H

// #include <LCEVC/common/class_utils.hpp>
// #include <LCEVC/common/memory.h>
#include <LCEVC/pipeline/picture.h>

// #include <memory>
#include <cstdint>

namespace lcevc_dec::pipeline {

class FrameBase;

// Description of a temporal buffer - size and timestamp
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
struct TemporalBufferBase
{
    virtual ~TemporalBufferBase() = default;

    // Reallocate buffer to match given description
    virtual void update(const TemporalBufferDesc& desc) = 0;

    // Description of this buffer
    TemporalBufferDesc desc;

    // Conservative CPU state: only a cleared buffer is known to contain no residuals.
    bool temporalBufferEmpty = false;

    // Timestamp upper limit that this buffer could fulfil
    uint64_t timestampLimit;

    // Frame that is using this buffer or null if available
    FrameBase* frame;
};

} // namespace lcevc_dec::pipeline

#endif // VN_LCEVC_PIPELINE_TEMPORAL_BUFFER_BASE_H
