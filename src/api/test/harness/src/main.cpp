// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/lcevc_dec.h"
//
#include "LCEVC/utility/base_decoder.h"
#include "LCEVC/utility/check.h"
#include "LCEVC/utility/configure.h"
#include "LCEVC/utility/md5.h"
#include "LCEVC/utility/picture_functions.h"
#include "LCEVC/utility/picture_layout.h"
#include "LCEVC/utility/picture_lock.h"
#include "LCEVC/utility/raw_writer.h"
#include "LCEVC/utility/types.h"
#include "LCEVC/utility/types_cli11.h"
#include "bin_writer.h"

//
#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <xxhash.h>
//
#include "hash.h"

#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace lcevc_dec::utility;

// Check if an LCEVC handle is null
template <typename H>
bool isNull(H handle)
{
    return handle.hdl == 0;
}

int main(int argc, char** argv)
{
    std::string inputFile;
    std::string inputFormat;
    std::string inputLcevcFile;
    std::string inputBaseFile;
    LCEVC_ColorFormat baseFormat{};
    bool baseExternal = false;
    std::string outputRawFile;
    std::string outputBaseRawFile;
    std::string outputBinFile;
    std::string outputHashFile;
    std::string outputOplFile;
    std::string hashType;
    std::string configurationJson;
    bool verbose{false};

    CLI::App app{"LCEVC_DEC Test Harness"};
    app.add_option("-i,--input", inputFile, "Input file");
    app.add_option("-l,--lcevc", inputLcevcFile, "Input LCEVC file, BIN");
    app.add_option("-b,--base", inputBaseFile, "Input base file, RAW");
    app.add_option(",--input-format", inputFormat, "Override input stream format");
    app.add_option("-f,--base-format", baseFormat, "Override input file base format")->default_val(LCEVC_ColorFormat_Unknown);
    app.add_flag("--base-external", baseExternal, "Use externally allocated memory for base pictures.");
    app.add_option("-o,--output", outputRawFile, "Output file, RAW");
    app.add_option("--output-base", outputBaseRawFile, "Output base file, RAW");
    app.add_option("--output-bin", outputBinFile, "Output enhancement file, BIN");
    app.add_option("-x,--output-hash", outputHashFile, "Output json hash file");
    app.add_option("--output-opl", outputOplFile,
                   "Output hashes per frame, per plane as CSV (Conformance OPL format)");
    app.add_option("-t,--hash-type", hashType, "Type of hash to use: xxhash or md5")->default_val("xxhash");
    app.add_option("-c,--configuration", configurationJson,
                   "JSON configuration (Inline json, or json filename)");
    app.add_flag("-v,--verbose", verbose, "Enable verbose logging");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // Setup base decoder
    std::unique_ptr<BaseDecoder> baseDecoder;

    if (!inputFile.empty()) {
        baseDecoder = createBaseDecoderLibAV(inputFile, inputFormat, baseFormat);
    } else if (!inputLcevcFile.empty() && !inputBaseFile.empty()) {
        baseDecoder = createBaseDecoderBin(inputBaseFile, inputLcevcFile);
    } else {
        fmt::print("No input specified\n");
        return EXIT_FAILURE;
    }

    if (!baseDecoder) {
        fmt::print("Could not open input {}\n", inputLcevcFile);
        return EXIT_FAILURE;
    }

    // RAW outputs
    std::unique_ptr<RawWriter> outputBaseRaw;
    if (!outputBaseRawFile.empty()) {
        outputBaseRaw = createRawWriter(outputBaseRawFile);
    }

    std::unique_ptr<RawWriter> outputRaw;
    if (!outputRawFile.empty()) {
        outputRaw = createRawWriter(outputRawFile);
    }

    // Overall Hashing
    std::unique_ptr<std::ostream> outputHash;
    std::unique_ptr<Hash> baseHash;
    std::unique_ptr<Hash> highHash;

    if (!outputHashFile.empty()) {
        outputHash = std::make_unique<std::ofstream>(outputHashFile);
        if (!outputHash->good()) {
            fmt::print("Could not open output hash {}\n", outputHashFile);
            return EXIT_FAILURE;
        }

        baseHash = createHash(hashType);
        highHash = createHash(hashType);
    }

    // Per frame per plane hashing
    std::unique_ptr<std::ostream> outputOpl;
    if (!outputOplFile.empty()) {
        outputOpl = std::make_unique<std::ofstream>(outputOplFile);
        fmt::print(*outputOpl, "PicOrderCntVal,pic_width_max,pic_height_max,md5_y,md5_u,md5_v\n");
    }

    // BIN output
    std::unique_ptr<BinWriter> outputBin;
    if (!outputBinFile.empty()) {
        outputBin = createBinWriter(outputBinFile);
        if (!outputBin) {
            fmt::print("Could not open output bin {}\n", outputBinFile);
            return EXIT_FAILURE;
        }
    }

    // Create and initialize LCEVC decoder
    LCEVC_DecoderHandle decoder = {};
    VN_LCEVC_CHECK(LCEVC_CreateDecoder(&decoder, LCEVC_AccelContextHandle{}));

    // Default to stdout for logs
    LCEVC_ConfigureDecoderBool(decoder, "log_stdout", true);

    // Apply a JSON config
    if (!configurationJson.empty()) {
        configureDecoderFromJson(decoder, configurationJson);
    }

    // Simple command line option for verbose logging
    if (verbose) {
        LCEVC_ConfigureDecoderInt(decoder, "log_level", 5);
    }

    VN_LCEVC_CHECK(LCEVC_InitializeDecoder(decoder));

    // Base picture waiting to be sent to decoder
    LCEVC_PictureHandle basePicture{};
    int64_t basePictureTimestamp{-1};

    // Create an initial output picture - decoder will set correct description on output pictures
    LCEVC_PictureHandle outputPicture{};

    LCEVC_PictureDesc outputDesc;
    // Use 2x2 as a safe small size
    LCEVC_DefaultPictureDesc(&outputDesc, baseDecoder->description().colorFormat, 2, 2);
    VN_LCEVC_CHECK(LCEVC_AllocPicture(decoder, &outputDesc, &outputPicture));

    // Output frame counter
    uint32_t inputEnhancedCount{0};
    uint32_t outputFrameCount{0};

    // Frame loop - consume data from base
    while (baseDecoder->update()) {
        // Make sure LCEVC data is sent before base frame
        if (baseDecoder->hasEnhancement()) {
            // Fetch encoded enhancement data from base decoder
            BaseDecoder::Data enhancementData = {};
            baseDecoder->getEnhancement(enhancementData);

            // Try to send enhancement data into decoder.
            if (VN_LCEVC_AGAIN(LCEVC_SendDecoderEnhancementData(
                    decoder, enhancementData.timestamp, false, enhancementData.ptr, enhancementData.size))) {
                if (outputBin) {
                    outputBin->write(inputEnhancedCount, enhancementData.timestamp,
                                     enhancementData.ptr, enhancementData.size);
                }
                baseDecoder->clearEnhancement();
                ++inputEnhancedCount;
            }
        }

        if (baseDecoder->hasImage() && isNull(basePicture)) {
            BaseDecoder::Data baseImage = {};
            baseDecoder->getImage(baseImage);
            if (baseExternal) {
                // Allocate an external buffer and copy base image into it
                LCEVC_PictureBufferDesc pictureBufferDesc = {nullptr, baseImage.size, {0}, LCEVC_Access_Read};
                pictureBufferDesc.data = new uint8_t[baseImage.size]; // NOLINT:cppcoreguidelines-owning-memory
                memcpy(pictureBufferDesc.data, baseImage.ptr, baseImage.size);

                VN_LCEVC_CHECK(LCEVC_AllocPictureExternal(decoder, &baseDecoder->description(),
                                                          &pictureBufferDesc, nullptr, &basePicture));
            } else {
                VN_LCEVC_CHECK(LCEVC_AllocPicture(decoder, &baseDecoder->description(), &basePicture));
                VN_LCEVC_CHECK(copyPictureFromMemory(decoder, basePicture, baseImage.ptr, baseImage.size));
            }
            basePictureTimestamp = baseImage.timestamp;
            baseDecoder->clearImage();

            // Generate output image and hash from the picture
            if (outputBaseRaw) {
                outputBaseRaw->write(decoder, basePicture);
            }

            if (baseHash) {
                baseHash->updatePicture(decoder, basePicture);
            }
        }

        if (!isNull(basePicture)) {
            // Try to send base picture into LCEVC decoder
            if (VN_LCEVC_AGAIN(LCEVC_SendDecoderBase(decoder, basePictureTimestamp, false,
                                                     basePicture, 1000000, nullptr))) {
                basePicture = LCEVC_PictureHandle{};
            }
        }

        {
            // Has decoder finished with a base picture?
            LCEVC_PictureHandle doneBasePicture;
            if (VN_LCEVC_AGAIN(LCEVC_ReceiveDecoderBase(decoder, &doneBasePicture))) {
                if (baseExternal) {
                    // Release external buffer
                    LCEVC_PictureBufferDesc pictureBufferDesc;
                    VN_LCEVC_CHECK(LCEVC_GetPictureBuffer(decoder, doneBasePicture, &pictureBufferDesc));
                    delete pictureBufferDesc.data; // NOLINT:cppcoreguidelines-owning-memory
                }
                VN_LCEVC_CHECK(LCEVC_FreePicture(decoder, doneBasePicture));
            }
        }

        if (!isNull(outputPicture)) {
            // Send destination picture into LCEVC decoder
            if (VN_LCEVC_AGAIN(LCEVC_SendDecoderPicture(decoder, outputPicture))) {
                // Allocate next output
                VN_LCEVC_CHECK(LCEVC_AllocPicture(decoder, &outputDesc, &outputPicture));
            }
        }

        {
            // Has decoder produced a picture?
            LCEVC_PictureHandle decodedPicture;
            LCEVC_DecodeInformation decodeInformation;
            if (VN_LCEVC_AGAIN(LCEVC_ReceiveDecoderPicture(decoder, &decodedPicture, &decodeInformation))) {
                uint32_t planeCount{0};
                VN_LCEVC_CHECK(LCEVC_GetPicturePlaneCount(decoder, decodedPicture, &planeCount));
                LCEVC_PictureDesc desc{};
                VN_LCEVC_CHECK(LCEVC_GetPictureDesc(decoder, decodedPicture, &desc));

                fmt::print("Frame {}: {:#08x} {}x{}\n", outputFrameCount,
                           decodeInformation.timestamp, desc.width, desc.height);

                if (outputRaw) {
                    outputRaw->write(decoder, decodedPicture);
                }

                if (highHash) {
                    highHash->updatePicture(decoder, decodedPicture);
                }

                if (outputOpl) {
                    fmt::print(*outputOpl, "{},{},{}", outputFrameCount, desc.width, desc.height);
                    for (uint32_t plane = 0; plane < planeCount; ++plane) {
                        auto hash = createHash(hashType);
                        hash->updatePlane(decoder, decodedPicture, plane);
                        fmt::print(*outputOpl, ",{}", hash->hexDigest());
                    }
                    fmt::print(*outputOpl, "\n");
                }

                VN_LCEVC_CHECK(LCEVC_FreePicture(decoder, decodedPicture));
                outputFrameCount++;
            }
        }
    }

    LCEVC_DestroyDecoder(decoder);

    // Write out json hashes
    if (outputHash && baseHash && highHash) {
        fmt::print(*outputHash, "{{\n    \"base\":\"{}\",\n    \"high\":\"{}\"\n}}\n",
                   baseHash->hexDigest(), highHash->hexDigest());
    }
    return EXIT_SUCCESS;
}
