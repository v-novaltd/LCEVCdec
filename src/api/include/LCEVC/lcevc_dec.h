/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#ifndef LCEVC_DEC_H
#define LCEVC_DEC_H

/* NOLINTBEGIN(modernize-use-using) */
/* NOLINTBEGIN(modernize-deprecated-headers) */

#include <stdbool.h>
#include <stdint.h>

/* clang-format off */

#include "LCEVC/lcevc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Opaque decoder instance type.
 */
typedef struct LCEVC_DecoderHandle
{
    uintptr_t hdl;  /**< Unique identifying number, not user-legible */
} LCEVC_DecoderHandle;

/*!
 * Opaque picture handle, either an input or output
 */
typedef struct LCEVC_PictureHandle
{
    uintptr_t hdl;  /**< Unique identifying number, not user-legible */
} LCEVC_PictureHandle;

/*!
 * Opaque type for decoder acceleration context.
 */
typedef struct LCEVC_AccelContextHandle
{
    uintptr_t hdl;  /**< Unique identifying number, not user-legible */
} LCEVC_AccelContextHandle;

/*!
 * Opaque type for decoder acceleration buffer.
 */
typedef struct LCEVC_AccelBufferHandle
{
    uintptr_t hdl;  /**< Unique identifying number, not user-legible */
} LCEVC_AccelBufferHandle;

/*!
 * Opaque type for locked picture plane
 */
typedef struct LCEVC_PictureLockHandle
{
    uintptr_t hdl;  /**< Unique identifying number, not user-legible */
} LCEVC_PictureLockHandle;

/*!
 * This enum represents the possible API return codes.
 */
typedef enum LCEVC_ReturnCode
{
    LCEVC_Success       =  0,  /**< The API call completed successfully */

    LCEVC_Again         = -1,  /**< Not an error - the requested operation can't be performed, try again later. Only returned by LCEC_Send... and LCEVC_Receive... functions */
    LCEVC_NotFound      = -2,  /**< Not an error - A query call failed to find the item by name */

    LCEVC_Error         = -3,  /**< A generic catch all error */
    LCEVC_Uninitialized = -4,  /**< The decoder has not been initialized - eg: trying to Send/Recv before Initialize */
    LCEVC_Initialized   = -5,  /**< The decoder has been configured and initialized - eg: trying to configure after Initialize */
    LCEVC_InvalidParam  = -6,  /**< The user supplied an invalid parameter to the function call */
    LCEVC_NotSupported  = -7,  /**< The functionality requested is not supported on the running system. */
    LCEVC_Flushed       = -8,  /**< The requested operation failed because it was flushed via LCEVC_FlushDecoder. */
    LCEVC_Timeout       = -9,  /**< The requested operation failed because it timed out. */

    LCEVC_ReturnCode_ForceInt32 = 0x7fffffff
} LCEVC_ReturnCode;

/*!
 * This enum represents the YUV samples color range.
 */
typedef enum LCEVC_ColorRange
{
    LCEVC_ColorRange_Unknown   = 0,

    LCEVC_ColorRange_Full      = 1,  /**< Full range. Y, Cr and Cb component values range from 0 to 255 for 8-bit content */
    LCEVC_ColorRange_Limited   = 2,  /**< Limited range. Y component values range from 16 to 235 for 8-bit content. Cr, Cb values range from 16 to 240 for 8-bit content */

    LCEVC_ColorRange_ForceInt32 = 0x7fffffff
} LCEVC_ColorRange;

/*!
 * This enum represents the color primaries as defined in Table 2 of Rec. ITU-T H.273.
 */
typedef enum LCEVC_ColorStandard
{
    LCEVC_ColorStandard_Unknown    = 2,  /**< As required by ITU-T H.273, 0 is reserved. 2 is for unknown. */

    LCEVC_ColorStandard_BT709      = 1,  /**< BT.709 color chromacity coordinates with KR = 0.2126, KB = 0.0722 */
    LCEVC_ColorStandard_BT601_PAL  = 5,  /**< BT.601 625 color chromacity coordinates with KR = 0.299, KB = 0.114 */
    LCEVC_ColorStandard_BT601_NTSC = 6,  /**< BT.601 525 color chromacity coordinates with KR = 0.299, KB = 0.114 */
    LCEVC_ColorStandard_BT2020     = 9,  /**< BT.2020 color chromacity coordinates with KR = 0.2627, KB = 0.0593 */

    LCEVC_ColorStandard_ForceInt32 = 0x7fffffff
} LCEVC_ColorStandard;

/*!
 * This enum represents the color transfer characteristics as defined in Table 3, Rec. ITU-T H.273.
 * Note that SDR (colorTransfer 1) is mathematically equivalent to SMPTE 170 (colorTransfer 6), and
 * ITU-R BT.2020-2 (colorTransfers 14 and 15).
 */
