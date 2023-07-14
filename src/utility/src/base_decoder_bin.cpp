// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/base_decoder.h"

#include "LCEVC/utility/picture_layout.h"
#include "LCEVC/utility/byte_order.h"
#include "LCEVC/utility/raw_reader.h"
#include "bin_reader.h"

namespace lcevc_dec::utility {

// Base Decoder that uses LCWEVC BIN files alongside a base YUV file.
//
class BaseDecoderBin final : public BaseDecoder
{
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
    friend std::unique_ptr<BaseDecoder> createBaseDecoderBin(std::string_view rawFile, std::string_view binFile);
    void close();

    bool openBin(const std::string& binFile);
    bool readHeader();
    bool readBlock(int64_t& decodeIndex, int64_t& presentationIndex, std::vector<uint8_t>& payload);

    // Convenience copies of decoded video image properties
    int m_width = 0;
    int m_height = 0;

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
};

BaseDecoderBin::~BaseDecoderBin() {}

bool BaseDecoderBin::hasImage() const { return m_imageData.ptr && m_imageData.size; }

bool BaseDecoderBin::getImage(Data& data) const
{
    if (!hasImage()) {
        return false;
    }

    data = m_imageData;
    return true;
}

void BaseDecoderBin::clearImage()
{
    m_imageData = {};
}

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

void BaseDecoderBin::clearEnhancement()
{
    m_enhancementData = {};
}

bool BaseDecoderBin::update()
{
    // Don't move on until current data is cleared
    if(hasImage() || hasEnhancement())
        return true;

    // Read next enhancement block from BIN file
    int64_t decodeIndex;
    if(!m_binReader->read(decodeIndex, m_enhancementData.timestamp, m_enhancement)) {
        return false;
    }

    // Read next base frame from raw file
    if(!m_rawReader->read(m_image)) {
        return false;
    }

    // Got everything mark both parts as available
    m_enhancementData.ptr = m_enhancement.data();
    m_imageData.ptr = m_image.data();

    return true;
}

std::unique_ptr<BaseDecoder> createBaseDecoderBin(std::string_view rawFile, std::string_view binFile)
{
    auto rawReader = createRawReader(rawFile);
    if(!rawReader) {
        return nullptr;
    }

    auto binReader = createBinReader(binFile);
    if(!rawReader) {
        return nullptr;
    }

    auto decoder = std::make_unique<BaseDecoderBin>();

    decoder->m_pictureDesc = rawReader->description();
    decoder->m_pictureLayout = PictureLayout(decoder->m_pictureDesc);
    decoder->m_rawReader = std::move(rawReader);
    decoder->m_binReader = std::move(binReader);

    return decoder;
}

} // namespace lcevc_dec::utility
