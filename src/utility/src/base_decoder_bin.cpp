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

#include "LCEVC/utility/base_decoder.h"
#include "LCEVC/utility/bin_reader.h"
#include "LCEVC/utility/byte_order.h"
#include "LCEVC/utility/picture_layout.h"
#include "LCEVC/utility/raw_reader.h"

#if __MINGW32__ && (__cplusplus >= 202207L)
#include <fmt/core.h>
#endif

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
    ~BaseDecoderBin() override;

    BaseDecoderBin(const BaseDecoderBin&) = delete;
    BaseDecoderBin(BaseDecoderBin&&) = delete;
    BaseDecoderBin& operator=(const BaseDecoderBin&) = delete;
    BaseDecoderBin& operator=(BaseDecoderBin&&) = delete;

    const LCEVC_PictureDesc& description() const override { return m_pictureDesc; }
    const PictureLayout& layout() const override { return m_pictureLayout; }

    int maxReorder() const override { return 0; }

    bool hasImage() const override;
    bool getImage(Data& data) const override;
    void clearImage() override;

    bool hasEnhancement() const override;
    bool getEnhancement(Data& data) const override;
    void clearEnhancement() override;

    bool update() override;

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
            m_enhancementData.baseDecodeStart = std::chrono::high_resolution_clock::now();
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
#if __MINGW32__ && (__cplusplus >= 202207L)
        fmt::print("base_decoder_bin: probe found empty BIN file.\n");
#else
        printf("base_decoder_bin: probe found empty BIN file.\n");
#endif
        return false;
    }

    if (timestamps.size() != count) {
#if __MINGW32__ && (__cplusplus >= 202207L)
        fmt::print("base_decoder_bin: probe found duplicate timestamps in BIN file.\n");
#else
        printf("base_decoder_bin: probe found empty BIN file.\n");
#endif
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
#if __MINGW32__ && (__cplusplus >= 202207L)
            fmt::print("base_decoder_bin: warning: probe found missing timestamp {}.\n", ts);
#else
            printf("base_decoder_bin: warning: probe found missing timestamp %lld.\n", (int64_t)ts);
#endif
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