typedef enum LCEVC_ColorTransfer
{
    LCEVC_ColorTransfer_Unknown = 2,   /**< As required by ITU-T H.273, 0 is reserved. 2 is for unknown. */

    LCEVC_ColorTransfer_SDR     = 1,   /**< ITU-R BT.709-6, "conventional colour gamut system" */
    LCEVC_ColorTransfer_Linear  = 8,   /**< Linear transfer characteristic curve */
    LCEVC_ColorTransfer_ST2084  = 16,  /**< SMPTE ST 2084 transfer function. Also called PQ */
    LCEVC_ColorTransfer_HLG     = 18,  /**< ARIB STD-B67 hybrid-log-gamma transfer function */

    LCEVC_ColorTransfer_ForceInt32 = 0x7fffffff
} LCEVC_ColorTransfer;

/*!
 * Identifies per picture metadata items.
 */
typedef enum LCEVC_PictureFlag {
    LCEVC_PictureFlag_Unknown    = 0,

    LCEVC_PictureFlag_IDR        = 1, /**< Base picture decoded from an IDR frame */
    LCEVC_PictureFlag_Interlaced = 2, /**< Base picture has two interlaced fields */

    LCEVC_PictureFlag_ForceInt32 = 0x7fffffff
} LCEVC_PictureFlag;

/*!
 * This struct represents the Static Metadata Descriptor payload, excluding the descriptor ID, of a Dynamic Range and Mastering InfoFrame as defined by CTA-861.3.
 *
 * The descriptor payload has a fixed size of 24 bytes. Display primaries describe the chromaticity of the Red, Green and Blue color primaries of the mastering display.
 * The correspondence between Red, Green and Blue color primaries and indices 0, 1, or 2 is determined by the following relationship: 
 * The Red color primary corresponds to the index of the largest display_primaries_x[] value. 
 * The Green color primary corresponds to the index of the largest display_primaries_y[] value. 
 * The Blue color primary corresponds to the index with neither the largest display_primaries_y[] value nor the largest display_primaries_x[] value
 * NOTE: individual fields are LSB first, so little-endian.
 */
typedef struct LCEVC_HDRStaticInfo
{
    uint16_t        displayPrimariesX0;              /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        displayPrimariesY0;              /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        displayPrimariesX1;              /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        displayPrimariesY1;              /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        displayPrimariesX2;              /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        displayPrimariesY2;              /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        whitePointX;                     /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        whitePointY;                     /**< Value in units of 0.00002, where 0x0000 represents zero and 0xC350 represents 1.0000 */
    uint16_t        maxDisplayMasteringLuminance;    /**< Value in units of 1 cd/m2, where 0x0001 represents 1 cd/m2 and 0xFFFF represents 65535 cd/m2 */
    uint16_t        minDisplayMasteringLuminance;    /**< Value in units of 0.0001 cd/m2, where 0x0001 represents 0.0001 cd/m2 and 0xFFFF represents 6.5535 cd/m2 */
    uint16_t        maxContentLightLevel;            /**< Value in units of 1 cd/m2, where 0x0001 represents 1 cd/m2 and 0xFFFF represents 65535 cd/m2 */
    uint16_t        maxFrameAverageLightLevel;       /**< Value in units of 1 cd/m2, where 0x0001 represents 1 cd/m2 and 0xFFFF represents 65535 cd/m2 */

} LCEVC_HDRStaticInfo;

/*!
 * This structure captures properties related to the decoding process at a particular timestamp.
 */
typedef struct LCEVC_DecodeInformation
{
    int64_t   timestamp;          /**< Presentation timestamp of picture */
    bool      hasBase;            /**< Base data is available for this picture */
    bool      hasEnhancement;     /**< Enhancement data is available for this picture */
    bool      skipped;            /**< LCEVC_SkipDecoder was requested for this timestamp */
    bool      enhanced;           /**< The picture has been enhanced by lcevc decoding */

    uint32_t  baseWidth;          /**< Width of base picture */
    uint32_t  baseHeight;         /**< Height of base picture */
    uint8_t   baseBitdepth;       /**< Bitdepth of base picture */

    void*     baseUserData;       /**< User data associated with picture via LCEVC_SendDecoderBase or LCEVC_SetPictureUserData */
} LCEVC_DecodeInformation;

/*!
 * The color formats that can be used in a picture.
 *
 * For a detailed description of the formats see: https://gstreamer.freedesktop.org/documentation/additional/design/mediatype-video-raw.html
 */
