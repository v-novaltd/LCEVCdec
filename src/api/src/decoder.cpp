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

#include "decoder.h"

#include "accel_context.h"
#include "enums.h"
#include "event_manager.h"
#include "handle.h"
#include "interface.h"
#include "lcevc_config.h"
#include "log.h"
#include "picture.h"
#include "picture_lock.h"
#include "pool.h"
#include "timestamps.h"

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/PerseusDecoder.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

static const LogComponent kComp = LogComponent::Decoder;

// Assume that we will need not-very-many accel contexts. We may need a surprisingly large amount
// of pictures though (enough to max out the unprocessed, temporary/pending, and processed queues).
static const size_t kAccelContextPoolCapacity = 16;
static const size_t kPicturePoolCapacity = 1024;
static const size_t kPictureLockPoolCapacity = kPicturePoolCapacity;

// - Decoder --------------------------------------------------------------------------------------

Decoder::Decoder(const LCEVC_AccelContextHandle& accelContext, LCEVC_DecoderHandle& apiHandle)
    : m_accelContextPool(kAccelContextPoolCapacity)
    , m_pictureLockPool(kPictureLockPoolCapacity)
    , m_picturePool(kPicturePoolCapacity)
    , m_lcevcProcessor(m_coreDecoder, m_bufferManager)
    , m_eventManager(apiHandle)
{
    VN_UNUSED(accelContext);
}

Decoder::~Decoder()
{
    if (m_isInitialized) {
        release();
    }
}

bool Decoder::initialize()
{
    // Initialisation order:
    // 1) The config, so the rest of the initialization can be logged if needed.
    // 2) The event manager, just in case the subsequent steps have to send events (although that
    //    should really wait until the end of initialization)
    // 3) Everything else, in no particular order.
    if (!initializeConfig()) {
        VNLogError("Failed to initialize Config. Decoder: %p.\n", this);
        return false;
    }

    initializeEventManager(); // No failure case

    if (!initializeCoreDecoder()) {
        VNLogError("Failed to initialize Core Decoder. Decoder: %p.\n", this);
        return false;
    }
    if (!initializeLcevcProcessor()) {
        VNLogError("Failed to initialize LCEVC Processor. Decoder: %p.\n", this);
        return false;
    }

    // Initialization done. Note that we trigger "can send enhancement" first, in case the client
    // is blindly sending data every time they get a "can send", without checking that they've sent
    // the enhancement before the base.
    m_isInitialized = true;
    triggerEvent(LCEVC_CanSendEnhancement);
    triggerEvent(LCEVC_CanSendBase);
    triggerEvent(LCEVC_CanSendPicture);

    return true;
}

bool Decoder::initializeConfig()
{
    // Logs first, then the rest of the config.
    m_config.initialiseLogs();
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
    releaseEventManager();

    m_isInitialized = false;
}

void Decoder::releaseCoreDecoder()
{
    perseus_decoder_close(m_coreDecoder);
    m_coreDecoder = nullptr;
}

void Decoder::releaseLcevcProcessor() { m_lcevcProcessor.release(); }

LCEVC_ReturnCode Decoder::feedBase(int64_t timestamp, bool discontinuity,
                                   Handle<Picture> baseHandle, uint32_t timeoutUs, void* userData)
{
    if (discontinuity) {
        m_baseDiscontinuityCount++;
    }

    if (isBaseQueueFull()) {
        VNLogInfo("Base container is full. Size is %zu but capacity is %u\n.",
                  m_baseContainer.size(), m_lcevcProcessor.getUnprocessedCapacity());
        return LCEVC_Again;
    }

    Picture* basePic = getPicture(baseHandle);
    if (basePic == nullptr) {
        return LCEVC_Error;
    }

    // set identifying data
    const uint64_t timehandle = getTimehandle(m_baseDiscontinuityCount, timestamp);
    basePic->setTimehandle(timehandle);
    basePic->setUserData(userData);

    m_baseContainer.emplace_back(baseHandle, m_clock.getElapsedTime(), timeoutUs);

    tryToQueueDecodes();

    return LCEVC_Success;
}

