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

#include "picture.h"

#include "buffer_manager.h"
#include "interface.h"
#include "log.h"
#include "picture_copy.h"
#include "picture_lock.h"

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/PerseusDecoder.h>

#include <cassert>
#include <cstring>
#include <memory>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

static const LogComponent kComp = LogComponent::Picture;

// - Picture --------------------------------------------------------------------------------------

// Public functions:

Picture::~Picture()
{
    // Should have already unlocked (and un-bound) by now, in the child class.
    assert(!isLocked());
}

bool Picture::copyMetadata(const Picture& source)
{
    // This copies all format information, as well as the timehandle (since the typical use case is
    // in passthrough mode). Other identifying information is not copied (since this is, after all,
    // meant to help uniquely identify a picture), and underlying data is not copied either (that's
    // copyData).
    if (!canModify()) {
        return false;
    }

    LCEVC_PictureDesc sourceDesc;
    source.getDesc(sourceDesc);
    if (!setDesc(sourceDesc)) {
        return false;
    }

    m_colorRange = source.m_colorRange;
    m_matrixCoefficients = source.m_matrixCoefficients;
    m_transferCharacteristics = source.m_transferCharacteristics;
    m_publicFlags = source.m_publicFlags;
    m_hdrStaticInfo = source.m_hdrStaticInfo;
    m_sampleAspectRatio = source.m_sampleAspectRatio;
    m_crop = source.m_crop;
    return true;
}

bool Picture::copyData(const Picture& source)
{
    if (!canModify()) {
        return false;
    }
    if (!isValid() || !source.isValid()) {
        return false;
    }

    // NV12->I420
    if (source.m_layout.isInterleaved() && !m_layout.isInterleaved() &&
        m_layout.colorSpace() == PictureLayout::YUV) {
        copyNV12ToI420Picture(source, *this);
        return true;
    }

    // No handling yet for I420->NV12
    if (!source.m_layout.isInterleaved() && source.m_layout.colorSpace() == PictureLayout::YUV &&
        m_layout.isInterleaved()) {
        VNLogError("CC %u, PTS %" PRId64
                   ":Cannot currently copy directly from non-NV12 to NV12 pictures\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()));
        return false;
    }

    if (source.m_layout.format() != m_layout.format()) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Cannot currently copy directly from format %u to format %u.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   source.m_layout.format(), m_layout.format());
        return false;
    }

    copyPictureToPicture(source, *this);
    return true;
}

bool Picture::toCoreImage(perseus_image& dest)
{
    int32_t interleaving = 0;
    if (!toCoreInterleaving(m_layout.format(), m_layout.isInterleaved(), interleaving)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to get interleaving from <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   toString().c_str());
        return false;
    }
    dest.ilv = static_cast<perseus_interleaving>(interleaving);

    int32_t bitdepth = PSS_DEPTH_8;
    if (!toCoreBitdepth(m_layout.sampleBits(), bitdepth)) {
        VNLogError("CC %u, PTS %" PRId64 ": Failed to get bit depth from <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   toString().c_str());
        return false;
    }
    dest.depth = static_cast<perseus_bitdepth>(bitdepth);

    for (uint32_t i = 0; i < getNumPlanes(); i++) {
        dest.plane[i] = getPlaneFirstSample(i);
        dest.stride[i] = getPlaneSampleStride(i); // Core needs stride in samples
    }

    return true;
}

bool Picture::isValid() const
{
    if (m_layout.format() == LCEVC_ColorFormat::LCEVC_ColorFormat_Unknown) {
        return false;
    }
    if (getPlaneFirstSample(0) == nullptr) {
        return false;
    }
    return true;
}

void Picture::setPublicFlag(uint8_t flag, bool value)
{
    // flags start at 1 so, subtract 1 to make sure we use all 8 bits.
    if (value) {
        m_publicFlags = m_publicFlags | (0x1 << (flag - 1));
    } else {
        m_publicFlags = m_publicFlags & ~(0x1 << (flag - 1));
    }
}

