/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_PICTURE_H_
#define VN_API_PICTURE_H_

#include "handle.h"
#include "interface.h"
#include "uPictureFormatDesc.h"
#include "uTimestamps.h"
#include "uTypes.h"

#include <array>
#include <memory>
#include <string>

// ------------------------------------------------------------------------------------------------

struct LCEVC_PictureBufferDesc;
struct LCEVC_PictureDesc;
struct perseus_image;

namespace lcevc_dec::decoder {

class BufferManager;
class Decoder;
class Picture;
class PictureLock;

using PictureBuffer = lcevc_dec::api_utility::DataBuffer;

// - Picture --------------------------------------------------------------------------------------

class Picture
{
protected:
    static constexpr uint8_t kMaxNumPlanes = lcevc_dec::api_utility::PictureFormatDesc::kMaxNumPlanes;

public:
    Picture() = default;
    virtual ~Picture() = 0;
    Picture& operator=(const Picture&) = delete;
    Picture& operator=(Picture&&) = delete;
    Picture(const Picture&) = delete;
    Picture(Picture&&) = delete;

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
    void getDesc(LCEVC_PictureDesc& descOut) const;
    virtual bool setDesc(const LCEVC_PictureDesc& newDesc);

    // Note: All widths and heights are always post-cropping (whereas strides and memory sizes are
    // independent from cropping).
    uint32_t getWidth() const { return m_desc.GetWidth() - (m_crop.left + m_crop.right); }
    uint32_t getHeight() const { return m_desc.GetHeight() - (m_crop.top + m_crop.bottom); }
    uint32_t getBitdepth() const { return m_desc.GetBitDepth(); }
    uint32_t getBytedepth() const { return m_desc.GetByteDepth(); }
    uint32_t getNumPlanes() const { return m_desc.GetPlaneCount(); }
    uint32_t getPlaneWidth(uint32_t planeIndex) const
    {
        return m_desc.GetPlaneWidth(planeIndex) -
               ((m_crop.left + m_crop.right) >> planeHorizontalShift(m_desc.GetFormat(), planeIndex));
    }
    uint32_t getPlaneHeight(uint32_t planeIndex) const
    {
        return m_desc.GetPlaneHeight(planeIndex) -
               ((m_crop.top + m_crop.bottom) >> planeVerticalShift(m_desc.GetFormat(), planeIndex));
    }
    uint32_t getPlaneWidthBytes(uint32_t planeIndex) const { return getPlaneWidth(planeIndex) * getBytedepth(); }
    uint32_t getPlaneBytesPerPixel(uint32_t planeIndex) const
    {
        // Bytes per pixel, where "UVUVUV" is considered 3 pixels wide. So, that's samples per
        // pixel times bytes per sample.
        return m_desc.GetPlanePixelStride(planeIndex) * m_desc.GetByteDepth();
    }
    uint32_t getPlaneByteStride(uint32_t planeIndex) const
    {
        // Bytes per row for this plane (also called row byte stride).
        return m_desc.GetPlaneStrideBytes(planeIndex);
    }
    uint32_t getPlaneSampleStride(uint32_t planeIndex) const
    {
        // Samples per row for this plane (if not interleaved, this equals planePixelStride).
        // (bytes/row) / (bytes/sample) = samples/row
        // An equivalent formula would be (samples/pixel)*(pixels/row)=samples/row, like so:
        // return m_pictureDesc.GetPlaneStridePixels(i) * m_pictureDesc.GetPlanePixelStride(i);
        return m_desc.GetPlaneStrideBytes(planeIndex) / m_desc.GetByteDepth();
    }
    uint32_t getPlaneMemorySize(uint32_t planeIndex) const { return m_desc.GetPlaneMemorySize(planeIndex); }
    uint8_t* getPlaneFirstSample(uint32_t planeIdx) { return internalGetPlaneFirstSample(planeIdx); }
    const uint8_t* getPlaneFirstSample(uint32_t planeIdx) const { return internalGetPlaneFirstSample(planeIdx); }

    virtual bool getBufferDesc(LCEVC_PictureBufferDesc& bufferDescOut) const = 0;
    virtual bool getPlaneDescArr(PicturePlaneDesc planeDescArrOut [kMaxNumPlanes]) const = 0;

    void* getUserData() const { return m_userData; }
    void setUserData(void* userData) { m_userData = userData; }
    uint64_t getTimehandle() const { return m_timehandle; }
    void setTimehandle(uint64_t timehandle) { m_timehandle = timehandle; }
    // clang-format on

    // Access management
    bool lock(Access newAccess, Handle<PictureLock> newLock);
    Handle<PictureLock> getLock() const { return m_lock; }
    bool unlock();
    virtual bool canModify() const { return !isLocked(); }

    // Memory allocation
    // TODO: During DEC-363, make it so that "unbind" is the first step of "bind", since we're
    // currently doing this manually, for both Picture types.
    virtual bool bindMemory();
    virtual bool unbindMemory();

protected:
    void setName(const std::string& name);

    virtual bool
    setDesc(const LCEVC_PictureDesc& newDesc,
            const uint32_t planeStridesBytes[lcevc_dec::api_utility::PictureFormatDesc::kMaxNumPlanes]);

