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

#include "temporal_buffer_cpu.h"
//
#include <LCEVC/common/constants.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/buffer_alignment.h>

namespace lcevc_dec::pipeline_cpu {

// Make a temporal buffer match the given description
void TemporalBufferCPU::update(const pipeline::TemporalBufferDesc& newDesc)
{
    const size_t paddedWidth = alignU32(newDesc.width, kBufferRowAlignment);
    const size_t byteStride{paddedWidth * sizeof(uint16_t)};
    const size_t bufferSize{byteStride * static_cast<size_t>(newDesc.height)};

    if (!VNIsAllocated(allocation) || desc.width != newDesc.width || desc.height != newDesc.height) {
        // Reallocate buffer
        if (!newDesc.clear && newDesc.timestamp != kInvalidTimestamp) {
            // Frame was expecting prior residuals - but dimensions are wrong!?
            VNLogWarning("Temporal buffer does not match: %08d Got %dx%d, Wanted %dx%d",
                         newDesc.timestamp, desc.width, desc.height, newDesc.width, newDesc.height);
        }

        // Build PlaneDesc
        VNAllocateAlignedZeroArray(allocator, &allocation, uint8_t, kBufferRowAlignment, bufferSize,
                                   "TemporalBuffer_Data");
        planeDesc.firstSample = VNAllocationPtr(allocation, uint8_t);
        planeDesc.rowByteStride = static_cast<uint32_t>(byteStride);

        // Clear new buffer to zero
        memset(planeDesc.firstSample, 0, bufferSize);
        temporalBufferEmpty = true;
    } else if (newDesc.clear) {
        memset(planeDesc.firstSample, 0, bufferSize);
        temporalBufferEmpty = true;
    }

    // Update description
    desc = newDesc;
    desc.clear = false;
}

} // namespace lcevc_dec::pipeline_cpu
