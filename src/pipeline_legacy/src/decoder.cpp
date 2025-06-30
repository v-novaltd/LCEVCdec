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

#define VNComponentCurrent LdcComponentDecoder

#include "decoder.h"
//
#include <LCEVC/common/log.h>
#include <LCEVC/common/return_code.h>
#include <LCEVC/legacy/PerseusDecoder.h>
#include <LCEVC/pipeline/event_sink.h>
//
#include "core_interface.h"
#include "decode_information.h"
#include "picture.h"
//
#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <memory>
#include <utility>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// - Decoder --------------------------------------------------------------------------------------

Decoder::Decoder(const DecoderConfig& config, pipeline::EventSink* eventSink)
    : m_lcevcProcessor(m_coreDecoder, m_bufferManager)
    , m_config(config)
    , m_eventSink(eventSink)
{}

Decoder::~Decoder() { release(); }

bool Decoder::initialize()
{
    // Initialisation order:
    // 1) The config, so the rest of the initialization can be logged if needed.
    // 2) The event manager, just in case the subsequent steps have to send events (although that
    //    should really wait until the end of initialization)
    // 3) Everything else, in no particular order.
    if (!initializeConfig()) {
        VNLogError("Failed to initialize Config. Decoder: %p.", (void*)this);
        return false;
    }

    m_eventSink->enableEvents(m_config.getEvents());

    if (!initializeCoreDecoder()) {
        VNLogError("Failed to initialize Core Decoder. Decoder: %p.", (void*)this);
        return false;
    }
    if (!initializeLcevcProcessor()) {
        VNLogError("Failed to initialize LCEVC Processor. Decoder: %p.", (void*)this);
        return false;
    }

    // Initialization done. Note that we trigger "can send enhancement" first, in case the client
    // is blindly sending data every time they get a "can send", without checking that they've sent
    // the enhancement before the base.
    m_eventSink->generate(pipeline::EventCanSendEnhancement);
    m_eventSink->generate(pipeline::EventCanSendBase);
    m_eventSink->generate(pipeline::EventCanSendPicture);

    return true;
}

bool Decoder::initializeConfig()
{
    // Logs first, then the rest of the config.
    return m_config.validate();
}

bool Decoder::initializeCoreDecoder()
{
    perseus_decoder_config coreCfg = {};
    m_config.initialiseCoreConfig(coreCfg);
    if (perseus_decoder_open(&m_coreDecoder, &coreCfg) != 0) {
        return false;
    }
    perseus_decoder_debug(m_coreDecoder,
                          m_config.getHighlightResiduals() ? HIGHLIGHT_RESIDUALS : NO_DEBUG_MODE);
    return true;
}

bool Decoder::initializeLcevcProcessor()
{
    return m_lcevcProcessor.initialise(m_config.getLoqUnprocessedCap(),
                                       m_config.getResidualSurfaceFPSetting());
}

void Decoder::release()
{
    // Release resources in the reverse of the order they were initialized in, in case of
    // dependencies.
    releaseLcevcProcessor();
    releaseCoreDecoder();

    m_eventSink->generate(pipeline::EventExit);
}

void Decoder::releaseCoreDecoder()
{
    perseus_decoder_close(m_coreDecoder);
    m_coreDecoder = nullptr;
}

void Decoder::releaseLcevcProcessor() { m_lcevcProcessor.release(); }

LdcReturnCode Decoder::sendBasePicture(uint64_t timestamp, LdpPicture* baseLdpPicture,
                                       uint32_t timeoutUs, void* userData)
{
    Picture* basePicture = fromLdpPicturePtr(baseLdpPicture);
    assert(basePicture != nullptr);

    if (isBaseQueueFull()) {
        VNLogInfo("Base container is full. Size is %zu but capacity is %u.", m_baseContainer.size(),
                  m_lcevcProcessor.getUnprocessedCapacity());
        return LdcReturnCodeAgain;
    }

    if (basePicture == nullptr) {
        return LdcReturnCodeError;
    }

    // set identifying data
    basePicture->setTimestamp(timestamp);
    basePicture->setUserData(userData);

    m_baseContainer.emplace_back(basePicture, m_clock.getElapsedTime(), timeoutUs);

    tryToQueueDecodes();

    return LdcReturnCodeSuccess;
}

