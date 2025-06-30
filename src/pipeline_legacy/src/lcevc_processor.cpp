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

#include "lcevc_processor.h"

#include <LCEVC/common/constants.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/return_code.h>
#include <LCEVC/legacy/PerseusDecoder.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/sequencer/lcevc_container.h>
//
#include "buffer_manager.h"
//
#include <cinttypes>
#include <cstdint>
#include <limits>
#include <memory>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// ------------------------------------------------------------------------------------------------

LcevcProcessor::LcevcProcessor(perseus_decoder& decoder, BufferManager& bufferManager)
    : m_coreDecoderRef(decoder)
    , m_skipTemporalAccumulator(bufferManager)
{}

bool LcevcProcessor::initialise(uint32_t unprocessedLcevcCap, int32_t residualSurfaceFPSetting)
{
    m_residualSurfaceFPSetting = residualSurfaceFPSetting;

    LdcMemoryAllocator* allocator = ldcMemoryAllocatorMalloc();
    m_unprocessedLcevcContainer = lcevcContainerCreate(allocator, &allocation, unprocessedLcevcCap);
    return (m_unprocessedLcevcContainer != nullptr);
}

void LcevcProcessor::release()
{
    if (m_unprocessedLcevcContainer != nullptr) {
        lcevcContainerDestroy(m_unprocessedLcevcContainer);
    }
    m_unprocessedLcevcContainer = nullptr;
}

void LcevcProcessor::flush() { lcevcContainerClear(m_unprocessedLcevcContainer); }

LdcReturnCode LcevcProcessor::insertUnprocessedLcevcData(const uint8_t* data, uint32_t byteSize,
                                                         uint64_t timestamp, uint64_t inputTime)
{
    if (m_unprocessedLcevcContainer == nullptr) {
        VNLogError("Decoder is being fed enhancement data, but the LCEVC container has not been "
                   "initialised. The LcevcProcessor which holds the LCEVC Container is: %p",
                   static_cast<void*>(this));
        return LdcReturnCodeUninitialized;
    }

    if (!lcevcContainerInsert(m_unprocessedLcevcContainer, data, byteSize, timestamp, false, inputTime)) {
        VNLogError("timestamp %" PRId64
                   ": Failed to insert into LCEVC Container. Possible duplicate timestamp?",
                   timestamp);
        return LdcReturnCodeError;
    }

    return LdcReturnCodeSuccess;
}

std::shared_ptr<perseus_decoder_stream> LcevcProcessor::extractProcessedLcevcData(uint64_t timestamp,
                                                                                  bool discardProcessed)
{
    if (auto itToProcessedData = m_processedLcevcContainer.find(timestamp);
        itToProcessedData != m_processedLcevcContainer.end()) {
        // Found it in the pre-processed data
        std::shared_ptr<perseus_decoder_stream> out = itToProcessedData->second;
        if (discardProcessed) {
            m_processedLcevcContainer.erase(itToProcessedData);
        }
        return out;
    }
    return processUpToTimestamp(timestamp, discardProcessed);
}

uint32_t LcevcProcessor::getUnprocessedCapacity() const
{
    return static_cast<uint32_t>(lcevcContainerCapacity(m_unprocessedLcevcContainer));
}

bool LcevcProcessor::isUnprocessedQueueFull() const
{
    return (lcevcContainerSize(m_unprocessedLcevcContainer) >=
            lcevcContainerCapacity(m_unprocessedLcevcContainer));
}

std::shared_ptr<perseus_decoder_stream> LcevcProcessor::processUpToTimestamp(uint64_t timestamp,
                                                                             bool discardProcessed)
{
    // This currently fails to account for Peek operations, see DEC-277
    uint32_t numProcessed = 0;
    uint64_t lastExtractedTS = kInvalidTimestamp;
    std::shared_ptr<perseus_decoder_stream> processedData =
        processUpToTimestampLoop(timestamp, numProcessed, lastExtractedTS, discardProcessed);
    if (lastExtractedTS != timestamp) {
        VNLogWarning(
            "timestamp %" PRId64
            ": Could not find lcevc data. The last one we COULD find was timestamp %" PRId64
            ". Extracted and processed %u",
            timestamp, lastExtractedTS, numProcessed);
        return nullptr;
    }

    if (numProcessed > 1) {
        VNLogDebug("timestamp %"
                   ": processed %u to reach this frame's lcevc data",
                   timestamp, numProcessed);
    }

    return processedData;
}

