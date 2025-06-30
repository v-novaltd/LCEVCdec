/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#ifndef VN_LCEVC_ENHANCEMENT_CONFIG_POOL_H
#define VN_LCEVC_ENHANCEMENT_CONFIG_POOL_H

#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/config_types.h>
//
#include <LCEVC/common/memory.h>
#include <LCEVC/common/vector.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! Config pool structure containing the memory for all configs in the pool */
typedef struct LdeConfigPool
{
    LdcMemoryAllocator* allocator;
    LdcVector globalConfigs;             /**< Active global configs of in-flight frames */
    LdeGlobalConfig* latestGlobalConfig; /**< Most recent global config, the last element in the vector */
    LdeQuantMatrix quantMatrix; /**< State between frames in the LdeFrameConfig, this parameter is used to hold the latest */
    bool ditherEnabled; /**< State between frames in the LdeFrameConfig, holds the last dither enabled state - other dithering params are then parsed */
} LdeConfigPool;

/*! \brief Initializes sizes and allocations of the config pool
 *
 * \param[in]     allocator        Memory allocator
 * \param[in]     configPool       Fresh config pool
 * \param[in]     bitstreamVersion Initialize the global configs with a specific version, use BitstreamVersionUnspecified for auto detection
 */
void ldeConfigPoolInitialize(LdcMemoryAllocator* allocator, LdeConfigPool* configPool,
                             LdeBitstreamVersion bitstreamVersion);

/*! \brief Releases all memory associated with the config pool
 *
 * \param[in]     configPool     Initialized config pool
 */
void ldeConfigPoolRelease(LdeConfigPool* configPool);

/*! \brief Inserts new serialized data into the config pool with a given timestamp, deserializes
 *         the unprocessed data and returns pointers to a frame config and a global config.
 *         Importantly data must be in timestamp order - this is not checked at this stage, use the
 *         sequencer module for out of order serialized packets. Global configs are internally
 *         re-used until a change is detected. If a duplicate and unreleased timestamp is given,
 *         pre-processed configs will be returned.
 *
 * \param[in]     configPool     Initialized config pool
 * \param[in]     timestamp      Timestamp of new frame
 * \param[in]     serialized     Pointer to serialized frame
 * \param[in]     serializedSize Size of serialized frame
 *                               be parsed when the LCEVC data is unencapsulated from the NAL unit.
 * \param[out]    globalConfig   Output pointer to global config
 * \param[out]    frameConfig    Output pointer to frame config
 *
 * \return True on success, otherwise false
 */
bool ldeConfigPoolFrameInsert(LdeConfigPool* configPool, uint64_t timestamp,
                              const uint8_t* serialized, size_t serializedSize,
                              LdeGlobalConfig** globalConfig, LdeFrameConfig* frameConfig);

/*! \brief Release a frame from the pool by its timestamp. Removes the frame config from the pool
 *         and the global config if it is not in use by other unreleased frames
 *
 * \param[in]     configPool     Initialized config pool
 * \param[in]     frameConfig    The frame config pointer passed to a previous ldeConfigPoolFrameInsert() call
 * \param[in]     globalConfig   The global config pointer returned by a previous ldeConfigPoolFrameInsert() call
 *
 * \return True on success, otherwise false
 */
bool ldeConfigPoolFrameRelease(LdeConfigPool* configPool, LdeFrameConfig* frameConfig,
                               LdeGlobalConfig* globalConfig);

/*! \brief Fill in the global and frame config for a frame that is being passed through the decoder.
 *         The current global config will be referenced, and the frame config will be the default.
 *
 * \param[in]     configPool     Initialized config pool
 * \param[out]    globalConfig   Output pointer to global config
 * \param[out]    frameConfig    Output pointer to frame config
 */
void ldeConfigPoolFramePassthrough(LdeConfigPool* configPool, LdeGlobalConfig** globalConfig,
                                   LdeFrameConfig* frameConfig);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_ENHANCEMENT_CONFIG_POOL_H
