/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

#ifndef VN_LCEVC_PIPELINE_LEGACY_PICTURE_H
#define VN_LCEVC_PIPELINE_LEGACY_PICTURE_H

//
#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/constants.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/picture_layout.hpp>
#include <LCEVC/pipeline/types.h>
//
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

// ------------------------------------------------------------------------------------------------

struct LCEVC_PictureBufferDesc;
struct LCEVC_PictureDesc;
struct perseus_image;

namespace lcevc_dec::decoder {

class BufferManager;
class Decoder;
class PictureLock;

using PictureBuffer = std::vector<uint8_t>;

// - Picture --------------------------------------------------------------------------------------
//
class Picture : public LdpPicture
{
public:
    Picture();
    virtual ~Picture() = 0;

    bool copyMetadata(const Picture& source);
    bool copyData(const Picture& source);
    bool toCoreImage(perseus_image& dest);

    bool isValid() const;

    virtual bool isManaged() const = 0;

    // Getters and setters. We want inline getters, but they bloat the header if they take multiple
    // lines, hence format disabling here.
    // clang-format off

    std::string getShortDbgString() const;
    std::string toString() const;

    bool getPublicFlag(uint8_t flag) const;
    void setPublicFlag(uint8_t flag, bool value);

    // The base implementation of setDesc is not lazy (i.e. it will not check whether descriptions
    // have changed before setting). However, ALL CHILD IMPLEMENTATIONS should be lazy-setters.
    void getDesc(LdpPictureDesc& descOut) const;
    virtual bool setDesc(const LdpPictureDesc& newDesc);

    // Note: All widths and heights are always post-cropping (whereas strides and memory sizes are
    // independent of cropping).
    uint32_t getWidth() const { return ldpPictureLayoutWidth(&layout) - (margins.left + margins.right); }
    uint32_t getHeight() const { return ldpPictureLayoutHeight(&layout) - (margins.top + margins.bottom); }
    uint8_t getBitdepth() const { return ldpPictureLayoutSampleBits(&layout); }
    uint8_t getBytedepth() const { return ldpPictureLayoutSampleSize(&layout); }
    uint8_t getNumPlanes() const { return ldpPictureLayoutPlanes(&layout); }
    uint32_t getPlaneWidth(uint32_t planeIndex) const
    {
        return ldpPictureLayoutPlaneWidth(&layout, planeIndex) -
               ((margins.left + margins.right) >> ldpColorFormatPlaneWidthShift(ldpPictureLayoutFormat(&layout), planeIndex));
    }
    uint32_t getPlaneHeight(uint32_t planeIndex) const
    {
        return ldpPictureLayoutPlaneHeight(&layout, planeIndex) -
               ((margins.top + margins.bottom) >> ldpColorFormatPlaneHeightShift(ldpPictureLayoutFormat(&layout), planeIndex));
    }
    uint32_t getPlaneWidthBytes(uint32_t planeIndex) const { return getPlaneWidth(planeIndex) * getBytedepth(); }
    uint32_t getPlaneBytesPerPixel(uint32_t planeIndex) const
    {
        // Bytes per pixel, where "UVUVUV" is considered 3 pixels wide. So, that's samples per
        // pixel times bytes per sample.
        return ldpPictureLayoutSampleStride(&layout, planeIndex);
    }
    uint32_t getPlaneByteStride(uint32_t planeIndex) const
    {
        // Bytes per row for this plane (also called row byte stride).
        return ldpPictureLayoutRowStride(&layout, planeIndex);
    }
    uint32_t getPlaneSampleStride(uint32_t planeIndex) const
    {
        // Samples per row for this plane (if not interleaved, this equals planePixelStride).
        // (bytes/row) / (bytes/sample) = samples/row
        // An equivalent formula would be (samples/pixel)*(pixels/row)=samples/row, like so:
        // return m_pictureDesc.GetPlaneStridePixels(i) * m_pictureDesc.GetPlanePixelStride(i);
        return ldpPictureLayoutRowStride(&layout, planeIndex) / ldpPictureLayoutSampleSize(&layout);
    }
    uint32_t getPlaneMemorySize(uint32_t planeIndex) const { return ldpPictureLayoutPlaneSize(&layout, planeIndex); }
    uint8_t* getPlaneFirstSample(uint32_t planeIdx) { return internalGetPlaneFirstSample(planeIdx); }
    const uint8_t* getPlaneFirstSample(uint32_t planeIdx) const { return internalGetPlaneFirstSample(planeIdx); }

    virtual bool getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const = 0;
    virtual bool getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut [kLdpPictureMaxNumPlanes]) const = 0;

