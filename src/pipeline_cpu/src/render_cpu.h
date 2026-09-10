/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#ifndef VN_LCEVC_PIPELINE_CPU_RENDER_CPU_H
#define VN_LCEVC_PIPELINE_CPU_RENDER_CPU_H

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/return_code.h>
#include <LCEVC/pipeline/picture_base.h>

#if VN_OS(ANDROID)
struct ANativeWindow;
#endif

namespace lcevc_dec::pipeline_cpu {

// RenderCPU
//
// CPU-side presentation of decoded pictures to a platform window.
//
// On Android this writes decoded picture data directly into the SurfaceFlinger
// buffer queue of an ANativeWindow via ANativeWindow_lock/unlockAndPost.
// On other platforms rendering is not supported.
//
class RenderCPU
{
public:
    RenderCPU() = default;
    ~RenderCPU();

    // True on platforms where the CPU pipeline can render
    static bool isSupported();

    // Set (or clear, with nullptr) the destination window. The window is an
    // ANativeWindow* on Android.
    LdcReturnCode setWindow(void* window);

    // Copy the picture's pixel data into the window's next buffer ready for composition.
    LdcReturnCode sendPicture(pipeline::PictureBase* picture);

    VNNoCopyNoMove(RenderCPU);

private:
#if VN_OS(ANDROID)
    ANativeWindow* m_window = nullptr;

    // Geometry currently configured on the window's buffer queue
    uint32_t m_configuredWidth = 0;
    uint32_t m_configuredHeight = 0;
    bool m_useRgbaFallback = false;
#endif
};

} // namespace lcevc_dec::pipeline_cpu

#endif // VN_LCEVC_PIPELINE_CPU_RENDER_CPU_H