LdcReturnCode Decoder::sendEnhancementData(uint64_t timestamp, const uint8_t* data, uint32_t byteSize)
{
    if (isUnprocessedEnhancementQueueFull()) {
        VNLogInfo("Unprocessed enhancement container is full. Unprocessed container "
                  "capacity is %u..",
                  m_lcevcProcessor.getUnprocessedCapacity());
        return LdcReturnCodeAgain;
    }

    const uint64_t inputTime = m_clock.getElapsedTime();
    LdcReturnCode insertRes =
        m_lcevcProcessor.insertUnprocessedLcevcData(data, byteSize, timestamp, inputTime);
    if (insertRes != LdcReturnCodeSuccess) {
        return insertRes;
    }

    tryToQueueDecodes();

    return LdcReturnCodeSuccess;
}

LdcReturnCode Decoder::sendOutputPicture(LdpPicture* outputLdpPicture)
{
    Picture* outputPicture = fromLdpPicturePtr(outputLdpPicture);

    assert(outputPicture != nullptr);
    if (outputPicture == nullptr) {
        return LdcReturnCodeError;
    }
    if (m_pendingOutputContainer.size() >= m_lcevcProcessor.getUnprocessedCapacity()) {
        VNLogDebug("Pending outputs container is full. Size is %zu but capacity is %u.",
                   m_pendingOutputContainer.size(), m_lcevcProcessor.getUnprocessedCapacity());
        return LdcReturnCodeAgain;
    }

    m_pendingOutputContainer.emplace(outputPicture);

    tryToQueueDecodes();

    return LdcReturnCodeSuccess;
}

LdpPicture* Decoder::receiveOutputPicture(LdpDecodeInformation& decodeInfoOut)
{
    if (m_resultsQueue.empty()) {
        return nullptr;
    }

    // Get result (as a copy, so we can pop immediately)
    Result nextResult = m_resultsQueue.front();
    m_resultsQueue.pop_front();
    tryToQueueDecodes(); // Queue more decodes, now there's a free spot at the end of the assembly line.

    // Set output params
    decodeInfoOut = nextResult.decodeInfo;
    Picture* outputPicture = nextResult.picture;

    m_eventSink->generate(pipeline::EventOutputPictureDone, outputPicture, &nextResult.decodeInfo);
    if (nextResult.returnCode != LdcReturnCodeSuccess) {
        return nullptr;
    }
    return outputPicture;
}

LdcReturnCode Decoder::flush(uint64_t timestamp)
{
    // This throws away all bases, enhancements, and NOT-YET-DECODED output pictures. RESULTS, on
    // the other hand, are preserved so that we can return the picture and return code.
    flushInputs();
    flushOutputs();

    return LdcReturnCodeSuccess;
}

void Decoder::flushInputs()
{
    // Enhancements
    const bool enhancementsFull = isUnprocessedEnhancementQueueFull();
    m_lcevcProcessor.flush();
    if (enhancementsFull && !isUnprocessedEnhancementQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendEnhancement);
    }

    // Bases
    const bool basesFull = isBaseQueueFull();
    while (!m_baseContainer.empty()) {
        Picture* finishedBase = m_baseContainer.front().nonNullPicture;
        m_baseContainer.pop_front();
        m_eventSink->generate(pipeline::EventBasePictureDone, finishedBase);
    }
    if (basesFull && !isBaseQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendBase);
    }
}

void Decoder::flushOutputs()
{
    // Pending (not-yet-decoded) outputs
    const bool pendingOutputsFull = isOutputQueueFull();
    while (!m_pendingOutputContainer.empty()) {
        m_pendingOutputContainer.pop();
    }
    if (pendingOutputsFull && !isOutputQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendPicture);
    }

    // Decoded outputs: don't flush the Results. Instead, release the Picture's stored data (memory
    // is precious) and set the result to "Flushed".
    for (auto& result : m_resultsQueue) {
        Picture* output = result.picture;
        output->unbindMemory();
        result.returnCode = LdcReturnCodeFlushed;
    }
}

