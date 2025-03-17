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

// A simple interface for base decoders used by samples and test harnesses.
//
#ifndef VN_LCEVC_UTILITY_BASE_DECODER_H
#define VN_LCEVC_UTILITY_BASE_DECODER_H

#include "LCEVC/lcevc_dec.h"

#include <LCEVC/api_utility/picture_layout.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace lcevc_dec::utility {

/*! Common interface for base decoders.
 */
class BaseDecoder
{
protected:
    BaseDecoder() = default;

public:
    BaseDecoder(const BaseDecoder&) = delete;
    BaseDecoder(BaseDecoder&&) = delete;
    BaseDecoder& operator=(const BaseDecoder&) = delete;
    BaseDecoder& operator=(BaseDecoder&&) = delete;

    virtual ~BaseDecoder() = 0;

    // Information about video
    virtual const LCEVC_PictureDesc& description() const = 0;
    virtual const PictureLayout& layout() const = 0;

    virtual int maxReorder() const = 0;

    // Simple timestamp-data pair
    using StampedBuffer = std::pair<const int64_t, std::vector<uint8_t>>;

    // Block of data from base decoder
    struct Data
    {
        Data() = default;
        Data(const Data& other) = default;
        explicit Data(const StampedBuffer& buffer)
            : ptr(buffer.second.data())
            , size(static_cast<uint32_t>(buffer.second.size()))
            , timestamp(buffer.first)
        {}

        const uint8_t* ptr = nullptr;
        uint32_t size = 0;
        int64_t timestamp = -1;
        std::chrono::high_resolution_clock::time_point baseDecodeStart;

        bool empty() const { return ptr == nullptr; }
        void clear()
        {
            ptr = nullptr;
            size = 0;
            timestamp = -1;
            baseDecodeStart = std::chrono::time_point<std::chrono::high_resolution_clock>();
        }
    };

    enum class Type
    {
        LibAV,
        BinNonLinear,
        BinLinear,

        Count
    };

    // Virtual call to determine class type
    virtual Type getType() const = 0;

    // Return true if there is a decoded image ready to get
    virtual bool hasImage() const = 0;
    // Copy image pointer, size & timestamp - pointer will be valid until next update()
    virtual bool getImage(Data& packet) const = 0;
    // Image data has been consumed
    virtual void clearImage() = 0;

    // Return true if there is enhancement data ready to get
    virtual bool hasEnhancement() const = 0;
    // Copy enhancement data pointer, size & timestamp - pointer will be valid until next update()
    virtual bool getEnhancement(Data& packet) const = 0;
    // Enhancement data has been consumed
    virtual void clearEnhancement() = 0;

    // Advance decoder - Update image and/or enhancement state
    // Returns true if the decoder is at end of stream.
    virtual bool update() = 0;
};

/*!
 * \brief Create a base video stream decoder that uses the libavcodec/libavformat libraries.
 *
 * @param[in]       source          The source stream, passed to avformat_open_input()
 * @param[in]       baseFormat      Optional output format for decoded base images.
 *
 * @return                          Unique pointer to a new base Decoder, or nullptr if failed.
 */
std::unique_ptr<BaseDecoder>
createBaseDecoderLibAV(std::string_view source, std::string_view sourceFormat = std::string_view(),
                       LCEVC_ColorFormat baseFormat = LCEVC_ColorFormat_Unknown, bool verbose = false);

/*!
 * \brief Create a base video stream decoder that reads LCEVC bin files and raw base frames in
 *        linear presentation order.
 *
 * @param[in]       rawFile         The raw file with base pictures - the base format and size is
 *                                  parsed from this name.
 * @param[in]       binFile         The LCEVC bin file with enhancement data.
 *
 * @return                          Unique pointer to a new base Decoder, or nullptr if failed.
 */
std::unique_ptr<BaseDecoder> createBaseDecoderBinLinear(std::string_view rawFile, std::string_view binFile);

/*!
 * \brief Create a base video stream decoder that reads LCEVC bin files and raw base frames in
 *        non-linear decode order.
 *
 * @param[in]       rawFile         The raw file with base pictures - the base format and size is
 *                                  parsed from this name.
 * @param[in]       binFile         The LCEVC bin file with enhancement data.
 *
 * @return                          Unique pointer to a new base Decoder, or nullptr if failed.
 */
std::unique_ptr<BaseDecoder> createBaseDecoderBinNonLinear(std::string_view rawFile,
                                                           std::string_view binFile);

} // namespace lcevc_dec::utility

#endif