typedef enum LCEVC_ColorFormat
{
    LCEVC_ColorFormat_Unknown          = 0,

    LCEVC_I420_8                       = 1001,  /**< 8 bit 4:2:0 YUV Planar color format: Y, U and V on separate planes */
    LCEVC_I420_10_LE                   = 1002,  /**< 10 bit Little Endian 4:2:0 YUV Planar color format: Y, U and V on separate planes */
    LCEVC_I420_12_LE                   = 1003,  /**< 12 bit Little Endian 4:2:0 YUV Planar color format: Y, U and V on separate planes */
    LCEVC_I420_14_LE                   = 1004,  /**< 14 bit Little Endian 4:2:0 YUV Planar color format: Y, U and V on separate planes */
    LCEVC_I420_16_LE                   = 1005,  /**< 16 bit Little Endian 4:2:0 YUV Planar color format: Y, U and V on separate planes */

    LCEVC_YUV420_RASTER_8              = 1006,  /**< 8bit 4:2:0 YUV raster colour format: Y, U and V on a single plane on three channels, GPU sampler requires GL_EXT_YUV_target extension */

    LCEVC_NV12_8                       = 2001,  /**< 8 bit 4:2:0 YUV SemiPlanar color format: Y plane and UV interleaved plane */
    LCEVC_NV21_8                       = 2002,  /**< 8 bit 4:2:0 YUV SemiPlanar color format: Y plane and VU interleaved plane */

    LCEVC_RGB_8                        = 3001,  /**< 8 bit Interleaved R, G, B planes 24 bit per sample */
    LCEVC_BGR_8                        = 3002,  /**< 8 bit Interleaved R, G, B planes 24 bit per sample */
    LCEVC_RGBA_8                       = 3003,  /**< 8 bit Interleaved R, G, B and A planes 32 bit per sample */
    LCEVC_BGRA_8                       = 3004,  /**< 8 bit Interleaved R, G, B and A planes 32 bit per sample */
    LCEVC_ARGB_8                       = 3005,  /**< 8 bit Interleaved R, G, B and A planes 32 bit per sample */
    LCEVC_ABGR_8                       = 3006,  /**< 8 bit Interleaved R, G, B and A planes 32 bit per sample */

    LCEVC_RGBA_10_2_LE                 = 4001,  /**< 10 bit Little Endian interleaved R, G, and B channels, with a 2bit A channel. 32 bits per sample. */

    LCEVC_GRAY_8                       = 5001,  /**< 8 bit 4:0:0 (Monochrome) YUV Planar color format: U and V planes are unused */
    LCEVC_GRAY_10_LE                   = 5002,  /**< 10 bit Little Endian 4:0:0 (Monochrome) YUV Planar color format: U and V planes are unused */
    LCEVC_GRAY_12_LE                   = 5003,  /**< 12 bit Little Endian 4:0:0 (Monochrome) YUV Planar color format: U and V planes are unused */
    LCEVC_GRAY_14_LE                   = 5004,  /**< 14 bit Little Endian 4:0:0 (Monochrome) YUV Planar color format: U and V planes are unused */
    LCEVC_GRAY_16_LE                   = 5005,  /**< 16 bit Little Endian 4:0:0 (Monochrome) YUV Planar color format: U and V planes are unused */

   LCEVC_ColorFormat_ForceInt32 = 0x7fffffff,
} LCEVC_ColorFormat;

/*!
 * The configuration of a single picture.
 */
typedef struct LCEVC_PictureDesc
{
    uint32_t              width;                    /**< Nominal net width of the picture in luma samples, i.e. no alignment to macroblocks or striding */
    uint32_t              height;                   /**< Nominal net height of the picture in luma samples, i.e. no sliceheight or padding */
    LCEVC_ColorFormat     colorFormat;              /**< Color format of the picture */
    LCEVC_ColorRange      colorRange;               /**< Color range of the picture */
    LCEVC_ColorStandard   colorStandard;            /**< Color standard of the picture */
    LCEVC_ColorTransfer   colorTransfer;            /**< Color transfer to use to display picture */
    LCEVC_HDRStaticInfo   hdrStaticInfo;            /**< Hdr static metadata for custom color primaries */
    uint32_t              sampleAspectRatioNum;     /**< Sample Aspect Ratio of width to height, this is the numerator, sar = (sample_aspect_ratio_num)/(sample_aspect_ratio_den) */
    uint32_t              sampleAspectRatioDen;     /**< Sample Aspect Ratio of width to height, this is the denominator, sar = (sample_aspect_ratio_num)/(sample_aspect_ratio_den) */

    uint32_t              cropTop;                  /**< Vertical top offset of the crop area, aka active area, where the Image samples are to be found. NOTE: crop size might differ from sample size */
    uint32_t              cropBottom;               /**< Vertical bottom offset of the crop area, aka active area, where the Image samples are to be found. NOTE: crop size might differ from sample size */
    uint32_t              cropLeft;                 /**< Horizontal left offset of the crop area, aka active area, where the Image samples are to be found. NOTE: crop size might differ from sample size */
    uint32_t              cropRight;                /**< Horizontal right offset of the crop area, aka active area, where the Image samples are to be found. NOTE: crop size might differ from sample size */

} LCEVC_PictureDesc;