LdcReturnCode Decoder::peek(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut)
{
    // Rarely, we get the easy case, where the client has already sent base, enhancement, and
    // destination pictures, so we have a finished decode ready to go.
    if (const Result* res = findDecodeResult(timestamp); res != nullptr) {
        const Picture& pic = *res->picture;
        widthOut = pic.getWidth();
        heightOut = pic.getHeight();
        return res->returnCode;
    }

    // If the client has NOT sent destination pictures (for example, if they're using peek to
    // decide what size their pictures should be), then we have to work out for ourselves what the
    // output might look like.

    // Get data
    const std::shared_ptr<perseus_decoder_stream> lcevcData =
        m_lcevcProcessor.extractProcessedLcevcData(timestamp, false);
    const BaseData* baseData = findBaseData(timestamp);

    // Always need base OR lcevc
    if (baseData == nullptr && lcevcData == nullptr) {
        return LdcReturnCodeNotFound;
    }

    // In never-passthrough, we need the enhancement.
    if (m_config.getPassthroughMode() == PassthroughPolicy::Disable && lcevcData == nullptr) {
        return LdcReturnCodeNotFound;
    }

    // If we don't have a base, then either fail, or rely entirely on lcevc.
    if (baseData == nullptr) {
        if (m_config.getPassthroughMode() == PassthroughPolicy::Force) {
            return LdcReturnCodeNotFound;
        }
        widthOut = lcevcData->global_config.width;
        heightOut = lcevcData->global_config.height;
        return LdcReturnCodeSuccess;
    }

    // Finally, if we DO have the base, we can simply use the same check used by the actual decode.
    const Picture& basePic = *baseData->nonNullPicture;
    const bool timeout = static_cast<int64_t>(baseData->insertionTime + baseData->timeoutUs) <
                         m_clock.getElapsedTime();
    const bool lcevcAvailable = (lcevcData != nullptr);
    const auto [shouldPassthrough, shouldFail] = shouldPassthroughOrFail(timeout, lcevcAvailable);
    if (shouldPassthrough) {
        widthOut = basePic.getWidth();
        heightOut = basePic.getHeight();
    } else if (!shouldFail) {
        widthOut = lcevcData->global_config.width;
        heightOut = lcevcData->global_config.height;
    }

    const LdcReturnCode returnCode =
        (timeout ? LdcReturnCodeTimeout : (shouldFail ? LdcReturnCodeError : LdcReturnCodeSuccess));
    return returnCode;
}

LdcReturnCode Decoder::skip(uint64_t timestamp)
{
    // Erase bases (up to and including this one)
    const bool basesFull = isBaseQueueFull();
    Picture* base = (m_baseContainer.empty() ? nullptr : m_baseContainer.front().nonNullPicture);
    while (base != nullptr) {
        const Picture& curBase = *base;
        if (curBase.getTimestamp() > timestamp) {
            break;
        }
        m_baseContainer.pop_front();

        m_eventSink->generate(pipeline::EventBasePictureDone, base);
        m_finishedBaseContainer.push(base);

        base = (m_baseContainer.empty() ? nullptr : m_baseContainer.front().nonNullPicture);
    }
    if (basesFull && !isBaseQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendBase);
    }

    // Process-and-erase enhancements (up to and including this one).
    const bool enhancementsFull = isUnprocessedEnhancementQueueFull();
    m_lcevcProcessor.extractProcessedLcevcData(timestamp);
    if (enhancementsFull && !isUnprocessedEnhancementQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendEnhancement);
    }

    // If we have any decode results for this or earlier timestamps, set them to skipped. Note: we
    // only need to do this for decodes that have already produced a decode result. For other
    // timestamps, it's like they simply never happened.
    for (auto& thAndInfo : m_resultsQueue) {
        if (thAndInfo.decodeInfo.timestamp <= timestamp) {
            thAndInfo.decodeInfo.skipped = true;
        }
    }

    return LdcReturnCodeSuccess;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
