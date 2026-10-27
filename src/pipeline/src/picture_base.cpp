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
#include <LCEVC/pipeline/picture_base.h>
//
#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/buffer_alignment.h>

namespace lcevc_dec::pipeline {

namespace {
    extern const LdpPictureFunctions kPictureFunctions;
}

PictureBase::PictureBase(PipelineBase& pipeline)
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

PictureBase::~PictureBase()
{
    // Should have already unlocked (and unbound) by now, in the child class.
    assert(!isLocked());
}

bool PictureBase::isValid() const
{
    if (ldpPictureLayoutFormat(&layout) == LdpColorFormatUnknown) {
        return false;
    }

    return true;
}

bool PictureBase::getPublicFlag(uint8_t flag) const
{
    return ((publicFlags & (0x01 << (flag - 1))) != 0);
}

void PictureBase::setPublicFlag(uint8_t flag, bool value)
{
    // flags start at 1 so, subtract 1 to make sure we use all 8 bits.
    if (value) {
        publicFlags = publicFlags | static_cast<uint8_t>((0x1 << (flag - 1)));
    } else {
        publicFlags = publicFlags & static_cast<uint8_t>(~(0x1 << (flag - 1)));
    }
}

void PictureBase::getDesc(LdpPictureDesc& desc) const
{
    desc.colorFormat = ldpPictureLayoutFormat(&layout);
    desc.colorRange = colorRange;
    desc.colorPrimaries = colorPrimaries;
    desc.matrixCoefficients = matrixCoefficients;
    desc.transferCharacteristics = transferCharacteristics;
    desc.hdrStaticInfo = hdrStaticInfo;
    desc.sampleAspectRatioDen = sampleAspectRatio.denominator;
    desc.sampleAspectRatioNum = sampleAspectRatio.numerator;
    desc.width = getActiveWidth();
    desc.height = getActiveHeight();
    desc.cropTop = margins.top;
    desc.cropBottom = margins.bottom;
    desc.cropLeft = margins.left;
    desc.cropRight = margins.right;
}

const LdpPictureLayout* PictureBase::bufferPictureLayout() const
{
    if (buffer == nullptr) {
        return nullptr;
    }
    return static_cast<const BufferBase*>(buffer)->getPictureLayout();
}

LdpPictureDesc PictureBase::resolvePictureDesc(const LdpPictureDesc& desc) const
{
    LdpPictureDesc effectiveDesc = desc;
    const LdpPictureLayout* implicitLayout = bufferPictureLayout();
    if (!implicitLayout) {
        // Buffer does not have an implicit layout - provided one in good
        return effectiveDesc;
    }

    // Extract details from buffer and make an appropriate PictureDesc - complain if
    // dimensions are not what was expected
    const uint32_t implicitWidth = ldpPictureLayoutWidth(implicitLayout);
    const uint32_t implicitHeight = ldpPictureLayoutHeight(implicitLayout);
    const LdpColorFormat implicitFormat = ldpPictureLayoutFormat(implicitLayout);

    if ((desc.width != 0 && desc.width != implicitWidth) ||
        (desc.height != 0 && desc.height != implicitHeight)) {
        VNLogWarning("Picture desc dimensions %ux%u do not match external buffer dimensions %ux%u; "
                     "using external buffer dimensions.",
                     desc.width, desc.height, implicitWidth, implicitHeight);
    }

    effectiveDesc.width = implicitWidth;
    effectiveDesc.height = implicitHeight;
    if (implicitFormat != LdpColorFormatUnknown) {
        effectiveDesc.colorFormat = implicitFormat;
    }
    return effectiveDesc;
}

bool PictureBase::descsMatch(const LdpPictureDesc& desc) const
{
    if (ldpPictureLayoutPlanes(&layout) == 0) {
        return false; // Picture isn't initialized so cannot match
    }
    LdpPictureDesc curDesc{};
    getDesc(curDesc);
    return resolvePictureDesc(desc) == curDesc;
}

bool PictureBase::setDesc(const LdpPictureDesc& newDesc)
{
    if (descsMatch(newDesc)) {
        // Don't need to do anything
        return true;
    }

    if (!initializeDesc(newDesc, nullptr)) {
        // Bad PictureDesc
        return false;
    }

    if (!m_hasExternalBuffer) {
        // Release buffer if not owned
        if (!unbindBuffer()) {
            return false;
        }
    }

    return true;
}

bool PictureBase::setDescAndBind(const LdpPictureDesc& newDesc, pipeline::BufferUsage usage)
{
    if (!setDesc(newDesc)) {
        return false;
    }

    // Make sure there is a buffer
    if (!buffer) {
        return bindBuffer(usage);
    }

    return true;
}

bool PictureBase::getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const
{
    if (!m_hasExternalBufferDesc) {
        return false;
    }
    bufferDescOut = m_externaBufferDesc;
    return true;
}

bool PictureBase::getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes]) const
{
    if (!m_hasExternalPlanes) {
        return false;
    }

    for (uint32_t i = 0; i < kLdpPictureMaxNumPlanes; ++i) {
        planeDescArrOut[i] = m_externalPlaneDescs[i];
    }
    return true;
}