/*!
 * The intended use of a locked picture plane or picture buffer.
 */
typedef enum LCEVC_Access
{
    LCEVC_Access_Unknown = 0,

    LCEVC_Access_Read    = 1,  /**< Existing plane data will be read */
    LCEVC_Access_Modify  = 2,  /**< Existing plane data will be read and new data written */
    LCEVC_Access_Write   = 3,  /**< New data will be written - all existing data in plane can be dropped */

    LCEVC_Access_ForceInt32 = 0x7fffffff,
} LCEVC_Access;

/*!
 * Describes a picture buffer, as used when allocating an external picture
 */
typedef struct LCEVC_PictureBufferDesc
{
    uint8_t* data;                          /**< Pointer to start of buffer */
    uint32_t byteSize;                      /**< Total size of buffer in bytes */
    LCEVC_AccelBufferHandle accelBuffer;    /**< Opaque reference to any accleration support for buffer. If this is set, data & size may be zero */
    LCEVC_Access access;                    /**< The permitted access modes for this buffer. */

} LCEVC_PictureBufferDesc;

/*!
 * This struct describes the layout of a locked plane.
 */
typedef struct LCEVC_PicturePlaneDesc
{
    uint8_t* firstSample;         /**< Pointer to first byte of the first active sample of plane within buffer, NOTE: active means no cropping, the active origin is 0,0 */
    uint32_t sampleByteStride;    /**< Distance in bytes between consecutive sample values of the same channel, for ex. this will be 2 for the UV plane in LCEVC_NV12_8 and LCEVC_NV21_8. Note: 0 means to use default sample_stride relative to LCEVC_PictureDesc.color_format */
    uint32_t rowByteStride;       /**< Distance in bytes between the first sample of two consecutive rows, including any trailing pading */
    uint32_t sampleByteSize;      /**< Size of sample in bytes: eg:   LCEVC_I420_8 will be 1, and LCEVC_I420_10_LE will be 2 */
    uint32_t rowByteSize;         /**< Size of row in bytes - including sample stride, not including row padding */
    uint32_t width;               /**< Width of this plane in samples - will be equal to rowByteSize / sampleByteStride */
    uint32_t height;              /**< Height of this plane in rows */

} LCEVC_PicturePlaneDesc;

