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

#ifndef VN_LCEVC_ENHANCEMENT_APPROXIMATE_PA_H
#define VN_LCEVC_ENHANCEMENT_APPROXIMATE_PA_H

#include <LCEVC/enhancement/config_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief Some residual apply mechanisms don't have the required performance to perform the
 *         predicted average transform as a separate step after upscaling. Calling this function
 *         will modify the `globalConfig.kernel` to be closer to the correct predicted average
 *         result after a single upscaling pass. Many modes will not require modification of the
 *         kernel, checking `globalConfig.kernel.approximatedPA` will be true if this kernel was
 *         set. Call `ldeConfigsParse` to populate the globalConfig prior to using this function.
 *         Do not use this function in conjunction with the standard apply workflows in LCEVCdec,
 *         this is for specific low-performance apply environments and is not bit-accurate to the
 *         LCEVC MPEG-5 Part 2 specification. Also known as a 'pre-baked kernel'.
 *
 * \param[in]     globalConfig      Pointer to global config
 *
 * \return True on success, otherwise false.
 */
bool ldeApproximatePA(LdeGlobalConfig* globalConfig);

#ifdef __cplusplus
}
#endif
#endif // VN_LCEVC_ENHANCEMENT_APPROXIMATE_PA_H
