/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#ifndef VN_API_DECODER_H_
#define VN_API_DECODER_H_

#include "accel_context.h"
#include "buffer_manager.h"
#include "decoder_config.h"
#include "event_manager.h"
#include "handle.h"
#include "interface.h"
#include "lcevc_processor.h"
#include "picture_lock.h"
#include "pool.h"

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/PerseusDecoder.h>
#include <LCEVC/utility/chrono.h>

#include <cassert>
#include <cstdint>
#include <deque>
#include <memory>
#include <queue>
#include <utility>

// ------------------------------------------------------------------------------------------------

// Forward declarations
struct LCEVC_AccelContext;
struct LCEVC_AccelContextHandle;
struct LCEVC_DecodeInformation;
struct LCEVC_DecoderHandle;
struct LCEVC_PictureHandle;
struct LCEVC_PictureLockHandle;

namespace lcevc_dec::decoder {

class AccelContext;
class Picture;
class PictureLock;

// - BaseData -------------------------------------------------------------------------------------

struct BaseData
{
    BaseData(Handle<Picture> baseIn, uint64_t insertionTimeIn, uint32_t timeoutUsIn)
        : nonNullHandle(baseIn)
        , insertionTime(insertionTimeIn)
        , timeoutUs(timeoutUsIn)
    {}

    Handle<Picture> nonNullHandle;
    uint64_t insertionTime;
    uint32_t timeoutUs;
};

// - Result ---------------------------------------------------------------------------------------

struct Result
{
    Result(Handle<Picture> handleIn, LCEVC_ReturnCode returnCodeIn, uint16_t discontinuityCountIn,
           const DecodeInformation& decodeInfoIn)
        : pictureHandle(handleIn)
        , returnCode(returnCodeIn)
        , discontinuityCount(discontinuityCountIn)
        , decodeInfo(decodeInfoIn)
    {
        // Note that we never have an invalid handle, even failed decodes get a handle (think of it
        // like "who failed?").
        assert(pictureHandle != kInvalidHandle);
    }

    Result(Handle<Picture> handleIn, LCEVC_ReturnCode returnCodeIn, uint16_t discontinuityCountIn,
           int64_t timestamp, bool skipped)
        : Result(handleIn, returnCodeIn, discontinuityCountIn, DecodeInformation(timestamp, skipped))
    {}

    Handle<Picture> pictureHandle;
    LCEVC_ReturnCode returnCode;
    uint16_t discontinuityCount;
    DecodeInformation decodeInfo;
};

// - Decoder --------------------------------------------------------------------------------------

class Decoder
{
    using Clock = lcevc_dec::utility::ScopedTimer<lcevc_dec::utility::MicroSecond>;

public:
    Decoder(const LCEVC_AccelContextHandle& accelContext, LCEVC_DecoderHandle& apiHandle);
    ~Decoder();
    Decoder& operator=(Decoder&& other) = delete;
    Decoder& operator=(const Decoder& other) = delete;
    Decoder(Decoder&& moveSrc) = delete;
    Decoder(const Decoder& copySrc) = delete;

    // Config
    template <typename T>
    bool setConfig(const char* name, const T& val)
    {
        return m_config.set(name, val);
    }

    bool initialize();
    void release();

    // Send/receive
    LCEVC_ReturnCode feedBase(int64_t timestamp, bool discontinuity, Handle<Picture> baseHandle,
                              uint32_t timeoutUs, void* userData);
    LCEVC_ReturnCode feedEnhancementData(int64_t timestamp, bool discontinuity, const uint8_t* data,
                                         uint32_t byteSize);
    LCEVC_ReturnCode feedOutputPicture(Handle<Picture> outputHandle);

    LCEVC_ReturnCode produceOutputPicture(LCEVC_PictureHandle& outputHandle,
                                          LCEVC_DecodeInformation& decodeInfoOut);
    LCEVC_ReturnCode produceFinishedBase(LCEVC_PictureHandle& baseHandle);

    // "Trick-play"
    LCEVC_ReturnCode flush();
    LCEVC_ReturnCode peek(int64_t timestamp, uint32_t& widthOut, uint32_t& heightOut);
    LCEVC_ReturnCode skip(int64_t timestamp);
    LCEVC_ReturnCode synchronize(bool dropPending);

    // Picture-handling
    bool allocPictureManaged(const LCEVC_PictureDesc& desc, LCEVC_PictureHandle& pictureHandle);
    bool allocPictureExternal(const LCEVC_PictureDesc& desc, LCEVC_PictureHandle& pictureHandle,
                              const LCEVC_PicturePlaneDesc* planeDescArr,
                              const LCEVC_PictureBufferDesc* buffer);
    bool releasePicture(Handle<Picture> pictureHandle);
    Picture* getPicture(Handle<Picture> handle) { return m_picturePool.lookup(handle); }

    // PictureLock-handling
    bool lockPicture(Picture& picture, Access lockAccess, LCEVC_PictureLockHandle& lockHandleOut);
    bool unlockPicture(Handle<PictureLock> pictureLock);
    bool pictureLockExists(Handle<PictureLock> handle) { return m_pictureLockPool.isValid(handle); }
    PictureLock* getPictureLock(Handle<PictureLock> handle)
    {
        return m_pictureLockPool.lookup(handle);
    }

