/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#ifndef VN_LCEVC_UTILITY_ASSET_LOADER_H
#define VN_LCEVC_UTILITY_ASSET_LOADER_H

#include <LCEVC/api_utility/picture_layout.h>
#include <LCEVC/extract/extract.h>
#include <LCEVC/lcevc_dec.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {
class BaseDecoder;
} // namespace lcevc_dec::utility

using lcevc_dec::utility::PictureLayout;

extern const std::filesystem::path kTestAssets;
constexpr uint32_t kRowAlignment = 64;
constexpr int32_t kCodecH264 = LCEVC_CodecType_H264;
constexpr int32_t kFormatAnnexB = LCEVC_NALFormat_AnnexB;
constexpr int32_t kFormatLengthPrefix = LCEVC_NALFormat_LengthPrefix;

struct PixelBuffer
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t strides[3] = {0};
    uint32_t sampleAspectRatioNum = 1;
    uint32_t sampleAspectRatioDen = 1;

    std::vector<uint8_t> data;
};

bool operator==(const PixelBuffer& lhs, const PixelBuffer& rhs);

LCEVC_PictureHandle allocPictureForPixelBuffer(LCEVC_DecoderHandle decoder, const PixelBuffer& pixelBuffer,
                                               const LCEVC_PictureDesc* inputDesc = nullptr);
LCEVC_PictureHandle allocPictureForPixelBuffer(LCEVC_DecoderHandle decoder, PixelBuffer* pixelBuffer);

std::unique_ptr<PixelBuffer> allocPixelBuffer(uint32_t width, uint32_t height);
std::unique_ptr<PixelBuffer> allocPixelBuffer(LCEVC_DecoderHandle decoder, uint32_t width, uint32_t height);

LCEVC_ReturnCode feedNalUnit(LCEVC_DecoderHandle decoder, const std::vector<uint8_t>& nalu,
                             uint64_t timestamp, int32_t format, int32_t codec);

class AssetLoader
{
    struct Unit
    {
        PixelBuffer base;
        std::vector<uint8_t> enhancement;
    };

public:
    AssetLoader(const std::filesystem::path& media, uint32_t limit = UINT32_MAX);
    AssetLoader(const std::filesystem::path& media, const std::filesystem::path& hashes,
                uint32_t limit = UINT32_MAX);

    size_t size() const { return m_timestamps.size(); }

    uint64_t timestamp(uint32_t index) const;
    const PixelBuffer& base(uint32_t index) const;
    const std::vector<uint8_t>& enhancement(uint32_t index) const;

    void checkDecoded(uint32_t index, const PixelBuffer& pb);
    static std::string checksumPlane(const PictureLayout& layout, const std::vector<uint8_t>& data,
                                     uint8_t plane);

private:
    void loadUnits(std::string_view file, uint32_t count);
    void loadHashes(std::string_view filename, uint32_t count);
    const Unit& unit(uint32_t index) const;

    std::filesystem::path m_source;
    PictureLayout m_layout;
    std::map<uint64_t, Unit> m_units;
    std::vector<uint64_t> m_timestamps;
    std::vector<std::string> m_hashes;
};

#endif // VN_LCEVC_UTILITY_ASSET_LOADER_H
