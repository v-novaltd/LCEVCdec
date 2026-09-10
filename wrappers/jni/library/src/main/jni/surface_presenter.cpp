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

// weak_api29.h keeps the API 29 SurfaceControl/AHardwareBuffer_lockPlanes
// symbols weak regardless of the platform level this translation unit is
// compiled for, so the library still loads on older devices; isSupported()
// checks the symbols are present before this presenter is used.

#include "surface_presenter.h"

#include "weak_api29.h"

#include <android/log.h>

#include <algorithm>
#include <chrono>

constexpr const char* PRESENTER_TAG = "lcevc_dec_presenter";

#define PLOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, PRESENTER_TAG, __VA_ARGS__))
#define PLOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, PRESENTER_TAG, __VA_ARGS__))
#define PLOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, PRESENTER_TAG, __VA_ARGS__))
#define PLOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, PRESENTER_TAG, __VA_ARGS__))

// Enough buffers for the client's decode pipeline: up to 3 concurrent decodes,
// 6 queued decoded frames, one on screen and one in flight.
static constexpr uint32_t kMaxBuffers = 12;

bool SurfacePresenter::isSupported()
{
    return ASurfaceControl_createFromWindow != nullptr && ASurfaceTransaction_create != nullptr &&
           AHardwareBuffer_lockPlanes != nullptr;
}

SurfacePresenter::~SurfacePresenter()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    destroySurfaceControlLocked(lock);
    releaseBuffersLocked();
    if (m_presentedBuffer != nullptr) {
        AHardwareBuffer_release(m_presentedBuffer);
        m_presentedBuffer = nullptr;
    }
    if (m_window != nullptr) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }
}

bool SurfacePresenter::setWindow(ANativeWindow* window)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (window == m_window) {
        return true;
    }

    destroySurfaceControlLocked(lock);

    if (m_window != nullptr) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }

    if (window == nullptr) {
        return true;
    }

    ANativeWindow_acquire(window);
    m_window = window;
    m_surfaceControl = ASurfaceControl_createFromWindow(window, "LcevcVideo");
    if (m_surfaceControl == nullptr) {
        PLOGE("setWindow: ASurfaceControl_createFromWindow failed");
        ANativeWindow_release(m_window);
        m_window = nullptr;
        return false;
    }

    // Force geometry to be reapplied on the next present
    m_lastFrameWidth = 0;
    m_lastFrameHeight = 0;

    PLOGI("setWindow: attached to window %p", static_cast<void*>(window));
    return true;
}

bool SurfacePresenter::ready()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_surfaceControl != nullptr;
}

// Choose the pool buffer usage flags. Prefer COMPOSER_OVERLAY (lets
// SurfaceFlinger scan the buffer out directly), but some devices (e.g. Pixel 4a)
// return an opaque, non-CPU-lockable plane layout for overlay-usage YUV buffers -
// probe once and fall back to GPU composition there.
uint64_t SurfacePresenter::chooseBufferUsageLocked()
{
    const uint64_t base = AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    const uint64_t withOverlay = base | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY;

    const AHardwareBuffer_Desc probeDesc = {
        .width = 64,
        .height = 64,
        .layers = 1,
        .format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
        .usage = withOverlay,
    };

    AHardwareBuffer* probe = nullptr;
    if (AHardwareBuffer_allocate(&probeDesc, &probe) != 0) {
        PLOGW("chooseBufferUsage: overlay probe allocation failed, using GPU composition");
        return base;
    }

    AHardwareBuffer_Planes planes{};
    bool lockable = AHardwareBuffer_lockPlanes(probe, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                               nullptr, &planes) == 0;
    if (lockable) {
        lockable = planes.planeCount >= 3 && planes.planes[0].rowStride != 0 &&
                   planes.planes[1].pixelStride != 0;
        AHardwareBuffer_unlock(probe, nullptr);
    }
    AHardwareBuffer_release(probe);

    if (!lockable) {
        PLOGI("chooseBufferUsage: overlay-usage buffers are not CPU-lockable on this device, "
              "using GPU composition");
        return base;
    }
    return withOverlay;
}

AHardwareBuffer* SurfacePresenter::allocateBufferLocked(uint32_t width, uint32_t height)
{
    if (m_bufferUsage == 0) {
        m_bufferUsage = chooseBufferUsageLocked();
    }

    const AHardwareBuffer_Desc desc = {
        .width = width,
        .height = height,
        .layers = 1,
        .format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
        .usage = m_bufferUsage,
    };

    AHardwareBuffer* buffer = nullptr;
    if (const int ret = AHardwareBuffer_allocate(&desc, &buffer); ret != 0) {
        PLOGE("allocateBuffer: AHardwareBuffer_allocate %ux%u failed: %d", width, height, ret);
        return nullptr;
    }
    m_allocatedCount++;
    PLOGD("allocateBuffer: %ux%u YCbCr420 (%u allocated)", width, height, m_allocatedCount);
    return buffer;
}

AHardwareBuffer* SurfacePresenter::acquireBuffer(uint32_t width, uint32_t height)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Reuse a free buffer of matching size
    for (auto it = m_freeBuffers.begin(); it != m_freeBuffers.end(); ++it) {
        AHardwareBuffer_Desc desc{};
        AHardwareBuffer_describe(*it, &desc);
        if (desc.width == width && desc.height == height) {
            AHardwareBuffer* buffer = *it;
            m_freeBuffers.erase(it);
            return buffer;
        }
    }

    // Discard one stale-sized free buffer per acquire to let the pool adapt
    if (!m_freeBuffers.empty()) {
        AHardwareBuffer_release(m_freeBuffers.back());
        m_freeBuffers.pop_back();
        m_allocatedCount--;
    }

    if (m_allocatedCount >= kMaxBuffers) {
        PLOGW("acquireBuffer: pool exhausted (%u buffers)", m_allocatedCount);
        return nullptr;
    }

    return allocateBufferLocked(width, height);
}

