/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

// libFuzzer entry point for the enhancement parse + decode path. Each fuzzer input is treated as
// a sequence of LCEVC NAL unit payloads separated by the literal bytes 0x000001: the harness
// frames each segment with the 0x000001 start code and a 0x80 trailing byte (the envelope every
// fixed bitstream in test_parser_malformed.cpp uses), then drives them in order through one
// decoder - ldeConfigsParse and, on success, ldeDecodeEnhancement for every enabled
// loq/plane/tile, mirroring the MalformedDecoder harness in that test - so later frames decode
// against the global config established by earlier ones. A separator-free input yields a single
// segment and behaves exactly as the previous single-frame harness; emulation prevention
// guarantees the separator cannot occur inside a valid NAL body. Each sequence is run once
// unseeded, then again after each valid start block from the malformed parser tests.

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/config_types.h>
#include <LCEVC/enhancement/decode.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

struct NalBuffer
{
    const uint8_t* data;
    size_t size;
};

template <size_t N>
constexpr NalBuffer nal(const uint8_t (&arr)[N])
{
    return NalBuffer{arr, N};
}

const uint8_t startBlockTemporal[] = {
    0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00, 0x04, 0xB4, 0x00, 0x50, 0x00, 0x01, 0x40,
    0x04, 0x40, 0xA1, 0x34, 0x57, 0x00, 0x00, 0x53, 0xE2, 0x0C, 0x52, 0xFF, 0xFF, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x03, 0x00, 0x1E, 0x1E, 0x32, 0x41, 0xE3, 0x07, 0x55, 0xD5, 0x04, 0xC0,
    0x9F, 0xD1, 0x7F, 0xE5, 0x06, 0x01, 0x35, 0x09, 0x10, 0x09, 0x00, 0x80};

const uint8_t startBlockTemporalEnabledGlobalOnly[] = {
    0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00, 0x04, 0xB4, 0x00, 0x50,
    0x00, 0x01, 0x40, 0x04, 0x40, 0xA1, 0x34, 0x57, 0x40, 0x00, 0x53, 0x80};

const uint8_t startBlockTiledCompressedGlobalOnly[] = {
    0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00, 0x04, 0xB4, 0x00, 0x50, 0x00,
    0x01, 0x40, 0x04, 0x40, 0xE1, 0x06, 0x08, 0x57, 0x00, 0x10, 0x53, 0x05, 0x80};

const uint8_t startBlockTiledDDS[] = {
    0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00, 0x04, 0xB4, 0x00, 0x50, 0x00, 0x01, 0x40, 0x04,
    0x40, 0xE1, 0x0D, 0x23, 0x43, 0xE0, 0x80, 0x00, 0x06, 0xD8, 0x39, 0x50, 0x0F, 0xD1, 0x02, 0x49,
    0xA2, 0x1A, 0x0A, 0xEF, 0x00, 0x42, 0xE3, 0x81, 0x10, 0x55, 0x55, 0x55, 0x55, 0x55, 0x75, 0x5F,
    0x55, 0x0A, 0xC0, 0x81, 0x74, 0xBE, 0xD4, 0x17, 0xBA, 0x82, 0xEB, 0x72, 0x71, 0xC0, 0x88, 0x03,
    0xC4, 0xBA, 0x36, 0xC2, 0x81, 0x00, 0xC2, 0x93, 0x62, 0xBE, 0x88, 0x21, 0xBC, 0xA1, 0x7B, 0xBE,
    0xB3, 0x51, 0xBE, 0xD7, 0x68, 0xBE, 0x8F, 0x7B, 0xBE, 0x95, 0x1F, 0xBE, 0x3C, 0xC4, 0x5D, 0x42,
    0xBE, 0x21, 0xC2, 0x96, 0x1D, 0xC2, 0x58, 0xBE, 0x6E, 0xC4, 0x84, 0x24, 0xBE, 0x8A, 0x7C, 0xBE,
    0x83, 0x2D, 0xBE, 0x83, 0x1A, 0xBE, 0x3F, 0xBE, 0x5C, 0x3C, 0xBC, 0x03, 0xBE, 0x81, 0x1B, 0xBE,
    0x8B, 0x6E, 0xBE, 0x96, 0x2C, 0xC2, 0x81, 0x07, 0xBE, 0x4D, 0xC2, 0x83, 0x0C, 0x3E, 0xBE, 0x81,
    0x3F, 0xC2, 0x89, 0x2B, 0xBE, 0x81, 0x56, 0xBC, 0x01, 0xBE, 0x81, 0x79, 0xBE, 0x81, 0x10, 0xC2,
    0x81, 0x07, 0xBE, 0x81, 0x34, 0xC2, 0x8D, 0x33, 0xC2, 0x91, 0x70, 0xBE, 0x85, 0x6C, 0x0A, 0xC0,
    0xC2, 0x3A, 0xC2, 0x81, 0x00, 0xC2, 0x82, 0xFE, 0x43, 0x45, 0x17, 0x42, 0x80};

const uint8_t startBlockSeiVersion0[] = {0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00,
                                         0x04, 0xB4, 0x00, 0x50, 0x00, 0x00, 0x80};