bool PictureBase::initializeDesc(const LdpPictureDesc& desc,
                                 const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes])
{
    if (isLocked()) {
        VNLogError("Picture is locked, so cannot set desc.");
        return false;
    }

    const LdpPictureDesc effectiveDesc = resolvePictureDesc(desc);
    const LdpPictureLayout* implicitLayout = bufferPictureLayout();

    if (effectiveDesc.colorFormat == LdpColorFormatUnknown) {
        VNLogError("Invalid format, cannot set desc.");
        return false;
    }

    colorRange = effectiveDesc.colorRange;
    colorPrimaries = effectiveDesc.colorPrimaries;
    matrixCoefficients = effectiveDesc.matrixCoefficients;
    transferCharacteristics = effectiveDesc.transferCharacteristics;
    hdrStaticInfo = effectiveDesc.hdrStaticInfo;
    sampleAspectRatio = {effectiveDesc.sampleAspectRatioNum, effectiveDesc.sampleAspectRatioDen};

    if (rowStridesBytes) {
        if (!ldpPictureDescCheckValidStrides(&effectiveDesc, rowStridesBytes)) {
            VNLogError("Invalid strides given for %dx%d plane", effectiveDesc.width, effectiveDesc.height);
            return false;
        }
        ldpPictureLayoutInitializeDescStrides(&layout, &effectiveDesc, rowStridesBytes);
    } else if (implicitLayout) {
        layout = *implicitLayout;
    } else {
        ldpPictureLayoutInitializeDesc(&layout, &effectiveDesc, kBufferRowAlignment);
    }

    if (((effectiveDesc.cropLeft + effectiveDesc.cropRight) > effectiveDesc.width) ||
        ((effectiveDesc.cropTop + effectiveDesc.cropBottom) > effectiveDesc.height)) {
        VNLogError("Requested to crop out more than the whole picture. Requested crops are: left "
                   "%u, right %u, top %u, bottom %u. Size is %ux%u.",
                   effectiveDesc.cropLeft, effectiveDesc.cropRight, effectiveDesc.cropTop,
                   effectiveDesc.cropBottom, effectiveDesc.width, effectiveDesc.height);
        return false;
    }
    margins = {effectiveDesc.cropLeft, effectiveDesc.cropTop, effectiveDesc.cropRight,
               effectiveDesc.cropBottom};

    return true;
}

void PictureBase::setExternalBufferDesc(const LdpPictureBufferDesc* bufferDesc)
{
    if (bufferDesc) {
        // m_externalBuffer is only set when an externally-owned BufferBase is
        // attached
        m_externaBufferDesc = *bufferDesc;
        m_hasExternalBufferDesc = true;
    }
}

void PictureBase::setExternalPlaneDesc(const LdpPicturePlaneDesc* planeDescArr)
{
    m_hasExternalPlanes = true;
    for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(&layout); ++plane) {
        if (planeDescArr) {
            m_externalPlaneDescs[plane] = planeDescArr[plane];
            layout.rowStrides[plane] = planeDescArr[plane].rowByteStride;
        } else {
            m_externalPlaneDescs[plane].firstSample =
                m_externaBufferDesc.data + ldpPictureLayoutPlaneOffset(&layout, plane);
            m_externalPlaneDescs[plane].rowByteStride =
                ldpPictureLayoutDefaultRowStride(&layout, plane, kBufferRowAlignment);
        }
    }

    if (planeDescArr) {
        // Keep plane offsets and size consistent with the external strides, as a pipeline may
        // lay out its own copy of the picture (e.g. a Vulkan output buffer) from this layout.
        uint32_t offset = 0;
        for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(&layout); ++plane) {
            layout.planeOffsets[plane] = offset;
            offset += layout.rowStrides[plane] * ldpPictureLayoutPlaneHeight(&layout, plane);
        }
        layout.size = offset;
    }
}

uint32_t PictureBase::getRequiredSize() const
{
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < ldpPictureLayoutPlanes(&layout); i++) {
        totalSize += ldpPictureLayoutPlaneSize(&layout, i);
    }
    return totalSize;
}

void PictureBase::getPlaneDescInternal(uint32_t plane, LdpPicturePlaneDesc& desc) const
{
    assert(plane < kLdpPictureMaxNumPlanes);

    if (!m_hasExternalPlanes) {
        VNLogError("Cannot get PlaneDesc");
        assert(0);
    } else {
        desc = m_externalPlaneDescs[plane];
    }
}

