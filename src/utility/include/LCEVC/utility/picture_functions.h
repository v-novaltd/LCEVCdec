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

// Functions for common Picture operations - read/write/dump.
//
#ifndef VN_LCEVC_UTILITY_PICTURE_FUNCTIONS_H
#define VN_LCEVC_UTILITY_PICTURE_FUNCTIONS_H

#include <LCEVC/lcevc_dec.h>

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
 * time a distinct name is used by a running program, the output file will be truncated before
 * writing the picture. Subsequent uses will append the picture to the existing file.
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
LCEVC_ReturnCode copyPictureFromMemory(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture,
                                       const uint8_t* data, uint32_t size);

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
LCEVC_ReturnCode copyPictureToMemory(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture,
                                     uint8_t* data, uint32_t size);

/*!
 * \brief Fill buffer and plane descriptors of a padded picture for a given non-padded input.
 *        Strides of output will be rounded up to the next power of 2.
 *
 * @param[in]       srcDesc        Un-padded input picture desc
 * @param[in]       data           Pointer to the start of the input image to be copied
 * @param[out]      dstBufferDesc  Padded output picture buffer desc
 * @param[out]      dstPlaneDesc   Padded output picture plane desc
 *
 * @return                      LCEVC_Success or LCEVC_Error
 */
LCEVC_ReturnCode createPaddedDesc(const LCEVC_PictureDesc& srcDesc, const uint8_t* data,
                                  LCEVC_PictureBufferDesc* dstBufferDesc,
                                  LCEVC_PicturePlaneDesc* dstPlaneDesc);

} // namespace lcevc_dec::utility

#endif // VN_LCEVC_UTILITY_PICTURE_FUNCTIONS_H
