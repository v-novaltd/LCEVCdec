/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "decoder_pool.h"

#include "decoder.h"

namespace lcevc_dec::decoder {

// Default-initialise the singleton (16 should be plenty).
DecoderPool DecoderPool::instance(16); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

DecoderPool::DecoderPool(size_t capacity)
    : BaseClass(capacity)
    , m_decoderMutexes(capacity)
{}

Handle<Decoder> DecoderPool::allocate(std::unique_ptr<Decoder>&& ptrToDecoder)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return BaseClass::allocate(std::move(ptrToDecoder));
}

std::mutex& DecoderPool::lookupMutex(Handle<Decoder> handle)
{
    if (!isValid(handle)) {
        VNAssert(false);
    }
    return m_decoderMutexes[handleIndex(handle)];
}

} // namespace lcevc_dec::decoder