LCEVC_ReturnCode Decoder::feedEnhancementData(int64_t timestamp, bool discontinuity,
                                              const uint8_t* data, uint32_t byteSize)
{
    if (discontinuity) {
        m_enhancementDiscontinuityCount++;
    }

    if (isUnprocessedEnhancementQueueFull()) {
        VNLogInfo("Unprocessed enhancement container is full. Unprocessed container "
                  "capacity is %u.\n.",
                  m_lcevcProcessor.getUnprocessedCapacity());
        return LCEVC_Again;
    }

    const uint64_t timehandle = getTimehandle(m_enhancementDiscontinuityCount, timestamp);
    const uint64_t inputTime = m_clock.getElapsedTime();
    auto insertRes = static_cast<LCEVC_ReturnCode>(
        m_lcevcProcessor.insertUnprocessedLcevcData(data, byteSize, timehandle, inputTime));
    if (insertRes != LCEVC_Success) {
        return insertRes;
    }

    tryToQueueDecodes();

    return LCEVC_Success;
}

LCEVC_ReturnCode Decoder::feedOutputPicture(Handle<Picture> outputHandle)
{
    if (m_pendingOutputContainer.size() >= m_lcevcProcessor.getUnprocessedCapacity()) {
        VNLogDebug("Pending outputs container is full. Size is %zu but capacity is %u\n.",
                   m_pendingOutputContainer.size(), m_lcevcProcessor.getUnprocessedCapacity());
        return LCEVC_Again;
    }

    Picture* outputPic = getPicture(outputHandle);
    if (outputPic == nullptr) {
        return LCEVC_Error;
    }
    m_pendingOutputContainer.emplace(outputHandle);

    tryToQueueDecodes();

    return LCEVC_Success;
}

LCEVC_ReturnCode Decoder::produceOutputPicture(LCEVC_PictureHandle& outputHandle,
                                               LCEVC_DecodeInformation& decodeInfoOut)
{
    if (m_resultsQueue.empty()) {
        return LCEVC_Again;
    }

    // Get result (as a copy, so we can pop immediately)
    Result nextResult = m_resultsQueue.front();
    m_resultsQueue.pop_front();
    tryToQueueDecodes(); // Queue more decodes, now there's a free spot at the end of the assembly line.

    // Set output params
    memcpy(&decodeInfoOut, &(nextResult.decodeInfo), sizeof(decodeInfoOut));
    outputHandle.hdl = nextResult.pictureHandle.handle;

    triggerEvent(Event(static_cast<int32_t>(LCEVC_OutputPictureDone),
                       Handle<Picture>(outputHandle.hdl), nextResult.decodeInfo));

    return nextResult.returnCode;
}

LCEVC_ReturnCode Decoder::flush()
{
    // This throws away all bases, enhancements, and NOT-YET-DECODED output pictures. RESULTS, on
    // the other hand, are preserved so that we can return the picture handle and return code.
    flushInputs();
    flushOutputs();

    return LCEVC_Success;
}

void Decoder::flushInputs()
{
    // Enhancements
    const bool enhancementsFull = isUnprocessedEnhancementQueueFull();
    m_lcevcProcessor.flush();
    if (enhancementsFull && !isUnprocessedEnhancementQueueFull()) {
        triggerEvent(LCEVC_CanSendEnhancement);
    }

    // Bases
    const bool basesFull = isBaseQueueFull();
    while (!m_baseContainer.empty()) {
        Handle<Picture> finishedBase = m_baseContainer.front().nonNullHandle;
        m_baseContainer.pop_front();
        triggerEvent(Event(LCEVC_BasePictureDone, finishedBase));
    }
    if (basesFull && !isBaseQueueFull()) {
        triggerEvent(LCEVC_CanSendBase);
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
        triggerEvent(LCEVC_CanSendPicture);
    }

    // Decoded outputs: don't flush the Results. Instead, release the Picture's stored data (memory
    // is precious) and set the result to "Flushed".
    for (auto& result : m_resultsQueue) {
        Picture* output = getPicture(result.pictureHandle);
        output->unbindMemory();
        result.returnCode = LCEVC_Flushed;
    }
}

