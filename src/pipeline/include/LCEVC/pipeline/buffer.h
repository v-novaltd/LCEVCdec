/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#ifndef VN_LCEVC_PIPELINE_BUFFER_H
#define VN_LCEVC_PIPELINE_BUFFER_H

#include <LCEVC/pipeline/types.h>
#include <stdint.h>

// NOLINTBEGIN(modernize-use-using)

typedef struct LdpBufferFunctions LdpBufferFunctions;

// Default minimum row alignment for internal allocations
#define kBufferRowAlignment 8

// Buffer
//

typedef struct LdpBuffer
{
    // Function pointer table
    const LdpBufferFunctions* functions;

    void* userData;
} LdpBuffer;

// Stored state of a mapped region of a buffer.
//
typedef struct LdpBufferMapping
{
    LdpBuffer* buffer; // Underlying buffer
    uint32_t offset;   // Offset within buffer in bytes
    uint32_t size;     // Size of mapped area in bytes
    uint8_t* ptr;      // Pointer to mapped area in bytes
    LdpAccess access;  // Access type

    void* userData;
} LdpBufferMapping;

struct LdpBufferFunctions
{
    /*! Map a region of a buffer
     *
     * @param[in]       buffer       The buffer object
     * @param[out]      mapping      The mapping to fill out
     * @param[in]       offset       Offset within buffer in bytes
     * @param[in]       size         Size of mapped area in bytes
     * @param[in]       access       Access type - read or write
     *
     * @return  True on success otherwise false
     */
    bool (*map)(struct LdpBuffer* buffer, LdpBufferMapping* mapping, int32_t offset, uint32_t size,
                LdpAccess access);

    /*! Unmap a buffer
     *
     * @param[in]       buffer       The buffer object
     * @param[out]      mapping      The mapping to remove
     */
    void (*unmap)(struct LdpBuffer* buffer, const LdpBufferMapping* mapping);
};

static inline bool ldpBufferMap(LdpBuffer* buffer, LdpBufferMapping* mapping, uint32_t offset,
                                uint32_t size, LdpAccess access)
{
    return buffer->functions->map(buffer, mapping, offset, size, access);
}

static inline void ldpBufferUnmap(LdpBuffer* buffer, const LdpBufferMapping* mapping)
{
    buffer->functions->unmap(buffer, mapping);
}

// NOLINTEND(modernize-use-using)

#endif // VN_LCEVC_PIPELINE_BUFFER_H
