// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// A simple interface for base decoders used by samples and test harnesses.
//
#ifndef VN_LCEVC_UTILITY_BASE_DECODER_H
#define VN_LCEVC_UTILITY_BASE_DECODER_H

#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/picture_layout.h"

#include <memory>
#include <string_view>
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

    // Block of data from base decoder
    struct Data
    {
        uint8_t* ptr;
        uint32_t size;
        int64_t timestamp;

        bool empty() const { return ptr == nullptr; }
        void clear()
        {
            ptr = nullptr;
            size = 0;
            timestamp = -1;
        }
    };

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
                       LCEVC_ColorFormat baseFormat = LCEVC_ColorFormat_Unknown);

/*!
 * \brief Create a base video stream decoder that reads LCEVC bin files and raw base frames
 *
 * @param[in]       rawFile         The raw file with base pictures - the base format and size is parsed from this name.
 * @param[in]        binFile        The LCEVC bin file with enhancement data.
 *
 * @return                          Unique pointer to a new base Decoder, or nullptr if failed.
 */
std::unique_ptr<BaseDecoder> createBaseDecoderBin(std::string_view rawFile, std::string_view binFile);

} // namespace lcevc_dec::utility

#endif