LCEVC_ReturnCode Decoder::peek(int64_t timestamp, uint32_t& widthOut, uint32_t& heightOut)
{
    const uint64_t baseThToFind = getTimehandle(m_baseDiscontinuityCount, timestamp);

    // Rarely, we get the easy case, where the client has already sent base, enhancement, and
    // destination pictures, so we have a finished decode ready to go (in this case, we know that
    // base and lcevc will have the same discontinuity count).
    if (const Result* res = findDecodeResult(baseThToFind); res != nullptr) {
        const Picture& pic = *(getPicture(res->pictureHandle));
        widthOut = pic.getWidth();
        heightOut = pic.getHeight();
        return res->returnCode;
    }

    // If the client has NOT sent destination pictures (for example, if they're using peek to
    // decide what size their pictures should be), then we have to work out for ourselves what the
    // output might look like.

    // Get data
    const uint64_t enhancementThToFind = getTimehandle(m_enhancementDiscontinuityCount, timestamp);
    const std::shared_ptr<perseus_decoder_stream> lcevcData =
        m_lcevcProcessor.extractProcessedLcevcData(enhancementThToFind, false);
    const BaseData* baseData = findBaseData(baseThToFind);

    // Always need base OR lcevc
    if (baseData == nullptr && lcevcData == nullptr) {
        return LCEVC_NotFound;
    }

    // In never-passthrough, we need the enhancement.
    if (m_config.getPassthroughMode() == PassthroughPolicy::Disable && lcevcData == nullptr) {
        return LCEVC_NotFound;
    }

    // If we don't have a base, then either fail, or rely entirely on lcevc.
    if (baseData == nullptr) {
        if (m_config.getPassthroughMode() == PassthroughPolicy::Force) {
            return LCEVC_NotFound;
        }
        widthOut = lcevcData->global_config.width;
        heightOut = lcevcData->global_config.height;
        return LCEVC_Success;
    }

    // Finally, if we DO have the base, we can simply use the same check used by the actual decode.
    const Picture& basePic = *(getPicture(baseData->nonNullHandle));
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

    const LCEVC_ReturnCode returnCode =
        (timeout ? LCEVC_Timeout : (shouldFail ? LCEVC_Error : LCEVC_Success));
    return returnCode;
}