/*!
 * Get an initialised LCEVC_PictureDesc with default values, only basic size and color
 * format parameters are required.
 * 
 * @param[in]    format              An LCEVC_ColorFormat enum
 * @param[in]    width               Image width
 * @param[in]    height              Image height
 * @param[out]   pictureDesc         Picture description structure
 * @return                           LCEVC_Error if pictureDesc contains incompatible values
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_DefaultPictureDesc( LCEVC_PictureDesc* pictureDesc,
                                           LCEVC_ColorFormat format,
                                           uint32_t width,
                                           uint32_t height);

/*!
 * Returns a handle to a picture instance that uses buffers managed by the decoder.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    pictureDesc         Describing picture configurations
 * @param[out]   picture             Result LCEVC_PictureHandle if creation is successful, 
 *                                   otherwise NULL
 * @return                           LCEVC_Error if pictureDesc contains incompatible values
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_AllocPicture( LCEVC_DecoderHandle decHandle,
                                     const LCEVC_PictureDesc* pictureDesc,
                                     LCEVC_PictureHandle* picture );

/*!
 * Returns a handle to a picture instance that uses buffers external to the decoder.
 * One buffer per plane of the specified picture format is required.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    pictureDesc         Describing picture format and dimensions
 * @param[in]    bufferCount         Number of underlying buffers (must match format requirements)
 * @param[in]    buffers             Underlying pixel buffer(s)
 * @param[out]   picture             Result LCEVC_PictureHandle if creation is successful,
 *                                   otherwise NULL
 * @return                           LCEVC_Error if pictureDesc contains incompatible values
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_AllocPictureExternal( LCEVC_DecoderHandle decHandle,
                                             const LCEVC_PictureDesc* pictureDesc,
                                             uint32_t bufferCount,
                                             const LCEVC_PictureBufferDesc* buffers,
                                             LCEVC_PictureHandle* picture );

/*!
 * Releases a picture instance.
 *
 * @attention                        No reference calls to picture handle should be made after this
 *                                   method
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be released
 * @return                           LCEVC_InvalidParam if an invalid picHandle is given, 
 *                                   LCEVC_Error if the picture cannot be release, otherwise 
 *                                   LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_FreePicture( LCEVC_DecoderHandle decHandle, LCEVC_PictureHandle picHandle );

/*!
 * Set per picture metadata.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be updated
 * @param[in]    flag                Identify flag value to set in picture
 * @param[in]    value               Value that flag will be set to
 * @return                           LCEVC_InvalidParam for an invalid picHandle or flag, otherwise
 *                                   LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SetPictureFlag( LCEVC_DecoderHandle decHandle,
                                       LCEVC_PictureHandle picHandle,
                                       LCEVC_PictureFlag flag,
                                       bool value );

/*!
 * Fetch per picture metadata.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be queried
 * @param[in]    flag                Identify flag value to get from picture
 * @param[out]   value               Value of the flag that will be fetched
 * @return                           LCEVC_InvalidParam for an invalid picHandle or flag, otherwise
 *                                   LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureFlag( LCEVC_DecoderHandle decHandle,
                                       LCEVC_PictureHandle picHandle,
                                       LCEVC_PictureFlag flag,
                                       bool* value );

/*!
 * Get the description of a picture instance.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be queried
 * @param[out]   desc                Contents of the pointer will be rewritten with picture
 *                                   descriptions
 * @return                           LCEVC_InvalidParam for an invalid picHandle or desc, otherwise
 *                                   LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureDesc( LCEVC_DecoderHandle decHandle,
                                       LCEVC_PictureHandle picHandle,
                                       LCEVC_PictureDesc* desc );

/*!
 * Update the description of a picture instance, and potentially reallocate memory.
 * 
 * When using a decoder-managed picture (created by LCEVC_AllocPicture) the underlying
 * memory will be released and re-allocated automatically. Otherwise,
 * memory reallocation will not be handled by this function.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be adjusted
 * @param[in]    desc                The new picture description
 * @return                           LCEVC_InvalidParam for an invalid picHandle or desc, otherwise
 *                                   LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SetPictureDesc( LCEVC_DecoderHandle decHandle,
                                       LCEVC_PictureHandle picHandle,
                                       const LCEVC_PictureDesc* desc );

/*!
 * Get the number of buffers from a picture instance.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be queried
 * @param[out]   bufferCount         Will be filled with number of buffers
 * @return                           LCEVC_InvalidParam for an invalid picHandle, otherwise
 *                                   LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureBufferCount( LCEVC_DecoderHandle decHandle,
                                              LCEVC_PictureHandle picHandle,
                                              uint32_t* bufferCount );

/*!
 * Get a description of a buffer from a picture instance.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be queried
 * @param[in]    bufferIndex         Index of buffer to be queried
 * @param[out]   bufferDesc          Destination for buffer description
 * @return                           LCEVC_InvalidParam for an invalid picHandle or bufferDesc,
 *                                   otherwise LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureBuffer( LCEVC_DecoderHandle decHandle,
                                         LCEVC_PictureHandle picHandle,
                                         uint32_t bufferIndex,
                                         LCEVC_PictureBufferDesc* bufferDesc );

/*!
 * Get the number of planes from a picture instance.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be queried
 * @param[out]   planeCount          Will be filled with number of planes
 * @return                           LCEVC_InvalidParam for an invalid picHandle or bufferDesc,
 *                                   otherwise LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPicturePlaneCount( LCEVC_DecoderHandle decHandle,
                                             LCEVC_PictureHandle picHandle,
                                             uint32_t* planeCount );

/*!
 * Set the user data pointer in a picture instance. This will be overwritten if you later call
 * LCEVC_SendDecoderBase using the same picture.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be updated
 * @param[in]    userData            The user data to associate with the picture
 * @return                           LCEVC_InvalidParam if the picture is not valid, 
 *                                   LCEVC_Success otherwise
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SetPictureUserData( LCEVC_DecoderHandle decHandle,
                                           LCEVC_PictureHandle picHandle,
                                           void* userData );

/*!
 * Get the user data pointer from a picture instance.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to be queried
 * @param[out]   userData            The user data to associate with the picture
 * @return                           LCEVC_InvalidParam if the picture is invalid or userData is
 *                                   invalid, LCEVC_Success otherwise
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureUserData( LCEVC_DecoderHandle decHandle,
                                           LCEVC_PictureHandle picHandle,
                                           void** userData );

/*!
 * Request access to a picture instance so that the contents of a plane can be accessed via a
 * strided pointer. This creates a picture lock (only one lock is allowed per picture).
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    picHandle           Handle to picture instance to lock
 * @param[in]    access              How plane data will be accessed (Read, Write, or Modify)
 * @param[out]   pictureLock         Destination PictureLockHandle representing the locked state
 * @return                           LCEVC_InvalidParam if the picture or access is invalid,
 *                                   LCEVC_Error if picture has already been locked, and 
 *                                   LCEVC_Success otherwise.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_LockPicture( LCEVC_DecoderHandle decHandle,
                                    LCEVC_PictureHandle picHandle,
                                    LCEVC_Access access,
                                    LCEVC_PictureLockHandle* pictureLock );

/*!
 * Get the description of a locked picture buffer.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    pictureLock         Lock instance to query
 * @param[in]    bufferIndex         Index of buffer within locked picture to query
 * @param[out]   bufferDesc          Destination PictureBufferDesc describing locked memory layout
 * @return                           LCEVC_InvalidParam if the lock is invalid, LCEVC_Success
 *                                   otherwise
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureLockBufferDesc( LCEVC_DecoderHandle decHandle,
                                                 LCEVC_PictureLockHandle pictureLock,
                                                 uint32_t bufferIndex,
                                                 LCEVC_PictureBufferDesc* bufferDesc );

/*!
 * Get the description of a locked picture plane.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    pictureLock         pictureLock instance to query
 * @param[in]    planeIndex          Index of plane within locked picture to query
 * @param[out]   planeDesc           Destination PicturePlaneDesc describing locked memory layout
 * @return                           LCEVC_InvalidParam if the lock is invalid, LCEVC_Success
 *                                   otherwise
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_GetPictureLockPlaneDesc( LCEVC_DecoderHandle decHandle,
                                                LCEVC_PictureLockHandle pictureLock,
                                                uint32_t planeIndex,
                                                LCEVC_PicturePlaneDesc* planeDesc );

/*!
 * Release access to picture instance's plane.
 *
 * @param[in]    decHandle           LCEVC Decoder handle
 * @param[in]    pictureLock         Lock instance returned by LCEVC_LockPicture
 * @return                           LCEVC_InvalidParam if the lock is invalid
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_UnlockPicture( LCEVC_DecoderHandle decHandle,
                                      LCEVC_PictureLockHandle pictureLock );

/*!
 * Create an instance of an LCEVC Decoder.
 *
 * @param[in]    accelContext        If not null, a handle for connecting to some decoder
 *                                   acceleration mechanism, e.g. GPU/FPGA/other hardware. This
 *                                   handle will be generated by a seperate API specific to the
 *                                   acclerator component.
 * @param[out]   decHandle           Created decoder instance
 * @return                           LCEVC_InvalidParam if decHandle is NULL, otherwise
 *                                   LCEVC_Success
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_CreateDecoder( LCEVC_DecoderHandle* decHandle, 
                                      LCEVC_AccelContextHandle accelContext);

/*! 
 * @defgroup LCEVC_ConfigureDecoder LCEVC_ConfigureDecoder[type]
 * 
 * Set a configuration variable of type [type]. [type] may be: Bool, Int, Float, String, or an array 
 * of those (e.g. BoolArray).
 * @{
 */
LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderBool( LCEVC_DecoderHandle decHandle, const char* name, bool val );
LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderInt( LCEVC_DecoderHandle decHandle, const char* name, int32_t val );
LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderFloat( LCEVC_DecoderHandle decHandle, const char* name, float val );
LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderString( LCEVC_DecoderHandle decHandle, const char* name, const char* val );

LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderBoolArray( LCEVC_DecoderHandle decHandle, const char* name, uint32_t count, const bool* arr );
LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderIntArray( LCEVC_DecoderHandle decHandle, const char* name, uint32_t count, const int32_t* arr );
LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderFloatArray( LCEVC_DecoderHandle decHandle, const char* name, uint32_t count, const float* arr );
LCEVC_API LCEVC_ReturnCode LCEVC_ConfigureDecoderStringArray( LCEVC_DecoderHandle decHandle, const char* name, uint32_t count, const char* const* arr );
/** @} */

/*!
 * Initialize a configured Decoder.
 *
 * @param[in]    decHandle           Decoder handle instance returned by CreateDecoder
 * @return                           Returns LCEVC_Success or an appropiate error status
 *                                   if unable to lock and initialise the given instance
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_InitializeDecoder( LCEVC_DecoderHandle decHandle );

/*!
 * Destroy an instance of a Decoder, releasing memory.
 *
 * @param[in]    decHandle           Decoder instance to be destroyed
 */
LCEVC_API
void LCEVC_DestroyDecoder( LCEVC_DecoderHandle decHandle );

