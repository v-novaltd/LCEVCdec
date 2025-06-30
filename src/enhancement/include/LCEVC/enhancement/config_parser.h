/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#ifndef VN_LCEVC_ENHANCEMENT_CONFIG_PARSER_H
#define VN_LCEVC_ENHANCEMENT_CONFIG_PARSER_H

#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/config_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief Initialize global config to a default state.
 *
 * \param[in] forceBitstreamVersion    Use older versions of the LCEVC MPEG-5 Part 2 standard. Use
 *                                     BitstreamVersionUnspecified to default to the most recent or
 *                                     a signalled version if unsure.
 * \param[out] globalConfig            Global config instance to initialize.
 */
void ldeGlobalConfigInitialize(uint8_t forceBitstreamVersion, LdeGlobalConfig* globalConfig);

/*! \brief Initialize frame config to a default state.
 *
 * \param[in] allocator                Memory allocator to use for the frame config's encapsulated
 *                                     data and output chunks.
 * \param[out] frameConfig             Frame config instance to initialize.
 */
void ldeFrameConfigInitialize(LdcMemoryAllocator* allocator, LdeFrameConfig* frameConfig);

/*! \brief Release allocations in the frame config's encapsulated data and output chunks.
 *
 *  \param[in] frameConfig   Frame config to release */
void ldeConfigsReleaseFrame(LdeFrameConfig* frameConfig);

/*! \brief Parse a serialized frame to config structs, taking into account state from previous frames
 *
 * \param[in]     serialized           Serialised data to deserialize.
 * \param[in]     serializedSize       Byte size of the serialized data.
 * \param[inout]  globalConfig         Input the last frame's global config for modification,
 *                                     globalConfigModified is set true if modified
 * \param[inout]  frameConfig          Input the last frame's frame config primarily to read the
 *                                     state from the quantMatrix. Read output parameters including
 *                                     chunks
 * \param[out]    globalConfigModified Output flag if the input global config was modified
 *
 * \return True on success, otherwise false.
 */
bool ldeConfigsParse(const uint8_t* serialized, size_t serializedSize, LdeGlobalConfig* globalConfig,
                     LdeFrameConfig* frameConfig, bool* globalConfigModified);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_ENHANCEMENT_CONFIG_PARSER_H
