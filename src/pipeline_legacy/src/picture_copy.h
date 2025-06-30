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

// Various functions to aid copying from one Picture to another.
//
#ifndef VN_LCEVC_PIPELINE_LEGACY_PICTURE_COPY_H
#define VN_LCEVC_PIPELINE_LEGACY_PICTURE_COPY_H

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

class Picture;

//! Copy src (an NV12 picture) to dest (an I420 picture).
//!
void copyNV12ToI420Picture(const Picture& src, Picture& dest);

//! Copy src to dest without any conversion. Suitable when the destination has exactly the same
//! format as the source (e.g. two 8bit NV12 images, two 10bit I420 images, etc.). Src and dest CAN
//! have different resolutions; the copied region will be the minimum shared width/height.
//!
void copyPictureToPicture(const Picture& src, Picture& dest);

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_PIPELINE_LEGACY_PICTURE_COPY_H