/*!
 * Send enhancement data to the LCEVC Decoder.
 *
 * Feed a buffer of pre-parsed LCEVC payload data for the Access Unit identified by timestamp.
 *
 * Any encapsulation or ecsaping that is part of the overall stream or transport should beremoved
 * befiore the data is passed to this function.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[in]    timestamp           Timestamp for the passed LCEVC data
 * @param[in]    discontinuity       Set true if enhancement is not continuous with the last sent
 *                                   enhancement
 * @param[in]    data                pointer to the LCEVC enhancenemtn data buffer
 * @param[in]    byteSize            size of the LCEVC enhancement data buffer
 * @return                           LCEVC_Again if the decoder cannot consume the enhancement
 *                                   data in its current state, but may be able to later
 *                                   (typically this means receiving decoded pictures). If
 *                                   LCEVC_Again is returned, then the decoder state has not 
 *                                   changed, EXCEPT to accommodate discontinuities.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SendDecoderEnhancementData( LCEVC_DecoderHandle decHandle,
                                                   int64_t timestamp,
                                                   bool discontinuity,
                                                   const uint8_t* data,
                                                   uint32_t byteSize );

/*!
 * Send a base picture to the LCEVC Decoder.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[in]    timestamp           Timestamp for the base picture
 * @param[in]    discontinuity       Set true if base frames are not continuous
 * @param[in]    base                Decoded base picture. This should not be used again by the
 *                                   client until it has been sent back via
 *                                   LCEVC_ReceiveDecoderBase.
 * @param[in]    timeoutUs           Maximum decode time in uSecs
 * @param[in]    userData            A user pointer that is propagated to the DecoderInformation
 *                                   for all decoded timestamps that result from this base.
 *                                   Overwrites any existing userData.
 * @return                           LCEVC_Again if the decoder cannot consume the base data in
 *                                   its current state, but may be able to later (typically this
 *                                   means receiving decoded pictures). If LCEVC_Again is returned,
 *                                   then the decoder state has not changed, EXCEPT to accommodate
 *                                   discontinuities.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SendDecoderBase( LCEVC_DecoderHandle decHandle,
                                        int64_t timestamp,
                                        bool discontinuity,
                                        LCEVC_PictureHandle base,
                                        uint32_t timeoutUs,
                                        void* userData );

/*!
 * Get used base picture from Decoder.
 *
 * Get the next base picture previously sent with SendDecoderBase() that the decoder has finished
 * using.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[out]   output              Pointer to LCEVC base picture
 * @return                           LCEVC_Again if the decoder has no base pictures that are free
 *                                   to recycle.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_ReceiveDecoderBase( LCEVC_DecoderHandle decHandle,
                                           LCEVC_PictureHandle* output);


/*!
 * Send a picture to be used later for output
 *
 * AFTER this function is called, and before the picture is output by ReceiveDecoderPicture, the
 * decoder will update the entire LCEVC_PictureDesc of the output picture. This may have a
 * performance cost, unless the LCEVC_PictureDesc is already correct for this picture.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[out]   output              Pointer to LCEVC enhanced (or base pass-through) output
                                     picture. This should not be used again by the client until
                                     it has been sent back via LCEVC_ReceiveDecoderPicture.
 * @return                           LCEVC_Again if the decoder cannot store another reusable
 *                                   output picture.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SendDecoderPicture( LCEVC_DecoderHandle decHandle,
                                           LCEVC_PictureHandle output );

/*!
 * Get output picture from Decoder.
 *
 * Get the next decoded picture if available.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[out]   output              pointer to LCEVC enhanced output picture
 * @param[out]   decodeInformation   pointer to decoder information structure that the LCEVC
                                     Decoder will fill
 * @return                           LCEVC_InvalidParam for an invalid decHandle or output;
 *                                   LCEVC_Flushed if the next picture got flushed during/after it
 *                                   was decoded; LCEVC_Timeout if the picture took too long to 
 *                                   decode (set by timeoutUs in LCEVC_SendDecoderBase) or 
 *                                   LCEVC_Error for a failed decode. Otherwise returns
 *                                   LCEVC_Success.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_ReceiveDecoderPicture( LCEVC_DecoderHandle decHandle,
                                              LCEVC_PictureHandle* output,
                                              LCEVC_DecodeInformation* decodeInformation );

/*!
 * Get details of a picture within decoder.
 *
 * Get details of the decoding process for the Access Unit identified by timestamp. Does not
 * generally affect the state of the Decoder.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[in]    timestamp           Time reference of the picture
 * @param[out]   pictureDesc         Where description of frame that will be decoded is written
 * @param[out]   information         pointer to decoder information structure that the LCEVC
                                     Decoder will fill
 * @return                           LCEVC_Again if the decoder cannot produce a decoded picture in
 *                                   its current state, but may be able to later (typically this 
 *                                   means sending base and/or enhancement data). Returns
 *                                   LCEVC_Flushed if the timestamp was flushed. Returns
 *                                   LCEVC_Success if this timestamp was found.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_PeekDecoder( LCEVC_DecoderHandle decHandle,
                                    int64_t timestamp,
                                    LCEVC_PictureDesc* pictureDesc,
                                    LCEVC_DecodeInformation* information );

/*!
 * Tell the decoder that any picture at or earlier than timestamp is not being presented.
 *
 * The decoder will do the minimum processing to keep internal state consistent for the next
 * picture. Skipped bases can still be received via LCEVC_ReceiveDecoderBase. However, they will
 * not come out of LCEVC_ReceiveDecoderPicture UNLESS they have already been decoded into an output
 * picture. In that case, they will be emitted with "skipped = true".
 *
 * All appropriate events will be generated for any skipped frames.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[in]    timestamp           time reference for the Access Unit to be skipped
 * @return                           LCEVC_InvalidParam if the handle is invalid,
 *                                   LCEVC_Uninitialised if the decoder isn't initialised,
 *                                   otherwise LCEVC_Success.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SkipDecoder( LCEVC_DecoderHandle decHandle,
                                    int64_t timestamp );

/*! 
 * Synchronize client and LCEVC Decoder.
 *
 * All pending frame events will be generated.
 *
 * Any pending frames can be optionally dropped.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[in]    dropPending         Discard any pending frames
 * @return                           LCEVC_InvalidParam if the handle is invalid,
 *                                   LCEVC_Uninitialised if the decoder isn't initialised,
 *                                   otherwise LCEVC_Success.
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SynchronizeDecoder( LCEVC_DecoderHandle decHandle,
                                           bool dropPending );

/*!
 * Throw away any data that hasn't been emitted yet.
 *
 * After this is called, calls to LCEVC_PeekDecoder and LCEVC_ReceiveDecoderPicture will return 
 * LCEVC_Flushed for any pictures that were in the decoder when flushed.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @return                           LCEVC_Success on successful flush
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_FlushDecoder( LCEVC_DecoderHandle decHandle );

/*!
 * Events generated by Decoder
 *        
 * The possible events are selected when the decoder is configured.
 *
 * Parameters to the callback are used to associate data with events:
 * - LCEVC_Log: 'data' and 'data_size' describe a printable string.
 * - LCEVC_OutputPictureDone: 'picture' is a handle to the picture that the event refers to.
 * 'decode_information' is a pointer to the decode information for the relevant frame - 
 * it is only valid for duration of event callback.
 */
