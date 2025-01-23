/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
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
#include "LCEVC/utility/base_decoder.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

// Base Decoder that uses LCEVC BIN files alongside a base YUV file.
//
class BaseDecoderBinNonLinear final : public BaseDecoderBin
{
    using BaseClass = BaseDecoderBin;

public:
    BaseDecoderBinNonLinear() = default;
    BaseDecoderBinNonLinear(std::string_view rawFile, std::string_view binFile)
        : BaseClass(rawFile, binFile)
    {}
    ~BaseDecoderBinNonLinear() override;

    BaseDecoderBinNonLinear(const BaseDecoderBinNonLinear&) = delete;
    BaseDecoderBinNonLinear(BaseDecoderBinNonLinear&&) = delete;
    BaseDecoderBinNonLinear& operator=(const BaseDecoderBinNonLinear&) = delete;
    BaseDecoderBinNonLinear& operator=(BaseDecoderBinNonLinear&&) = delete;

    Type getType() const override { return Type::BinNonLinear; }

    bool hasImage() const override;
    bool getImage(Data& data) const override;
    void clearImage() override;

    bool hasEnhancement() const override;
    bool getEnhancement(Data& data) const override;
    void clearEnhancement() override;

    bool update() override;

private:
    friend std::unique_ptr<BaseDecoder> createBaseDecoderBinNonLinear(std::string_view rawFile,
                                                                      std::string_view binFile);

    // Holding buffers for output packets
    Data m_imageData;
    Data m_enhancementData;

    // Holding buffer for output images
    std::vector<uint8_t> m_image;

    // Holding buffer for enhanced data
    std::vector<uint8_t> m_enhancement;

    std::set<int64_t> m_pendingBase;
};

BaseDecoderBinNonLinear::~BaseDecoderBinNonLinear() = default;

bool BaseDecoderBinNonLinear::hasImage() const { return m_imageData.ptr && m_imageData.size; }

bool BaseDecoderBinNonLinear::getImage(Data& data) const
{
    if (!hasImage()) {
        return false;
    }

    data = m_imageData;
    return true;
}

void BaseDecoderBinNonLinear::clearImage() { m_imageData = {}; }

bool BaseDecoderBinNonLinear::hasEnhancement() const
{
    return m_enhancementData.ptr && m_enhancementData.size;
}

bool BaseDecoderBinNonLinear::getEnhancement(Data& data) const
{
    if (!hasEnhancement()) {
        return false;
    }

    data = m_enhancementData;
    return true;
}

void BaseDecoderBinNonLinear::clearEnhancement() { m_enhancementData = {}; }

bool BaseDecoderBinNonLinear::update()
{
    // Any more data?
    if (!hasEnhancement() && !hasImage() && !getBinGood() && m_pendingBase.empty()) {
        return false;
    }

    if (!hasEnhancement() && getBinGood()) {
        // Read next enhancement block from BIN file
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        if (binRead(decodeIndex, presentationIndex, m_enhancement)) {
            // Add timestamp to pending decoder set
            m_pendingBase.insert(presentationIndex);

            // Mark enhancement parts as available
            m_enhancementData.ptr = m_enhancement.data();
            m_enhancementData.size = static_cast<uint32_t>(m_enhancement.size());
            m_enhancementData.timestamp = presentationIndex;
            m_enhancementData.baseDecodeStart = std::chrono::high_resolution_clock::now();
        } else {
            setBinGood(false);
        }
    }

    if (!hasImage() && getRawGood() && !m_pendingBase.empty() &&
        *m_pendingBase.begin() == getLastBaseTimestamp()) {
        if (rawRead(m_image)) {
            m_imageData.ptr = m_image.data();
            m_imageData.size = static_cast<uint32_t>(m_image.size());
            m_imageData.timestamp = getLastBaseTimestamp();
            // Remove decoded timestamp from pending set
            m_pendingBase.erase(m_pendingBase.begin());

            incrementLastBaseTimestamp();
        } else {
            // End of RAW file
            setRawGood(false);
            return false;
        }
    }

    return true;
}

std::unique_ptr<BaseDecoder> createBaseDecoderBinNonLinear(std::string_view rawFile, std::string_view binFile)
{
    auto decoder = std::make_unique<BaseDecoderBinNonLinear>(rawFile, binFile);
    if (!decoder || !decoder->isInitialised()) {
        return nullptr;
    }
    return decoder;
}

} // namespace lcevc_dec::utility