LdcReturnCode Decoder::synchronize(bool dropPending)
{
    // For now, this is (rightly) empty, i.e. we're already always synchronized. Once we implement
    // AccelContext, this function will do something like:
    //
    // AccelContext* context = m_accelContextPool.lookup(m_accelContextHandle);
    // context->synchronize(dropPending);
    VN_UNUSED(dropPending);
    return LdcReturnCodeSuccess;
}
LdpPicture* Decoder::receiveFinishedBasePicture()
{
    if (m_finishedBaseContainer.empty()) {
        return nullptr;
    }
    Picture* base = m_finishedBaseContainer.front();

    m_finishedBaseContainer.pop();
    return base;
}

Picture* Decoder::getNextDecodeData(BaseData& nextBase,
                                    std::shared_ptr<const perseus_decoder_stream>& nextProcessedLcevcData)
{
    if (m_resultsQueue.size() >= m_config.getResultsQueueCap()) {
        VNLogDebug("Results container is full. Size is %zu but capacity is %u. Client "
                   "should try calling EventReceiveDecoderPicture.",
                   m_resultsQueue.size(), m_config.getResultsQueueCap());
        return nullptr;
    }

    // Have a valid base.
    if (m_baseContainer.empty()) {
        return nullptr;
    }
    const Picture* nonNullPicture = m_baseContainer.front().nonNullPicture;
    const Picture& base = *nonNullPicture;
    const uint64_t timestamp = base.getTimestamp();
    if (timestamp == kInvalidTimestamp) {
        return nullptr;
    }

    // Have a valid output
    if (m_pendingOutputContainer.empty()) {
        return nullptr;
    }

    // Don't need to check for valid lcevc data: lcevc data is expected to be sent first, so if we
    // don't have it now, we won't ever. From here on, function is guaranteed to succeed.

    // Check transition from full to non-full.
    const bool basesFull = isBaseQueueFull();
    const bool enhancementsFull = isUnprocessedEnhancementQueueFull();
    const bool pendingOutputsFull = isOutputQueueFull();

    // Steal the data from our containers.
    Picture* picture = m_pendingOutputContainer.front();
    nextProcessedLcevcData = m_lcevcProcessor.extractProcessedLcevcData(timestamp);
    nextBase = m_baseContainer.front();
    m_baseContainer.pop_front();
    m_pendingOutputContainer.pop();

    // Trigger non-full events
    if (basesFull && !isBaseQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendBase);
    }
    if (enhancementsFull && !isUnprocessedEnhancementQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendEnhancement);
    }
    if (pendingOutputsFull && !isOutputQueueFull()) {
        m_eventSink->generate(pipeline::EventCanSendPicture);
    }

    return picture;
}

std::pair<bool, bool> Decoder::shouldPassthroughOrFail(bool timeout, bool lcevcAvailable) const
{
    const bool needToPassthrough = timeout || !lcevcAvailable;

    switch (m_config.getPassthroughMode()) {
        case PassthroughPolicy::Disable: {
            return {false, needToPassthrough};
        }

        case PassthroughPolicy::Allow: {
            return {needToPassthrough, false};
        }

        case PassthroughPolicy::Force: {
            return {true, false};
        }
    }
    return {false, false};
}

Result& Decoder::populateDecodeResult(Picture* decodeDest, const BaseData& baseData, bool lcevcAvailable,
                                      bool shouldFail, bool shouldPassthrough, bool wasTimeout)
{
    const Picture& base = *baseData.nonNullPicture;

    const LdcReturnCode returnCode =
        (shouldFail ? LdcReturnCodeError : (wasTimeout ? LdcReturnCodeTimeout : LdcReturnCodeSuccess));
    auto& result = m_resultsQueue.emplace_back(
        decodeDest, returnCode, DecodeInformation(base, lcevcAvailable, shouldPassthrough, shouldFail));

    return result;
}

Result* Decoder::findDecodeResult(uint64_t timestamp)
{
    auto existingResult =
        std::find_if(m_resultsQueue.begin(), m_resultsQueue.end(), [timestamp](const Result& res) {
            return (res.decodeInfo.timestamp == timestamp);
        });
    return (existingResult == m_resultsQueue.end() ? nullptr : &(*existingResult));
}

