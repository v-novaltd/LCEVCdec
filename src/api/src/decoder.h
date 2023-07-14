/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_DECODER_H_
#define VN_API_DECODER_H_

#include "accel_context.h"
#include "buffer_manager.h"
#include "clock.h"
#include "decoder_config.h"
#include "event_manager.h"
#include "handle.h"
#include "interface.h"
#include "lcevc_processor.h"
#include "picture_lock.h"
#include "pool.h"

#include <LCEVC/PerseusDecoder.h>
#include <LCEVC/lcevc_dec.h>

#include <map>
#include <queue>

// ------------------------------------------------------------------------------------------------

// Forward declarations
struct LCEVC_AccelContext;
struct LCEVC_DecodeInformation;
struct LCEVC_DecoderHandle;
struct LCEVC_PictureHandle;
struct LCEVC_AccelContextHandle;
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
           int64_t timestamp, bool skipped)
        : pictureHandle(handleIn)
        , returnCode(returnCodeIn)
        , discontinuityCount(discontinuityCountIn)
        , decodeInfo(timestamp)
    {
        decodeInfo.skipped = skipped;

        // Note that we never have an invalid handle, even failed decodes get a handle (think of it
        // like "who failed?").
        VNAssert(pictureHandle != kInvalidHandle);

        // All skips are considered "successes":
        VNAssert(!skipped || returnCode == LCEVC_Success);
    }
    Handle<Picture> pictureHandle;
    LCEVC_ReturnCode returnCode;
    uint16_t discontinuityCount;
    DecodeInformation decodeInfo;
};

// - Decoder --------------------------------------------------------------------------------------

class Decoder
{
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
    LCEVC_ReturnCode skip(int64_t timestamp);
    LCEVC_ReturnCode synchronize(bool dropPending);

    // Picture-handling
    bool allocPictureManaged(const LCEVC_PictureDesc& desc, LCEVC_PictureHandle& pictureHandle);
    bool allocPictureExternal(const LCEVC_PictureDesc& desc, LCEVC_PictureHandle& pictureHandle,
                              uint32_t bufferCount, const LCEVC_PictureBufferDesc* bufferDescArr);
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
    bool shouldPassthrough(bool timeout, bool forcePassthrough, bool lcevcAvailable,
                           bool& shouldFailOut) const;
    void tryToQueueDecodes();
    Result& populateDecodeResult(Handle<Picture> decodeDest, const BaseData& baseData, bool lcevcAvailable,
                                 bool shouldFail, bool shouldPassthrough, bool wasTimeout);
    Result* findDecodeResult(uint64_t timehandle);
    LCEVC_ReturnCode doDecode(const BaseData& baseData, const perseus_decoder_stream* processedLcevcData,
                              Handle<Picture> decodeDest, Result*& resultOut);
    LCEVC_ReturnCode decodePassthrough(const BaseData& baseData, Picture& decodeDest);
    LCEVC_ReturnCode decodeEnhance(const BaseData& baseData,
                                   const perseus_decoder_stream& processedLcevcData, Picture& decodeDest);
    static bool decodeSetupOutputPic(Picture& enhancedPic, const perseus_decoder_stream* processedLcevcData,
                                     const Picture& basePic);
    std::shared_ptr<Picture> decodeEnhanceGetBase(Picture& originalBase,
                                                  const perseus_decoder_stream& processedLcevcData);
    static bool decodeEnhanceSetupCoreImages(Picture& basePic, Picture& enhancedPic,
                                             perseus_image& baseOut, perseus_image& enhancedOut);
    LCEVC_ReturnCode decodeEnhanceCore(uint64_t timehandle, const perseus_image& coreBase,
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
    std::queue<BaseData> m_baseContainer; // Input
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
