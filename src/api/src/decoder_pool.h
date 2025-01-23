/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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
