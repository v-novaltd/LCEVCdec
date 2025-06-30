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

#ifndef VN_LCEVC_ENHANCEMENT_CHUNK_H
#define VN_LCEVC_ENHANCEMENT_CHUNK_H

#include <LCEVC/enhancement/config_types.h>

/*! \brief Obtain the array of non-temporal (layer) chunk data for a given plane index, LOQ and tile
 *         index from the frame config.
 *
 * This function will output NULL chunk data if enhancement is disabled. The number
 * of chunks can be determined from num_layers.
 *
 * \param[in]  globalConfig  Global config to reference
 * \param[in]  frameConfig   Frame config to get chunk from
 * \param[in]  loq           The LOQ index to obtain the layer chunks for.
 * \param[in]  planeIdx      The plane index to obtain the layer chunks for
 * \param[in]  tileIdx       The tile index to obtain the layer chunks for
 * \param[out] chunks        Double pointer to populate with the array of chunks to read from. This
 *                           will be set to NULL if layer enhancement is currently disabled.
 *
 *  \return True on success, otherwise false. */
bool getLayerChunks(const LdeGlobalConfig* globalConfig, const LdeFrameConfig* frameConfig,
                    uint32_t planeIdx, LdeLOQIndex loq, uint32_t tileIdx, LdeChunk** chunks);

/*! \brief Obtain a pointer to the temporal chunk data for a given plane from the
 *         frame config chunks. This function will output NULL chunk data if temporal
 *         is disabled.
 *
 * \param[in]  globalConfig  Global config to reference
 * \param[in]  frameConfig   Frame config to get chunk from
 * \param[in]  planeIdx      The plane index to obtain the temporal chunk for
 * \param[in]  tileIdx       The tile index to obtain the temporal chunk for
 * \param[out] chunk         Double pointer to populate the temporal chunk to read from
 *                           This will be set to NULL if temporal enhancement is disabled.
 *
 * \return True on success, otherwise false. */
bool getTemporalChunk(const LdeGlobalConfig* globalConfig, const LdeFrameConfig* frameConfig,
                      uint32_t planeIdx, uint32_t tileIdx, LdeChunk** chunk);

bool temporalChunkEnabled(const LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig);

uint32_t getLayerChunkIndex(const LdeFrameConfig* frameConfig, const LdeGlobalConfig* globalConfig,
                            LdeLOQIndex loq, uint32_t planeIdx, uint32_t tileIdx, uint32_t layer);

#endif // VN_LCEVC_ENHANCEMENT_CHUNK_H
