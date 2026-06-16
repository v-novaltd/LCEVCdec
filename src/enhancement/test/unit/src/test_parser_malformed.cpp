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

// Feeds deliberately malformed LCEVC NAL units through the enhancement parse + decode path to
// exercise error handling.

#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/config_types.h>
#include <LCEVC/enhancement/decode.h>

#include <cstdint>
#include <vector>

// Malformed LCEVC bitstreams

namespace {

// Prerequisite IDR config blocks that establish a global config before a temporal-only frame.
const uint8_t startBlockTemporal[] = {
    0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00, 0x04, 0xB4, 0x00, 0x50, 0x00, 0x01, 0x40,
    0x04, 0x40, 0xA1, 0x34, 0x57, 0x00, 0x00, 0x53, 0xE2, 0x0C, 0x52, 0xFF, 0xFF, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x03, 0x00, 0x1E, 0x1E, 0x32, 0x41, 0xE3, 0x07, 0x55, 0xD5, 0x04, 0xC0,
    0x9F, 0xD1, 0x7F, 0xE5, 0x06, 0x01, 0x35, 0x09, 0x10, 0x09, 0x00, 0x80};

// Non-tiled global config block with temporal_enabled_flag set, used before a temporal-only frame.
const uint8_t startBlockTemporalEnabledGlobalOnly[] = {
    0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00, 0x04, 0xB4, 0x00, 0x50,
    0x00, 0x01, 0x40, 0x04, 0x40, 0xA1, 0x34, 0x57, 0x40, 0x00, 0x53, 0x80};

// Tiled global config with per-tile entropy flags RLE-compressed and per-tile sizes compressed.
const uint8_t startBlockTiledCompressedGlobalOnly[] = {
    0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00, 0x04, 0xB4, 0x00, 0x50, 0x00,
    0x01, 0x40, 0x04, 0x40, 0xE1, 0x06, 0x08, 0x57, 0x00, 0x10, 0x53, 0x05, 0x80};

// Tiled DDS IDR config block (1280x720).
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

// Start blocks for various version types
const uint8_t startBlockSeiVersion0[] = {0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00,
                                         0x04, 0xB4, 0x00, 0x50, 0x00, 0x00, 0x80};
const uint8_t startBlockSeiVersion1[] = {0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00,
                                         0x04, 0xB4, 0x00, 0x50, 0x00, 0x01, 0x80};
// Version byte 0x03 is above BitstreamVersionCurrent (2): the SEI parse must reject it.
const uint8_t startBlockSeiVersionUnsupported[] = {0x00, 0x00, 0x01, 0x7B, 0xFF, 0xE5, 0x07, 0x00,
                                                   0x04, 0xB4, 0x00, 0x50, 0x00, 0x03, 0x80};

// Residual chunk signals a 1-byte payload but no residual payload bytes remain. Should fail while
// parsing encoded_data.
const uint8_t malformedTruncatedChunkNalData[] = {
    0x00,       0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80,
    0b01100010, 0x12, 0xFF, 0xFE,       0b01100011, 0x80,       0x00, 0x01, 0x80};

// First residual chunk is Huffman-coded but zero size.
const uint8_t malformedHuffmanZeroSizeNalData[] = {
    0x00,       0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80,
    0b01100010, 0x12, 0xFF, 0xFE,       0b01100011, 0x80,       0x00, 0x00, 0x80};

// First residual chunk is Huffman-coded but zero size, followed by a valid RLE-only chunk.
const uint8_t malformedHuffmanZeroSizeWithResidualNalData[] = {
    0x00, 0x00, 0x01,       0b01111011, 0b11111111, 0b10000001, 0x08,       0x40,
    0x08, 0x80, 0b01100010, 0x12,       0xFF,       0xFE,       0b11100011, 0x07,
    0xB0, 0x00, 0x00,       0x03,       0xC0,       0xF0,       0x3F,       0x80};

// First residual chunk is Huffman-coded and non-empty, but the RL table is internally inconsistent
// (min == max code length == 1, symbol_count == 0).
const uint8_t malformedInvalidRlHuffmanNalData[] = {
    0x00, 0x00,       0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08,
    0x80, 0b01100010, 0x12, 0xFF,       0xFE,       0b11100011, 0x0A, 0x80, 0x00,
    0x07, 0x00,       0x20, 0x00,       0x00,       0x00,       0x84, 0x00, 0x80};

// As above, plus a trailing valid RLE-only residual chunk.
const uint8_t malformedInvalidRlHuffmanWithResidualNalData[] = {
    0x00, 0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80, 0b01100010,
    0x12, 0xFF, 0xFE, 0b11100011, 0x0E,       0xB0,       0x00, 0x07, 0x00, 0x20, 0x00,
    0x00, 0x00, 0x84, 0x00,       0x03,       0xC0,       0xF0, 0x3F, 0x80};

// RL Huffman table declares two zero-length symbols, plus a trailing valid RLE-only chunk.
const uint8_t malformedHuffmanRlZeroLengthSymbolsNalData[] = {
    0x00, 0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80, 0b01100010,
    0x12, 0xFF, 0xFE, 0b11100011, 0x10,       0xB0,       0x00, 0x09, 0x00, 0x20, 0x00,
    0x00, 0x00, 0x04, 0x20,       0x00,       0x08,       0x03, 0xC0, 0xF0, 0x3F, 0x80};

// RL Huffman table densely declares all 256 symbols with 1-bit codes. This over-subscribes the
// 10-bit small Huffman LUT and can drive generateCodesAndLut past the end of its table.
const uint8_t malformedHuffmanRlDenseShortCodesNalData[] = {
    0x00, 0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80, 0b01100010,
    0x12, 0xFF, 0xFE, 0b11100011, 0x2B,       0x80,       0x00, 0x28, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x03, 0x00,       0x87,       0xFF,       0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,       0xFF,       0xFF,       0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,       0xFF,       0xFF,       0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFE,       0x00,       0x00,       0x80};

// First residual chunk is RLE-only but zero size.
const uint8_t malformedRleOnlyZeroSizeNalData[] = {
    0x00,       0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80,
    0b01100010, 0x12, 0xFF, 0xFE,       0b01100011, 0xC0,       0x00, 0x00, 0x80};

// First residual chunk is RLE-only but zero size, followed by a valid RLE-only chunk.
const uint8_t malformedRleOnlyZeroSizeWithResidualNalData[] = {
    0x00, 0x00, 0x01,       0b01111011, 0b11111111, 0b10000001, 0x08,       0x40,
    0x08, 0x80, 0b01100010, 0x12,       0xFF,       0xFE,       0b11100011, 0x07,
    0xF0, 0x00, 0x00,       0x03,       0xC0,       0xF0,       0x3F,       0x80};

// First residual chunk is RLE-only with a 1-byte payload signaling a run-length continuation, but
// no further bytes remain in the chunk.
const uint8_t malformedRleOnlyTruncatedRunNalData[] = {
    0x00,       0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80,
    0b01100010, 0x12, 0xFF, 0xFE,       0b10000011, 0xC0,       0x00, 0x01, 0x80, 0x80};

// First residual chunk is RLE-only with a single complete symbol. Parsing accepts it, then decode
// exhausts the chunk before the surface is complete.
const uint8_t malformedRleOnlyExhaustedNalData[] = {
    0x00,       0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80,
    0b01100010, 0x12, 0xFF, 0xFE,       0b10000011, 0xC0,       0x00, 0x01, 0x00, 0x80};

// As above, plus a trailing valid RLE-only residual chunk.
const uint8_t malformedRleOnlyTruncatedRunWithResidualNalData[] = {
    0x00, 0x00,       0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08,
    0x80, 0b01100010, 0x12, 0xFF,       0xFE,       0b11100011, 0x08, 0xF0, 0x00,
    0x01, 0x80,       0x03, 0xC0,       0xF0,       0x3F,       0x80};

// First residual chunk is RLE-only with a 6-byte payload whose continuation bytes signal a run
// length larger than the decoder's signed run-length accumulator can represent.
const uint8_t malformedRleOnlyHugeRunNalData[] = {
    0x00, 0x00,       0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08,
    0x80, 0b01100010, 0x12, 0xFF,       0xFE,       0b11100011, 0x09, 0xC0, 0x00,
    0x06, 0x80,       0xFF, 0xFF,       0xFF,       0xFF,       0x7F, 0x80};

// As above, plus a trailing valid RLE-only residual chunk.
const uint8_t malformedRleOnlyHugeRunWithResidualNalData[] = {
    0x00,       0x00, 0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x08, 0x80,
    0b01100010, 0x12, 0xFF, 0xFE,       0b11100011, 0x0D,       0xF0, 0x00, 0x06, 0x80,
    0xFF,       0xFF, 0xFF, 0xFF,       0x7F,       0x03,       0xC0, 0xF0, 0x3F, 0x80};

// Temporal signaling present, RLE-only temporal chunk containing a zero run length.
const uint8_t malformedTemporalZeroRunRleOnlyNalData[] = {
    0x00, 0x00,       0x01, 0b01111011, 0b11111111, 0b10000001, 0x08, 0x40, 0x48,
    0x80, 0b00100010, 0x81, 0b10000011, 0xC0,       0x02,       0x00, 0x00, 0x80};

// Temporal signaling present, RLE-only temporal chunk with zero size.
const uint8_t malformedTemporalRleOnlyZeroSizeNalData[] = {
    0x00, 0x00, 0x01,       0b01111011, 0b11111111, 0b10000001, 0x08, 0x40,
    0x48, 0x80, 0b00100010, 0x81,       0b01000011, 0xC0,       0x00, 0x80};

// Temporal-only frame (no global config) signaling a one-run temporal RLE chunk.
const uint8_t malformedTemporalSL2OneResidualNalData[] = {
    0x00,       0x00, 0x01, 0b01111001, 0b11111111, 0b00100010, 0x81,
    0b10000011, 0xC0, 0x02, 0x00,       0x01,       0x80};

// Tiled encoded-data layout version of the one-run temporal RLE chunk above.
const uint8_t malformedTemporalSL2OneResidualTiledNalData[] = {
    0x00,       0x00, 0x01, 0b01111001, 0b11111111, 0b00100010, 0x81,
    0b10100011, 0x80, 0x80, 0x02,       0x00,       0x01,       0x80};

// Tiled encoded-data with a compressed per-tile entropy flag run enabling one chunk, but no
// compressed chunk-size stream following it.
const uint8_t malformedTiledCompressedSizeNalData[] = {
    0x00, 0x00, 0x01, 0b01111001, 0b11111111, 0xE2, 0x0C, 0x52, 0xFF, 0xFF, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x03, 0x00, 0x1E,       0x1E,       0x32, 0x41, 0x84, 0x00, 0x01, 0x01, 0x1F, 0x80};

// Tiled encoded-data with an invalid compressed per-tile entropy flag stream.
const uint8_t malformedTiledCompressedEntropyFlagsNalData[] = {
    0x00, 0x00, 0x01, 0b01111001, 0b11111111, 0xE2, 0x0C, 0x52, 0xFF, 0xFF, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x03, 0x00,       0x1E,       0x1E, 0x32, 0x41, 0x44, 0x00, 0x02, 0x80};

// ---------------------------------------------------------------------------------------------- //

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

struct DecodeOutcome
{
    bool parseSucceeded = false;
    bool decodeAttempted = false;    // at least one ldeDecodeEnhancement call was made
    bool allDecodesSucceeded = true; // every ldeDecodeEnhancement call returned true
    uint32_t totalCommands = 0;      // sum of cmdBuffer.count across all loq/plane/tiles
    size_t totalResidualBytes = 0;   // sum of residual storage across all loq/plane/tiles
};

// Drives a sequence of NAL units through ldeConfigsParse, then decodes every enabled LOQ/plane/tile
// into a CPU command buffer. The returned outcome describes the LAST buffer in the sequence (the
// malformed one); earlier buffers establish prerequisite global/frame config state.
class MalformedDecoder
{
public:
    MalformedDecoder()
    {
        ldcDiagnosticsLogLevel(LdcLogLevelError);
        m_allocator = ldcMemoryAllocatorMalloc();
        ldeGlobalConfigInitialize(BitstreamVersionUnspecified, &m_globalConfig);
        ldeFrameConfigInitialize(m_allocator, &m_frameConfig);
        m_frameConfig.chunkAllocation = m_chunkAllocation;
        ldeCmdBufferCpuInitialize(m_allocator, &m_cmdBuffer, 0);
    }

