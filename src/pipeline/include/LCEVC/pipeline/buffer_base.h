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

#ifndef VN_LCEVC_PIPELINE_BUFFER_BASE_H
#define VN_LCEVC_PIPELINE_BUFFER_BASE_H

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/memory.h>
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/picture_layout.h>

namespace lcevc_dec::pipeline {

// Buffer usage categories to tell pipeline how buffer will be used
enum BufferUsage
{
    BufferUsageNone = 0,

    BufferUsageIn,      // API writes, pipelines reads (e.g. base picture)
    BufferUsageOut,     // Pipeline writes, API reads (e.g. output picture)
    BufferUsageInternal // No API access (e.g.temporal, intermediate)
};

class BufferBase : public LdpBuffer
{
public:
    BufferBase();
    virtual ~BufferBase() = 0;

    virtual uint32_t size() const = 0;
    virtual BufferUsage usage() const = 0;

    // Return any picture layout that is implicit in the underlying buffer
    virtual const LdpPictureLayout* getPictureLayout() const { return nullptr; }

    virtual bool map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access) = 0;
    virtual void unmap(const LdpBufferMapping* mapping) = 0;

    virtual void clear() = 0;
    virtual void copyIn(uint32_t dstOffset, const void* src, uint32_t size) = 0;
    virtual void copyOut(void* dst, uint32_t srcOffset, uint32_t size) = 0;

    virtual bool resize(uint32_t size) = 0;

    VNNoCopyNoMove(BufferBase);
};

} // namespace lcevc_dec::pipeline

#endif // VN_LCEVC_PIPELINE_BUFFER_BASE_H
