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

#include "asset_loader.h"

#include <find_assets_dir.h>
#include <gtest/gtest.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/extract/extract.h>
#include <LCEVC/utility/base_decoder.h>
#include <LCEVC/utility/md5.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

using namespace lcevc_dec::utility;

const std::filesystem::path kTestAssets{findAssetsDir("src/api/test/assets")};

bool operator==(const PixelBuffer& lhs, const PixelBuffer& rhs)
{
    if (lhs.width != rhs.width || lhs.height != rhs.height) {
        return false;
    }

    const uint32_t heights[3] = {lhs.height, lhs.height / 2, lhs.height / 2};
    const uint32_t widths[3] = {lhs.width, lhs.width / 2, lhs.width / 2};

    const uint8_t* lhsData = lhs.data.data();
    const uint8_t* rhsData = rhs.data.data();

    for (uint32_t plane = 0; plane < 3; ++plane) {
        for (uint32_t row = 0; row < heights[plane]; ++row) {
            if (memcmp(lhsData, rhsData, widths[plane])) {
                return false;
            }
            lhsData += lhs.strides[plane];
            rhsData += rhs.strides[plane];
        }
    }
    return true;
}

namespace {

LCEVC_PictureHandle allocPictureForPixelBufferImpl(LCEVC_DecoderHandle decoder,
                                                   const PixelBuffer& pixelBuffer,
                                                   const LCEVC_PictureDesc* inputDesc, bool setUserData)
{
    LCEVC_PictureDesc pictureDesc = {};
    if (inputDesc) {
        pictureDesc = *inputDesc;
    } else {
        EXPECT_EQ(LCEVC_Success, LCEVC_DefaultPictureDesc(&pictureDesc, LCEVC_I420_8,
                                                          pixelBuffer.width, pixelBuffer.height));
        pictureDesc.sampleAspectRatioNum = pixelBuffer.sampleAspectRatioNum;
        pictureDesc.sampleAspectRatioDen = pixelBuffer.sampleAspectRatioDen;
    }

    pictureDesc.width = pixelBuffer.width;
    pictureDesc.height = pixelBuffer.height;
    PictureLayout layout(pictureDesc, pixelBuffer.strides);
    LCEVC_PicturePlaneDesc planes[PictureLayout::kMaxNumPlanes] = {};
    auto* data = const_cast<uint8_t*>(pixelBuffer.data.data());
    for (uint8_t plane = 0; plane < layout.planes(); ++plane) {
        planes[plane].firstSample = data + layout.planeOffset(plane);
        planes[plane].rowByteStride = layout.rowStride(plane);
    }

    LCEVC_PictureHandle picture = {};
    EXPECT_EQ(LCEVC_Success, LCEVC_AllocPictureExternal(decoder, &pictureDesc, nullptr, planes, &picture));
    if (setUserData) {
        LCEVC_SetPictureUserData(decoder, picture, const_cast<PixelBuffer*>(&pixelBuffer));
    }
    return picture;
}

} // namespace

LCEVC_PictureHandle allocPictureForPixelBuffer(LCEVC_DecoderHandle decoder, const PixelBuffer& pixelBuffer,
                                               const LCEVC_PictureDesc* inputDesc)
{
    return allocPictureForPixelBufferImpl(decoder, pixelBuffer, inputDesc, false);
}

LCEVC_PictureHandle allocPictureForPixelBuffer(LCEVC_DecoderHandle decoder, PixelBuffer* pixelBuffer)
{
    return allocPictureForPixelBufferImpl(decoder, *pixelBuffer, nullptr, true);
}

std::unique_ptr<PixelBuffer> allocPixelBuffer(uint32_t width, uint32_t height)
{
    LCEVC_PictureDesc pictureDesc = {};
    EXPECT_EQ(LCEVC_Success, LCEVC_DefaultPictureDesc(&pictureDesc, LCEVC_I420_8, width, height));

    uint32_t strides[PictureLayout::kMaxNumPlanes] = {
        alignU32(width, kRowAlignment),
        alignU32(width / 2, kRowAlignment),
        alignU32(width / 2, kRowAlignment),
    };

    PictureLayout layout(pictureDesc, strides);

    auto pixelBuffer = std::make_unique<PixelBuffer>();
    pixelBuffer->width = layout.width();
    pixelBuffer->height = layout.height();
    pixelBuffer->strides[0] = layout.rowStride(0);
    pixelBuffer->strides[1] = layout.rowStride(1);
    pixelBuffer->strides[2] = layout.rowStride(2);
    pixelBuffer->data.resize(layout.size());

    return pixelBuffer;
}

std::unique_ptr<PixelBuffer> allocPixelBuffer(LCEVC_DecoderHandle, uint32_t width, uint32_t height)
{
    return allocPixelBuffer(width, height);
}

LCEVC_ReturnCode feedNalUnit(LCEVC_DecoderHandle decoder, const std::vector<uint8_t>& nalu,
                             uint64_t timestamp, int32_t format, int32_t codec)
{
    std::vector<uint8_t> enhancementData(nalu.size());
    uint32_t actualSize = 0;
    int32_t r = LCEVC_extractEnhancementFromNAL(
        nalu.data(), static_cast<uint32_t>(nalu.size()), static_cast<LCEVC_NALFormat>(format),
        static_cast<LCEVC_CodecType>(codec), enhancementData.data(),
        static_cast<uint32_t>(enhancementData.size()), &actualSize);
    if (r == 0) {
        return LCEVC_NotFound;
    }

    enhancementData.resize(actualSize);
    return LCEVC_SendDecoderEnhancementData(decoder, timestamp, enhancementData.data(),
                                            static_cast<uint32_t>(enhancementData.size()));
}

