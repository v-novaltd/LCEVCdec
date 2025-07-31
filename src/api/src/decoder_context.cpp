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

#include "decoder_context.h"
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/common_configuration.h>
#include <LCEVC/common/configure.hpp>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>

#if VN_SDK_STATIC
#if VN_SDK_PIPELINE(CPU)
#include <LCEVC/pipeline_cpu/create_pipeline.h>
#endif
#if VN_SDK_PIPELINE(VULKAN)
#include <LCEVC/pipeline_vulkan/create_pipeline.h>
#endif
#if VN_SDK_PIPELINE(LEGACY)
#include <LCEVC/pipeline_legacy/create_pipeline.h>
#endif
#else
#include <LCEVC/common/shared_library.h>
#endif

#include <algorithm>
#include <memory>
//
#include "event_dispatcher.h"
#include "pool.h"

namespace lcevc_dec::decoder {

// - Decoder Pool -----------------------------------------------------------------------------
//
// The pool holds the decoder contexts, alongside the implementations needed for events and handles.
// The Decoder is then given an interface for event generation.
//
// Default-initialize the singleton (16 should be plenty).
//
namespace {
    Pool<DecoderContext> decoderPool(16); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    std::mutex decoderPoolMutex; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

// Locked add to decoder pool - take ownership of the pointer.
//
Handle<DecoderContext> DecoderContext::decoderPoolAdd(std::unique_ptr<DecoderContext>&& ptr)
{
    std::scoped_lock lock(decoderPoolMutex);
    return decoderPool.add(std::move(ptr));
}

// Locked remove to decoder pool - ownership is passed back to caller.
// The pool entry is removed before the lock is released, to prevent duplicate access
//
std::unique_ptr<DecoderContext> DecoderContext::decoderPoolRemove(Handle<DecoderContext> handle)
{
    std::scoped_lock lock(decoderPoolMutex);
    std::unique_ptr<DecoderContext> dp{decoderPool.remove(handle)};
    return dp;
}

DecoderContext* DecoderContext::lookupDecoder(Handle<DecoderContext> handle)
{
    return decoderPool.lookup(handle);
}

// A scoped lock on a decoder from the pool
//
LockedDecoder::LockedDecoder(Handle<DecoderContext> handle)
{
    std::scoped_lock lock(decoderPoolMutex);
    m_context = decoderPool.lookup(handle); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    if (m_context) {
        m_context->lock();
    }
}

LockedDecoder::~LockedDecoder()
{
    if (m_context) {
        m_context->unlock();
    }
}

//
DecoderContext::DecoderContext()
    : m_commonConfiguration{common::getCommonConfiguration()}
    , m_eventDispatcher{createEventDispatcher(this)}
{}

//
DecoderContext::~DecoderContext() { VNLogVerbose("DecodeContext destroyed"); }

//
void DecoderContext::releasePools()
{
    // Clear out any allocated picture locks
    for (;;) {
        Handle<LdpPictureLock> plh = m_pictureLockPool.at(0);
        if (plh == kInvalidHandle) {
            break;
        }
        VNLogVerbose("Unreleased PictureLock: %08x.", plh.handle);
        LdpPictureLock* picLock{m_pictureLockPool.remove(plh)};
        assert(picLock);
        ldpPictureUnlock(picLock->picture, picLock);
    }

    // Clear out any allocated pictures
    for (;;) {
        Handle<LdpPicture> ph = m_picturePool.at(0);
        if (ph == kInvalidHandle) {
            break;
        }
        VNLogVerbose("Unreleased Picture: %08x. size:%zu", ph.handle, m_picturePool.size());
        LdpPicture* picture{m_picturePool.remove(ph)};
        assert(picture);
        pipeline()->freePicture(picture);
    }
}

//
pipeline::PipelineBuilder* DecoderContext::pipelineBuilder()
{
    // If something needs the builder, and it has not been created yet - construct it given
    // configured name.
    pipeline::PipelineBuilder* pb = nullptr;

    if (!m_pipelineBuilder) {
#if VN_SDK_STATIC
#if VN_SDK_PIPELINE(CPU)
        if (m_pipelineName == "cpu") {
            pb = createPipelineBuilderCPU(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
        }
#endif
#if VN_SDK_PIPELINE(VULKAN)
        if (m_pipelineName == "vulkan") {
            pb = createPipelineBuilderVulkan(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
        }
#endif
#if VN_SDK_PIPELINE(LEGACY)
        if (m_pipelineName == "legacy") {
            pb = createPipelineBuilderLegacy(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
        }
#endif
#else
        std::string libraryName = "lcevc_dec_pipeline_";
        libraryName.append(m_pipelineName);
        m_pipelineLibrary = ldcSharedLibraryFind(libraryName.c_str());
        if (!m_pipelineLibrary) {
            VNLogErrorF("Cannot load %s pipeline shared library", m_pipelineName.c_str());
            return nullptr;
        }

        CreatePipelineBuilderFn builderFn = (CreatePipelineBuilderFn)ldcSharedLibrarySymbol(
            m_pipelineLibrary, "createPipelineBuilder");

        pb = builderFn(ldcDiagnosticsStateGet(), (void*)ldcAccelerationGet());
#endif
        assert(pb);

        m_pipelineBuilder = std::unique_ptr<pipeline::PipelineBuilder>(pb);
    }

    return m_pipelineBuilder.get();
}

//
bool DecoderContext::initialize()
{
    pipeline::PipelineBuilder* builder = pipelineBuilder();
    assert(builder);

    assert(!m_pipeline);

    m_pipeline = builder->finish(m_eventDispatcher.get());
    if (!m_pipeline) {
        return false;
    }

    m_pipelineBuilder.reset();
    return true;
}

// Configuration
//
// General pattern is:
//
// 1) Try common (global) config
// 2) Try Context specific config (pipeline and events)
// 3) Try pipeline builder config
//
bool DecoderContext::configure(std::string_view name, const std::string& val)
{
    if (m_commonConfiguration->configure(name, val)) {
        return true;
    }

    if (name == "pipeline") {
        m_pipelineName = val;
        // Nuke any existing pipeline
        if (m_pipelineBuilder) {
            VNLogWarning("Changing pipeline: configuration may be lost.");
            m_pipelineBuilder.reset();
        }
        return true;
    }

    return pipelineBuilder()->configure(name, val);
}

bool DecoderContext::configure(std::string_view name, const std::vector<int32_t>& arr)
{
    if (m_commonConfiguration->configure(name, arr)) {
        return true;
    }

    if (name == "events") {
        m_eventDispatcher->enableEvents(arr);
        return true;
    }

    return pipelineBuilder()->configure(name, arr);
}

bool DecoderContext::configure(std::string_view name, bool val)
{
    return m_commonConfiguration->configure(name, val) || pipelineBuilder()->configure(name, val);
}

bool DecoderContext::configure(std::string_view name, int32_t val)
{
    return m_commonConfiguration->configure(name, val) || pipelineBuilder()->configure(name, val);
}

bool DecoderContext::configure(std::string_view name, float val)
{
    return m_commonConfiguration->configure(name, val) || pipelineBuilder()->configure(name, val);
}

bool DecoderContext::configure(std::string_view name, const std::vector<bool>& arr)
{
    return m_commonConfiguration->configure(name, arr) || pipelineBuilder()->configure(name, arr);
}

bool DecoderContext::configure(std::string_view name, const std::vector<float>& arr)
{
    return m_commonConfiguration->configure(name, arr) || pipelineBuilder()->configure(name, arr);
}

bool DecoderContext::configure(std::string_view name, const std::vector<std::string>& arr)
{
    return m_commonConfiguration->configure(name, arr) || pipelineBuilder()->configure(name, arr);
}

} // namespace lcevc_dec::decoder