    ~MalformedDecoder()
    {
        ldeCmdBufferCpuFree(&m_cmdBuffer);
        ldeConfigsReleaseFrame(&m_frameConfig);
    }

    DecodeOutcome run(const std::vector<NalBuffer>& nalUnits)
    {
        DecodeOutcome out;
        for (const NalBuffer& unit : nalUnits) {
            out = DecodeOutcome{};
            bool globalConfigModified = false;
            out.parseSucceeded = ldeConfigsParse(unit.data, unit.size, &m_globalConfig,
                                                 &m_frameConfig, &globalConfigModified);
            if (!out.parseSucceeded) {
                continue;
            }
            decodeAll(out);
        }
        return out;
    }

private:
    void decodeAll(DecodeOutcome& out)
    {
        for (const LdeLOQIndex loq : {LOQ1, LOQ0}) {
            if (!m_frameConfig.loqEnabled[loq]) {
                continue;
            }
            for (uint32_t plane = 0; plane < m_globalConfig.numPlanes; ++plane) {
                const uint32_t numTiles = m_globalConfig.numTiles[plane][loq];
                for (uint32_t tile = 0; tile < numTiles; ++tile) {
                    ldeCmdBufferCpuReset(&m_cmdBuffer, m_globalConfig.numLayers);
                    out.decodeAttempted = true;
                    const bool ok = ldeDecodeEnhancement(&m_globalConfig, &m_frameConfig, loq, plane,
                                                         tile, &m_cmdBuffer, nullptr, nullptr);
                    out.allDecodesSucceeded = out.allDecodesSucceeded && ok;
                    out.totalCommands += m_cmdBuffer.count;
                    out.totalResidualBytes += ldeCmdBufferCpuGetResidualSize(&m_cmdBuffer);
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

} // namespace

// --- Rejected during parsing: ldeConfigsParse returns false.

TEST(ParserMalformed, TruncatedChunk)
{
    // Residual chunk claims a 1-byte payload but no payload bytes remain.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedTruncatedChunkNalData)}).parseSucceeded);
}

TEST(ParserMalformed, HuffmanZeroSize)
{
    // Zero-size Huffman residual chunk: rejected at parse rather than decoding a surface of zeros
    // from uninitialized Huffman state.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedHuffmanZeroSizeNalData)}).parseSucceeded);
}

TEST(ParserMalformed, HuffmanZeroSizeWithResidual)
{
    // Zero-size Huffman chunk followed by a second chunk - the trailing chunk fails to parse.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedHuffmanZeroSizeWithResidualNalData)}).parseSucceeded);
}

TEST(ParserMalformed, RleOnlyZeroSizeWithResidual)
{
    // Zero-size RLE-only chunk followed by a second chunk - the trailing chunk fails to parse.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedRleOnlyZeroSizeWithResidualNalData)}).parseSucceeded);
}

// --- Rejected during decode: parse succeeds, ldeDecodeEnhancement returns false.

TEST(ParserMalformed, TemporalZeroRunRleOnly)
{
    // Temporal RLE chunk with a zero run length is accepted by the parser but rejected by decode.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedTemporalZeroRunRleOnlyNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

// --- Zero-size RLE-only chunks: rejected during parsing. ---------------------------------------
// parseChunk rejects any entropy-enabled RLE-only chunk signalled with zero size (residual or
// temporal), which would otherwise be read out of bounds in getNextSymbolRLEOnly during decode.

TEST(ParserMalformed, RleOnlyZeroSize)
{
    // Zero-size RLE-only residual chunk.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedRleOnlyZeroSizeNalData)}).parseSucceeded);
}

TEST(ParserMalformed, TemporalRleOnlyZeroSize)
{
    // Zero-size RLE-only temporal chunk.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedTemporalRleOnlyZeroSizeNalData)}).parseSucceeded);
}

TEST(ParserMalformed, RleOnlyTruncatedRun)
{
    // RLE-only chunk whose final byte signals a run-length continuation with no following byte:
    // rejected at parse rather than reading past the chunk in getNextSymbolRLEOnly.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedRleOnlyTruncatedRunNalData)}).parseSucceeded);
}

TEST(ParserMalformed, RleOnlyExhausted)
{
    // RLE-only chunk with a complete first symbol but no bytes for later decode iterations: decode
    // reports stream corruption instead of reading past chunk->data.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedRleOnlyExhaustedNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
}

TEST(ParserMalformed, RleOnlyTruncatedRunWithResidual)
{
    // Truncated RLE run in the first chunk, followed by a valid residual chunk; still rejected.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(malformedRleOnlyTruncatedRunWithResidualNalData)}).parseSucceeded);
}

TEST(ParserMalformed, TemporalSL2OneResidual)
{
    // Temporal signaling present in a frame whose active (non-tiled) global config has temporal
    // disabled: parseEncodedData rejects it rather than dereferencing the absent temporal chunk.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(startBlockTemporal), nal(malformedTemporalSL2OneResidualNalData)}).parseSucceeded);
}

TEST(ParserMalformed, TemporalSL2OneResidualWithTemporalEnabled)
{
    // The temporal-only complement of TemporalSL2OneResidual: temporal is enabled by the active
    // global config, so the temporal chunk is parsed and the malformed RLE stream fails at decode.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run(
        {nal(startBlockTemporalEnabledGlobalOnly), nal(malformedTemporalSL2OneResidualNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

TEST(ParserMalformed, TemporalSL2OneResidualTiled)
{
    // A Huffman temporal chunk too short to be decodable. The frame is block-consistent so parsing
    // accepts it, but decode now bails out gracefully when the temporal Huffman stream is exhausted
    // rather than spinning forever on a never-satisfied run-length continuation.
    MalformedDecoder decoder;
    const DecodeOutcome out =
        decoder.run({nal(startBlockTiledDDS), nal(malformedTemporalSL2OneResidualTiledNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
}

TEST(ParserMalformed, TiledCompressedEntropyFlags)
{
    // perTileCompressionEnabled routes entropy flags through tiledRLEDecoderInitialize; an invalid
    // first symbol is rejected before any chunk data is parsed.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder
                     .run({nal(startBlockTiledCompressedGlobalOnly),
                           nal(malformedTiledCompressedEntropyFlagsNalData)})
                     .parseSucceeded);
}

TEST(ParserMalformed, TiledCompressedSize)
{
    // perTileCompressionEnabled enables the first tile chunk via tiledRLEDecoderRead, and
    // tileSizeCompression then routes chunk sizes through tiledSizeDecoderInitialize.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder
                     .run({nal(startBlockTiledCompressedGlobalOnly), nal(malformedTiledCompressedSizeNalData)})
                     .parseSucceeded);
}

// --- Invalid Huffman tables: parse succeeds, but decode rejects them gracefully. ----------------

TEST(ParserMalformed, InvalidRlHuffman)
{
    // Internally inconsistent RL Huffman table.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedInvalidRlHuffmanNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

TEST(ParserMalformed, InvalidRlHuffmanWithResidual)
{
    // Inconsistent RL Huffman table followed by a valid residual chunk.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedInvalidRlHuffmanWithResidualNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

TEST(ParserMalformed, HuffmanRlZeroLengthSymbols)
{
    // RL Huffman table with zero-length symbols - the entropy decode fails and is now propagated
    // rather than emitting a phantom zero-residual command.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedHuffmanRlZeroLengthSymbolsNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

TEST(ParserMalformed, HuffmanRlDenseShortCodes)
{
    // The RL Huffman LUT should reject an over-subscribed short-code table cleanly. The current
    // generateCodesAndLut implementation can write past the 10-bit small table instead.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedHuffmanRlDenseShortCodesNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

// --- Oversized RLE run: parse succeeds, but decode rejects it gracefully. -----------------------

TEST(ParserMalformed, RleOnlyHugeRun)
{
    // RLE run length larger than the signed run-length accumulator can represent.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedRleOnlyHugeRunNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

TEST(ParserMalformed, RleOnlyHugeRunWithResidual)
{
    // Oversized RLE run followed by a valid residual chunk.
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({nal(malformedRleOnlyHugeRunWithResidualNalData)});
    EXPECT_TRUE(out.parseSucceeded);
    EXPECT_TRUE(out.decodeAttempted);
    EXPECT_FALSE(out.allDecodesSucceeded);
    EXPECT_EQ(out.totalCommands, 0u);
}

// --- Bitstream-version state: the same residual payloads re-parsed under a sticky version. -------
// globalConfig.bitstreamVersion is never reset between NAL units, and a global-config block only
// adopts the current version if one has not been set already. A prior SEI therefore pins the
// version for every following payload, routing it through a different bitWidth() length table
// (huffman.c) and the < BitstreamVersionNewCodeLengths "x++" branch. Run on their own, the payloads
// below decode at version 2 (Current); these cases additionally exercise versions 0 and 1, which
// the current single-NAL fuzzer (fresh globalConfig per input) can never reach. The invariant is
// unchanged across versions: malformed entropy never yields decoded residual commands - it is
// rejected at parse, or rejected during decode with no output.

struct VersionedCase
{
    NalBuffer versionBlock;
    NalBuffer payload;
};

class ParserMalformedVersioned : public ::testing::TestWithParam<VersionedCase>
{};

TEST_P(ParserMalformedVersioned, NoResidualCommandsAcrossVersions)
{
    MalformedDecoder decoder;
    const DecodeOutcome out = decoder.run({GetParam().versionBlock, GetParam().payload});
    EXPECT_EQ(out.totalCommands, 0u);
    if (out.parseSucceeded && out.decodeAttempted) {
        EXPECT_FALSE(out.allDecodesSucceeded);
    }
}

INSTANTIATE_TEST_SUITE_P(
    EntropyPayloads, ParserMalformedVersioned,
    ::testing::Values(
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedTruncatedChunkNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedHuffmanZeroSizeNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedHuffmanZeroSizeWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedInvalidRlHuffmanNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedInvalidRlHuffmanWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedHuffmanRlZeroLengthSymbolsNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedHuffmanRlDenseShortCodesNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedRleOnlyZeroSizeNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedRleOnlyZeroSizeWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedRleOnlyTruncatedRunNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedRleOnlyTruncatedRunWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedRleOnlyHugeRunNalData)},
        VersionedCase{nal(startBlockSeiVersion0), nal(malformedRleOnlyHugeRunWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedTruncatedChunkNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedHuffmanZeroSizeNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedHuffmanZeroSizeWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedInvalidRlHuffmanNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedInvalidRlHuffmanWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedHuffmanRlZeroLengthSymbolsNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedHuffmanRlDenseShortCodesNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedRleOnlyZeroSizeNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedRleOnlyZeroSizeWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedRleOnlyTruncatedRunNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedRleOnlyTruncatedRunWithResidualNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedRleOnlyHugeRunNalData)},
        VersionedCase{nal(startBlockSeiVersion1), nal(malformedRleOnlyHugeRunWithResidualNalData)}));

TEST(ParserMalformed, SeiUnsupportedBitstreamVersion)
{
    // A V-Nova SEI advertising a bitstream version above BitstreamVersionCurrent is rejected at
    // parse rather than pinning an out-of-range version that later indexes the bitWidth() table.
    MalformedDecoder decoder;
    EXPECT_FALSE(decoder.run({nal(startBlockSeiVersionUnsupported)}).parseSucceeded);
}
