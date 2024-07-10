/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

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
        assert(false);
    }
    return m_decoderMutexes[handleIndex(handle)];
}

} // namespace lcevc_dec::decoder
