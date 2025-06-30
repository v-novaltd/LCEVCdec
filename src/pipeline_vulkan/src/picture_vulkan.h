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

#ifndef VN_LCEVC_PIPELINE_VULKAN_PICTURE_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_PICTURE_VULKAN_H

#include "buffer_vulkan.h"
#include "picture_lock_vulkan.h"
#include "pipeline_vulkan.h"
//
#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/constants.h>

namespace lcevc_dec::pipeline_vulkan {

class PictureVulkan : public LdpPicture
{
public:
    explicit PictureVulkan(PipelineVulkan& pipeline);
    ~PictureVulkan();

    bool isValid() const;

    bool getPublicFlag(uint8_t flag) const;
    void setPublicFlag(uint8_t flag, bool value);

    void getDesc(LdpPictureDesc& descOut) const;
    bool setDesc(const LdpPictureDesc& newDesc);

    bool getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const;
    bool getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes]) const;

    void* getUserData() const { return userData; }
    void setUserData(void* val) { userData = val; }
    uint64_t getTimestamp() const { return m_timestamp; }
    void setTimestamp(uint64_t timestamp) { m_timestamp = timestamp; }
    // clang-format on

    // Access management
    bool lock(LdpAccess access, PictureLock*& lockOut);
    bool unlock(const PictureLock* lock);

    bool isLocked() const { return VNIsAllocated(m_lockAllocation); }

    PictureLock* getLock() const { return VNAllocationPtr(m_lockAllocation, PictureLock); }

    // Buffer management
    void setExternal(const LdpPicturePlaneDesc* planeDescArr, const LdpPictureBufferDesc* buffer);

    uint32_t getRequiredSize() const;

    // Internal fetch of plane pointer and stride
    void getPlaneDescInternal(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const;

    bool bindMemory();
    bool unbindMemory();

    VNNoCopyNoMove(PictureVulkan);

private:
    uint32_t getWidth() const
    {
        return ldpPictureLayoutWidth(&layout) - (margins.left + margins.right);
    }
    uint32_t getHeight() const
    {
        return ldpPictureLayoutHeight(&layout) - (margins.top + margins.bottom);
    }

    bool initializeDesc(const LdpPictureDesc& desc,
                        const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes]);

    bool descsMatch(const LdpPictureDesc& desc) const;

    // Owning pipeline
    PipelineVulkan& m_pipeline;

    // Timestamp (for debugging)
    uint64_t m_timestamp = kInvalidTimestamp;

    // Any current lock
    LdcMemoryAllocation m_lockAllocation = {};

    // Any external buffer and plane description
    bool m_external = false;
    LdpPicturePlaneDesc m_externalPlaneDescs[kLdpPictureMaxNumPlanes] = {};
    LdpPictureBufferDesc m_externalBufferDesc = {};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_PICTURE_VULKAN_H
