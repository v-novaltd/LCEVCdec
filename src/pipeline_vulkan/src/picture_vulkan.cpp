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

#include "picture_vulkan.h"

#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/picture_layout.h>
//
#include <LCEVC/common/log.h>

namespace lcevc_dec::pipeline_vulkan {

namespace {
    extern const LdpPictureFunctions kPictureFunctions;
}

PictureVulkan::PictureVulkan(PipelineVulkan& pipeline)
    : LdpPicture{&kPictureFunctions}
    , m_pipeline(pipeline)
{
    ldpPictureLayoutInitialize(&layout, LdpColorFormatUnknown, 0, 0, 0);

    // Fill in defaults
    colorRange = LdpColorRangeUnknown;
    colorPrimaries = LdpColorPrimariesUnspecified;
    matrixCoefficients = LdpMatrixCoefficientsUnspecified;
    transferCharacteristics = LdpTransferCharacteristicsUnspecified;
    hdrStaticInfo = {};
    publicFlags = 0;
    sampleAspectRatio = {1, 1};
    margins = {};
    userData = nullptr;
}

PictureVulkan::~PictureVulkan()
{
    // Should have already unlocked (and unbound) by now, in the child class.
    assert(!isLocked());
}

bool PictureVulkan::isValid() const
{
    if (ldpPictureLayoutFormat(&layout) == LdpColorFormatUnknown) {
        return false;
    }

    return true;
}

bool PictureVulkan::getPublicFlag(uint8_t flag) const
{
    return ((publicFlags & (0x01 << (flag - 1))) != 0);
}

void PictureVulkan::setPublicFlag(uint8_t flag, bool value)
{
    // flags start at 1 so, subtract 1 to make sure we use all 8 bits.
    if (value) {
        publicFlags = publicFlags | static_cast<uint8_t>((0x1 << (flag - 1)));
    } else {
        publicFlags = publicFlags & static_cast<uint8_t>(~(0x1 << (flag - 1)));
    }
}

void PictureVulkan::getDesc(LdpPictureDesc& desc) const
{
    desc.colorFormat = ldpPictureLayoutFormat(&layout);
    desc.colorRange = colorRange;
    desc.colorPrimaries = colorPrimaries;
    desc.matrixCoefficients = matrixCoefficients;
    desc.transferCharacteristics = transferCharacteristics;
    desc.hdrStaticInfo = hdrStaticInfo;
    desc.sampleAspectRatioDen = sampleAspectRatio.denominator;
    desc.sampleAspectRatioNum = sampleAspectRatio.numerator;
    desc.width = getWidth();
    desc.height = getHeight();
    desc.cropTop = margins.top;
    desc.cropBottom = margins.bottom;
    desc.cropLeft = margins.left;
    desc.cropRight = margins.right;
}

bool PictureVulkan::descsMatch(const LdpPictureDesc& desc) const
{
    if (ldpPictureLayoutPlanes(&layout) == 0) {
        return false; // Picture isn't initialized so cannot match
    }
    LdpPictureDesc curDesc;
    getDesc(curDesc);
    return desc == curDesc;
}

bool PictureVulkan::setDesc(const LdpPictureDesc& newDesc)
{
    if (descsMatch(newDesc)) {
        // Don't need to do anything
        return true;
    }

    if (!initializeDesc(newDesc, nullptr)) {
        return false;
    }

    if (!unbindMemory()) {
        return false;
    }

    return bindMemory();
}

bool PictureVulkan::getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const
{
    if (!m_external) {
        return false;
    }
    bufferDescOut = m_externalBufferDesc;
    return true;
}

bool PictureVulkan::getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes]) const
{
    if (!m_external) {
        return false;
    }

    for (uint32_t i = 0; i < kLdpPictureMaxNumPlanes; ++i) {
        planeDescArrOut[i] = m_externalPlaneDescs[i];
    }
    return true;
}

bool PictureVulkan::lock(LdpAccess access, PictureLock*& lockOut)
{
    if (isLocked()) {
        return false;
    }

    if (access != LdpAccessRead && access != LdpAccessModify && access != LdpAccessWrite) {
        return false;
    }

    if (buffer == nullptr) {
        return false;
    }

    // Allocate lock object, and in-place construct
    PictureLock* pictureLock = VNAllocate(m_pipeline.allocator(), &m_lockAllocation, PictureLock);
    lockOut = new (pictureLock) PictureLock(this, access); // NOLINT(cppcoreguidelines-owning-memory)

    return true;
}

bool PictureVulkan::unlock(const PictureLock* lock)
{
    if (!isLocked()) {
        return false;
    }

    if (lock != VNAllocationPtr(m_lockAllocation, PictureLock)) {
        return false;
    }

    // Release the lock object
    if (VNIsAllocated(m_lockAllocation)) {
        VNFree(m_pipeline.allocator(), &m_lockAllocation);
    }

    return true;
}