LCEVC_ReturnCode Decoder::skip(int64_t timestamp)
{
    // We need to decide which discontinuity level we're skipping (for example, if we have data
    // from two rungs of the ABR ladder for this timestamp). Usually they'll be the same, but if
    // not, emit an info log and use the max (so we skip from this rung AND all prior rungs).
    // TODO: Taking the max will be wrong when the discontinuity count exceeds UINT16_MAX and loops
    //       back to 0.
    uint16_t discontinuityCount = 0;
    if (discontinuityCount != m_enhancementDiscontinuityCount) {
        discontinuityCount = std::max(m_baseDiscontinuityCount, m_enhancementDiscontinuityCount);
        VNLogInfo("Base discontinuity count (%u) differs from enhancement discontinuity count "
                  "(%u). This may mean that we skip frames from the wrong rung of the ABR "
                  "ladder. Using %u as our discontinuity count, to skip from ALL known rungs.\n",
                  m_baseDiscontinuityCount, m_enhancementDiscontinuityCount, discontinuityCount);
    }

    const uint64_t timehandle = getTimehandle(discontinuityCount, timestamp);

    // Erase bases (up to and including this one)
    const bool basesFull = isBaseQueueFull();
    const uint64_t baseTHToSkip = getTimehandle(m_baseDiscontinuityCount, timestamp);
    Handle<Picture> base =
        (m_baseContainer.empty() ? kInvalidHandle : m_baseContainer.front().nonNullHandle);
    while (base != kInvalidHandle) {
        const Picture& curBase = *getPicture(base);
        if (curBase.getTimehandle() > baseTHToSkip) {
            break;
        }
        m_baseContainer.pop_front();

        triggerEvent(Event(LCEVC_BasePictureDone, base));
        m_finishedBaseContainer.push(base);

        base = (m_baseContainer.empty() ? kInvalidHandle : m_baseContainer.front().nonNullHandle);
    }
    if (basesFull && !isBaseQueueFull()) {
        triggerEvent(LCEVC_CanSendBase);
    }

    // Process-and-erase enhancements (up to and including this one).
    const bool enhancementsFull = isUnprocessedEnhancementQueueFull();
    m_lcevcProcessor.extractProcessedLcevcData(timehandle);
    if (enhancementsFull && !isUnprocessedEnhancementQueueFull()) {
        triggerEvent(LCEVC_CanSendEnhancement);
    }

    // If we have any decode results for this or earlier timestamps, set them to skipped. Note: we
    // only need to do this for decodes that have already produced a decode result. For other
    // timestamps, it's like they simply never happened.
    for (auto& thAndInfo : m_resultsQueue) {
        if (thAndInfo.decodeInfo.timestamp <= timestamp) {
            thAndInfo.decodeInfo.skipped = true;
        }
    }

    return LCEVC_Success;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
LCEVC_ReturnCode Decoder::synchronize(bool dropPending)
{
    // For now, this is (rightly) empty, i.e. we're already always synchronized. Once we implement
    // AccelContext, this function will do something like:
    //
    // AccelContext* context = m_accelContextPool.lookup(m_accelContextHandle);
    // context->synchronize(dropPending);
    VN_UNUSED(dropPending);
    return LCEVC_Success;
}

LCEVC_ReturnCode Decoder::produceFinishedBase(LCEVC_PictureHandle& baseHandle)
{
    if (m_finishedBaseContainer.empty()) {
        return LCEVC_Again;
    }
    baseHandle.hdl = m_finishedBaseContainer.front().handle;
    m_finishedBaseContainer.pop();
    return LCEVC_Success;
}

bool Decoder::getNextDecodeData(BaseData& nextBase,
                                std::shared_ptr<const perseus_decoder_stream>& nextProcessedLcevcData,
                                Handle<Picture>& nextOutput)
{
    if (m_resultsQueue.size() >= m_config.getResultsQueueCap()) {
        VNLogDebug("Results container is full. Size is %zu but capacity is %u. Client "
                   "should try calling LCEVC_ReceiveDecoderPicture.\n",
                   m_resultsQueue.size(), m_config.getResultsQueueCap());
        return false;
    }

    // Have a valid base.
    if (m_baseContainer.empty()) {
        return false;
    }
    Handle<Picture> nonNullBaseHandle = m_baseContainer.front().nonNullHandle;
    Picture& base = *(getPicture(nonNullBaseHandle));
    const uint64_t timehandle = base.getTimehandle();
    if (timehandle == kInvalidTimehandle) {
        return false;
    }

    // Have a valid output
    if (m_pendingOutputContainer.empty()) {
        return false;
    }

    // Don't need to check for valid lcevc data: lcevc data is expected to be sent first, so if we
    // don't have it now, we won't ever. From here on, function is guaranteed to succeed.

    // Check transition from full to non-full.
    const bool basesFull = isBaseQueueFull();
    const bool enhancementsFull = isUnprocessedEnhancementQueueFull();
    const bool pendingOutputsFull = isOutputQueueFull();

    // Steal the data from our containers.
    nextOutput = m_pendingOutputContainer.front();
    nextProcessedLcevcData = m_lcevcProcessor.extractProcessedLcevcData(timehandle);
    nextBase = m_baseContainer.front();
    m_baseContainer.pop_front();
    m_pendingOutputContainer.pop();

    // Trigger non-full events
    if (basesFull && !isBaseQueueFull()) {
        triggerEvent(LCEVC_CanSendBase);
    }
    if (enhancementsFull && !isUnprocessedEnhancementQueueFull()) {
        triggerEvent(LCEVC_CanSendEnhancement);
    }
    if (pendingOutputsFull && !isOutputQueueFull()) {
        triggerEvent(LCEVC_CanSendPicture);
    }

    return true;
}

bool Decoder::allocPictureManaged(const LCEVC_PictureDesc& desc, LCEVC_PictureHandle& pictureHandle)
{
    if (!rawAllocPicture<PictureManaged>(pictureHandle, m_bufferManager)) {
        return false;
    }

    auto* freshPicture = static_cast<PictureManaged*>(getPicture(pictureHandle.hdl));
    return freshPicture->setDesc(desc);
}

bool Decoder::allocPictureExternal(const LCEVC_PictureDesc& desc, LCEVC_PictureHandle& pictureHandle,
                                   const LCEVC_PicturePlaneDesc* planeDescArr,
                                   const LCEVC_PictureBufferDesc* buffer)
{
    if (!rawAllocPicture<PictureExternal>(pictureHandle)) {
        return false;
    }

    auto* freshPicture = static_cast<PictureExternal*>(getPicture(pictureHandle.hdl));
    return freshPicture->setDescExternal(desc, planeDescArr, buffer);
}

template <typename PicType, typename... Args>
bool Decoder::rawAllocPicture(LCEVC_PictureHandle& handleOut, Args&&... ctorArgs)
{
    const bool isManaged = std::is_same<PictureManaged, PicType>::value;
    std::unique_ptr<PicType> newPic = std::make_unique<PicType>(ctorArgs...);
    if (newPic == nullptr) {
        VNLogError("Unable to create a %s Picture!\n", (isManaged ? "Managed" : "External"));
        return false;
    }

    handleOut.hdl = m_picturePool.allocate(std::move(newPic)).handle;

    if (handleOut.hdl == kInvalidHandle) {
        VNLogError("Unable to allocate a handle for a %s Picture!\n", (isManaged ? "Managed" : "External"));
        // Didn't allocate, so don't need to release.
        return false;
    }
    return true;
}

bool Decoder::releasePicture(Handle<Picture> handle)
{
    if (!m_picturePool.isValid(handle)) {
        VNLogError("Trying to release a picture that was never allocated\n");
        return false;
    }
    m_picturePool.release(handle);
    return true;
}

bool Decoder::lockPicture(Picture& picture, Access lockAccess, LCEVC_PictureLockHandle& lockHandleOut)
{
    if (picture.getLock() != kInvalidHandle) {
        VNLogError("CC %u PTS %" PRId64 ": Already have a lock for Picture <%s>.\n",
                   timehandleGetCC(picture.getTimehandle()),
                   timehandleGetTimestamp(picture.getTimehandle()), picture.getShortDbgString().c_str());
        return false;
    }
    std::unique_ptr<PictureLock> newPicLock = std::make_unique<PictureLock>(picture, lockAccess);
    Handle<PictureLock> picLockHandle = m_pictureLockPool.allocate(std::move(newPicLock));
    lockHandleOut.hdl = picLockHandle.handle;
    if (!picture.lock(lockAccess, picLockHandle)) {
        m_pictureLockPool.release(picLockHandle);
        lockHandleOut.hdl = kInvalidHandle;
        return false;
    }
    return true;
}

bool Decoder::unlockPicture(Handle<PictureLock> pictureLock)
{
    if (!m_pictureLockPool.isValid(pictureLock)) {
        VNLogError("Unrecognised picture lock handle %llu\n", pictureLock.handle);
        return false;
    }
    // Unlocking is done in the lock's destructor.
    m_pictureLockPool.release(pictureLock);
    return true;
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

Result& Decoder::populateDecodeResult(Handle<Picture> decodeDest, const BaseData& baseData,
                                      bool lcevcAvailable, bool shouldFail, bool shouldPassthrough,
                                      bool wasTimeout)
{
    const Picture& base = *(getPicture(baseData.nonNullHandle));

    const LCEVC_ReturnCode returnCode =
        (shouldFail ? LCEVC_Error : (wasTimeout ? LCEVC_Timeout : LCEVC_Success));
    const uint16_t discontinuityCount = timehandleGetCC(base.getTimehandle());
    auto& result = m_resultsQueue.emplace_back(
        decodeDest, returnCode, discontinuityCount,
        DecodeInformation(base, lcevcAvailable, shouldPassthrough, shouldFail));

    return result;
}

Result* Decoder::findDecodeResult(uint64_t timehandle)
{
    auto existingResult =
        std::find_if(m_resultsQueue.begin(), m_resultsQueue.end(), [timehandle](const Result& res) {
            return (res.decodeInfo.timestamp == timehandleGetTimestamp(timehandle)) &&
                   (res.discontinuityCount == timehandleGetCC(timehandle));
        });
    return (existingResult == m_resultsQueue.end() ? nullptr : &(*existingResult));
}

BaseData* Decoder::findBaseData(uint64_t timehandle)
{
    if (m_baseContainer.empty()) {
        return nullptr;
    }

    const auto findBase = [this, timehandle](const BaseData& baseData) {
        const Picture& pic = *(getPicture(baseData.nonNullHandle));
        return (pic.getTimehandle() == timehandle);
    };
    auto iterToBase = std::find_if(m_baseContainer.begin(), m_baseContainer.end(), findBase);
    return (iterToBase == m_baseContainer.end() ? nullptr : &(*iterToBase));
}

void Decoder::tryToQueueDecodes()
{
    BaseData nextBase(kInvalidHandle, 0, 0);
    std::shared_ptr<const perseus_decoder_stream> nextProcessedLcevcData = nullptr;
    Handle<Picture> decodeDest = kInvalidHandle;
    Result* newResultLoc = nullptr;
    while (getNextDecodeData(nextBase, nextProcessedLcevcData, decodeDest)) {
        const LCEVC_ReturnCode res =
            doDecode(nextBase, nextProcessedLcevcData.get(), decodeDest, newResultLoc);
        newResultLoc->returnCode = res;

        // Trigger "canReceive" even if we failed, because in any case, we know it's done
        triggerEvent(LCEVC_CanReceive);

        triggerEvent(Event(LCEVC_BasePictureDone, nextBase.nonNullHandle));
        m_finishedBaseContainer.push(nextBase.nonNullHandle);
    }
}

LCEVC_ReturnCode Decoder::doDecode(const BaseData& baseData, const perseus_decoder_stream* processedLcevcData,
                                   Handle<Picture> decodeDest, Result*& resultOut)
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
    Picture& base = *(getPicture(baseData.nonNullHandle));
    const uint64_t timehandle = base.getTimehandle();
    if (shouldFail) {
        VNLogError("CC %u, PTS %" PRId64
                   ": We were%s able to find lcevc data, failing decode. Passthrough mode is %d\n",
                   timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle),
                   (lcevcAvailable ? "" : " NOT"), m_config.getPassthroughMode());
        return (timeout ? LCEVC_Timeout : LCEVC_Error);
    }

    // Not failing, i.e. either passthrough or enhance, so set up the destination pic.
    Picture& decodeDestPic = *getPicture(decodeDest);
    if (!decodeSetupOutputPic(decodeDestPic, shouldPassthrough ? nullptr : processedLcevcData, base)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to setup output pic. Perhaps invalid "
                   "formats, or unmodifiable destination?\n",
                   timehandleGetCC(timehandle), timehandleGetTimestamp(timehandle));
        return LCEVC_Error;
    }

    // Now, passthrough or enhance.
    if (shouldPassthrough) {
        if (!timeout && !(m_config.getPassthroughMode() == PassthroughPolicy::Force)) {
            VNLogInfo("CC %u, PTS %" PRId64 ": Doing passthrough, due to lack of lcevc data.\n",
                      timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle));
        }
        return decodePassthrough(baseData, decodeDestPic);
    }
    return decodeEnhance(baseData, *processedLcevcData, decodeDestPic);
}