bool LcevcProcessor::accumulateTemporalFromSkippedFrame(const perseus_decoder_stream& processedLcevcData)
{
    LdpPictureDesc i420Desc = {};
    i420Desc.colorFormat = LdpColorFormatI420_8;
    i420Desc.width = processedLcevcData.global_config.width;
    i420Desc.height = processedLcevcData.global_config.height;
    m_skipTemporalAccumulator.setDesc(i420Desc);
    perseus_image coreSkipAccumulator;
    m_skipTemporalAccumulator.toCoreImage(coreSkipAccumulator);

    // NOTE: the m_dpiSkipImg surfaces are NULL so that only the temporal is accumulated
    //       and the full size frame is not copied to.
    if (perseus_decoder_decode_high(m_coreDecoderRef, &coreSkipAccumulator) != 0) {
        return false;
    }

    return true;
}

std::shared_ptr<perseus_decoder_stream>
LcevcProcessor::processUpToTimestampLoop(uint64_t timestamp, uint32_t& numProcessedOut,
                                         uint64_t& lastExtractedTS, bool discardProcessed)
{
    size_t currentQueueSize = std::numeric_limits<size_t>::max();
    StampedBuffer* lcevcDataToProcess = nullptr;
    lastExtractedTS = kInvalidTimestamp;
    numProcessedOut = 0;
    std::shared_ptr<perseus_decoder_stream> dataOut = nullptr;
    while ((lastExtractedTS < timestamp) || (lastExtractedTS == kInvalidTimestamp)) {
        lcevcDataToProcess = lcevcContainerExtractNextInOrder(m_unprocessedLcevcContainer, true,
                                                              &lastExtractedTS, &currentQueueSize);
        if (lcevcDataToProcess == nullptr) {
            return nullptr;
        }
        numProcessedOut++;

        const uint8_t* rawData = stampedBufferGetBuffer(lcevcDataToProcess);
        const size_t rawDataLen = stampedBufferGetBufSize(lcevcDataToProcess);
        dataOut = std::make_shared<perseus_decoder_stream>();

        if (rawData == nullptr && rawDataLen == 0) {
            VNLogDebug("timestamp %" PRId64 ": No LCEVC data, will passthrough", timestamp);
            stampedBufferRelease(&lcevcDataToProcess);
            return nullptr;
        }

        if (perseus_decoder_parse(m_coreDecoderRef, rawData, rawDataLen, dataOut.get()) != 0) {
            VNLogError("timestamp %" PRId64 ": Failed to parse lcevc data", timestamp);
            stampedBufferRelease(&lcevcDataToProcess);
            return nullptr;
        }

        setLiveDecoderConfig(dataOut->global_config);

        if ((lastExtractedTS < timestamp) || (lastExtractedTS == kInvalidTimestamp)) {
            // This means we'll need to do another iteration (in other words, we're skipping this
            // frame), so we need to do some processing to accumulate temporal residuals.
            if (dataOut == nullptr || !accumulateTemporalFromSkippedFrame(*dataOut)) {
                VNLogError("timestamp %" PRId64 " Failed to skip and accumulate temporal residuals",
                           lastExtractedTS);
            }
        }

        stampedBufferRelease(&lcevcDataToProcess);

        if (lastExtractedTS <= timestamp && !discardProcessed) {
            m_processedLcevcContainer[lastExtractedTS] = dataOut;
        }
    }
    return dataOut;
}

void LcevcProcessor::setLiveDecoderConfig(const perseus_global_config& globalConfig) const
{
    perseus_decoder_live_config liveConfig = {};

    if (m_residualSurfaceFPSetting == -1) {
        const bool enhancedIs8bit = (globalConfig.bitdepths[PSS_LOQ_0] == PSS_DEPTH_8 &&
                                     globalConfig.bitdepths[PSS_LOQ_1] == PSS_DEPTH_8);
        liveConfig.format = (enhancedIs8bit ? PSS_SURFACE_U8 : PSS_SURFACE_S16);
    } else {
        liveConfig.format = static_cast<perseus_surface_format>(m_residualSurfaceFPSetting);
    }

    perseus_decoder_set_live_config(m_coreDecoderRef, liveConfig);
}

} // namespace lcevc_dec::decoder
