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

#ifndef VN_LCEVC_API_DECODER_CONTEXT_H
#define VN_LCEVC_API_DECODER_CONTEXT_H

#include "accel_context.h"
#include "handle.h"
#include "pool.h"
//
#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/configure.hpp>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/pipeline.h>
//
#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>

namespace lcevc_dec::decoder {

// Assume that we will need not-very-many accel contexts. We may need a surprisingly large amount
// of pictures though (enough to max out the unprocessed, temporary/pending, and processed queues).
static const size_t kAccelContextPoolCapacity = 16;
static const size_t kPicturePoolCapacity = 1024;
static const size_t kPictureLockPoolCapacity = kPicturePoolCapacity;

class AccelContext;
class Decoder;
class EventDispatcher;
class PictureLock;

// The collection of state needed to implement the API
//
class DecoderContext : public common::Configurable
{
public:
    DecoderContext();
    ~DecoderContext();
    // Decoder pool management (static)
    static Handle<DecoderContext> decoderPoolAdd(std::unique_ptr<DecoderContext>&& ptr);
    static std::unique_ptr<DecoderContext> decoderPoolRemove(Handle<DecoderContext> handle);
    static DecoderContext* lookupDecoder(Handle<DecoderContext> handle);

    void releasePools();

    // common::Configurable
    // NB:: Need to implement all configure() virtual functions to handle C++ function lookup
    bool configure(std::string_view name, bool val) override;
    bool configure(std::string_view name, int32_t val) override;
    bool configure(std::string_view name, float val) override;
    bool configure(std::string_view name, const std::string& val) override;

    bool configure(std::string_view name, const std::vector<bool>& arr) override;
    bool configure(std::string_view name, const std::vector<int32_t>& arr) override;
    bool configure(std::string_view name, const std::vector<float>& arr) override;
    bool configure(std::string_view name, const std::vector<std::string>& arr) override;

    // Accessors to state for API
    void lock() { m_mtx.lock(); }
    void unlock() { m_mtx.unlock(); }

    LCEVC_DecoderHandle handle() const { return m_handle; }

    void handleSet(LCEVC_DecoderHandle handle) { m_handle = handle; }

    EventDispatcher* eventDispatcher() { return m_eventDispatcher.get(); }
    const EventDispatcher* eventDispatcher() const { return m_eventDispatcher.get(); }

    pipeline::PipelineBuilder* pipelineBuilder();
    bool isPipelineBuilderValid() { return !!m_pipelineBuilder; }

    pipeline::Pipeline* pipeline() const { return m_pipeline.get(); }
    bool isPipelineValid() const { return !!m_pipeline; }

    const Pool<LdpPicture>& picturePool() const { return m_picturePool; }
    Pool<LdpPicture>& picturePool() { return m_picturePool; }

    const Pool<LdpPictureLock>& pictureLockPool() const { return m_pictureLockPool; }
    Pool<LdpPictureLock>& pictureLockPool() { return m_pictureLockPool; }

    // Convert pipelineBuilder into pipeline
    bool initialize();

    VNNoCopyNoMove(DecoderContext);

private:
    // State
    std::mutex m_mtx;

    // A copy of the external handle for this decoder
    LCEVC_DecoderHandle m_handle{kInvalidHandle};

    // The pipeline to use
    std::string m_pipelineName = "cpu";

    // The application-wide (logging/simd etc.) managed by 'common'
    common::Configurable* m_commonConfiguration;

    // Event management
    std::unique_ptr<EventDispatcher> m_eventDispatcher;

    // The underlying pipeline
    std::unique_ptr<pipeline::PipelineBuilder> m_pipelineBuilder;
    std::unique_ptr<pipeline::Pipeline> m_pipeline;

    Pool<AccelContext> m_accelContextPool{kAccelContextPoolCapacity};
    Pool<LdpPictureLock> m_pictureLockPool{kPictureLockPoolCapacity};
    Pool<LdpPicture> m_picturePool{kPicturePoolCapacity};
};

// A scoped lock on a decoder from the pool
//
class LockedDecoder
{
public:
    explicit LockedDecoder(Handle<DecoderContext> handle);
    ~LockedDecoder();
    DecoderContext* context() const { return m_context; }

    VNNoCopyNoMove(LockedDecoder);

private:
    DecoderContext* m_context = nullptr;
};

// Invoke callable with a locked decoder context - returns result
//
template <typename F>
static inline LCEVC_ReturnCode withLockedDecoder(const Handle<DecoderContext>& decHandle, F fn,
                                                 bool shouldBeInitialised = true)
{
    if (decHandle == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    LockedDecoder lockedDecoder(decHandle);

    if (lockedDecoder.context() == nullptr) {
        // If it's null and we expect it to be initialized, report the error as "uninitialized",
        // although a more accurate error would really be "uncreated".
        return (shouldBeInitialised ? LCEVC_Uninitialized : LCEVC_InvalidParam);
    }

    if (shouldBeInitialised) {
        if (!lockedDecoder.context()->isPipelineValid()) {
            return LCEVC_Uninitialized;
        }
    } else {
        if (lockedDecoder.context()->isPipelineValid()) {
            return LCEVC_Initialized;
        }
    }

    return fn(lockedDecoder.context());
}

template <typename F>
static inline LCEVC_ReturnCode withLockedUninitializedDecoder(const Handle<DecoderContext>& decHandle, F fn)
{
    return withLockedDecoder(decHandle, fn, false);
}

// As above, but also unwrap a picture handle and pass it
//
template <typename F>
static inline LCEVC_ReturnCode withLockedDecoderAndPicture(const Handle<DecoderContext>& decHandle,
                                                           const Handle<LdpPicture>& picHandle, F fn)
{
    if (decHandle == kInvalidHandle || picHandle == kInvalidHandle) {
        return LCEVC_InvalidParam;
    }

    LockedDecoder lockedDecoder(decHandle);

    if (lockedDecoder.context() == nullptr) {
        return LCEVC_Uninitialized;
    }

    if (!lockedDecoder.context()->isPipelineValid()) {
        return LCEVC_Uninitialized;
    }

    LdpPicture* ldpPicture = lockedDecoder.context()->picturePool().lookup(picHandle);
    if (ldpPicture == nullptr) {
        return LCEVC_InvalidParam;
    }

    return fn(lockedDecoder.context(), ldpPicture);
}

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_API_DECODER_CONTEXT_H
