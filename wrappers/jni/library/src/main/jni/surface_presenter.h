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

// SurfacePresenter - presents AHardwareBuffers to an ANativeWindow using
// SurfaceControl transactions (API 29+). Used for the zero-copy render path:
// decode output pictures are backed by pool buffers acquired here, and
// presented without any pixel copy.

#ifndef LCEVC_DEC_JNI_SURFACE_PRESENTER_H
#define LCEVC_DEC_JNI_SURFACE_PRESENTER_H

#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <android/surface_control.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

class SurfacePresenter
{
public:
    SurfacePresenter() = default;
    ~SurfacePresenter();

    SurfacePresenter(const SurfacePresenter&) = delete;
    SurfacePresenter& operator=(const SurfacePresenter&) = delete;

    // True when the device supports SurfaceControl presentation (API 29+)
    static bool isSupported();

    // Attach to (or detach from, with nullptr) the output window. Acquires a
    // reference on the window and creates a child SurfaceControl for the video.
    bool setWindow(ANativeWindow* window);

    // True once a window has been attached
    bool ready();

    // Get a buffer of the given size from the pool, allocating if needed.
    // Returns nullptr when the pool limit is reached (caller should fall back
    // to a copied render for this frame).
    AHardwareBuffer* acquireBuffer(uint32_t width, uint32_t height);

    // Return an unpresented buffer to the pool
    void recycleBuffer(AHardwareBuffer* buffer);

    // Present a buffer. The buffer returns to the pool automatically once a
    // later transaction replaces it on screen.
    bool presentBuffer(AHardwareBuffer* buffer, uint32_t frameWidth, uint32_t frameHeight);

private:
    // Travels with each presented buffer in the transaction completion callback
    struct PresentContext
    {
        SurfacePresenter* self;
        AHardwareBuffer* buffer;
    };

    static void onTransactionComplete(void* context, ASurfaceTransactionStats* stats);
    static void onShutdownComplete(void* context, ASurfaceTransactionStats* stats);

    AHardwareBuffer* allocateBufferLocked(uint32_t width, uint32_t height);
    uint64_t chooseBufferUsageLocked();
    void applyGeometryLocked(ASurfaceTransaction* transaction, uint32_t frameWidth, uint32_t frameHeight);
    void destroySurfaceControlLocked(std::unique_lock<std::mutex>& lock);
    void releaseBuffersLocked();

    std::mutex m_mutex;
    std::condition_variable m_shutdownCv;
    bool m_shutdownComplete = false;

    ANativeWindow* m_window = nullptr;
    ASurfaceControl* m_surfaceControl = nullptr;

    // Buffer pool
    std::vector<AHardwareBuffer*> m_freeBuffers;
    uint32_t m_allocatedCount = 0;

    // Usage flags for pool buffers, chosen by a one-off device probe (0 = not yet probed)
    uint64_t m_bufferUsage = 0;

    // Buffer currently on screen (returned to the pool by the next completion)
    AHardwareBuffer* m_presentedBuffer = nullptr;

    // Geometry state - reapplied when the frame or window size changes
    uint32_t m_lastFrameWidth = 0;
    uint32_t m_lastFrameHeight = 0;
    uint32_t m_lastWindowWidth = 0;
    uint32_t m_lastWindowHeight = 0;
};

#endif // LCEVC_DEC_JNI_SURFACE_PRESENTER_H