typedef enum LCEVC_Event {
    LCEVC_Log                = 0,  /**< A logging event from the decoder */
    LCEVC_Exit               = 1,  /**< The decoder will exit - no further events will be generated */
    LCEVC_CanSendBase        = 2,  /**< SendDecoderBase will not return LCEVC_Again */
    LCEVC_CanSendEnhancement = 3,  /**< SendDecoderEnhancementData will not return LCEVC_Again */
    LCEVC_CanSendPicture     = 4,  /**< SendDecoderPicture will not return LCEVC_Again */
    LCEVC_CanReceive         = 5,  /**< ReceiveDecoderPicture will not return LCEVC_Again */
    LCEVC_BasePictureDone    = 6,  /**< A base picture is no longer needed by decoder */
    LCEVC_OutputPictureDone  = 7,  /**< An output picture has been completed by the decoder */

    LCEVC_EventCount,

    LCEVC_Event_ForceInt32 = 0x7fffffff
} LCEVC_Event;

/*!
 * A user provided function that will be called by the decoder for events that match the event mask
 * set up at configuration time.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[in]    event               Identifies the event type
 * @param[in]    picHandle           Optional picture associated with event
 * @param[in]    decodeInformation   Optional decode information associated with event
 * @param[in]    data                Optional pointer to byte data associated with the event
 * @param[in]    dataSize            Optional size of byte data associated with the event
 * @param[in]    userData            A user pointer that was passed in to SetDecoderEventCallback
 */
typedef void (*LCEVC_EventCallback)( LCEVC_DecoderHandle decHandle,
                                     LCEVC_Event event,
                                     LCEVC_PictureHandle picHandle,
                                     const LCEVC_DecodeInformation* decodeInformation,
                                     const uint8_t* data, uint32_t dataSize,
                                     void* userData );

/*!
 * Set a callback in a decoder instance.
 *
 * @param[in]    decHandle           LCEVC Decoder instance
 * @param[in]    callback            An LCEVC_EventCallback which will be called when LCEVC
 *                                   triggers an event that was set up by the event mask during
 *                                   configuration.
 * @param[in]    userData            A pointer to any associated data
 * @return                           LCEVC_Success unless the decoder can't be locked
 */
LCEVC_API
LCEVC_ReturnCode LCEVC_SetDecoderEventCallback( LCEVC_DecoderHandle decHandle,
                                                LCEVC_EventCallback callback,
                                                void* userData );


#ifdef __cplusplus
}
#endif

/* NOLINTEND(modernize-deprecated-headers) */
/* NOLINTEND(modernize-use-using) */

#endif