BaseData* Decoder::findBaseData(uint64_t timestamp)
{
    if (m_baseContainer.empty()) {
        return nullptr;
    }

    const auto findBase = [timestamp](const BaseData& baseData) {
        const Picture& pic = *baseData.nonNullPicture;
        return (pic.getTimestamp() == timestamp);
    };
    auto iterToBase = std::find_if(m_baseContainer.begin(), m_baseContainer.end(), findBase);
    return (iterToBase == m_baseContainer.end() ? nullptr : &(*iterToBase));
}

void Decoder::tryToQueueDecodes()
{
    BaseData nextBase(nullptr, 0, 0);
    std::shared_ptr<const perseus_decoder_stream> nextProcessedLcevcData = nullptr;
    Picture* decodeDest = nullptr;
    Result* newResultLoc = nullptr;
    while ((decodeDest = getNextDecodeData(nextBase, nextProcessedLcevcData)) != nullptr) {
        const LdcReturnCode res =
            doDecode(nextBase, nextProcessedLcevcData.get(), decodeDest, newResultLoc);
        newResultLoc->returnCode = res;

        // Trigger "canReceive" even if we failed, because in any case, we know it's done
        m_eventSink->generate(pipeline::EventCanReceive);

        m_eventSink->generate(pipeline::EventBasePictureDone, nextBase.nonNullPicture);
        m_finishedBaseContainer.push(nextBase.nonNullPicture);
    }
}

LdcReturnCode Decoder::doDecode(const BaseData& baseData, const perseus_decoder_stream* processedLcevcData,
                                Picture* decodeDest, Result*& resultOut)
{
    // First, check whether we fail, passthrough, or enhance:
    const bool timeout =
        static_cast<int64_t>(baseData.insertionTime + baseData.timeoutUs) < m_clock.getElapsedTime();
    const bool lcevcAvailable = (processedLcevcData != nullptr);
    const auto [shouldPassthrough, shouldFail] = shouldPassthroughOrFail(timeout, lcevcAvailable);

    // Based on this, populate decode result (including whether it fails).
    resultOut = &populateDecodeResult(decodeDest, baseData, lcevcAvailable, shouldFail,
                                      shouldPassthrough, timeout);

    // NOW fail, if necessary
    const Picture& base = *baseData.nonNullPicture;
#ifdef VN_SDK_LOG_ENABLE_INFO
    const uint64_t timestamp = base.getTimestamp();
#endif
    if (shouldFail) {
        VNLogError("timestamp %" PRId64
                   ": We were%s able to find lcevc data, failing decode. Passthrough mode is %d",
                   timestamp, (lcevcAvailable ? "" : " NOT"), (uint8_t)m_config.getPassthroughMode());
        return (timeout ? LdcReturnCodeTimeout : LdcReturnCodeError);
    }

    // Not failing, i.e. either passthrough or enhance, so set up the destination pic.
    Picture& decodeDestPic = *decodeDest;
    if (!decodeSetupOutputPic(decodeDestPic, shouldPassthrough ? nullptr : processedLcevcData, base)) {
        VNLogError("timestamp %" PRId64 ": Failed to setup output pic. Perhaps invalid "
                   "formats, or unmodifiable destination?",
                   timestamp);
        return LdcReturnCodeError;
    }

    // Now, passthrough or enhance.
    if (shouldPassthrough) {
        if (!timeout && !(m_config.getPassthroughMode() == PassthroughPolicy::Force)) {
            VNLogInfo("timestamp %" PRId64 ": Doing passthrough due to lack of lcevc data", timestamp);
        }
        return decodePassthrough(baseData, decodeDestPic);
    }
    return decodeEnhance(baseData, *processedLcevcData, decodeDestPic);
}

LdcReturnCode Decoder::decodePassthrough(const BaseData& baseData, Picture& decodeDest)
{
    const Picture& base = *baseData.nonNullPicture;
    return (decodeDest.copyData(base) ? LdcReturnCodeSuccess : LdcReturnCodeError);
}

