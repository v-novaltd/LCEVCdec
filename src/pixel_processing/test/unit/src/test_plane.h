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

#ifndef VN_LCEVC_PIXEL_PROCESSING_TEST_PLANE_H
#define VN_LCEVC_PIXEL_PROCESSING_TEST_PLANE_H

#include "fp_types.h"

#include <LCEVC/pipeline/types.h>
#include <LCEVC/utility/md5.h>
#include <rng.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>

struct TestPlane
{
    LdpPicturePlaneDesc planeDesc;
    LdpFixedPoint fixedPoint;
    uint32_t width;
    uint32_t height;

    size_t size() const { return planeDesc.rowByteStride * height; }
    void initialize(uint32_t planeWidth, uint32_t planeHeight, uint32_t stride, LdpFixedPoint fp)
    {
        auto size = fixedPointByteSize(fp);
        const size_t planeSize = stride * planeHeight * size;
        planeDesc.firstSample = new uint8_t[planeSize];
        memset(planeDesc.firstSample, 0, planeSize);
        planeDesc.rowByteStride = stride * size;

        width = planeWidth;
        height = planeHeight;
        fixedPoint = fp;
    }

    ~TestPlane() { delete[] planeDesc.firstSample; }
};

template <typename T>
void fillPlaneWithNoiseT(TestPlane& plane)
{
    T* dst = (T*)plane.planeDesc.firstSample;
    const uint32_t count = (plane.planeDesc.rowByteStride * plane.height) / sizeof(T);

    lcevc_dec::utility::RNG data(static_cast<uint32_t>(fixedPointMaxValue(plane.fixedPoint)));

    const int32_t offset = fixedPointOffset(plane.fixedPoint);

    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = static_cast<T>(data() - offset);
    }
}

using PlaneNoiseFunctionT = std::function<void(TestPlane&)>;

const PlaneNoiseFunctionT kPlaneNoiseFunctions[LdpFPCount] = {
    &fillPlaneWithNoiseT<uint8_t>,  &fillPlaneWithNoiseT<uint16_t>, &fillPlaneWithNoiseT<uint16_t>,
    &fillPlaneWithNoiseT<uint16_t>, &fillPlaneWithNoiseT<int16_t>,  &fillPlaneWithNoiseT<int16_t>,
    &fillPlaneWithNoiseT<int16_t>,  &fillPlaneWithNoiseT<int16_t>,
};

inline void fillPlaneWithNoise(TestPlane& plane)
{
    if (plane.planeDesc.firstSample) {
        kPlaneNoiseFunctions[plane.fixedPoint](plane);
    }
}

inline void readBinaryFile(TestPlane& plane, std::string_view filePath)
{
    uint32_t fileSizeRemaining = static_cast<uint32_t>(std::filesystem::file_size(filePath.data()));
    uint32_t linesRemaining = plane.height;
    uint8_t* pixelPtr = plane.planeDesc.firstSample;
    auto pelSize = fixedPointByteSize(plane.fixedPoint);
    auto lineSize = pelSize * plane.width;

    std::ifstream file(filePath.data(), std::ifstream::binary);

    while (fileSizeRemaining > 0 && linesRemaining > 0) {
        file.read(static_cast<char*>(static_cast<void*>(pixelPtr)), lineSize);

        fileSizeRemaining -= lineSize;
        pixelPtr += plane.planeDesc.rowByteStride;
        linesRemaining--;
    }
}

inline std::string hashActiveRegion(TestPlane& plane)
{
    lcevc_dec::utility::MD5 hash;
    hash.reset();
    auto pelSize = fixedPointByteSize(plane.fixedPoint);
    auto* ptr = plane.planeDesc.firstSample;
    auto stride = plane.planeDesc.rowByteStride;
    for (uint32_t line = 0; line < plane.height; line++) {
        hash.update(ptr + line * stride, pelSize * plane.width);
    }

    return hash.hexDigest();
}

inline void writeBinaryFile(TestPlane& plane, std::string_view filePath)
{
    std::ofstream file(filePath.data(), std::ofstream::binary);
    uint32_t linesRemaining = plane.height;
    auto pelSize = fixedPointByteSize(plane.fixedPoint);
    auto writeSize = pelSize * plane.width;
    uint8_t* pixelPtr = plane.planeDesc.firstSample;

    while (linesRemaining) {
        file.write(static_cast<char*>(static_cast<void*>(pixelPtr)), writeSize);
        pixelPtr += plane.planeDesc.rowByteStride;
        linesRemaining--;
    }
}
#endif // VN_LCEVC_PIXEL_PROCESSING_TEST_PLANE_H