void SurfacePresenter::recycleBuffer(AHardwareBuffer* buffer)
{
    if (buffer == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_freeBuffers.push_back(buffer);
}

// Called when a transaction's updates are on screen - the previously presented
// buffer is no longer scanned out and can be reused.
void SurfacePresenter::onTransactionComplete(void* context, ASurfaceTransactionStats* stats)
{
    (void)stats;
    auto* presentContext = static_cast<PresentContext*>(context);
    SurfacePresenter* self = presentContext->self;

    {
        std::lock_guard<std::mutex> lock(self->m_mutex);
        if (self->m_presentedBuffer != nullptr) {
            self->m_freeBuffers.push_back(self->m_presentedBuffer);
        }
        self->m_presentedBuffer = presentContext->buffer;
    }

    delete presentContext;
}

void SurfacePresenter::applyGeometryLocked(ASurfaceTransaction* transaction, uint32_t frameWidth,
                                           uint32_t frameHeight)
{
    const auto windowWidth = static_cast<uint32_t>(ANativeWindow_getWidth(m_window));
    const auto windowHeight = static_cast<uint32_t>(ANativeWindow_getHeight(m_window));

    if (frameWidth == m_lastFrameWidth && frameHeight == m_lastFrameHeight &&
        windowWidth == m_lastWindowWidth && windowHeight == m_lastWindowHeight) {
        return;
    }
    if (frameWidth == 0 || frameHeight == 0 || windowWidth == 0 || windowHeight == 0) {
        return;
    }

    m_lastFrameWidth = frameWidth;
    m_lastFrameHeight = frameHeight;
    m_lastWindowWidth = windowWidth;
    m_lastWindowHeight = windowHeight;

    // Scale to fit the window, centred (letterbox/pillarbox)
    const float scaleX = static_cast<float>(windowWidth) / static_cast<float>(frameWidth);
    const float scaleY = static_cast<float>(windowHeight) / static_cast<float>(frameHeight);
    const float scale = std::min(scaleX, scaleY);

    const auto drawWidth = static_cast<int32_t>(static_cast<float>(frameWidth) * scale);
    const auto drawHeight = static_cast<int32_t>(static_cast<float>(frameHeight) * scale);
    const int32_t left = (static_cast<int32_t>(windowWidth) - drawWidth) / 2;
    const int32_t top = (static_cast<int32_t>(windowHeight) - drawHeight) / 2;

    const ARect source = {0, 0, static_cast<int32_t>(frameWidth), static_cast<int32_t>(frameHeight)};
    const ARect destination = {left, top, left + drawWidth, top + drawHeight};
    ASurfaceTransaction_setGeometry(transaction, m_surfaceControl, source, destination, /*transform=*/0);

    PLOGI("applyGeometry: frame %ux%u window %ux%u scale %.3f dst (%d,%d %dx%d)", frameWidth,
          frameHeight, windowWidth, windowHeight, scale, left, top, drawWidth, drawHeight);
}

bool SurfacePresenter::presentBuffer(AHardwareBuffer* buffer, uint32_t frameWidth, uint32_t frameHeight)
{
    if (buffer == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_surfaceControl == nullptr) {
        PLOGW("presentBuffer: no surface control - dropping frame");
        m_freeBuffers.push_back(buffer);
        return false;
    }

    auto* presentContext = new PresentContext{this, buffer};

    ASurfaceTransaction* transaction = ASurfaceTransaction_create();
    ASurfaceTransaction_setBuffer(transaction, m_surfaceControl, buffer, /*acquire_fence_fd=*/-1);
    applyGeometryLocked(transaction, frameWidth, frameHeight);
    ASurfaceTransaction_setOnComplete(transaction, presentContext, onTransactionComplete);
    ASurfaceTransaction_apply(transaction);
    ASurfaceTransaction_delete(transaction);

    return true;
}

void SurfacePresenter::onShutdownComplete(void* context, ASurfaceTransactionStats* stats)
{
    (void)stats;
    auto* self = static_cast<SurfacePresenter*>(context);
    std::lock_guard<std::mutex> lock(self->m_mutex);
    self->m_shutdownComplete = true;
    self->m_shutdownCv.notify_all();
}

// Tear down the surface control, waiting for SurfaceFlinger to commit the
// removal so no completion callbacks arrive after destruction.
void SurfacePresenter::destroySurfaceControlLocked(std::unique_lock<std::mutex>& lock)
{
    if (m_surfaceControl == nullptr) {
        return;
    }

    m_shutdownComplete = false;
    {
        ASurfaceTransaction* transaction = ASurfaceTransaction_create();
        ASurfaceTransaction_reparent(transaction, m_surfaceControl, nullptr);
        ASurfaceTransaction_setOnComplete(transaction, this, onShutdownComplete);
        ASurfaceTransaction_apply(transaction);
        ASurfaceTransaction_delete(transaction);
    }

    if (!m_shutdownCv.wait_for(lock, std::chrono::milliseconds(500),
                               [this] { return m_shutdownComplete; })) {
        PLOGW("destroySurfaceControl: timeout waiting for teardown commit");
    }

    ASurfaceControl_release(m_surfaceControl);
    m_surfaceControl = nullptr;
}

void SurfacePresenter::releaseBuffersLocked()
{
    for (AHardwareBuffer* buffer : m_freeBuffers) {
        AHardwareBuffer_release(buffer);
    }
    m_freeBuffers.clear();
    m_allocatedCount = 0;
}
