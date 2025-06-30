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

#ifndef VN_LCEVC_PIPELINE_LEGACY_UTILS_H
#define VN_LCEVC_PIPELINE_LEGACY_UTILS_H

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/pipeline/picture_layout.h>

#include <memory>
#include <utility>
#include <vector>

// Helper types and consts ------------------------------------------------------------------------

using EnhancementWithData = std::pair<const uint8_t*, uint32_t>;
using SmartBuffer = std::shared_ptr<std::vector<uint8_t>>;

constexpr uint32_t kI420NumPlanes = 3; // i.e. Y, U, and V

// Helpers for PictureExternal:

void setupPictureExternal(LdpPictureBufferDesc& bufferDescOut, SmartBuffer& bufferOut,
                          LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes],
                          LdpColorFormat format, uint32_t width, uint32_t height,
                          LdpAccelBuffer* accelBufferHandle, LdpAccess access);
#endif // VN_LCEVC_PIPELINE_LEGACY_UTILS_H
