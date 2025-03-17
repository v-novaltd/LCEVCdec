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

#include "bin_writer.h"
#include "hash.h"

#include <CLI/CLI.hpp> // NOLINT(misc-include-cleaner)
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <LCEVC/api_utility/chrono.h>
#include <LCEVC/api_utility/picture_layout.h>
#include <LCEVC/lcevc_dec.h>
#include <LCEVC/utility/base_decoder.h>
#include <LCEVC/utility/check.h>
#include <LCEVC/utility/configure.h>
#include <LCEVC/utility/picture_functions.h>
#include <LCEVC/utility/raw_writer.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace lcevc_dec::utility;

// Check if an LCEVC handle is null
template <typename H>
inline bool isNull(H handle)
{
    return handle.hdl == 0;
}

struct Config
{
    // Inputs
    std::string inputFile;
    std::string inputFileFormat;
    std::string inputLcevcFile;
    std::string inputBaseFile;
    LCEVC_ColorFormat inputColorFormat{LCEVC_ColorFormat_Unknown};
    bool baseExternal{false};
    bool readBinLinearly = false;
    bool simulatePadding{false};
    // Outputs
    std::string outputRawFile;
    std::string outputBaseRawFile;
    std::string outputBinFile;
    std::string outputHashFile;
    std::string outputOplFile;
    std::string hashType;
    LCEVC_ColorFormat outputColorFormat{LCEVC_ColorFormat_Unknown};
    uint32_t outputFrameLimit{0};
    bool outputExternal{false};
    // Decoding config. Avoiding adding direct command-line interface for configs that are already
    // available via the JSON.
    std::string configurationJson;
    bool verbose{false};
};

struct Stats
{
    uint32_t inputEnhancedCount{0};
    uint32_t outputFrameCount{0};
    MilliSecondF64 latencyTotal{0};
    std::map<int64_t, TimePoint> frameStartMap = {};
    std::vector<TimePoint> frameEndTimes = {};
};

struct OutputPicData
{
    OutputPicData(const LCEVC_DecoderHandle decoder, const LCEVC_ColorFormat colorFormat,
                  const bool outputExternal)
    {
        // Output picture starts with 2x2 as a safe small size. If the output picture is managed,
        // then the decoder will set the right size and format. If the output picture is external,
        // then we simply overwrite this with a valid desc by peeking later.
        LCEVC_DefaultPictureDesc(&desc, colorFormat, 2, 2);
        if (outputExternal) {
            valid = true;
            return;
        }

        VN_LCEVC_CHECK(LCEVC_AllocPicture(decoder, &desc, &handle));
        valid = true;
    }
    // Create an initial output picture, starting with 2x2 as a safe small size. If the output
    // picture is managed, then the decoder will set the right size and format. If the output
    // picture is external, then we simply overwrite this with a valid desc by peeking later.
    LCEVC_PictureHandle handle{};
    LCEVC_PictureBufferDesc bufferDesc = {};
    LCEVC_PictureDesc desc{};
    std::unique_ptr<uint8_t[]> extBuffer = nullptr;
    bool valid = false;
};

