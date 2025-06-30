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

#ifndef VN_LCEVC_PIPELINE_LEGACY_DECODER_H
#define VN_LCEVC_PIPELINE_LEGACY_DECODER_H

#include <LCEVC/api_utility/chrono.h>
#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/return_code.h>
#include <LCEVC/legacy/PerseusDecoder.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/pipeline.h>
//
#include "buffer_manager.h"
#include "decoder_config.h"
#include "lcevc_processor.h"
#include "picture_lock.h"
//
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

namespace lcevc_dec::decoder {

class AccelContext;
class Picture;
class PictureLock;

// - BaseData -------------------------------------------------------------------------------------

struct BaseData
{
    BaseData(Picture* baseIn, uint64_t insertionTimeIn, uint32_t timeoutUsIn)
        : nonNullPicture(baseIn)
        , insertionTime(insertionTimeIn)
        , timeoutUs(timeoutUsIn)
    {}

    Picture* nonNullPicture;
    uint64_t insertionTime;
    uint32_t timeoutUs;
};

// - Result ---------------------------------------------------------------------------------------

struct Result
{
    Result(Picture* pictureIn, LdcReturnCode returnCodeIn, const LdpDecodeInformation& decodeInfoIn)
        : picture(pictureIn)
        , returnCode(returnCodeIn)
        , decodeInfo(decodeInfoIn)
    {
        // Note that we never have an invalid picture, even failed decodes get a handle (think of it
        // like "who failed?").
        assert(picture != nullptr);
    }

    Picture* picture;
    LdcReturnCode returnCode;
    LdpDecodeInformation decodeInfo;
};

// - Decoder --------------------------------------------------------------------------------------

class Decoder : public pipeline::Pipeline
{
    using Clock = utility::ScopedTimer<utility::MicroSecond>;

public:
    Decoder(const DecoderConfig& config, pipeline::EventSink* eventSink);
    ~Decoder();

    // Send/receive
    LdcReturnCode sendBasePicture(uint64_t timestamp, LdpPicture* baseLdpPicture,
                                  uint32_t timeoutUs, void* userData) override;
    LdcReturnCode sendEnhancementData(uint64_t timestamp, const uint8_t* data, uint32_t byteSize) override;
    LdcReturnCode sendOutputPicture(LdpPicture* outputLdpPicture) override;

    LdpPicture* receiveOutputPicture(LdpDecodeInformation& decodeInfoOut) override;
    LdpPicture* receiveFinishedBasePicture() override;

    // "Trick-play"
    LdcReturnCode flush(uint64_t timestamp) override;
    LdcReturnCode peek(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut) override;
    LdcReturnCode skip(uint64_t timestamp) override;
    LdcReturnCode synchronize(bool dropPending) override;

    LdpPicture* allocPictureManaged(const LdpPictureDesc& desc) override;
    LdpPicture* allocPictureExternal(const LdpPictureDesc& desc, const LdpPicturePlaneDesc* planeDescArr,
                                     const LdpPictureBufferDesc* buffer) override;

    void freePicture(LdpPicture* picture) override;

    // Functions:
    bool initialize();
    void release();

    VNNoCopyNoMove(Decoder);

private:
    // Initialization helpers
    bool initializeConfig();
    bool initializeCoreDecoder();
    bool initializeLcevcProcessor();

    // Release helpers
    void releaseCoreDecoder();
    void releaseLcevcProcessor();

    // Decoding

    // If getNextDecodeData returns true, then the containers have had their data removed, and
    // nextBase and nextOutput will be non-null (nextProcessedLcevcData CAN still be null). The
    // shared pointers are because otherwise the data would get thrown away when removed from their
    // containers (whereas Picture* is safe because pictures are managed by the picturePool).
    Picture* getNextDecodeData(BaseData& nextBase,
                               std::shared_ptr<const perseus_decoder_stream>& nextProcessedLcevcData);

    // First bool is whether or not this configuration should passthrough, second bool is whether
    // or not it should fail.
    std::pair<bool, bool> shouldPassthroughOrFail(bool timeout, bool lcevcAvailable) const;
    void tryToQueueDecodes();
    Result& populateDecodeResult(Picture* decodeDest, const BaseData& baseData, bool lcevcAvailable,
                                 bool shouldFail, bool shouldPassthrough, bool wasTimeout);
    Result* findDecodeResult(uint64_t timestamp);
    BaseData* findBaseData(uint64_t timestamp);
    LdcReturnCode doDecode(const BaseData& baseData, const perseus_decoder_stream* processedLcevcData,
                           Picture* decodeDest, Result*& resultOut);
    LdcReturnCode decodePassthrough(const BaseData& baseData, Picture& decodeDest);
    LdcReturnCode decodeEnhance(const BaseData& baseData,
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
    LdcReturnCode decodeEnhanceCore(uint64_t timestamp, const perseus_image& coreBase,
                                    const perseus_image& coreIntermediate, const perseus_image& coreEnhanced,
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

    // Members:

    // Decoder & decoding tools (Crucially, m_bufferManager comes before m_picturePool, because it
    // must be created before, and destroyed after, m_picturePool).
    perseus_decoder m_coreDecoder = nullptr;
    Clock m_clock;

    BufferManager m_bufferManager;

    // Containers:
    std::deque<BaseData> m_baseContainer; // Input
    std::queue<Picture*> m_pendingOutputContainer; // Between input and output (cap = unprocessed lcevc data cap)
    LcevcProcessor m_lcevcProcessor;               // Holds unprocessed and processed LCEVC data.
    std::deque<Result> m_resultsQueue;            // Output (cap = processed lcevc data cap)
    std::queue<Picture*> m_finishedBaseContainer; // Output

    // Configuration
    DecoderConfig m_config = {};

    // Events
    pipeline::EventSink* m_eventSink;
};

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_PIPELINE_LEGACY_DECODER_H