bool Picture::getPublicFlag(uint8_t flag) const
{
    return ((m_publicFlags & (0x01 << (flag - 1))) != 0);
}

void Picture::getDesc(LCEVC_PictureDesc& desc) const
{
    desc.colorFormat = m_layout.format();
    desc.colorRange = m_colorRange;
    desc.colorPrimaries = m_colorPrimaries;
    desc.matrixCoefficients = m_matrixCoefficients;
    desc.transferCharacteristics = m_transferCharacteristics;
    desc.hdrStaticInfo = m_hdrStaticInfo;
    desc.sampleAspectRatioDen = m_sampleAspectRatio.denominator;
    desc.sampleAspectRatioNum = m_sampleAspectRatio.numerator;
    desc.width = getWidth();
    desc.height = getHeight();
    desc.cropTop = m_crop.top;
    desc.cropBottom = m_crop.bottom;
    desc.cropLeft = m_crop.left;
    desc.cropRight = m_crop.right;
}

uint8_t* Picture::internalGetPlaneFirstSample(uint32_t planeIdx) const
{
    // This is the default behaviour: pictures are presumed to have 1 buffer with at least 1 plane,
    // but child classes may override this behaviour.
    uint8_t* out = getBuffer();
    if (out == nullptr) {
        return nullptr;
    }

    for (uint32_t prevPlane = 0; prevPlane < planeIdx; prevPlane++) {
        out += getPlaneMemorySize(prevPlane);
    }
    return out;
}

bool Picture::lock(Access access, Handle<PictureLock> newLock)
{
    if (isLocked()) {
        return false;
    }

    switch (access) {
        case Access::Unknown: return false;

        case Access::Read:
        case Access::Modify:
        case Access::Write: m_lock = newLock; break;
    }

    return true;
}

bool Picture::unlock()
{
    if (!isLocked()) {
        return false;
    }
    m_lock = kInvalidHandle;
    return true;
}

bool Picture::setDesc(const LCEVC_PictureDesc& newDesc) { return setDesc(newDesc, nullptr); }

// Private functions:

void Picture::setName(const std::string& name) { m_name = "Picture:" + name; }

