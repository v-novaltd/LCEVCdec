/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include "base_decoder_bin.h"

#include <LCEVC/api_utility/picture_layout.h>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/utility/base_decoder.h>
#include <LCEVC/utility/bin_reader.h>
#include <LCEVC/utility/raw_reader.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

// Base Decoder that uses LCEVC BIN files alongside a base YUV file.
//     Note: in a "real" base decoder, the base and LCEVC data are provided simultaneously for each
//     timestamp, but not in order. In THIS "decoder", the base is provided in order, whereas the
//     LCEVC data is not. To handle this, we buffer both, and provide them in order. This has the
//     added bonus that you can take accurate timing measurements without counting disk-read time.
//
class BaseDecoderBinLinear final : public BaseDecoderBin
{
    using BaseClass = BaseDecoderBin;

public:
    BaseDecoderBinLinear() = default;
    BaseDecoderBinLinear(std::string_view rawFile, std::string_view binFile)
        : BaseClass(rawFile, binFile)
    {}
    ~BaseDecoderBinLinear() override;

    BaseDecoderBinLinear(const BaseDecoderBinLinear&) = delete;
    BaseDecoderBinLinear(BaseDecoderBinLinear&&) = delete;
    BaseDecoderBinLinear& operator=(const BaseDecoderBinLinear&) = delete;
    BaseDecoderBinLinear& operator=(BaseDecoderBinLinear&&) = delete;

    Type getType() const override { return Type::BinLinear; }

    bool hasImage() const override { return hasData(); }
    bool getImage(Data& data) const override;
    void clearImage() override;

    bool hasEnhancement() const override { return hasData(); }
    bool getEnhancement(Data& data) const override;
    void clearEnhancement() override;

    bool update() override;

private:
    friend std::unique_ptr<BaseDecoder> createBaseDecoderBinLinear(std::string_view rawFile,
                                                                   std::string_view binFile);

    bool hasData() const;

    // Internally buffer image and enhancement data, then provide them simultaneously.
    // ManagedData m_imageData = {nullptr};
    // ManagedData m_enhancementData = {nullptr};
    std::map<int64_t, std::vector<uint8_t>> m_imageDataList = {};
    std::map<int64_t, std::vector<uint8_t>> m_enhancementDataList = {};
};

BaseDecoderBinLinear::~BaseDecoderBinLinear() = default;

bool BaseDecoderBinLinear::hasData() const
{
    return !m_imageDataList.empty() && !m_enhancementDataList.empty() &&
           m_imageDataList.begin()->first == m_enhancementDataList.begin()->first;
}

bool BaseDecoderBinLinear::getImage(Data& data) const
{
    if (!hasImage()) {
        return false;
    }

    data = Data(*m_imageDataList.begin());
    return true;
}

// This clears BOTH base and enhancement, to ensure we always strictly provide the data in base-
// enhancement pairs
void BaseDecoderBinLinear::clearImage()
{
    m_imageDataList.erase(m_imageDataList.begin());
    m_enhancementDataList.erase(m_enhancementDataList.begin());
}

bool BaseDecoderBinLinear::getEnhancement(Data& data) const
{
    if (!hasEnhancement()) {
        return false;
    }

    data = Data(*m_enhancementDataList.begin());
    return true;
}

void BaseDecoderBinLinear::clearEnhancement()
{ // Do nothing: instead, clear BOTH when we clear image.
}

bool BaseDecoderBinLinear::update()
{
    // Any more data?
    if (!hasData() && !getBinGood() && !getRawGood()) {
        return false;
    }

    while (!hasData() && (getRawGood() || getBinGood())) {
        if (getBinGood()) {
            // Read next enhancement block from BIN file
            int64_t decodeIndex = 0;
            int64_t presentationIndex = 0;
            if (std::vector<uint8_t> buffer; binRead(decodeIndex, presentationIndex, buffer)) {
                // Insert enhancement into timestamp->buffer map
                m_enhancementDataList[presentationIndex] = buffer;
            } else {
                setBinGood(false);
            }
        }

        if (getRawGood()) {
            if (std::vector<uint8_t> buffer; rawRead(buffer)) {
                m_imageDataList[getLastBaseTimestamp()] = buffer;
                incrementLastBaseTimestamp();
            } else {
                // End of RAW file
                setRawGood(false);
                return false;
            }
        }
    }

    return true;
}

std::unique_ptr<BaseDecoder> createBaseDecoderBinLinear(std::string_view rawFile, std::string_view binFile)
{
    auto decoder = std::make_unique<BaseDecoderBinLinear>(rawFile, binFile);
    if (!decoder || !decoder->isInitialized()) {
        return nullptr;
    }
    return decoder;
}

} // namespace lcevc_dec::utility