AssetLoader::AssetLoader(const std::filesystem::path& media, uint32_t limit)
{
    loadUnits(media.string(), limit);
}

AssetLoader::AssetLoader(const std::filesystem::path& media, const std::filesystem::path& hashes,
                         uint32_t limit)
{
    loadUnits(media.string(), limit);
    loadHashes(hashes.string(), limit);
}

uint64_t AssetLoader::timestamp(uint32_t index) const
{
    EXPECT_LT(index, m_timestamps.size());
    return m_timestamps[index];
}

const PixelBuffer& AssetLoader::base(uint32_t index) const { return unit(index).base; }

const std::vector<uint8_t>& AssetLoader::enhancement(uint32_t index) const
{
    return unit(index).enhancement;
}

void AssetLoader::checkDecoded(uint32_t index, const PixelBuffer& pb)
{
    EXPECT_LT(index, m_hashes.size());
    const PictureLayout layout(m_layout.format(), pb.width, pb.height);

    std::stringstream ss;
    ss << index << "," << layout.width() << "," << layout.height();
    for (uint8_t plane = 0; plane < layout.planes(); ++plane) {
        ss << "," << checksumPlane(layout, pb.data, plane);
    }

    EXPECT_EQ(ss.str(), m_hashes[index]);
}

std::string AssetLoader::checksumPlane(const PictureLayout& layout,
                                       const std::vector<uint8_t>& data, uint8_t plane)
{
    MD5 sum;

    const uint8_t* ptr = data.data() + layout.planeOffset(plane);

    for (uint32_t y = 0; y < layout.planeHeight(plane); ++y) {
        sum.update(ptr, layout.rowSize(plane));
        ptr += layout.rowStride(plane);
    }

    return sum.hexDigest();
}

void AssetLoader::loadUnits(std::string_view file, uint32_t count)
{
    std::unique_ptr<BaseDecoder> baseDecoder =
        createBaseDecoderLibAV(file, "", LCEVC_ColorFormat_Unknown, false, false);
    ASSERT_TRUE(baseDecoder);

    uint32_t completeUnits = 0;
    while (completeUnits < count) {
        if (!baseDecoder->update()) {
            break;
        }

        if (baseDecoder->hasEnhancement()) {
            BaseDecoder::Data enhancement = {};
            ASSERT_TRUE(baseDecoder->getEnhancement(enhancement));
            Unit& unit = m_units[enhancement.pts];
            const bool wasComplete = !unit.base.data.empty() && !unit.enhancement.empty();
            EXPECT_TRUE(unit.enhancement.empty());
            unit.enhancement.assign(enhancement.ptr, enhancement.ptr + enhancement.size);
            if (!wasComplete && !unit.base.data.empty()) {
                ++completeUnits;
            }

            baseDecoder->clearEnhancement();
        }

        if (baseDecoder->hasImage()) {
            BaseDecoder::Data base = {};
            ASSERT_TRUE(baseDecoder->getImage(base));
            Unit& unit = m_units[base.pts];
            const bool wasComplete = !unit.base.data.empty() && !unit.enhancement.empty();
            EXPECT_TRUE(unit.base.data.empty());
            unit.base = *allocPixelBuffer(baseDecoder->layout().width(), baseDecoder->layout().height());
            EXPECT_LE(baseDecoder->layout().planes(), 3u);

            for (uint8_t plane = 0; plane < baseDecoder->layout().planes(); ++plane) {
                unit.base.strides[plane] = baseDecoder->layout().rowStride(plane);
            }

            unit.base.data.assign(base.ptr, base.ptr + base.size);
            unit.base.sampleAspectRatioNum = baseDecoder->description().sampleAspectRatioNum;
            unit.base.sampleAspectRatioDen = baseDecoder->description().sampleAspectRatioDen;
            if (!wasComplete && !unit.enhancement.empty()) {
                ++completeUnits;
            }

            baseDecoder->clearImage();
        }
    }

    m_timestamps.reserve(m_units.size());
    for (const auto& [pts, unit] : m_units) {
        if (!unit.base.data.empty() && !unit.enhancement.empty()) {
            m_timestamps.push_back(pts);
        }
    }

    m_layout = baseDecoder->layout();
}

void AssetLoader::loadHashes(std::string_view filename, uint32_t count)
{
    std::ifstream file{std::string(filename)};
    EXPECT_TRUE(!!file);

    std::string line;
    std::getline(file, line);

    while (m_hashes.size() <= count && std::getline(file, line)) {
        m_hashes.push_back(line);
    }
}

const AssetLoader::Unit& AssetLoader::unit(uint32_t index) const
{
    EXPECT_LT(index, m_timestamps.size());
    const uint64_t pts = m_timestamps[index];
    EXPECT_EQ(m_units.count(pts), 1);
    return m_units.find(pts)->second;
}
