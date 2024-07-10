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

#include "lcevc_processor.h"

#include "buffer_manager.h"
#include "handle.h"
#include "log.h"
#include "timestamps.h"

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/PerseusDecoder.h>
#include <lcevc_container.h>

#include <cinttypes>
#include <cstdint>
#include <memory>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

static const LogComponent kComp = LogComponent::LCEVCProcessor;

// ------------------------------------------------------------------------------------------------

LcevcProcessor::LcevcProcessor(perseus_decoder& decoder, BufferManager& bufferManager)
    : m_coreDecoderRef(decoder)
    , m_skipTemporalAccumulator(bufferManager)
{}

bool LcevcProcessor::initialise(uint32_t unprocessedLcevcCap, int32_t residualSurfaceFPSetting)
{
    m_residualSurfaceFPSetting = residualSurfaceFPSetting;

    m_unprocessedLcevcContainer = lcevcContainerCreate(unprocessedLcevcCap);
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

int32_t LcevcProcessor::insertUnprocessedLcevcData(const uint8_t* data, uint32_t byteSize,
                                                   uint64_t timehandle, uint64_t inputTime)
{
    if (m_unprocessedLcevcContainer == nullptr) {
        VNLogError("Decoder is being fed enhancement data, but the LCEVC container has not been "
                   "initialised. The LcevcProcessor which holds the LCEVC Container is: %p\n",
                   this);
        return LCEVC_Uninitialized;
    }

    if (!lcevcContainerInsert(m_unprocessedLcevcContainer, data, byteSize, timehandle, inputTime)) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Failed to insert into LCEVC Container. Possible duplicate timehandle?.\n",
                   timehandleGetCC(timehandle), timehandleGetTimestamp(timehandle));
        return LCEVC_Error;
    }

    return LCEVC_Success;
}

std::shared_ptr<perseus_decoder_stream> LcevcProcessor::extractProcessedLcevcData(uint64_t timehandle,
                                                                                  bool discardProcessed)
{
    if (auto itToProcessedData = m_processedLcevcContainer.find(timehandle);
        itToProcessedData != m_processedLcevcContainer.end()) {
        if (discardProcessed) {
            m_processedLcevcContainer.erase(itToProcessedData);
        }
        return itToProcessedData->second;
    }
    return processUpToTimehandle(timehandle, discardProcessed);
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

bool LcevcProcessor::contains(uint64_t timehandle) const
{
    bool dummyIsHeadOut = false;
    return lcevcContainerExists(m_unprocessedLcevcContainer, timehandle, &dummyIsHeadOut) ||
           (m_processedLcevcContainer.count(timehandle) > 0);
}

std::shared_ptr<perseus_decoder_stream> LcevcProcessor::processUpToTimehandle(uint64_t timehandle,
                                                                              bool discardProcessed)
{
    // This currently fails to account for Peek operations, see DEC-277
    uint32_t numProcessed = 0;
    uint64_t lastExtractedTH = kInvalidHandle;
    std::shared_ptr<perseus_decoder_stream> processedData =
        processUpToTimehandleLoop(timehandle, numProcessed, lastExtractedTH, discardProcessed);
    if (lastExtractedTH != timehandle) {
        VNLogWarning(
            "CC %u PTS %" PRId64
            ": Could not find lcevc data. The last one we COULD find was CC %u PTS %" PRId64
            ". Extracted and processed %u.\n",
            timehandleGetCC(timehandle), timehandleGetTimestamp(timehandle),
            timehandleGetCC(lastExtractedTH), timehandleGetTimestamp(lastExtractedTH), numProcessed);
        return nullptr;
    }

    if (numProcessed > 1) {
        VNLogDebug("CC %u PTS %"
                   ": processed %u to reach this frame's lcevc data.\n",
                   timehandleGetCC(timehandle), timehandleGetTimestamp(timehandle), numProcessed);
    }

    return processedData;
}

bool LcevcProcessor::accumulateTemporalFromSkippedFrame(const perseus_decoder_stream& processedLcevcData)
{
    LCEVC_PictureDesc i420Desc = {};
    i420Desc.colorFormat = LCEVC_I420_8;
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
LcevcProcessor::processUpToTimehandleLoop(uint64_t timehandle, uint32_t& numProcessedOut,
                                          uint64_t& lastExtractedTH, bool discardProcessed)
{
    size_t currentQueueSize = std::numeric_limits<size_t>::max();
    StampedBuffer_t* lcevcDataToProcess = nullptr;
    lastExtractedTH = kInvalidTimehandle;
    numProcessedOut = 0;
    std::shared_ptr<perseus_decoder_stream> dataOut = nullptr;
    while ((lastExtractedTH < timehandle) || (lastExtractedTH == kInvalidTimehandle)) {
        lcevcDataToProcess = lcevcContainerExtractNextInOrder(m_unprocessedLcevcContainer, true,
                                                              &lastExtractedTH, &currentQueueSize);
        if (lcevcDataToProcess == nullptr) {
            return nullptr;
        }
        numProcessedOut++;

        const uint8_t* rawData = stampedBufferGetBuffer(lcevcDataToProcess);
        const size_t rawDataLen = stampedBufferGetBufSize(lcevcDataToProcess);
        dataOut = std::make_shared<perseus_decoder_stream>();

        if (perseus_decoder_parse(m_coreDecoderRef, rawData, rawDataLen, dataOut.get()) != 0) {
            VNLogError("CC %u PTS %" PRId64 ": Failed to parse lcevc data.\n",
                       timehandleGetCC(timehandle), timehandleGetTimestamp(timehandle));
            return nullptr;
        }

        setLiveDecoderConfig(dataOut->global_config);

        if ((lastExtractedTH < timehandle) || (lastExtractedTH == kInvalidTimehandle)) {
            // This means we'll need to do another iteration (in other words, we're skipping this
            // frame), so we need to do some processing to accumulate temporal residuals.
            if (dataOut == nullptr || !accumulateTemporalFromSkippedFrame(*dataOut)) {
                VNLogError("CC %u PTS %" PRId64
                           " Failed to skip and accumulate temporal residuals.\n",
                           timehandleGetCC(lastExtractedTH), timehandleGetTimestamp(lastExtractedTH));
            }
        }

        stampedBufferRelease(&lcevcDataToProcess);

        if (lastExtractedTH < timehandle && !discardProcessed) {
            m_processedLcevcContainer[lastExtractedTH] = dataOut;
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