LdcReturnCode Decoder::decodeEnhance(const BaseData& baseData,
                                     const perseus_decoder_stream& processedLcevcData, Picture& decodeDest)
{
    // Get a base (either a non-deleting pointer to the base, or a deleting pointer to a copy).
    Picture& base = *baseData.nonNullPicture;
    const std::shared_ptr<Picture> baseToUse = decodeEnhanceGetBase(base, processedLcevcData);
    const std::shared_ptr<Picture> intermediatePicture =
        decodeEnhanceGetIntermediate(baseToUse, processedLcevcData);
    const uint64_t timestamp = baseToUse->getTimestamp();

    // Set up the images used by the Core decoder.
    perseus_image coreBase = {};
    perseus_image coreIntermediate = {};
    perseus_image coreEnhanced = {};

    if (!decodeEnhanceSetupCoreImages(*baseToUse, intermediatePicture, decodeDest, coreBase,
                                      coreIntermediate, coreEnhanced)) {
        VNLogError("timestamp %" PRId64 ": Failed to set up core images.", timestamp);
        return LdcReturnCodeError;
    }

    // Do the actual decode.
    return decodeEnhanceCore(timestamp, coreBase, coreIntermediate, coreEnhanced, processedLcevcData);
}

bool Decoder::decodeSetupOutputPic(Picture& enhancedPic, const perseus_decoder_stream* processedLcevcData,
                                   const Picture& basePic)
{
    enhancedPic.setTimestamp(basePic.getTimestamp());

    if (processedLcevcData == nullptr) {
        return enhancedPic.copyMetadata(basePic);
    }

    // Start with the existing desc, then update with info from processedLcevcData
    LdpPictureDesc modifiedDesc;
    enhancedPic.getDesc(modifiedDesc);

    LdpPictureDesc lcevcPictureDesc = modifiedDesc;
    if (!coreFormatToLdpPictureDesc(*processedLcevcData, lcevcPictureDesc)) {
        VNLogError("timestamp %" PRId64
                   ": Could not deduce a valid picture format from this frame's LCEVC data.",
                   enhancedPic.getTimestamp());
        return false;
    }

    return enhancedPic.setDesc(lcevcPictureDesc);
}

std::shared_ptr<Picture> Decoder::decodeEnhanceGetBase(Picture& originalBase,
                                                       const perseus_decoder_stream& processedLcevcData)
{
    // This gives you a non-deleting pointer to the base if the Core can be trusted with it, or
    // else it gives you a normal (i.e. deleting) smart pointer to a copy.

    // Precision mode makes a copy so it doesn't modify. And loq_1 is the base-most loq, so it's
    // the only one that would apply its residuals straight to the base.
    const bool coreWillModifyBase = (processedLcevcData.pipeline_mode != PPM_PRECISION &&
                                     processedLcevcData.loq_enabled[PSS_LOQ_1]);

    if (originalBase.canModify() || !coreWillModifyBase) {
        return {&originalBase, [](auto ptr) { VN_UNUSED(ptr); }};
    }

    std::shared_ptr<Picture> newPic = std::make_shared<PictureManaged>(m_bufferManager);
    newPic->copyData(originalBase);
    return newPic;
}

std::shared_ptr<Picture> Decoder::decodeEnhanceGetIntermediate(const std::shared_ptr<Picture>& basePicture,
                                                               const perseus_decoder_stream& processedLcevcData)
{
    const perseus_scaling_mode level1Scale = processedLcevcData.global_config.scaling_modes[PSS_LOQ_1];
    if (level1Scale == PSS_SCALE_0D) {
        return nullptr;
    }

    LdpPictureDesc intermediateDesc = {};
    basePicture->getDesc(intermediateDesc);
    intermediateDesc.height =
        (level1Scale == PSS_SCALE_2D) ? intermediateDesc.height * 2 : intermediateDesc.height;
    intermediateDesc.width *= 2;
    std::shared_ptr<Picture> newPic = std::make_shared<PictureManaged>(m_bufferManager);
    newPic->setDesc(intermediateDesc);
    return newPic;
}

