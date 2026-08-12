/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#include <LCEVC/pipeline/buffer_base.h>
//
#include <LCEVC/pipeline/buffer_alignment.h>

#include <cassert>

namespace lcevc_dec::pipeline {

namespace {
    extern const LdpBufferFunctions kBufferBaseFunctions;
}

BufferBase::BufferBase()
    : LdpBuffer{&kBufferBaseFunctions}
{}

BufferBase::~BufferBase() {}

// C function table to connect to C++ class
//
namespace {
    bool map(struct LdpBuffer* buffer, LdpBufferMapping* mapping, int32_t offset, uint32_t size,
             LdpAccess access)
    {
        if (!buffer || !mapping) {
            return false;
        }
        return static_cast<BufferBase*>(buffer)->map(mapping, offset, size, access);
    }
    void unmap(struct LdpBuffer* buffer, const LdpBufferMapping* mapping)
    {
        if (!buffer || !mapping) {
            return;
        }
        static_cast<BufferBase*>(buffer)->unmap(mapping);
    }

    const LdpBufferFunctions kBufferBaseFunctions = {
        map,
        unmap,
    };
} // namespace

} // namespace lcevc_dec::pipeline