bool Picture::setDesc(const LCEVC_PictureDesc& newDesc,
                      const uint32_t rowStridesBytes[PictureLayout::kMaxNumPlanes])
{
    // This is either called via setDescExternal (in which case planeStridesBytes is set from the
    // plane descs, if provide), or else via the normal public setDesc function (in which case
    // rowStridesBytes is automatically set).
    if (!initializeDesc(newDesc, rowStridesBytes)) {
        VNLogError("CC %u, PTS %" PRId64 ": Invalid new desc for Picture <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    return true;
}

std::string Picture::getShortDbgString() const
{
    std::string result;

    char tmp[512];
    uint32_t width = (m_layout.planes() > 0) ? getWidth() : 0;
    uint32_t height = (m_layout.planes() > 0) ? getHeight() : 0;
    snprintf(tmp, sizeof(tmp) - 1, "%s, %s, %p, fmt %d:%d, byteDepth=%d, bitDepthPP=%d, size=%dx%d.",
             m_name.c_str(), isManaged() ? "Managed" : "Unmanaged", this, m_layout.format(),
             m_layout.isInterleaved(), m_layout.sampleSize(), m_layout.sampleBits(), width, height);
    return {tmp};
}

std::string Picture::toString() const
{
    std::string result;

    result = getShortDbgString() + "\n";

    char tmp[256];
    for (uint32_t i = 0; i < getNumPlanes(); ++i) {
        snprintf(tmp, sizeof(tmp) - 1, "Plane %d/%d. sampleByteStride:%d, rowByteStride:%d. ", i,
                 getNumPlanes(), getPlaneBytesPerPixel(i), getPlaneByteStride(i));
        result += tmp;
        result += "\n";
    }
    return result;
}

bool Picture::initializeDesc(const LCEVC_PictureDesc& desc, const uint32_t rowStridesBytes[kMaxNumPlanes])
{
    // Note that error messages in this function just uses the name, rather than the full debug
    // string. This is because the debug string reports format data that isn't meaningful until
    // AFTER initializeDesc succeeds.

    if (!canModify()) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Picture cannot be modified, so cannot set desc. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    if (desc.colorFormat == LCEVC_ColorFormat_Unknown) {
        VNLogError("CC %u, PTS %" PRId64 ": Invalid format, cannot set desc. Picture: <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   m_name.c_str());
        return false;
    }

    m_colorRange = desc.colorRange;
    m_matrixCoefficients = desc.matrixCoefficients;
    m_transferCharacteristics = desc.transferCharacteristics;
    memcpy(&m_hdrStaticInfo, &desc.hdrStaticInfo, sizeof(LCEVC_HDRStaticInfo));
    m_sampleAspectRatio = {desc.sampleAspectRatioNum, desc.sampleAspectRatioDen};
    if (rowStridesBytes) {
        if (!PictureLayout::checkValidStrides(desc, rowStridesBytes)) {
            VNLogError("Invalid strides given for %dx%d plane\n", desc.width, desc.height);
            return false;
        }
        m_layout = PictureLayout(desc, rowStridesBytes);
    } else {
        m_layout = PictureLayout(desc);
    }

    if (((desc.cropLeft + desc.cropRight) > desc.width) ||
        ((desc.cropTop + desc.cropBottom) > desc.height)) {
        VNLogError("CC %u, PTS %" PRId64
                   ". Requested to crop out more than the whole picture. Requested crops are: left "
                   "%u, right %u, top %u, bottom %u. Size is %ux%u. Picture: <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   desc.cropLeft, desc.cropRight, desc.cropTop, desc.cropBottom, desc.width,
                   desc.height, m_name.c_str());
        return false;
    }
    m_crop = {desc.cropLeft, desc.cropTop, desc.cropRight, desc.cropBottom};

    return true;
}

bool Picture::bindMemory()
{
    if (!canModify()) {
        VNLogError("CC %u, PTS %" PRId64 ": Locked, cannot bind memory. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   getShortDbgString().c_str());
        return false;
    }
    return true;
}

bool Picture::unbindMemory()
{
    VNLogTrace("CC %u, PTS %" PRId64 ": UNBIND <%s>\n", timehandleGetCC(getTimehandle()),
               timehandleGetTimestamp(getTimehandle()), toString().c_str());
    if (!canModify()) {
        VNLogError("CC %u, PTS %" PRId64 ": Locked, cannot unbind memory. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   getShortDbgString().c_str());
        return false;
    }
    return true;
}

uint32_t Picture::getRequiredSize() const
{
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < m_layout.planes(); i++) {
        totalSize += m_layout.planeSize(i);
        VNLogTrace("CC %u, PTS %" PRId64 ": [%d] S %dx%d size %d, Total Size: %d (plane loc: %p)\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()), i,
                   m_layout.planeWidth(i), m_layout.planeHeight(i), m_layout.planeSize(i),
                   totalSize, getPlaneFirstSample(i));
    }
    return totalSize;
}

// - PictureExternal ------------------------------------------------------------------------------

PictureExternal::~PictureExternal()
{
    unlock();
    unbindMemory();
}

bool PictureExternal::descsMatch(const LCEVC_PictureDesc& newDesc,
                                 const LCEVC_PicturePlaneDesc* newPlaneDescArr,
                                 const LCEVC_PictureBufferDesc* newBufferDesc)
{
    if (m_layout.planes() == 0) {
        return false; // Picture isn't initialised so cannot match
    }
    LCEVC_PictureDesc curDesc;
    getDesc(curDesc);
    if (!equals(newDesc, curDesc)) {
        return false;
    }

    // If one's null and the other's not, they mismatch
    if ((m_bufferDesc == nullptr) != (newBufferDesc == nullptr)) {
        return false;
    }

    if (m_bufferDesc != nullptr) {
        // We're here so they're both NOT null
        LCEVC_PictureBufferDesc curBufferDesc = {};
        toLCEVCPictureBufferDesc(*m_bufferDesc, curBufferDesc);
        if (!equals(curBufferDesc, *newBufferDesc)) {
            return false;
        }
    }

    // If one's null and the other's not, they mismatch
    if ((m_planeDescs == nullptr) != (newPlaneDescArr == nullptr)) {
        return false;
    }

    if (m_planeDescs != nullptr) {
        LCEVC_PicturePlaneDesc reusablePlaneDesc = {};
        const PictureLayout newLayout = PictureLayout(newDesc);
        for (uint32_t planeIdx = 0; planeIdx < newLayout.planes(); planeIdx++) {
            toLCEVCPicturePlaneDesc((*m_planeDescs)[planeIdx], reusablePlaneDesc);
            if (!equals(reusablePlaneDesc, newPlaneDescArr[planeIdx])) {
                return false;
            }
        }
    }

    return true;
}

bool PictureExternal::setDesc(const LCEVC_PictureDesc& newDesc,
                              const uint32_t rowStridesBytes[PictureLayout::kMaxNumPlanes])
{
    if (!BaseClass::setDesc(newDesc, rowStridesBytes)) {
        return false;
    }

    // When both are provided, bufferDesc is the authority on allocation size, while planeDesc is
    // authority on memory locations.
    uint32_t totalAllocatedBytes = 0;
    if (m_bufferDesc != nullptr) {
        totalAllocatedBytes = m_bufferDesc->byteSize;
    } else {
        for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
            totalAllocatedBytes += getPlaneMemorySize(planeIdx);
        }
    }

    // If planeDesc was provided, then it will dictate getRequiredSize. In other words, when both
    // are provided, this checks that planeDesc doesn't exceed bufferDesc (smaller is fine though).
    if (getRequiredSize() > totalAllocatedBytes) {
        VNLogWarning(
            "CC %u, PTS %" PRId64
            ": Did not allocate enough memory for the new desc. New desc is %ux%u, %u bits "
            "per sample, with a format of %d. Picture is <%s>\n",
            timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
            newDesc.width, newDesc.height, bitdepthFromLCEVCDescColorFormat(newDesc.colorFormat),
            newDesc.colorFormat, getShortDbgString().c_str());
        return false;
    }

    return true;
}

bool PictureExternal::setDesc(const LCEVC_PictureDesc& newDesc)
{
    // This is only used to RE-set an external picture's plane desc. So use the existing plane and
    // buffer descs and pass it on to the normal setDescExternal function (which checks for changes
    // before doing anything).

    std::unique_ptr<LCEVC_PictureBufferDesc> curBufferDesc = nullptr;
    if (m_bufferDesc != nullptr) {
        curBufferDesc = std::make_unique<LCEVC_PictureBufferDesc>();
        toLCEVCPictureBufferDesc(*m_bufferDesc, *curBufferDesc);
    }

    std::unique_ptr<LCEVC_PicturePlaneDesc[]> planeDescArr = nullptr;
    if (m_planeDescs != nullptr) {
        planeDescArr = std::make_unique<LCEVC_PicturePlaneDesc[]>(getNumPlanes());
        const PictureLayout newLayout = PictureLayout(newDesc);
        for (uint32_t planeIdx = 0; planeIdx < newLayout.planes(); planeIdx++) {
            toLCEVCPicturePlaneDesc((*m_planeDescs)[planeIdx], planeDescArr[planeIdx]);
        }
    }

    return setDescExternal(newDesc, planeDescArr.get(), curBufferDesc.get());
}

bool PictureExternal::setDescExternal(const LCEVC_PictureDesc& newDesc,
                                      const LCEVC_PicturePlaneDesc* newPlaneDescArr,
                                      const LCEVC_PictureBufferDesc* newBufferDesc)
{
    // Check for changes, then bind, THEN set desc.

    if (descsMatch(newDesc, newPlaneDescArr, newBufferDesc)) {
        return true;
    }

    m_layout = PictureLayout(newDesc);
    if (!bindMemoryBufferAndPlanes(m_layout.planes(), newPlaneDescArr, newBufferDesc)) {
        VNLogError("Failed to bind memory for external picture at %p\n", this);
        return false;
    }

    // If there's a manual stride, set it up:
    std::unique_ptr<uint32_t[]> rowStridesBytes = nullptr;
    if (newPlaneDescArr != nullptr) {
        rowStridesBytes = std::make_unique<uint32_t[]>(PictureLayout::kMaxNumPlanes);
        for (uint32_t planeIdx = 0; planeIdx < m_layout.planes(); planeIdx++) {
            rowStridesBytes[planeIdx] = newPlaneDescArr[planeIdx].rowByteStride;
        }
    }

    return setDesc(newDesc, rowStridesBytes.get());
}

uint8_t* PictureExternal::internalGetPlaneFirstSample(uint32_t planeIdx) const
{
    if (m_planeDescs != nullptr) {
        return (*m_planeDescs)[planeIdx].firstSample;
    }

    return BaseClass::internalGetPlaneFirstSample(planeIdx);
}

bool PictureExternal::getBufferDesc(LCEVC_PictureBufferDesc& bufferDescOut) const
{
    if (m_bufferDesc == nullptr) {
        return false;
    }

    const PictureBufferDesc& desc = *m_bufferDesc;
    bufferDescOut = {desc.data, desc.byteSize, {desc.accelBuffer.handle}, static_cast<LCEVC_Access>(desc.access)};
    return true;
}

bool PictureExternal::getPlaneDescArr(PicturePlaneDesc planeDescArrOut[kMaxNumPlanes]) const
{
    if (m_planeDescs == nullptr) {
        for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
            planeDescArrOut[planeIdx].firstSample = internalGetPlaneFirstSample(planeIdx);
            planeDescArrOut[planeIdx].rowByteStride = getPlaneByteStride(planeIdx);
        }
        return true;
    }

    for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
        planeDescArrOut[planeIdx] = (*m_planeDescs)[planeIdx];
    }
    return true;
}

bool PictureExternal::bindMemoryBufferAndPlanes(uint32_t numPlanes,
                                                const LCEVC_PicturePlaneDesc* planeDescArr,
                                                const LCEVC_PictureBufferDesc* bufferDesc)
{
    if (!BaseClass::bindMemory()) {
        return false;
    }

    // This should have already been validated. Normally, non-null is communicated by references,
    // so we wouldn't need this assert
    assert(bufferDesc != nullptr || planeDescArr != nullptr);

    // If we're rebinding, we need to reset our descs (for example, if we used to have a bufferDesc
    // but the client no longer wants us to know the bufferDesc).
    m_bufferDesc = nullptr;
    m_planeDescs = nullptr;

    if (bufferDesc != nullptr) {
        // Can't use make_unique (before C++20), it doesn't pass brace-initializers to structs.
        // NOLINTNEXTLINE(modernize-make-unique)
        m_bufferDesc = std::unique_ptr<PictureBufferDesc>(new PictureBufferDesc{
            bufferDesc->data, bufferDesc->byteSize, bufferDesc->accelBuffer.hdl, bufferDesc->access});
    }

    if (planeDescArr != nullptr) {
        m_planeDescs = std::make_unique<std::array<PicturePlaneDesc, kMaxNumPlanes>>();

        for (uint32_t planeIdx = 0; planeIdx < numPlanes; planeIdx++) {
            (*m_planeDescs)[planeIdx] = {planeDescArr[planeIdx].firstSample,
                                         planeDescArr[planeIdx].rowByteStride};
        }
    }

    return true;
}

bool PictureExternal::unbindMemory()
{
    if (!BaseClass::unbindMemory()) {
        return false;
    }

    m_bufferDesc = nullptr;
    m_planeDescs = nullptr;
    return true;
}

// - PictureManaged -------------------------------------------------------------------------------

PictureManaged::~PictureManaged()
{
    unlock();
    unbindMemory();
}

bool PictureManaged::getBufferDesc(LCEVC_PictureBufferDesc& bufferDescOut) const
{
    if (m_buffer == nullptr) {
        return false;
    }

    bufferDescOut.data = m_buffer->data();
    bufferDescOut.byteSize = static_cast<uint32_t>(m_buffer->size() * sizeof((*m_buffer)[0]));
    bufferDescOut.accelBuffer.hdl = kInvalidHandle;
    bufferDescOut.access = LCEVC_Access_Unknown;
    return true;
}

bool PictureManaged::getPlaneDescArr(PicturePlaneDesc planeDescArrOut[kMaxNumPlanes]) const
{
    for (uint32_t planeIdx = 0; planeIdx < getNumPlanes(); planeIdx++) {
        planeDescArrOut[planeIdx].firstSample = internalGetPlaneFirstSample(planeIdx);
        planeDescArrOut[planeIdx].rowByteStride = getPlaneByteStride(planeIdx);
    }
    return true;
}

bool PictureManaged::descsMatch(const LCEVC_PictureDesc& newDesc)
{
    if (m_layout.planes() == 0) {
        return false; // Picture isn't initialised so cannot match
    }
    LCEVC_PictureDesc curDesc;
    getDesc(curDesc);
    return equals(newDesc, curDesc);
}

bool PictureManaged::setDesc(const LCEVC_PictureDesc& newDesc)
{
    // Check for changes, then set descs, THEN bind (based on new descs).
    if (descsMatch(newDesc)) {
        return true;
    }

    if (!BaseClass::setDesc(newDesc)) {
        return false;
    }

    if (!unbindMemory()) {
        VNLogError("CC %u, PTS %" PRId64
                   ": Failed to unbind old memory for managed picture <%s>.\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   getShortDbgString().c_str());
        return false;
    }
    bindMemory();
    return true;
}

bool PictureManaged::bindMemory()
{
    if (!BaseClass::bindMemory()) {
        return false;
    }

    const uint32_t requiredSize = getRequiredSize();
    if (requiredSize == 0) {
        VNLogError("CC %u, PTS %" PRId64 ": Binding to nothing. Picture: <%s>\n",
                   timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
                   getShortDbgString().c_str());
    }

    // m_buffer might be set, if we are resizing.
    if (m_buffer == nullptr) {
        m_buffer = m_bufferManagerHandle.getBuffer(requiredSize);
    } else {
        m_buffer->clear();
        if (requiredSize > m_buffer->size()) {
            m_buffer->resize(requiredSize);
        }
    }
    VNLogTrace("CC %u, PTS %" PRId64 ": Allocated %" PRId64
               " total bytes. Picture full description: <%s>\n",
               timehandleGetCC(getTimehandle()), timehandleGetTimestamp(getTimehandle()),
               requiredSize, toString().c_str());
    return true;
}

bool PictureManaged::unbindMemory()
{
    if (!BaseClass::unbindMemory()) {
        return false;
    }

    bool ret = true;
    if (m_buffer != nullptr) {
        ret = m_bufferManagerHandle.releaseBuffer(m_buffer);
        m_buffer = nullptr;
    }
    return ret;
}

} // namespace lcevc_dec::decoder