LCEVC_ReturnCode Decoder::decodePassthrough(const BaseData& baseData, Picture& decodeDest)
{
    const Picture& base = *(getPicture(baseData.nonNullHandle));
    return (decodeDest.copyData(base) ? LCEVC_Success : LCEVC_Error);
}

LCEVC_ReturnCode Decoder::decodeEnhance(const BaseData& baseData,
                                        const perseus_decoder_stream& processedLcevcData, Picture& decodeDest)
{
    // Get a base (either a non-deleting pointer to the base, or a deleting pointer to a copy).
    Picture& base = *(getPicture(baseData.nonNullHandle));
    std::shared_ptr<Picture> baseToUse = decodeEnhanceGetBase(base, processedLcevcData);
    std::shared_ptr<Picture> intermediatePicture =
        decodeEnhanceGetIntermediate(baseToUse, processedLcevcData);
    const uint64_t timehandle = baseToUse->getTimehandle();

    // Set up the images used by the Core decoder.
    perseus_image coreBase = {};
    perseus_image coreIntermediate = {};
    perseus_image coreEnhanced = {};

    if (!decodeEnhanceSetupCoreImages(*baseToUse, intermediatePicture, decodeDest, coreBase,
                                      coreIntermediate, coreEnhanced)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to set up core images.\n",
                   timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle));
        return LCEVC_Error;
    }

    // Do the actual decode.
    return decodeEnhanceCore(timehandle, coreBase, coreIntermediate, coreEnhanced, processedLcevcData);
}

