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

#include "render_cpu.h"

#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/picture_layout.h>

#include <algorithm>
#include <cstring>

#if VN_OS(ANDROID)
#include <android/native_window.h>
#endif

namespace lcevc_dec::pipeline_cpu {

#if VN_OS(ANDROID)

// HAL_PIXEL_FORMAT_YCrCb_420_SP (NV21) - the legacy semi-planar 4:2:0 format that
// gralloc supports for CPU-written ANativeWindow buffers. Not exposed by the NDK
// headers, so defined here.
static constexpr int32_t kWindowFormatYCrCb420SP = 0x11;

struct YuvToRgbCoefficients
{
    int32_t yOffset;
    int32_t y;
    int32_t rv;
    int32_t gu;
    int32_t gv;
    int32_t bu;
};

uint8_t clampToByte(int32_t value) { return static_cast<uint8_t>(std::clamp(value, 0, 255)); }

YuvToRgbCoefficients getYuvToRgbCoefficients(const pipeline::PictureBase& picture, uint32_t width)
{
    const bool fullRange = picture.colorRange == LdpColorRangeFull;
    // Matrix metadata is commonly unspecified for MediaCodec output. Follow the conventional
    // Android player heuristic of treating HD video as BT.709 and SD video as BT.601.
    const bool bt709 = picture.matrixCoefficients == LdpMatrixCoefficientsBT709 ||
                       (picture.matrixCoefficients == LdpMatrixCoefficientsUnspecified && width >= 1280);

    if (fullRange) {
        return bt709 ? YuvToRgbCoefficients{0, 256, 403, -48, -120, 475}
                     : YuvToRgbCoefficients{0, 256, 359, -88, -183, 454};
    }
    return bt709 ? YuvToRgbCoefficients{16, 298, 459, -55, -136, 541}
                 : YuvToRgbCoefficients{16, 298, 409, -100, -208, 516};
}

static inline void writeRgbaPixel(uint8_t* dst, uint8_t y, uint8_t u, uint8_t v,
                                  const YuvToRgbCoefficients& coefficients)
{
    const int32_t luma = std::max(0, static_cast<int32_t>(y) - coefficients.yOffset);
    const int32_t cb = static_cast<int32_t>(u) - 128;
    const int32_t cr = static_cast<int32_t>(v) - 128;
    dst[0] = clampToByte((coefficients.y * luma + coefficients.rv * cr + 128) >> 8);
    dst[1] =
        clampToByte((coefficients.y * luma + coefficients.gu * cb + coefficients.gv * cr + 128) >> 8);
    dst[2] = clampToByte((coefficients.y * luma + coefficients.bu * cb + 128) >> 8);
    dst[3] = 255;
}

RenderCPU::~RenderCPU()
{
    if (m_window != nullptr) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }
}

bool RenderCPU::isSupported() { return true; }

LdcReturnCode RenderCPU::setWindow(void* window)
{
    ANativeWindow* newWindow = static_cast<ANativeWindow*>(window);
    if (newWindow == m_window) {
        return LdcReturnCodeSuccess;
    }

    if (newWindow != nullptr) {
        ANativeWindow_acquire(newWindow);
    }
    if (m_window != nullptr) {
        ANativeWindow_release(m_window);
    }
    m_window = newWindow;

    // Force the buffer geometry to be (re)configured on the next sendPicture()
    m_configuredWidth = 0;
    m_configuredHeight = 0;
    m_useRgbaFallback = false;

    VNLogDebug("RenderCPU: window set to %p", window);
    return LdcReturnCodeSuccess;
}