bool Decoder::decodeEnhanceSetupCoreImages(Picture& basePic,
                                           const std::shared_ptr<Picture>& intermediatePicture,
                                           Picture& enhancedPic, perseus_image& baseOut,
                                           perseus_image& intermediateOut, perseus_image& enhancedOut)
{
    if (!basePic.toCoreImage(baseOut)) {
        VNLogError("timestamp %" PRId64 ": Failed to get core image from base picture",
                   basePic.getTimestamp());
        return false;
    }

    if (intermediatePicture) {
        if (!intermediatePicture->toCoreImage(intermediateOut)) {
            VNLogError("timestamp %" PRId64 ": Failed to get core image from intermediate picture",
                       intermediatePicture->getTimestamp());
            return false;
        }
    }

    if (!enhancedPic.toCoreImage(enhancedOut)) {
        VNLogError("timestamp %" PRId64 ": Failed to get core image from enhanced picture",
                   basePic.getTimestamp());
        return false;
    }

    if (baseOut.ilv != enhancedOut.ilv) {
        VNLogError("timestamp %" PRId64
                   ": Base interleaving (%d) must match output interleaving (%d).",
                   basePic.getTimestamp(), (uint8_t)baseOut.ilv, (uint8_t)enhancedOut.ilv);
        return false;
    }

    return true;
}

LdcReturnCode Decoder::decodeEnhanceCore(uint64_t timestamp, const perseus_image& coreBase,
                                         const perseus_image& coreIntermediate,
                                         const perseus_image& coreEnhanced,
                                         const perseus_decoder_stream& processedLcevcData)
{
    const perseus_image* coreBaseInternal = &coreBase;
    if (processedLcevcData.global_config.scaling_modes[PSS_LOQ_1] != PSS_SCALE_0D) {
        perseus_decoder_upscale(m_coreDecoder, &coreIntermediate, &coreBase, PSS_LOQ_2);
        coreBaseInternal = &coreIntermediate;
    }

    // Decode base
    if (perseus_decoder_decode_base(m_coreDecoder, coreBaseInternal) != 0) {
        VNLogError("timestamp %" PRId64 ": Failed to decode Perseus base LOQ.", timestamp);
        return LdcReturnCodeError;
    }

    if (perseus_decoder_upscale(m_coreDecoder, &coreEnhanced, coreBaseInternal, PSS_LOQ_1) != 0) {
        VNLogError("timestamp %" PRId64 ": Failed to upscale Perseus.", timestamp);
        return LdcReturnCodeError;
    }

    // In-loop sharpening not supported by encoder, currently unreachable
    if ((processedLcevcData.s_info.mode == PSS_S_MODE_IN_LOOP) &&
        (perseus_decoder_apply_s(m_coreDecoder, &coreEnhanced) != 0)) {
        VNLogError("timestamp %" PRId64 ": Failed to apply sfilter in loop.", timestamp);
        return LdcReturnCodeError;
    }

    // Decode high
    if (perseus_decoder_decode_high(m_coreDecoder, &coreEnhanced) != 0) {
        VNLogError("timestamp %" PRId64 ": Failed to decode Perseus top LOQ.", timestamp);
        return LdcReturnCodeError;
    }

    // output.resizeCrop();

    if ((processedLcevcData.s_info.mode == PSS_S_MODE_OUT_OF_LOOP || m_config.getSFilterStrength() > 0.0f) &&
        (perseus_decoder_apply_s(m_coreDecoder, &coreEnhanced) != 0)) {
        VNLogError("timestamp %" PRId64 "Failed to apply sfilter out of loop.", timestamp);
        return LdcReturnCodeError;
    }

    return LdcReturnCodeSuccess;
}

//
LdpPicture* Decoder::allocPictureManaged(const LdpPictureDesc& desc)
{
    PictureManaged* ptr = new PictureManaged(m_bufferManager);
    if (!ptr->setDesc(desc)) {
        return nullptr;
    }

    return ptr;
}

//
LdpPicture* Decoder::allocPictureExternal(const LdpPictureDesc& desc, const LdpPicturePlaneDesc* planeDescArr,
                                          const LdpPictureBufferDesc* buffer)
{
    PictureExternal* ptr = new PictureExternal();
    if (!ptr->setDescExternal(desc, planeDescArr, buffer)) {
        return nullptr;
    }

    return ptr;
}

void Decoder::freePicture(LdpPicture* picture)
{
    // Convert back to the Picture base class - then delete it
    // Needed so that virtual destructor gets called.
    Picture* ptr{static_cast<Picture*>(picture)};
    delete ptr;
}

} // namespace lcevc_dec::decoder
