// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/base_decoder.h"
#include "LCEVC/utility/byte_order.h"
#include "LCEVC/utility/picture_layout.h"
#include "LCEVC/utility/raw_reader.h"
#include "bin_reader.h"

#include <fmt/core.h>

#include <iterator>
#include <set>
#include <vector>

namespace lcevc_dec::utility {

// Base Decoder that uses LCEVC BIN files alongside a base YUV file.
//
class BaseDecoderBin final : public BaseDecoder
{
    using BaseClass = BaseDecoder;

public:
    BaseDecoderBin() = default;
    ~BaseDecoderBin() final;

    BaseDecoderBin(const BaseDecoderBin&) = delete;
    BaseDecoderBin(BaseDecoderBin&&) = delete;
    BaseDecoderBin& operator=(const BaseDecoderBin&) = delete;
    BaseDecoderBin& operator=(BaseDecoderBin&&) = delete;

    const LCEVC_PictureDesc& description() const final { return m_pictureDesc; }
    const PictureLayout& layout() const final { return m_pictureLayout; }

    int maxReorder() const final { return 0; }

    bool hasImage() const final;
    bool getImage(Data& data) const final;
    void clearImage() final;

    bool hasEnhancement() const final;
    bool getEnhancement(Data& data) const final;
    void clearEnhancement() final;

    bool update() final;

private:
    friend std::unique_ptr<BaseDecoder> createBaseDecoderBin(std::string_view rawFile,
                                                             std::string_view binFile);
    void close();

    bool openBin(const std::string& binFile);
    bool readHeader();
    bool readBlock(int64_t& decodeIndex, int64_t& presentationIndex, std::vector<uint8_t>& payload);

    bool probe(std::string_view binFile);

    LCEVC_PictureDesc m_pictureDesc = {};
    PictureLayout m_pictureLayout;

    // Holding buffers for output packets
    Data m_imageData = {nullptr};
    Data m_enhancementData = {nullptr};

    // Holding buffer for output images
    std::vector<uint8_t> m_image;

    // Holding buffer for enhanced data
    std::vector<uint8_t> m_enhancement;

    // The readers for source files
    std::unique_ptr<RawReader> m_rawReader;
    std::unique_ptr<BinReader> m_binReader;

    // Probed values from presentation timestamp in BIN file
    int64_t m_timestampStart = 0;
    int64_t m_timestampStep = 0;

    // Next 'decoded' timestamp for base images
    int64_t m_timestampDecoded = 0;

    // True if the BIN file is still good
    bool m_binGood = true;
    // True if the RAW file is still good
    bool m_rawGood = true;

    std::set<int64_t> m_pendingBase;
};

BaseDecoderBin::~BaseDecoderBin() = default;

bool BaseDecoderBin::hasImage() const { return m_imageData.ptr && m_imageData.size; }

bool BaseDecoderBin::getImage(Data& data) const
{
    if (!hasImage()) {
        return false;
    }

    data = m_imageData;
    return true;
}

void BaseDecoderBin::clearImage() { m_imageData = {}; }

bool BaseDecoderBin::hasEnhancement() const
{
    return m_enhancementData.ptr && m_enhancementData.size;
}

bool BaseDecoderBin::getEnhancement(Data& data) const
{
    if (!hasEnhancement()) {
        return false;
    }

    data = m_enhancementData;
    return true;
}

void BaseDecoderBin::clearEnhancement() { m_enhancementData = {}; }

bool BaseDecoderBin::update()
{
    // Any more data?
    if (!hasEnhancement() && !hasImage() && !m_binGood && m_pendingBase.empty()) {
        return false;
    }

    if (!hasEnhancement() && m_binGood) {
        // Read next enhancement block from BIN file
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        if (m_binReader->read(decodeIndex, presentationIndex, m_enhancement)) {
            // Add timestamp to pending decoder set
            m_pendingBase.insert(presentationIndex);

            // Mark enhancement parts as available
            m_enhancementData.ptr = m_enhancement.data();
            m_enhancementData.size = static_cast<uint32_t>(m_enhancement.size());
            m_enhancementData.timestamp = presentationIndex;
        } else {
            m_binGood = false;
        }
    }

    if (!hasImage() && m_rawGood && !m_pendingBase.empty() && *m_pendingBase.begin() == m_timestampDecoded) {
        if (m_rawReader->read(m_image)) {
            m_imageData.ptr = m_image.data();
            m_imageData.size = static_cast<uint32_t>(m_image.size());
            m_imageData.timestamp = m_timestampDecoded;
            // Remove decoded timestamp from pending set
            m_pendingBase.erase(m_pendingBase.begin());

            m_timestampDecoded += m_timestampStep;
        } else {
            // End of RAW file
            m_rawGood = false;
            return false;
        }
    }

    return true;
}

// Work out starting PTS and PTS increment - by looking at up to first 100 frames
//
bool BaseDecoderBin::probe(std::string_view binFile)
{
    constexpr uint32_t kProbeFrameLimit = 100;

    auto binReader = createBinReader(binFile);
    if (!binReader) {
        return false;
    }

    std::set<int64_t> timestamps;

    uint32_t count = 0;
    for (; count < kProbeFrameLimit; ++count) {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;

        if (std::vector<uint8_t> payload; !binReader->read(decodeIndex, presentationIndex, payload)) {
            break;
        }
        timestamps.insert(presentationIndex);
    }

    if (timestamps.empty()) {
        fmt::print("base_decoder_bin: probe found empty BIN file.");
        return false;
    }

    if (timestamps.size() != count) {
        fmt::print("base_decoder_bin: probe found duplicate timestamps in BIN file.");
        return false;
    }

    m_timestampStart = *timestamps.begin();

    if (timestamps.size() == 1) {
        // If there is only one frame, guess '1' for step
        m_timestampStep = 1;
        return true;
    }

    m_timestampStep = *std::next(timestamps.begin()) - m_timestampStart;

    int64_t testTimestamp = m_timestampStart;
    for (const auto ts : timestamps) {
        if (ts != testTimestamp) {
            fmt::print("base_decoder_bin: warning: probe found missing timestamp {}.", ts);
            break;
        }
        testTimestamp += m_timestampStep;
    }

    return true;
}

std::unique_ptr<BaseDecoder> createBaseDecoderBin(std::string_view rawFile, std::string_view binFile)
{
    auto decoder = std::make_unique<BaseDecoderBin>();

    if (!decoder->probe(binFile)) {
        return nullptr;
    }

    auto binReader = createBinReader(binFile);
    if (!binReader) {
        return nullptr;
    }

    auto rawReader = createRawReader(rawFile);
    if (!rawReader) {
        return nullptr;
    }

    decoder->m_pictureDesc = rawReader->description();
    decoder->m_pictureLayout = PictureLayout(decoder->m_pictureDesc);
    decoder->m_rawReader = std::move(rawReader);
    decoder->m_binReader = std::move(binReader);

    decoder->m_timestampDecoded = decoder->m_timestampStart;
    return decoder;
}

} // namespace lcevc_dec::utility