    bool isLocked() const { return m_lock != kInvalidHandle; }

    // These are strictly internal, so it's fine to return non-const data of a const Picture.
    virtual uint8_t* internalGetPlaneFirstSample(uint32_t planeIdx) const;
    virtual uint8_t* getBuffer() const = 0;

    uint32_t getRequiredSize() const;

    static uint32_t planeHorizontalShift(lcevc_dec::api_utility::PictureFormat::Enum format,
                                         uint32_t planeIndex);
    static uint32_t planeVerticalShift(lcevc_dec::api_utility::PictureFormat::Enum format,
                                       uint32_t planeIndex);

    lcevc_dec::api_utility::PictureFormatDesc m_desc = {};

private:
    // This initializer is NOT lazy: it will set the desc without checking if it's changed or not.
    bool initializeDesc(
        const LCEVC_PictureDesc& desc,
        const uint32_t planeStridesPixels[lcevc_dec::api_utility::PictureFormatDesc::kMaxNumPlanes] = nullptr);

    // Identifying data
    uint64_t m_timehandle = lcevc_dec::api_utility::kInvalidTimehandle;
    void* m_userData = nullptr;
    std::string m_name = "unknown";

    // Format information
    ColorRange m_colorRange = ColorRange::Unknown;
    MatrixCoefficients m_matrixCoefficients = MatrixCoefficients::Unknown;
    TransferCharacteristics m_transferCharacteristics = TransferCharacteristics::Unknown;
    uint8_t m_publicFlags = 0;
    HDRStaticInfo m_hdrStaticInfo = {};
    AspectRatio m_sampleAspectRatio = {1, 1};
    lcevc_dec::api_utility::Margins m_crop = {};

    // State
    Handle<PictureLock> m_lock = kInvalidHandle;
};

class PictureExternal : public Picture
{
    using BaseClass = Picture;

public:
    PictureExternal() = default;
    ~PictureExternal() override;
    PictureExternal& operator=(const PictureExternal&) = delete;
    PictureExternal& operator=(PictureExternal&&) = delete;
    PictureExternal(const PictureExternal&) = delete;
    PictureExternal(PictureExternal&&) = delete;

    bool isManaged() const final { return false; }
    bool setDesc(const LCEVC_PictureDesc& newDesc) override;
    bool setDescExternal(const LCEVC_PictureDesc& newDesc, const LCEVC_PicturePlaneDesc* newPlaneDescArr,
                         const LCEVC_PictureBufferDesc* newBufferDesc);

    bool getBufferDesc(LCEVC_PictureBufferDesc& bufferDescOut) const final;
    bool getPlaneDescArr(PicturePlaneDesc planeDescArrOut[kMaxNumPlanes]) const final;

protected:
    bool bindMemoryBufferAndPlanes(uint32_t numPlanes, const LCEVC_PicturePlaneDesc* planeDescArr,
                                   const LCEVC_PictureBufferDesc* bufferDesc);
    bool unbindMemory() final;

    bool descsMatch(const LCEVC_PictureDesc& newDesc, const LCEVC_PicturePlaneDesc* newPlaneDescArr,
                    const LCEVC_PictureBufferDesc* newBufferDesc);

    bool setDesc(const LCEVC_PictureDesc& newDesc,
                 const uint32_t planeStridesPixels[lcevc_dec::api_utility::PictureFormatDesc::kMaxNumPlanes]) override;

    uint8_t* getBuffer() const final { return (m_bufferDesc ? m_bufferDesc->data : nullptr); }
    uint8_t* internalGetPlaneFirstSample(uint32_t planeIdx) const final;

private:
    std::unique_ptr<PictureBufferDesc> m_bufferDesc = nullptr;
    std::unique_ptr<std::array<PicturePlaneDesc, kMaxNumPlanes>> m_planeDescs = nullptr;
};

class PictureManaged : public Picture
{
    using BaseClass = Picture;

public:
    PictureManaged(BufferManager& bufferManagerHandle)
        : m_bufferManagerHandle(bufferManagerHandle)
    {}
    ~PictureManaged() override;
    PictureManaged& operator=(const PictureManaged&) = delete;
    PictureManaged& operator=(PictureManaged&&) = delete;
    PictureManaged(const PictureManaged&) = delete;
    PictureManaged(PictureManaged&&) = delete;

    bool isManaged() const final { return true; }

    bool setDesc(const LCEVC_PictureDesc& newDesc) override;

    bool getBufferDesc(LCEVC_PictureBufferDesc& bufferDescOut) const final;
    bool getPlaneDescArr(PicturePlaneDesc planeDescArrOut[kMaxNumPlanes]) const final;

protected:
    bool bindMemory() final;
    bool unbindMemory() final;

    bool descsMatch(const LCEVC_PictureDesc& newDesc);

    uint8_t* getBuffer() const final { return (m_buffer ? m_buffer->data() : nullptr); }

private:
    // This is a reference to the BufferManager which we want to manage our buffers for us.
    BufferManager& m_bufferManagerHandle;

    PictureBuffer* m_buffer = nullptr;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_PICTURE_H_
