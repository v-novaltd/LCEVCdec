/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#ifndef VN_LCEVC_PIPELINE_VULKAN_TEST_UTILITY_H
#define VN_LCEVC_PIPELINE_VULKAN_TEST_UTILITY_H

#include <LCEVC/pipeline/types.h>
#include <LCEVC/utility/md5.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

namespace vulkan_test_util {

inline std::string hashMd5(const uint8_t* data, size_t size)
{
    lcevc_dec::utility::MD5 hash;
    hash.reset();
    hash.update(data, size);

    return hash.hexDigest();
}

template <typename T>
inline std::vector<T> generateYUV420FromFixedSeed(uint32_t width, uint32_t height, int frames = 1)
{
    static_assert(std::is_integral<T>::value, "T must be an integral type");

    T min = 0;
    T max = 0;

    if constexpr (std::is_same_v<T, uint8_t>) { // 8 bit external
        min = 0;
        max = 255;
    } else if constexpr (std::is_same_v<T, uint16_t>) { // 10 bit external
        min = 0;
        max = 1023;
    } else if constexpr (std::is_same_v<T, int16_t>) { // 16 bit internal
        min = -16384;                                  // INT15_MIN;
        max = 16383;                                   // INT15_MAX;
    } else {
        static_assert(!std::is_same_v<T, T>, "Unsupported type.");
    }

    std::mt19937 rng(123456);
    std::uniform_int_distribution<int> dist(min, max);

    int numPixels = frames * 3 * width * height / 2;
    std::vector<T> pixels(numPixels);
    for (int i = 0; i < numPixels; ++i) {
        pixels[i] = static_cast<T>(dist(rng));
    }

    return pixels;
}

inline std::vector<uint8_t> readRaw(const char* filename) // TODO - use RawReader
{
    std::ifstream input(filename, std::ios::binary);
    if (!input) {
        std::cout << "Current working directory: " << std::filesystem::current_path() << "\n";
        throw std::runtime_error("Failed to open file");
    }
    std::vector<char> temp(std::istreambuf_iterator<char>(input), {});
    std::vector<uint8_t> buffer(temp.begin(), temp.end());
    return buffer;
}

inline void writeRaw(const char* filename, const uint8_t* data, size_t size) // TODO - use RawWriter
{
    std::ofstream output(filename, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to open file for writing");
    }

    output.write((const char*)data, size);
    if (!output) {
        throw std::runtime_error("Failed to write data to file");
    }
}

} // namespace vulkan_test_util
#endif // VN_LCEVC_PIPELINE_VULKAN_TEST_UTILITY_H