LdcReturnCode RenderCPU::sendPicture(pipeline::PictureBase* picture)
{
    if (picture == nullptr) {
        return LdcReturnCodeInvalidParam;
    }
    if (m_window == nullptr) {
        VNLogDebug("RenderCPU: no window set yet");
        return LdcReturnCodeAgain;
    }

    const LdpColorFormat format = ldpPictureLayoutFormat(&picture->layout);
    if (format != LdpColorFormatNV12_8 && format != LdpColorFormatI420_8) {
        VNLogError("RenderCPU: unsupported output format %d - only 8-bit NV12/I420 can be "
                   "rendered",
                   static_cast<int>(format));
        return LdcReturnCodeNotSupported;
    }

    const uint32_t width = picture->getActiveWidth();
    const uint32_t height = picture->getActiveHeight();

    if (width != m_configuredWidth || height != m_configuredHeight) {
        const int32_t windowFormat = m_useRgbaFallback ? WINDOW_FORMAT_RGBA_8888 : kWindowFormatYCrCb420SP;
        if (ANativeWindow_setBuffersGeometry(m_window, static_cast<int32_t>(width),
                                             static_cast<int32_t>(height), windowFormat) != 0) {
            VNLogError("RenderCPU: ANativeWindow_setBuffersGeometry(%ux%u) failed", width, height);
            return LdcReturnCodeError;
        }
        m_configuredWidth = width;
        m_configuredHeight = height;
        VNLogInfo("RenderCPU: configured window buffers to %ux%u %s", width, height,
                  m_useRgbaFallback ? "RGBA" : "semi-planar 4:2:0");
    }

    ANativeWindow_Buffer windowBuffer{};
    int lockResult = ANativeWindow_lock(m_window, &windowBuffer, nullptr);
    if (lockResult != 0 && !m_useRgbaFallback) {
        VNLogWarning("RenderCPU: YUV ANativeWindow_lock failed (%d), falling back to RGBA", lockResult);
        if (ANativeWindow_setBuffersGeometry(m_window, static_cast<int32_t>(width),
                                             static_cast<int32_t>(height), WINDOW_FORMAT_RGBA_8888) != 0) {
            VNLogError("RenderCPU: failed to configure RGBA fallback buffers");
            return LdcReturnCodeError;
        }
        m_useRgbaFallback = true;
        lockResult = ANativeWindow_lock(m_window, &windowBuffer, nullptr);
    }
    if (lockResult != 0) {
        VNLogError("RenderCPU: ANativeWindow_lock failed (%d)", lockResult);
        return LdcReturnCodeError;
    }

    const uint32_t copyWidth = std::min(width, static_cast<uint32_t>(windowBuffer.width));
    const uint32_t copyHeight = std::min(height, static_cast<uint32_t>(windowBuffer.height));

    LdpPicturePlaneDesc lumaPlane{};
    picture->getPlaneDescInternal(0, lumaPlane);

    LdpPicturePlaneDesc cbPlane{};
    LdpPicturePlaneDesc crPlane{};
    picture->getPlaneDescInternal(1, cbPlane);
    if (format == LdpColorFormatI420_8) {
        picture->getPlaneDescInternal(2, crPlane);
    }

    if (m_useRgbaFallback) {
        const YuvToRgbCoefficients coefficients = getYuvToRgbCoefficients(*picture, width);
        const size_t dstRowStride = static_cast<size_t>(windowBuffer.stride) * 4;
        auto* dstBase = static_cast<uint8_t*>(windowBuffer.bits);
        for (uint32_t row = 0; row < copyHeight; ++row) {
            const uint8_t* srcY = lumaPlane.firstSample + static_cast<size_t>(row) * lumaPlane.rowByteStride;
            const uint8_t* srcCb =
                cbPlane.firstSample + static_cast<size_t>(row / 2) * cbPlane.rowByteStride;
            const uint8_t* srcCr =
                format == LdpColorFormatI420_8
                    ? crPlane.firstSample + static_cast<size_t>(row / 2) * crPlane.rowByteStride
                    : nullptr;
            uint8_t* dstRow = dstBase + static_cast<size_t>(row) * dstRowStride;
            for (uint32_t x = 0; x < copyWidth; ++x) {
                const uint8_t u = format == LdpColorFormatNV12_8 ? srcCb[(x / 2) * 2] : srcCb[x / 2];
                const uint8_t v = format == LdpColorFormatNV12_8 ? srcCb[(x / 2) * 2 + 1] : srcCr[x / 2];
                writeRgbaPixel(dstRow + static_cast<size_t>(x) * 4, srcY[x], u, v, coefficients);
            }
        }
    } else {
        // 8-bit YUV, so the pixel stride is the byte stride.
        const uint32_t dstStride = static_cast<uint32_t>(windowBuffer.stride);
        uint8_t* dstLuma = static_cast<uint8_t*>(windowBuffer.bits);
        for (uint32_t row = 0; row < copyHeight; ++row) {
            memcpy(dstLuma + static_cast<size_t>(row) * dstStride,
                   lumaPlane.firstSample + static_cast<size_t>(row) * lumaPlane.rowByteStride, copyWidth);
        }

        // Semi-planar 4:2:0 gralloc layout: interleaved chroma follows the luma plane.
        uint8_t* dstChroma = dstLuma + static_cast<size_t>(dstStride) * windowBuffer.height;
        const uint32_t chromaHeight = copyHeight / 2;
        if (format == LdpColorFormatNV12_8) {
            // The window buffer is NV21 (Cr first), so swap each Cb/Cr pair of the
            // NV12 chroma plane while copying.
            for (uint32_t row = 0; row < chromaHeight; ++row) {
                const uint8_t* srcRow =
                    cbPlane.firstSample + static_cast<size_t>(row) * cbPlane.rowByteStride;
                uint8_t* dstRow = dstChroma + static_cast<size_t>(row) * dstStride;
                for (uint32_t x = 0; x + 1 < copyWidth; x += 2) {
                    dstRow[x] = srcRow[x + 1];
                    dstRow[x + 1] = srcRow[x];
                }
            }
        } else {
            // I420: interleave the planar chroma into the semi-planar destination,
            // Cr first to match the NV21 window format.
            const uint32_t chromaWidth = copyWidth / 2;
            for (uint32_t row = 0; row < chromaHeight; ++row) {
                const uint8_t* srcCb =
                    cbPlane.firstSample + static_cast<size_t>(row) * cbPlane.rowByteStride;
                const uint8_t* srcCr =
                    crPlane.firstSample + static_cast<size_t>(row) * crPlane.rowByteStride;
                uint8_t* dstRow = dstChroma + static_cast<size_t>(row) * dstStride;
                for (uint32_t x = 0; x < chromaWidth; ++x) {
                    dstRow[2 * x + 0] = srcCr[x];
                    dstRow[2 * x + 1] = srcCb[x];
                }
            }
        }
    }

    if (ANativeWindow_unlockAndPost(m_window) != 0) {
        VNLogError("RenderCPU: ANativeWindow_unlockAndPost failed");
        return LdcReturnCodeError;
    }

    return LdcReturnCodeSuccess;
}

#else // !VN_OS(ANDROID)

RenderCPU::~RenderCPU() = default;

bool RenderCPU::isSupported() { return false; }

LdcReturnCode RenderCPU::setWindow(void* window)
{
    VNUnused(window);
    return LdcReturnCodeNotSupported;
}

LdcReturnCode RenderCPU::sendPicture(pipeline::PictureBase* picture)
{
    VNUnused(picture);
    return LdcReturnCodeNotSupported;
}

#endif

} // namespace lcevc_dec::pipeline_cpu