const uint8_t startBlockSeiVersion1[] = {0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00,
                                         0x04, 0xB4, 0x00, 0x50, 0x00, 0x01, 0x80};

const NalBuffer startBlocks[] = {
    nal(startBlockTemporal),
    nal(startBlockTemporalEnabledGlobalOnly),
    nal(startBlockTiledCompressedGlobalOnly),
    nal(startBlockTiledDDS),
    nal(startBlockSeiVersion0),
    nal(startBlockSeiVersion1),
};

// Parses a single framed NAL unit, then decodes every enabled LOQ/plane/tile into a CPU command
// buffer. Allocations are released in the destructor so LeakSanitizer can attribute per-iteration
// leaks. This deliberately tracks MalformedDecoder in test_parser_malformed.cpp.
class FuzzDecoder
{
public:
    FuzzDecoder()
    {
        m_allocator = ldcMemoryAllocatorMalloc();
        ldcDiagnosticsLogLevel(LdcLogLevelNone);
        ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &m_globalConfig);
        ldeFrameConfigInitialize(m_allocator, &m_frameConfig);
        m_frameConfig.chunkAllocation = m_chunkAllocation;
        ldeCmdBufferCpuInitialize(m_allocator, &m_cmdBuffer, 0);
    }

    ~FuzzDecoder()
    {
        ldeCmdBufferCpuFree(&m_cmdBuffer);
        ldeConfigsReleaseFrame(&m_frameConfig);
    }

    FuzzDecoder(const FuzzDecoder&) = delete;
    FuzzDecoder& operator=(const FuzzDecoder&) = delete;

    bool run(const uint8_t* data, size_t size)
    {
        bool globalConfigModified = false;
        if (!ldeConfigsParse(data, size, &m_globalConfig, &m_frameConfig, &globalConfigModified)) {
            return false;
        }
        decodeAll();
        return true;
    }

private:
    void decodeAll()
    {
        for (const LdeLOQIndex loq : {LOQ1, LOQ0}) {
            for (uint32_t plane = 0; plane < m_globalConfig.numPlanes; ++plane) {
                const uint32_t numTiles = m_globalConfig.numTiles[plane][loq];
                for (uint32_t tile = 0; tile < numTiles; ++tile) {
                    ldeCmdBufferCpuReset(&m_cmdBuffer, m_globalConfig.numLayers);
                    ldeDecodeEnhancement(&m_globalConfig, &m_frameConfig, loq, plane, tile,
                                         &m_cmdBuffer, nullptr, nullptr);
                }
            }
        }
    }

    LdcMemoryAllocator* m_allocator{};
    LdcMemoryAllocation m_chunkAllocation = {0};
    LdeGlobalConfig m_globalConfig = {};
    LdeFrameConfig m_frameConfig = {};
    LdeCmdBufferCpu m_cmdBuffer = {};
};

// Run with a fresh uninitialized globalConfig and with it seeded to several key startBlocks
void runAllSeeds(const std::vector<std::vector<uint8_t>>& nals)
{
    {
        FuzzDecoder decoder;
        for (const std::vector<uint8_t>& nal : nals) {
            decoder.run(nal.data(), nal.size());
        }
    }

    for (const NalBuffer& startBlock : startBlocks) {
        FuzzDecoder decoder;
        if (decoder.run(startBlock.data, startBlock.size)) {
            for (const std::vector<uint8_t>& nal : nals) {
                decoder.run(nal.data(), nal.size());
            }
        }
    }
}

// Frame a payload segment as a single NAL unit: 0x000001 start code + payload + 0x80 trailer.
std::vector<uint8_t> frameNal(const uint8_t* data, size_t size)
{
    std::vector<uint8_t> nal;
    nal.reserve(size + 4);
    nal.push_back(0x00);
    nal.push_back(0x00);
    nal.push_back(0x01);
    nal.insert(nal.end(), data, data + size);
    nal.push_back(0x80);
    return nal;
}

// Split the input into payload segments on the literal 0x000001 separator. An input without the
// separator yields one segment, matching the previous single-frame harness byte-for-byte.
std::vector<std::vector<uint8_t>> splitNals(const uint8_t* data, size_t size)
{
    std::vector<std::vector<uint8_t>> nals;
    size_t segmentStart = 0;
    for (size_t pos = 0; size >= 3 && pos <= size - 3;) {
        if (data[pos] == 0x00 && data[pos + 1] == 0x00 && data[pos + 2] == 0x01) {
            nals.push_back(frameNal(data + segmentStart, pos - segmentStart));
            pos += 3;
            segmentStart = pos;
        } else {
            ++pos;
        }
    }
    nals.push_back(frameNal(data + segmentStart, size - segmentStart));
    return nals;
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/)
{
    // The diagnostics subsystem must be initialized before any log call - otherwise the global
    // state pointer is null and ldcDiagnosticsLogLevel dereferences it. In the unit tests
    // gtest_main does this; here the harness owns process setup, so do it once at startup.
    ldcDiagnosticsInitialize(nullptr);
    // Silence per-run logging - error level only, matching the malformed-bitstream unit tests.
    ldcDiagnosticsLogLevel(LdcLogLevelError);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    runAllSeeds(splitNals(data, size));
    return 0;
}