bool Decoder::decodeSetupOutputPic(Picture& enhancedPic, const perseus_decoder_stream* processedLcevcData,
                                   const Picture& basePic)
{
    enhancedPic.setTimehandle(basePic.getTimehandle());

    if (processedLcevcData == nullptr) {
        return enhancedPic.copyMetadata(basePic);
    }

    // Start with the existing desc, then update with info from processedLcevcData
    LCEVC_PictureDesc modifiedDesc;
    enhancedPic.getDesc(modifiedDesc);
    if (!coreFormatToLCEVCPictureDesc(*processedLcevcData, modifiedDesc)) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Could not deduce a valid LCEVC_PictureFormat from this frame's LCEVC data.\n",
                   timehandleGetTimestamp(enhancedPic.getTimehandle()),
                   timehandleGetCC(enhancedPic.getTimehandle()));
        return false;
    }

    return enhancedPic.setDesc(modifiedDesc);
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
        return std::shared_ptr<Picture>(&originalBase, [](auto p) {});
    }

    std::shared_ptr<Picture> newPic = std::make_shared<PictureManaged>(m_bufferManager);
    newPic->copyData(originalBase);
    return newPic;
}

std::shared_ptr<Picture> Decoder::decodeEnhanceGetIntermediate(std::shared_ptr<Picture> basePicture,
                                                               const perseus_decoder_stream& processedLcevcData)
{
    perseus_scaling_mode level1Scale = processedLcevcData.global_config.scaling_modes[PSS_LOQ_1];
    if (level1Scale != PSS_SCALE_0D) {
        LCEVC_PictureDesc intermediateDesc = {};
        basePicture->getDesc(intermediateDesc);
        intermediateDesc.height =
            (level1Scale == PSS_SCALE_2D) ? intermediateDesc.height * 2 : intermediateDesc.height;
        intermediateDesc.width *= 2;
        std::shared_ptr<Picture> newPic = std::make_shared<PictureManaged>(m_bufferManager);
        newPic->setDesc(intermediateDesc);
        return newPic;
    }
    return nullptr;
}