bool PictureBase::bindBuffer(pipeline::BufferUsage usage)
{
    VNLogVerbose("bindBuffer <%p>", (void*)this);

    if (isLocked()) {
        return false;
    }

    // Check for empty
    byteOffset = 0;
    byteSize = getRequiredSize();
    if (byteSize == 0) {
        return false;
    }

    if (m_hasExternalBuffer) {
        // Externally-owned buffers are already bound.
        return m_externaBufferDesc.data != nullptr || m_externaBufferDesc.accelBuffer != nullptr;
    }

    // No buffer - just make a new one
    if (buffer == nullptr) {
        buffer = m_pipeline.allocateBuffer(byteSize, usage);
        return buffer != nullptr;
    }

    // Existing buffer
    BufferBase* bp = static_cast<BufferBase*>(buffer);

    // Check usage
    if (bp->usage() != usage) {
        m_pipeline.releaseBuffer(static_cast<BufferBase*>(buffer));
        buffer = m_pipeline.allocateBuffer(byteSize, usage);
        return buffer != nullptr;
    }

    // Resize existing buffer
    if (byteSize > bp->size() && !bp->resize(byteSize)) {
        return false;
    }

    bp->clear();

    return true;
}

void PictureBase::setExternalBuffer(pipeline::BufferBase* externalBuffer)
{
    VNLogVerbose("setExternalBuffer <%p> <%p>", (void*)this, (void*)externalBuffer);

    if (isLocked()) {
        VNLogError("Picture is locked, so cannot set external buffer.");
        return;
    }

    // Release any existing pipeline-owned buffer first.
    if (buffer != nullptr && !m_hasExternalBuffer) {
        m_pipeline.releaseBuffer(static_cast<BufferBase*>(buffer));
    }
    buffer = externalBuffer;
    m_hasExternalBuffer = (externalBuffer != nullptr);

    // If the new buffer hasan implicit picture layout, poke that into picture.
    if (m_hasExternalBuffer && bufferPictureLayout()) {
        LdpPictureDesc desc{};
        getDesc(desc);
        initializeDesc(desc, nullptr);
    }
}

bool PictureBase::unbindBuffer()
{
    VNLogVerbose("unbindBuffer <%p>", (void*)this);

    if (isLocked()) {
        return false;
    }
    if (buffer != nullptr && !m_hasExternalBuffer) {
        m_pipeline.releaseBuffer(static_cast<BufferBase*>(buffer));
    }
    buffer = nullptr;
    m_hasExternalBuffer = false;
    return true;
}

// C function table to connect to C++ class
//
namespace {
    bool setDesc(LdpPicture* picture, const LdpPictureDesc* desc)
    {
        if (!picture || !desc) {
            return false;
        }
        return static_cast<PictureBase*>(picture)->setDesc(*desc);
    }
    void getDesc(const LdpPicture* picture, LdpPictureDesc* desc)
    {
        if (!picture || !desc) {
            return;
        }
        static_cast<const PictureBase*>(picture)->getDesc(*desc);
    }

    bool getBufferDesc(const LdpPicture* picture, LdpPictureBufferDesc* desc)
    {
        if (!picture || !desc) {
            return false;
        }
        return static_cast<const PictureBase*>(picture)->getBufferDesc(*desc);
    }

    bool setFlag(LdpPicture* picture, uint8_t flag, bool value)
    {
        if (!picture) {
            return false;
        }
        static_cast<PictureBase*>(picture)->setPublicFlag(flag, value);
        return true;
    }

    bool getFlag(const LdpPicture* picture, uint8_t flag)
    {
        if (!picture) {
            return false;
        }
        return static_cast<const PictureBase*>(picture)->getPublicFlag(flag);
    }

    bool lock(LdpPicture* picture, LdpAccess access, LdpPictureLock** pictureLock)
    {
        if (!picture || !pictureLock) {
            return false;
        }

        LdpPictureLock* ptr = nullptr;

        if (!static_cast<PictureBase*>(picture)->lock(access, ptr)) {
            return false;
        }

        *pictureLock = ptr;
        return true;
    }

    void unlock(LdpPicture* picture, LdpPictureLock* pictureLock)
    {
        if (!picture || !pictureLock) {
            return;
        }
        const PictureLockBase* const ptr = static_cast<PictureLockBase*>(pictureLock);
        static_cast<PictureBase*>(picture)->unlock(ptr);
    }

    LdpPictureLock* getLock(const LdpPicture* picture)
    {
        if (!picture) {
            return nullptr;
        }
        return static_cast<const PictureBase*>(picture)->getLock();
    }

    const LdpPictureFunctions kPictureFunctions = {
        setDesc, getDesc, getBufferDesc, setFlag, getFlag, lock, unlock, getLock,
    };
} // namespace
} // namespace lcevc_dec::pipeline