bool PictureVulkan::initializeDesc(const LdpPictureDesc& desc,
                                   const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes])
{
    // Note that error messages in this function just uses the name, rather than the full debug
    // string. This is because the debug string reports format data that isn't meaningful until
    // AFTER initializeDesc succeeds.

    if (isLocked()) {
        VNLogError("timestamp %" PRId64 ": Picture is locked, so cannot set desc.", getTimestamp());
        return false;
    }

    if (desc.colorFormat == LdpColorFormatUnknown) {
        VNLogError("timestamp %" PRId64 ": Invalid format, cannot set desc.", getTimestamp());
        return false;
    }

    colorRange = desc.colorRange;
    matrixCoefficients = desc.matrixCoefficients;
    transferCharacteristics = desc.transferCharacteristics;
    hdrStaticInfo = desc.hdrStaticInfo;
    sampleAspectRatio = {desc.sampleAspectRatioNum, desc.sampleAspectRatioDen};

    if (rowStridesBytes) {
        if (!ldpPictureDescCheckValidStrides(&desc, rowStridesBytes)) {
            VNLogError("Invalid strides given for %dx%d plane", desc.width, desc.height);
            return false;
        }
        ldpPictureLayoutInitializeDescStrides(&layout, &desc, rowStridesBytes);
    } else {
        ldpPictureLayoutInitializeDesc(&layout, &desc, kBufferRowAlignment);
    }

    if (((desc.cropLeft + desc.cropRight) > desc.width) ||
        ((desc.cropTop + desc.cropBottom) > desc.height)) {
        VNLogError("timestamp %" PRId64
                   ". Requested to crop out more than the whole picture. Requested crops are: left "
                   "%u, right %u, top %u, bottom %u. Size is %ux%u.",
                   getTimestamp(), desc.cropLeft, desc.cropRight, desc.cropTop, desc.cropBottom,
                   desc.width, desc.height);
        return false;
    }
    margins = {desc.cropLeft, desc.cropTop, desc.cropRight, desc.cropBottom};

    return true;
}

void PictureVulkan::setExternal(const LdpPicturePlaneDesc* planeDescArr, const LdpPictureBufferDesc* bufferDesc)
{
    m_external = true;
    for (uint32_t i = 0; i < ldpPictureLayoutPlanes(&layout); ++i) {
        m_externalPlaneDescs[i] = planeDescArr[i];
    }

    m_externalBufferDesc = *bufferDesc;
}

uint32_t PictureVulkan::getRequiredSize() const
{
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < ldpPictureLayoutPlanes(&layout); i++) {
        totalSize += ldpPictureLayoutPlaneSize(&layout, i);
    }
    return totalSize;
}

void PictureVulkan::getPlaneDescInternal(uint32_t plane, LdpPicturePlaneDesc& desc) const
{
    if (!m_external) {
        const BufferVulkan* bufferVulkan = static_cast<const BufferVulkan*>(buffer);
        desc.firstSample = bufferVulkan->ptr() + ldpPictureLayoutPlaneOffset(&layout, plane);
        desc.rowByteStride = ldpPictureLayoutRowStride(&layout, plane);
    } else {
        assert(plane < kLdpPictureMaxNumPlanes);
        desc = m_externalPlaneDescs[plane];
    }
}

bool PictureVulkan::bindMemory()
{
    VNLogVerbose("timestamp %" PRId64 ": BIND <%p>", getTimestamp(), (void*)this);

    if (isLocked()) {
        return false;
    }

    byteOffset = 0;
    byteSize = getRequiredSize();
    if (byteSize == 0) {
        return false;
    }

    // m_buffer might be set, if we are resizing.
    if (buffer == nullptr) {
        buffer = m_pipeline.allocateBuffer(byteSize);
    } else {
        BufferVulkan* bp = static_cast<BufferVulkan*>(buffer); // TODO - check this path

        bp->clear();
        if (byteSize > bp->size() && !bp->resize(byteSize)) {
            return false;
        }
    }

    return true;
}
bool PictureVulkan::unbindMemory()
{
    VNLogVerbose("timestamp %" PRId64 ": UNBIND <%p>", getTimestamp(), (void*)this);

    if (isLocked()) {
        return false;
    }
    bool ret = true;
    if (buffer != nullptr) {
        m_pipeline.releaseBuffer(static_cast<BufferVulkan*>(buffer));
        buffer = nullptr;
    }
    return ret;

    return true;
}

// C function table to connect to C++ class
//
namespace {
    bool setDesc(LdpPicture* picture, const LdpPictureDesc* desc)
    {
        return static_cast<PictureVulkan*>(picture)->setDesc(*desc);
    }
    void getDesc(const LdpPicture* picture, LdpPictureDesc* desc)
    {
        static_cast<const PictureVulkan*>(picture)->getDesc(*desc);
    }

    bool getBufferDesc(const LdpPicture* picture, LdpPictureBufferDesc* desc)
    {
        return static_cast<const PictureVulkan*>(picture)->getBufferDesc(*desc);
    }

    bool setFlag(LdpPicture* picture, uint8_t flag, bool value)
    {
        static_cast<PictureVulkan*>(picture)->setPublicFlag(flag, value);
        return true;
    }

    bool getFlag(const LdpPicture* picture, uint8_t flag)
    {
        return static_cast<const PictureVulkan*>(picture)->getPublicFlag(flag);
    }

    bool lock(LdpPicture* picture, LdpAccess access, LdpPictureLock** pictureLock)
    {
        PictureLock* ptr = nullptr;

        if (!static_cast<PictureVulkan*>(picture)->lock(access, ptr)) {
            return false;
        }

        *pictureLock = ptr;
        return true;
    }

    void unlock(LdpPicture* picture, LdpPictureLock* pictureLock)
    {
        const PictureLock* const ptr = static_cast<PictureLock*>(pictureLock);
        static_cast<PictureVulkan*>(picture)->unlock(ptr);
    }

    LdpPictureLock* getLock(const LdpPicture* picture)
    {
        return static_cast<const PictureVulkan*>(picture)->getLock();
    }

    const LdpPictureFunctions kPictureFunctions = {
        setDesc, getDesc, getBufferDesc, setFlag, getFlag, lock, unlock, getLock,
    };

} // namespace
} // namespace lcevc_dec::pipeline_vulkan
