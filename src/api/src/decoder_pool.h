/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_DECODER_POOL_H_
#define VN_API_DECODER_POOL_H_

#include "pool.h"

#include <mutex>
#include <vector>

namespace lcevc_dec::decoder {

class Decoder;

// Special singleton Pool for decoders. This is the only Pool which is a singleton. This is because
// pools own their objects, and every other pool is owned by the Decoder. Obviously, the Decoder
// can't own the thing that owns itself.
//
// The pool itself is protected by a mutex and also holds a mutex for each decoder (these, in turn,
// protected any other Pools).
//
// Note that clang-tidy outright rejects Singletons (or at least, it rejects non const global
// variables, which are a necessary part of Singletons)
class DecoderPool : public Pool<Decoder>
{
    using BaseClass = Pool<Decoder>;

public:
    static DecoderPool& get() { return instance; }

    Handle<Decoder> allocate(std::unique_ptr<Decoder>&& ptrToDecoder) override;

    std::mutex& lookupMutex(Handle<Decoder> handle);

private:
    explicit DecoderPool(size_t capacity);

    static DecoderPool instance; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    std::vector<std::mutex> m_decoderMutexes;
    mutable std::mutex m_mutex;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_DECODER_POOL_H_