    // AccelContext-handling (currently unused)
    AccelContext* getAccelContext(Handle<AccelContext> handle)
    {
        return m_accelContextPool.lookup(handle);
    }

    // Setters and getters
    bool isInitialized() const { return m_isInitialized; }
    void setEventCallback(EventCallback callback, void* userData)
    {
        m_eventManager.setEventCallback(callback, userData);
    }

private:
    // Functions:

    // Initialization helpers
    bool initializeConfig();
    bool initializeCoreDecoder();
    void initializeEventManager() { m_eventManager.initialise(m_config.getEvents()); }
    bool initializeLcevcProcessor();

    // Release helpers
    void releaseCoreDecoder();
    void releaseEventManager();
    void releaseLcevcProcessor();

    // Decoding

    // If getNextDecodeData returns true, then the containers have had their data removed, and
    // nextBase and nextOutput will be non-null (nextProcessedLcevcData CAN still be null). The
    // shared pointers are because otherwise the data would get thrown away when removed from their
    // containers (whereas Picture* is safe because pictures are managed by the picturePool).
    bool getNextDecodeData(BaseData& nextBase,
                           std::shared_ptr<const perseus_decoder_stream>& nextProcessedLcevcData,
                           Handle<Picture>& nextOutput);

    // First bool is whether or not this configuration should passthrough, second bool is whether
    // or not it should fail.
    std::pair<bool, bool> shouldPassthroughOrFail(bool timeout, bool lcevcAvailable) const;
    void tryToQueueDecodes();
    Result& populateDecodeResult(Handle<Picture> decodeDest, const BaseData& baseData, bool lcevcAvailable,
                                 bool shouldFail, bool shouldPassthrough, bool wasTimeout);
    Result* findDecodeResult(uint64_t timehandle);
    BaseData* findBaseData(uint64_t timehandle);
    LCEVC_ReturnCode doDecode(const BaseData& baseData, const perseus_decoder_stream* processedLcevcData,
                              Handle<Picture> decodeDest, Result*& resultOut);
    LCEVC_ReturnCode decodePassthrough(const BaseData& baseData, Picture& decodeDest);
    LCEVC_ReturnCode decodeEnhance(const BaseData& baseData,
                                   const perseus_decoder_stream& processedLcevcData, Picture& decodeDest);
    static bool decodeSetupOutputPic(Picture& enhancedPic, const perseus_decoder_stream* processedLcevcData,
                                     const Picture& basePic);
    std::shared_ptr<Picture> decodeEnhanceGetBase(Picture& originalBase,
                                                  const perseus_decoder_stream& processedLcevcData);
    std::shared_ptr<Picture> decodeEnhanceGetIntermediate(const std::shared_ptr<Picture>& basePicture,
                                                          const perseus_decoder_stream& processedLcevcData);
    static bool decodeEnhanceSetupCoreImages(Picture& basePic,
                                             const std::shared_ptr<Picture>& intermediatePicture,
                                             Picture& enhancedPic, perseus_image& baseOut,
                                             perseus_image& intermediateOut, perseus_image& enhancedOut);
    LCEVC_ReturnCode decodeEnhanceCore(uint64_t timehandle, const perseus_image& coreBase,
                                       const perseus_image& coreIntermediate,
                                       const perseus_image& coreEnhanced,
                                       const perseus_decoder_stream& processedLcevcData);

    // Flush helpers
    void flushInputs();
    void flushOutputs();

    // Capacity checking
    bool isBaseQueueFull() const
    {
        return (m_baseContainer.size() >= m_lcevcProcessor.getUnprocessedCapacity());
    }
    bool isUnprocessedEnhancementQueueFull() const
    {
        return m_lcevcProcessor.isUnprocessedQueueFull();
    }
    bool isOutputQueueFull() const
    {
        return (m_pendingOutputContainer.size() >= m_lcevcProcessor.getUnprocessedCapacity());
    }

    // Misc
    void triggerEvent(Event event) { m_eventManager.triggerEvent(event); }
    template <typename PicType, typename... Args>
    bool rawAllocPicture(LCEVC_PictureHandle& handleOut, Args&&... ctorArgs);

    // Members:

    // Decoder & decoding tools (Crucially, m_bufferManager comes before m_picturePool, because it
    // must be created before, and destroyed after, m_picturePool).
    BufferManager m_bufferManager;
    perseus_decoder m_coreDecoder = nullptr;
    Clock m_clock;

    // Object managers (for objects whose handles are emitted at the API)
    Pool<AccelContext> m_accelContextPool;
    Pool<PictureLock> m_pictureLockPool;
    Pool<Picture> m_picturePool;

    // Containers:
    std::deque<BaseData> m_baseContainer; // Input
    std::queue<Handle<Picture>> m_pendingOutputContainer; // Between input and output (cap = unprocessed lcevc data cap)
    LcevcProcessor m_lcevcProcessor;   // Holds unprocessed and processed LCEVC data.
    std::deque<Result> m_resultsQueue; // Output (cap = processed lcevc data cap)
    std::queue<Handle<Picture>> m_finishedBaseContainer; // Output

    // Configuration
    DecoderConfig m_config = {};

    // Events
    EventManager m_eventManager;

    // State
    bool m_isInitialized = false;
    uint16_t m_baseDiscontinuityCount = 0;
    uint16_t m_enhancementDiscontinuityCount = 0;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_DECODER_H_
