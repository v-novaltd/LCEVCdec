/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#ifndef VN_API_LCEVC_PARSER_H_
#define VN_API_LCEVC_PARSER_H_

#include "interface.h"
#include "picture.h"

#include <LCEVC/PerseusDecoder.h>

#include <map>
#include <memory>

using LCEVCContainer_t = struct LCEVCContainer;

namespace lcevc_dec::decoder {

class LcevcProcessor
{
public:
    LcevcProcessor(perseus_decoder& parser, BufferManager& bufferManager);

    bool initialise(uint32_t unprocessedLcevcCap, int32_t residualSurfaceFPSetting);
    void release();

    void flush();

    // Unprocessed in, processed out (return is an LCEVC_ReturnCode)
    int32_t insertUnprocessedLcevcData(const uint8_t* data, uint32_t byteSize, uint64_t timehandle,
                                       uint64_t inputTime);
    std::shared_ptr<perseus_decoder_stream> extractProcessedLcevcData(uint64_t timehandle);

    // This tells you if EITHER container has data for this timehandle, so we know that we can
    // decode the corresponding base.
    bool contains(uint64_t timehandle) const;

    uint32_t getUnprocessedCapacity() const;
    bool isUnprocessedQueueFull() const;

private:
    bool accumulateTemporalFromSkippedFrame(const perseus_decoder_stream& processedLcevcData);
    std::shared_ptr<perseus_decoder_stream> processUpToTimehandle(uint64_t timehandle);
    std::shared_ptr<perseus_decoder_stream> processUpToTimehandleLoop(uint64_t timehandle,
                                                                      uint32_t& numExtractedOut,
                                                                      uint64_t& lastExtractedTH);

    void setLiveDecoderConfig(const perseus_global_config& globalConfig) const;

    // Core parser (so long as the Core Decoder is strictly stateful, this is simply a reference to
    // the Decoder's Core decoder)
    perseus_decoder& m_coreDecoderRef;

    // Input holder
    LCEVCContainer_t* m_unprocessedLcevcContainer = nullptr;

    // Picture with no data (accumulates temporal when skipping)
    PictureManaged m_skipTemporalAccumulator;

    // Config (set in initialise, not constructor, so can't be const)
    int32_t m_residualSurfaceFPSetting = -1; // This is "pss_surface_fp_setting" (see dil_config_json.md)
};

} // namespace lcevc_dec::decoder
#endif
