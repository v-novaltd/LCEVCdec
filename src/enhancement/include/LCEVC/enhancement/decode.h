/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#ifndef VN_LCEVC_ENHANCEMENT_DECODE_H
#define VN_LCEVC_ENHANCEMENT_DECODE_H

#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <LCEVC/enhancement/config_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief Decodes a single loq-plane-tile from a given global config and frame config to a
 *         cmdbuffer. Where the global config and frame config have been deserialized by the config
 *         parser/pool. Chunked encoded data is contained within the frame config chunks. Output
 *         residuals are either to CPU or GPU cmdBuffers - if using CPU cmdBuffers provide nullptrs
 *         to cmdBufferGpu and cmdBufferBuilder, if using GPU cmdBuffers provide a nullptr to
 *         cmdBufferCpu.
 *
 * \param[in]     globalConfig      Pointer to global config
 * \param[in]     frameConfig       Pointer to frame config - valid within the global config
 * \param[in]     loq               LOQ to decode - either LOQ1 or LOQ0
 * \param[in]     planeIdx          Plane to decode - 0 for Y or 0, 1 & 2 for YUV chroma residuals
 * \param[in]     tileIdx           Tile to decode within the frame if using a tiled stream else 0
 * \param[out]    cmdBufferCpu      Pointer to an initialized and reset CPU cmdBuffer or nullptr
 * \param[out]    cmdBufferGpu      Pointer to an initialized and reset GPU cmdBuffer or nullptr
 * \param[in]     cmdBufferBuilder  Pointer to an initialized and reset GPU cmdBuffer builder or nullptr
 *
 * \return True on success, otherwise false.
 */
bool ldeDecodeEnhancement(const LdeGlobalConfig* globalConfig, const LdeFrameConfig* frameConfig,
                          const LdeLOQIndex loq, const uint32_t planeIdx, const uint32_t tileIdx,
                          LdeCmdBufferCpu* cmdBufferCpu, LdeCmdBufferGpu* cmdBufferGpu,
                          LdeCmdBufferGpuBuilder* cmdBufferBuilder);

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_ENHANCEMENT_DECODE_H