    void* getUserData() const { return userData; }
    void setUserData(void* val) { userData = val; }
    uint64_t getTimestamp() const { return m_timestamp; }
    void setTimestamp(uint64_t timestamp) { m_timestamp = timestamp; }
    // clang-format on

    // Access management
    bool lock(LdpAccess access, PictureLock*& lockOut);
    bool unlock(const PictureLock* lock);
    PictureLock* getLock() const { return m_lock.get(); }
    virtual bool canModify() const { return !isLocked(); }

    // Memory allocation
    virtual bool bindMemory();
    virtual bool unbindMemory();

    VNNoCopyNoMove(Picture);

protected:
    void setName(const std::string& name);

    virtual bool setDesc(const LdpPictureDesc& newDesc,
                         const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes]);

    bool isLocked() const { return m_lock != nullptr; }

    // These are strictly internal, so it's fine to return non-const data of a const Picture.
    virtual uint8_t* internalGetPlaneFirstSample(uint32_t planeIdx) const;
    virtual uint8_t* getBuffer() const = 0;

    uint32_t getRequiredSize() const;

private:
    // This initializer is NOT lazy: it will set the desc without checking if it's changed or not.
    bool initializeDesc(const LdpPictureDesc& desc,
                        const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes] = nullptr);

    // Timestamp
    uint64_t m_timestamp = kInvalidTimestamp;

    // Debugging name
    std::string m_name = "unknown";

    // Any current lock
    std::unique_ptr<PictureLock> m_lock;
};

// Wrappers for casting LdpPicture -> Picture
//
// NB: this will unpick vtbl offsets - the addresses will not be the same.
//
static inline Picture* fromLdpPicturePtr(LdpPicture* ldpPicture)
{
    return static_cast<Picture*>(ldpPicture);
}

static inline const Picture* fromLdpPicturePtr(const LdpPicture* ldpPicture)
{
    return static_cast<const Picture*>(ldpPicture);
}

class PictureExternal : public Picture
{
    using BaseClass = Picture;

public:
    PictureExternal() = default;
    ~PictureExternal() override;

    bool isManaged() const final { return false; }
    bool setDesc(const LdpPictureDesc& newDesc) override;
    bool setDescExternal(const LdpPictureDesc& newDesc, const LdpPicturePlaneDesc* newPlaneDescArr,
                         const LdpPictureBufferDesc* newBufferDesc);

    bool getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const final;
    bool getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes]) const final;

    VNNoCopyNoMove(PictureExternal);

protected:
    bool bindMemoryBufferAndPlanes(uint32_t numPlanes, const LdpPicturePlaneDesc* planeDescArr,
                                   const LdpPictureBufferDesc* bufferDesc);
    bool unbindMemory() final;

    bool descsMatch(const LdpPictureDesc& newDesc, const LdpPicturePlaneDesc* newPlaneDescArr,
                    const LdpPictureBufferDesc* newBufferDesc);

    bool setDesc(const LdpPictureDesc& newDesc,
                 const uint32_t rowStridesBytes[kLdpPictureMaxNumPlanes]) override;

    uint8_t* getBuffer() const final { return (m_bufferDesc ? m_bufferDesc->data : nullptr); }
    uint8_t* internalGetPlaneFirstSample(uint32_t planeIdx) const final;

private:
    std::unique_ptr<LdpPictureBufferDesc> m_bufferDesc = nullptr;
    std::unique_ptr<std::array<LdpPicturePlaneDesc, kLdpPictureMaxNumPlanes>> m_planeDescs = nullptr;
};

class PictureManaged : public Picture
{
    using BaseClass = Picture;

public:
    PictureManaged(BufferManager& bufferManagerHandle)
        : m_bufferManagerHandle(bufferManagerHandle)
    {}
    ~PictureManaged() override;

    bool isManaged() const final { return true; }

    bool setDesc(const LdpPictureDesc& newDesc) override;

    bool getBufferDesc(LdpPictureBufferDesc& bufferDescOut) const final;
    bool getPlaneDescArr(LdpPicturePlaneDesc planeDescArrOut[kLdpPictureMaxNumPlanes]) const final;

    VNNoCopyNoMove(PictureManaged);

protected:
    bool bindMemory() final;
    bool unbindMemory() final;

    bool descsMatch(const LdpPictureDesc& newDesc);

    uint8_t* getBuffer() const final { return (m_buffer ? m_buffer->data() : nullptr); }

private:
    // This is a reference to the BufferManager which we want to manage our buffers for us.
    BufferManager& m_bufferManagerHandle;

    PictureBuffer* m_buffer = nullptr;
};

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_PIPELINE_LEGACY_PICTURE_H
