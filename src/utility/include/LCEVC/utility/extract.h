/* Copyright (c) V-Nova International Limited 2023. All rights reserved.
 *
 * Unpacking/unescaping raw LCEVC data from NAL units.
 */
#ifndef VN_LCEVC_UTILITY_EXTRACT_H
#define VN_LCEVC_UTILITY_EXTRACT_H

#ifdef __cplusplus
#include <cstdint>
extern "C"
{
#else
#include <stdint.h>
#include <stdbool.h>
#endif

/*!
 * \brief The type of NAL unit encoding
 */
typedef enum LCEVC_CodecType // NOLINT
{
    LCEVC_CodecType_Unknown = 0,
    LCEVC_CodecType_H264 = 1,
    LCEVC_CodecType_H265 = 2,
    LCEVC_CodecType_H266 = 3
} LCEVC_CodecType;

/*!
 * \brief The type of NAL formatting
 */
typedef enum LCEVC_NALFormat // NOLINT
{
    LCEVC_NALFormat_Unknown = 0,
    LCEVC_NALFormat_LengthPrefix = 1,
    LCEVC_NALFormat_AnnexeB = 2,
} LCEVC_NALFormat;

/*!
 * \brief Extract LCEVC enhancement data from a buffer containing NAL Units
 *
 * @param[in]       nalData              Pointer to buffer containing NAL units.
 * @param[in]       nalSize              Size in bytes of input NAL data
 * @param[in]       nalFormat            How the NAL units are formatted
 * @param[in]       codecType            What coding standard to use for parsing NAL units
 * @param[out]      enhancementData      Where to put extracted enhancement data
 * @param[in]       enhancementCapacity  Capacity of enhancement data buffer
 * @param[out]      enhancementSize      Pointer to where to write the extracted size - will be 0 if none found
 */
void LCEVC_extractEnhancementFromNAL(const uint8_t* nalData, uint32_t nalSize, LCEVC_NALFormat nalFormat,
                                     LCEVC_CodecType codecType, uint8_t* enhancementData,
                                     uint32_t enhancementCapacity, uint32_t* enhancementSize);

/*!
 * \brief Extract LCEVC enhancement data from a buffer containing NAL Units, and remove extracted data from input buffer.
 *
 * @param[in]       nalData              Pointer to buffer containing NAL units.
 * @param[in]       nalSize              Size in bytes of input NAL data
 * @param[in]       nalFormat            How the NAL units are formatted
 * @param[in]       codecType            What coding standard to use for parsing NAL units
 * @param[out]      nalOutSize           Pointer to where to write the size of the NAL units buffer after the enhancement data is removed
 * @param[out]      enhancementData      Where to put extracted enhancement data
 * @param[in]       enhancementCapacity  Capacity of enhancement data buffer
 * @param[out]      enhancementSize      Pointer to where to write the extracted size - will be 0 if none found
 */
void LCEVC_extractAndRemoveEnhancementFromNAL(uint8_t* nalData, uint32_t nalSize,
                                              LCEVC_NALFormat nalFormat, LCEVC_CodecType codecType,
                                              uint32_t* nalOutSize, uint8_t* enhancementData,
                                              uint32_t enhancementCapacity, uint32_t* enhancementSize);

#ifdef __cplusplus
}
#endif

#endif
