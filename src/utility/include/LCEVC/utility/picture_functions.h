// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Functions for common Picture operations - read/write/dump.
//
#ifndef VN_LCEVC_UTILITY_PICTURE_FUNCTIONS_H
#define VN_LCEVC_UTILITY_PICTURE_FUNCTIONS_H

#include "LCEVC/lcevc_dec.h"

#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

/*!
 * \brief Write a picture to a file
 *
 * @param[in]       decoder     Decoder from which picture will be allocated
 * @param[in]       picture     Picture to write
 * @param[in]       filename    Filename of raw file

 * @return                      LCEVC_Success or LCEVC_Error
 */
LCEVC_ReturnCode writePictureToRaw(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture,
                                   std::string_view filename);

/*!
 * \brief Append a picture to a raw file, when dumping is enabled.
 *
 * If dumping is disabled, this function does nothing.
 *
 * The full name of the output is constructed from the picture description, and baseName. The first
 * time a distinct name is used by a running program, the output file will be truncated before writing the
 * picture. Subsequent uses will append the picture to the existing file.
 *
 * @param[in]       decoder     Decoder from which picture will be allocated
 * @param[in]       picture     Picture to write
 * @param[in]       baseName    Name of raw file
 */
void dumpPicture(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, std::string_view baseName);

/*!
 * \brief Globally enable dumpPicture()
 */
void enableDumpPicture();

/*!
 * \brief Globally disable dumpPicture()
 */
void disableDumpPicture();

/*!
 * \brief Read a named raw image into a new picture object.
 *
 * @param[in]       decoder     Decoder from which picture will be allocated
 * @param[in]       name        Filename of raw file
 * @param[out]      picture     Handle to allocated picture
 * @return                      LCEVC_Success or LCEVC_Error
 */
LCEVC_ReturnCode readPictureFromRaw(LCEVC_DecoderHandle decoder, std::string_view name,
                                    LCEVC_PictureHandle& picture);

/*!
 * \brief Copy image data from memory into an existing picture
 *
 * @param[in]       decoder     Decoder from which picture was allocated
 * @param[in]       picture     Handle to destination picture
 * @param[in]       data        Source of component data
 * @param[in]       size        Size in bytes of component data
 *
 * @return                      LCEVC_Success or LCEVC_InvalidParam
 */
LCEVC_ReturnCode copyPictureFromMemory(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, const uint8_t *data, uint32_t size);

/*!
 * \brief Copy image data to memory from an existing picture
 *
 * @param[in]       decoder     Decoder from which picture will be allocated
 * @param[in]       picture     Handle to source picture
 * @param[in]       data        Destination of component data
 * @param[in]       size        Size in bytes of component data
 *
 * @return                      LCEVC_Success or LCEVC_InvalidParam
 */
LCEVC_ReturnCode copyPictureToMemory(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, uint8_t *data, uint32_t size);

} // namespace lcevc_dec::utility

#endif