int setupConfig(int argc, char** argv, Config& cfgOut)
{
    CLI::App app{"LCEVCdec Test Harness"};
    // Input
    app.add_option("-i,--input", cfgOut.inputFile, "Input file");
    app.add_option("--input-file-format", cfgOut.inputFileFormat,
                   "Override AVInputFormat (e.g. h264, hevc), if using a non-raw stream");
    app.add_option("-l,--lcevc", cfgOut.inputLcevcFile, "Input LCEVC file, BIN");
    app.add_option("-b,--base", cfgOut.inputBaseFile, "Input base file, RAW");
    app.add_option("--input-color-format", cfgOut.inputColorFormat, "Override input color format");
    app.add_flag("--base-external", cfgOut.baseExternal,
                 "Use externally allocated memory for base pictures.");
    app.add_flag("--read-bin-linearly",
                 cfgOut.readBinLinearly, "Use this to measure LCEVC Decode performance. If true, then .bin+.yuv streams are decoded in presentation order (rather than decode order, as with encapsulated files)");
    app.add_flag("--simulate-padding", cfgOut.simulatePadding,
                 "Pad input stride rounded to the next power of 2 of the surface width");
    // Output
    app.add_option("-o,--output", cfgOut.outputRawFile, "Output file, RAW");
    app.add_option("--output-base", cfgOut.outputBaseRawFile, "Output base file, RAW");
    app.add_option("--output-bin", cfgOut.outputBinFile, "Output enhancement file, BIN");
    app.add_option("-x,--output-hash", cfgOut.outputHashFile, "Output json hash file");
    app.add_option("--output-opl", cfgOut.outputOplFile,
                   "Output hashes per frame, per plane as CSV (Conformance OPL format)");
    app.add_option("--output-color-format", cfgOut.outputColorFormat,
                   "Output color format. Not currently implemented by decoder.");
    app.add_option("--output-limit", cfgOut.outputFrameLimit, "Output frame limit");
    app.add_flag("--output-external", cfgOut.outputExternal,
                 "Use externally allocated memory for output pictures.");
    app.add_option("-t,--hash-type", cfgOut.hashType, "Type of hash to use: xxhash or md5")->default_val("xxhash");
    // Decoding config
    app.add_option("-c,--configuration", cfgOut.configurationJson,
                   "JSON configuration (Inline json, or json filename)");
    app.add_flag("-v,--verbose", cfgOut.verbose, "Enable verbose logging");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // Validate
    if (cfgOut.simulatePadding && !cfgOut.baseExternal) {
        fmt::print("base-external must be enabled when using simulate-padding\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

std::unique_ptr<BaseDecoder> createBaseDecoder(const Config& cfg)
{
    if (!cfg.inputFile.empty()) {
        std::unique_ptr<BaseDecoder> baseDecoderLibAV =
            createBaseDecoderLibAV(cfg.inputFile, cfg.inputFileFormat, cfg.inputColorFormat, cfg.verbose);
        if (!baseDecoderLibAV) {
            fmt::print("Could not open input {}\n", cfg.inputFile);
        }
        return baseDecoderLibAV;
    }

    if (!cfg.inputLcevcFile.empty() && !cfg.inputBaseFile.empty()) {
        std::unique_ptr<BaseDecoder> baseDecoderBin;
        if (cfg.readBinLinearly) {
            baseDecoderBin = createBaseDecoderBinLinear(cfg.inputBaseFile, cfg.inputLcevcFile);
        } else {
            baseDecoderBin = createBaseDecoderBinNonLinear(cfg.inputBaseFile, cfg.inputLcevcFile);
        }
        if (!baseDecoderBin) {
            fmt::print("Could not open input.\nBase: {}\nLCEVC: {}\n", cfg.inputBaseFile, cfg.inputLcevcFile);
        }
        return baseDecoderBin;
    }

    fmt::print("No input specified\n");
    return nullptr;
}

int createAndInitDecoder(const std::string_view configurationJson, bool verbose, LCEVC_DecoderHandle& decoderOut)
{
    VN_LCEVC_CHECK(LCEVC_CreateDecoder(&decoderOut, LCEVC_AccelContextHandle{}));

    // Default to stdout for logs
    LCEVC_ConfigureDecoderBool(decoderOut, "log_stdout", true);

    // Configure LCEVC_Decoder from JSON
    if (!configurationJson.empty() &&
        (configureDecoderFromJson(decoderOut, configurationJson) != LCEVC_Success)) {
        fmt::print("JSON configuration error - invalid parameter name or type in JSON\n");
        return EXIT_FAILURE;
    }

    if (verbose) {
        // Simple command line option for verbose logging
        VN_LCEVC_CHECK(LCEVC_ConfigureDecoderInt(decoderOut, "log_level", LCEVC_LogTrace));
    }

    VN_LCEVC_CHECK(LCEVC_InitializeDecoder(decoderOut));
    return EXIT_SUCCESS;
}

void updateExternalOutputPic(LCEVC_DecoderHandle decoder, uint64_t timestamp, OutputPicData& output)
{
    // Resize output based on enhancement info
    uint32_t newWidth = 0;
    uint32_t newHeight = 0;
    VN_LCEVC_CHECK(LCEVC_PeekDecoder(decoder, timestamp, &newWidth, &newHeight));
    if (newWidth != output.desc.width || newHeight != output.desc.height) {
        output.desc.width = newWidth;
        output.desc.height = newHeight;
        const PictureLayout layout(output.desc);
        output.bufferDesc.byteSize = layout.size();
        output.extBuffer = std::make_unique<uint8_t[]>(output.bufferDesc.byteSize);
        output.bufferDesc.data = output.extBuffer.get();
        VN_LCEVC_CHECK(LCEVC_AllocPictureExternal(decoder, &output.desc, &output.bufferDesc,
                                                  nullptr, &output.handle));
    }
}

void receiveDecodedPicture(LCEVC_DecoderHandle decoder, Stats& stats, Hashes& hashes,
                           std::unique_ptr<RawWriter>& outputRaw, const std::string_view outputRawFile)
{
    // Has decoder produced a picture?
    LCEVC_PictureHandle decodedPicture;
    LCEVC_DecodeInformation decodeInformation;
    if (!VN_LCEVC_AGAIN(LCEVC_ReceiveDecoderPicture(decoder, &decodedPicture, &decodeInformation))) {
        return;
    }

    LCEVC_PictureDesc desc{};
    VN_LCEVC_CHECK(LCEVC_GetPictureDesc(decoder, decodedPicture, &desc));

    const TimePoint end = getTimePoint();
    stats.frameEndTimes.push_back(end);
    stats.latencyTotal += (end - stats.frameStartMap[decodeInformation.timestamp]);
    fmt::print("Frame {}: {:#08x} {}x{}\n", stats.outputFrameCount, decodeInformation.timestamp,
               desc.width, desc.height);

    if (!outputRawFile.empty()) {
        if (!outputRaw) {
            outputRaw = createRawWriter(PictureLayout(desc).makeRawFilename(outputRawFile));
        }
        VN_UTILITY_CHECK_MSG(outputRaw->write(decoder, decodedPicture),
                             "Cannot write raw output image, likely a picture format issue");
    }

    hashes.updateHigh(decoder, decodedPicture);
    hashes.updateOpl(decoder, decodedPicture, stats.outputFrameCount, desc.width, desc.height);

    VN_LCEVC_CHECK(LCEVC_FreePicture(decoder, decodedPicture));
    stats.outputFrameCount++;
}

void sendEnhancement(const LCEVC_DecoderHandle decoder, BaseDecoder& baseDecoder, const Config& cfg,
                     BinWriter* outputBin, Stats& stats, OutputPicData& outputPic)
{
    // Fetch encoded enhancement data from base decoder
    BaseDecoder::Data enhancementData;
    baseDecoder.getEnhancement(enhancementData);

    // Try to send enhancement data into decoder.
    const TimePoint preSendTime = getTimePoint();
    if (VN_LCEVC_AGAIN(LCEVC_SendDecoderEnhancementData(decoder, enhancementData.timestamp, false,
                                                        enhancementData.ptr, enhancementData.size))) {
        if (baseDecoder.getType() == BaseDecoder::Type::BinLinear) {
            enhancementData.baseDecodeStart = preSendTime;
        }
        if (outputBin) {
            outputBin->write(stats.inputEnhancedCount, enhancementData.timestamp,
                             enhancementData.ptr, enhancementData.size);
        }
        baseDecoder.clearEnhancement();
        stats.inputEnhancedCount++;
        stats.frameStartMap.insert({enhancementData.timestamp, enhancementData.baseDecodeStart});

        if (cfg.outputExternal) {
            updateExternalOutputPic(decoder, enhancementData.timestamp, outputPic);
        }
    }
}

int main(int argc, char** argv)
{
    Config cfg;
    if (const int res = setupConfig(argc, argv, cfg); res != EXIT_SUCCESS) {
        return res;
    }

    std::unique_ptr<BaseDecoder> baseDecoder = createBaseDecoder(cfg);
    if (baseDecoder == nullptr) {
        return EXIT_FAILURE;
    }

    // RAW outputs - initialised once picture layouts are known
    std::unique_ptr<RawWriter> outputBaseRaw;
    std::unique_ptr<RawWriter> outputRaw;

    // Overall Hashing
    Hashes hashes(cfg.hashType);
    if (!cfg.outputHashFile.empty()) {
        if (const int res = hashes.initBaseAndHigh(cfg.outputHashFile); res != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
    }

    // Per frame per plane hashing
    if (!cfg.outputOplFile.empty()) {
        hashes.initOpl(cfg.outputOplFile);
    }

    // BIN output
    std::unique_ptr<BinWriter> outputBin;
    if (!cfg.outputBinFile.empty()) {
        outputBin = createBinWriter(cfg.outputBinFile);
        if (!outputBin) {
            fmt::print("Could not open output bin {}\n", cfg.outputBinFile);
            return EXIT_FAILURE;
        }
    }

    // Create and initialize LCEVC decoder
    LCEVC_DecoderHandle decoder = {};
    if (const int res = createAndInitDecoder(cfg.configurationJson, cfg.verbose, decoder);
        res != EXIT_SUCCESS) {
        LCEVC_DestroyDecoder(decoder);
        return res;
    }

    // Base picture waiting to be sent to decoder
    LCEVC_PictureHandle basePicture{};
    int64_t basePictureTimestamp{-1};

    if (cfg.outputColorFormat == LCEVC_ColorFormat_Unknown) {
        cfg.outputColorFormat = baseDecoder->description().colorFormat;
    }

    // Output picture
    OutputPicData outputPic(decoder, cfg.outputColorFormat, cfg.outputExternal);
    VN_UTILITY_CHECK(outputPic.valid);

    // Counters and timers
    Stats stats;

    // Frame loop - consume data from base
    while (baseDecoder->update()) {
        if (cfg.outputFrameLimit > 0 && stats.outputFrameCount >= cfg.outputFrameLimit) {
            break;
        }

        // Make sure LCEVC data is sent before base frame
        if (baseDecoder->hasEnhancement()) {
            sendEnhancement(decoder, *baseDecoder, cfg, outputBin.get(), stats, outputPic);
        }

        if (baseDecoder->hasImage() && isNull(basePicture)) {
            BaseDecoder::Data baseImage;
            baseDecoder->getImage(baseImage);
            if (cfg.baseExternal) {
                // Allocate an external buffer and copy base image into it
                LCEVC_PictureBufferDesc pictureBufferDesc = {nullptr, baseImage.size, {0}, LCEVC_Access_Read};

                if (cfg.simulatePadding) {
                    LCEVC_PicturePlaneDesc picturePlaneDesc[PictureLayout::PictureLayout::kMaxNumPlanes] = {};
                    VN_LCEVC_CHECK(createPaddedDesc(baseDecoder->description(), baseImage.ptr,
                                                    &pictureBufferDesc, picturePlaneDesc));

                    VN_LCEVC_CHECK(LCEVC_AllocPictureExternal(decoder, &baseDecoder->description(),
                                                              &pictureBufferDesc, picturePlaneDesc,
                                                              &basePicture));
                } else {
                    pictureBufferDesc.data = new uint8_t[baseImage.size]; // NOLINT:cppcoreguidelines-owning-memory
                    VN_LCEVC_CHECK(LCEVC_AllocPictureExternal(decoder, &baseDecoder->description(),
                                                              &pictureBufferDesc, nullptr, &basePicture));
                    memcpy(pictureBufferDesc.data, baseImage.ptr, baseImage.size);
                }
            } else {
                VN_LCEVC_CHECK(LCEVC_AllocPicture(decoder, &baseDecoder->description(), &basePicture));
                VN_LCEVC_CHECK(copyPictureFromMemory(decoder, basePicture, baseImage.ptr, baseImage.size));
            }
            basePictureTimestamp = baseImage.timestamp;
            baseDecoder->clearImage();

            // Generate output image and hash from the picture
            if (!cfg.outputBaseRawFile.empty()) {
                if (!outputBaseRaw) {
                    outputBaseRaw =
                        createRawWriter(baseDecoder->layout().makeRawFilename(cfg.outputBaseRawFile));
                }
                VN_UTILITY_CHECK_MSG(
                    outputBaseRaw->write(decoder, basePicture),
                    "Cannot write raw base output image, likely a picture format issue");
            }

            hashes.updateBase(decoder, basePicture);
        }

        // Try to send base picture into LCEVC decoder (since this is a testing program, don't ever
        // timeout).
        if (!isNull(basePicture) &&
            VN_LCEVC_AGAIN(LCEVC_SendDecoderBase(decoder, basePictureTimestamp, false, basePicture,
                                                 std::numeric_limits<uint32_t>::max(), nullptr))) {
            basePicture = LCEVC_PictureHandle{};
        }

        {
            // Has decoder finished with a base picture?
            LCEVC_PictureHandle doneBasePicture;
            if (VN_LCEVC_AGAIN(LCEVC_ReceiveDecoderBase(decoder, &doneBasePicture))) {
                if (cfg.baseExternal) {
                    // Release external buffer
                    LCEVC_PictureBufferDesc pictureBufferDesc;
                    VN_LCEVC_CHECK(LCEVC_GetPictureBuffer(decoder, doneBasePicture, &pictureBufferDesc));
                    VN_LCEVC_CHECK(LCEVC_FreePicture(decoder, doneBasePicture));
                    delete[] pictureBufferDesc.data; // NOLINT:cppcoreguidelines-owning-memory
                } else {
                    VN_LCEVC_CHECK(LCEVC_FreePicture(decoder, doneBasePicture));
                }
            }
        }

        // Send destination picture into LCEVC decoder
        if (!isNull(outputPic.handle) &&
            VN_LCEVC_AGAIN(LCEVC_SendDecoderPicture(decoder, outputPic.handle))) {
            // Allocate next output
            if (cfg.outputExternal) {
                VN_LCEVC_CHECK(LCEVC_AllocPictureExternal(
                    decoder, &outputPic.desc, &outputPic.bufferDesc, nullptr, &outputPic.handle));
            } else {
                VN_LCEVC_CHECK(LCEVC_AllocPicture(decoder, &outputPic.desc, &outputPic.handle));
            }
        }

        receiveDecodedPicture(decoder, stats, hashes, outputRaw, cfg.outputRawFile);
    }

    LCEVC_DestroyDecoder(decoder);

    MilliSecondF64 frameTimeTotal{0};
    if (stats.frameEndTimes.size() > 2) {
        for (int32_t i = 0; i < static_cast<int32_t>(stats.frameEndTimes.size()) - 1; i++) {
            frameTimeTotal += stats.frameEndTimes[i + 1] - stats.frameEndTimes[i];
        }
    }
    const auto frameTime = frameTimeTotal.count() / stats.outputFrameCount;
    const auto latency = stats.latencyTotal.count() / stats.outputFrameCount;
    fmt::print("Average frame latency: {:.4}ms, frame time (1 / throughput): {:.4}ms\n", latency, frameTime);

    return EXIT_SUCCESS;
}
