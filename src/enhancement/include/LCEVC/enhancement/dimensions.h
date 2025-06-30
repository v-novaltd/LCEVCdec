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

#ifndef VN_LCEVC_ENHANCEMENT_DIMENSIONS_H
#define VN_LCEVC_ENHANCEMENT_DIMENSIONS_H

#include <LCEVC/enhancement/config_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief Get the width and height of a plane from the global config for a given loq and YUV plane
 *
 * \param[in]  globalConfig  Config to reference
 * \param[in]  loq           The LOQ in calculate for
 * \param[in]  planeIdx      The plane to calculate for
 * \param[out] width         The location to store the calculated plane width.
 * \param[out] height        The location to store the calculated plane height.
 */
void ldePlaneDimensionsFromConfig(const LdeGlobalConfig* globalConfig, LdeLOQIndex loq,
                                  uint16_t planeIdx, uint16_t* width, uint16_t* height);

/*! \brief Get the width and height of a tile from the global config for a given loq, YUV plane
 *         index and tile index
 *
 * \param[in]  globalConfig  Config to reference
 * \param[in]  loq           The LOQ in calculate for
 * \param[in]  planeIdx      The plane to calculate for
 * \param[in]  tileIdx       The tile to calculate for
 * \param[out] width         The location to store the calculated tile width.
 * \param[out] height        The location to store the calculated tile height.
 */
void ldeTileDimensionsFromConfig(const LdeGlobalConfig* globalConfig, LdeLOQIndex loq, uint16_t planeIdx,
                                 uint16_t tileIdx, uint16_t* width, uint16_t* height);

/*! \brief Get the width and height of a tile from the global config for a given loq, YUV plane
 *         index and tile index
 *
 * \param[in]  globalConfig  Config to reference
 * \param[in]  loq           The LOQ in calculate for
 * \param[in]  planeIdx      The plane to calculate for
 * \param[in]  tileIdx       The tile to calculate for
 * \param[out] startX        The location to store the tile start X pixel coordinate
 * \param[out] startY        The location to store the tile start y pixel coordinate
 */
void ldeTileStartFromConfig(const LdeGlobalConfig* globalConfig, LdeLOQIndex loq, uint16_t planeIdx,
                            uint16_t tileIdx, uint16_t* startX, uint16_t* startY);

/*! \brief Retrieves the total number of tiles in a frame across all LOQs and planes
 *
 * \param[in]  globalConfig  Config to reference
 *
 * \return Number of tiles
 */
uint32_t ldeTotalNumTiles(const LdeGlobalConfig* globalConfig);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_ENHANCEMENT_DIMENSIONS_H