bool Decoder::decodeEnhanceSetupCoreImages(Picture& basePic, std::shared_ptr<Picture> intermediatePicture,
                                           Picture& enhancedPic, perseus_image& baseOut,
                                           perseus_image& intermediateOut, perseus_image& enhancedOut)
{
    if (!basePic.toCoreImage(baseOut)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to get core image from base picture\n",
                   timehandleGetCC(basePic.getTimehandle()),
                   timehandleGetTimestamp(basePic.getTimehandle()));
        return false;
    }

    if (intermediatePicture) {
        if (!intermediatePicture->toCoreImage(intermediateOut)) {
            VNLogError("CC %u, PTS %" PRId64
                       ": Failed to get core image from intermediate picture\n",
                       timehandleGetCC(intermediatePicture->getTimehandle()),
                       timehandleGetTimestamp(intermediatePicture->getTimehandle()));
            return false;
        }
    }

    if (!enhancedPic.toCoreImage(enhancedOut)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to get core image from enhanced picture\n",
                   timehandleGetCC(basePic.getTimehandle()),
                   timehandleGetTimestamp(basePic.getTimehandle()));
        return false;
    }

    if (baseOut.ilv != enhancedOut.ilv) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Base interleaving (%d) must match output interleaving (%d).\n",
                   timehandleGetCC(basePic.getTimehandle()),
                   timehandleGetTimestamp(basePic.getTimehandle()), baseOut.ilv, enhancedOut.ilv);
        return false;
    }

    return true;
}

LCEVC_ReturnCode Decoder::decodeEnhanceCore(uint64_t timehandle, const perseus_image& coreBase,
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
        VNLogError("CC %u, PTS %" PRId64 ": Failed to decode Perseus base LOQ.\n",
                   timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle));
        return LCEVC_Error;
    }

    if (perseus_decoder_upscale(m_coreDecoder, &coreEnhanced, coreBaseInternal, PSS_LOQ_1) != 0) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to upscale Perseus.\n",
                   timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle));
        return LCEVC_Error;
    }

    // In-loop sharpening not supported by encoder, currently unreachable
    if ((processedLcevcData.s_info.mode == PSS_S_MODE_IN_LOOP) &&
        (perseus_decoder_apply_s(m_coreDecoder, &coreEnhanced) != 0)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to apply sfilter in loop.\n",
                   timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle));
        return LCEVC_Error;
    }

    // Decode high
    if (perseus_decoder_decode_high(m_coreDecoder, &coreEnhanced) != 0) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to decode Perseus top LOQ.\n",
                   timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle));
        return LCEVC_Error;
    }

    // output.resizeCrop();

    if ((processedLcevcData.s_info.mode == PSS_S_MODE_OUT_OF_LOOP || m_config.getSFilterStrength() > 0.0f) &&
        (perseus_decoder_apply_s(m_coreDecoder, &coreEnhanced) != 0)) {
        VNLogError("CC %u, PTS %" PRId64 "Failed to apply sfilter out of loop.\n",
                   timehandleGetTimestamp(timehandle), timehandleGetCC(timehandle));
        return LCEVC_Error;
    }

    return LCEVC_Success;
}

void Decoder::releaseEventManager()
{
    triggerEvent(LCEVC_Exit);
    m_eventManager.release();
}

} // namespace lcevc_dec::decoder
