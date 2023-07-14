/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "uBlockLoader.h"

#include <LCEVC/PerseusDecoder.h>
#include <uChrono.h>
#include <uCmdLineProcessor.h>
#include <uHasher.h>
#include <uLCEVCBin.h>
#include <uLog.h>
#include <uMath.h>
#include <uUtility.h>
#include <uYUVFile.h>

#include <array>
#include <chrono>
#include <fstream>

using namespace lcevc_dec;
using namespace lcevc_dec::utility;
using namespace std::placeholders;

// -----------------------------------------------------------------------------

#define VNRet(cmd, msg, ...)                \
    {                                       \
        int32_t r = (cmd);                  \
        if (r != 0) {                       \
            VNLogError(msg, ##__VA_ARGS__); \
            return r;                       \
        }                                   \
    }

static VNInline perseus_interleaving GetPSSILVFromInterleaving(YUVFormat::Enum format,
                                                               YUVInterleaving::Enum interleaving)
{
    if (YUVFormat::IsRGB(format)) {
        switch (format) {
            case YUVFormat::RGB24: return PSS_ILV_RGB;
            case YUVFormat::RGBA32: return PSS_ILV_RGBA;
            default: VNFail("Unsupported RGB format"); break;
        }
    } else {
        switch (interleaving) {
            case YUVInterleaving::None: return PSS_ILV_NONE;
            case YUVInterleaving::NV12: return PSS_ILV_NV12;
            default: break;
        }
    }

    return PSS_ILV_NONE;
}

static bool EndsWith(const std::string& left, const std::string& right)
{
    if (right.length() > left.length())
        return false;

    return std::equal(left.rbegin(), left.rbegin() + right.length(), right.rbegin());
}

// -----------------------------------------------------------------------------

static void DeInterleaveNV12To420p(const DataBuffer& src, const YUVDesc& srcDesc, DataBuffer& dst,
                                   const YUVDesc& dstDesc)
{
    // @todo(rob): Support padded stride copy.
    assert(srcDesc.GetFormat() == YUVFormat::YUV8Planar420 &&
           srcDesc.GetInterleaving() == YUVInterleaving::NV12);
    assert(dstDesc.GetFormat() == YUVFormat::YUV8Planar420 &&
           dstDesc.GetInterleaving() == YUVInterleaving::None);

    uint8_t* srcPlanes[3];
    uint8_t* dstPlanes[3];

    srcDesc.GetPlanePointers(&src[0], (const uint8_t**)srcPlanes, nullptr);
    dstDesc.GetPlanePointers(&dst[0], (const uint8_t**)dstPlanes, nullptr);

    // Straight copy luma
    memcpy(dstPlanes[0], srcPlanes[0], srcDesc.GetPlaneMemorySize(0));

    // Deinterleave chroma.
    uint8_t* srcChroma = srcPlanes[1];
    uint8_t* dstU = dstPlanes[1];
    uint8_t* dstV = dstPlanes[2];

    for (uint32_t y = 0; y < dstDesc.GetPlaneHeight(1); ++y) {
        for (uint32_t x = 0; x < dstDesc.GetPlaneWidth(1); ++x) {
            *dstU++ = *srcChroma++;
            *dstV++ = *srcChroma++;
        }
    }
}

using DeinterleaveFn = std::function<void(const DataBuffer&, DataBuffer&)>;

static VNInline DeinterleaveFn GetDeinterleaveFunction(const YUVDesc& src_desc, const YUVDesc& dst_desc)
{
    // @todo(rob): Probably need a LUT here. This also should eventually be hoisted up into utility.
    if (src_desc.GetFormat() == YUVFormat::YUV8Planar420 &&
        src_desc.GetInterleaving() == YUVInterleaving::NV12 &&
        dst_desc.GetFormat() == YUVFormat::YUV8Planar420 &&
        dst_desc.GetInterleaving() == YUVInterleaving::None) {
        return std::bind(DeInterleaveNV12To420p, _1, src_desc, _2, dst_desc);
    }

    return nullptr;
}

static const char* BitDepthToString(uint8_t depth)
{
    switch (depth) {
        case 8: return "8bit";
        case 10: return "10bit";
        case 12: return "12bit";
        case 14: return "14bit";

        default: break;
    }

    return "InvalidBitDepth";
}

static uint32_t BitDepthToValue(perseus_bitdepth depth)
{
    switch (depth) {
        case PSS_DEPTH_8: break;
        case PSS_DEPTH_10: return 10;
        case PSS_DEPTH_12: return 12;
        case PSS_DEPTH_14: return 14;

        default: break; // Worst case scenario we return 8, best chance of not overreading.
    }

    return 8;
}

static const char* ToString(perseus_pipeline_mode mode)
{
    switch (mode) {
        case PPM_SPEED: return "speed";
        case PPM_PRECISION: return "precision";
    }

    return "";
}

static const char* ToString(perseus_simd_type type)
{
    switch (type) {
        case PSS_SIMD_AUTO: return "auto";
        case PSS_SIMD_DISABLED: return "disabled";
    }

    return "";
}

// -----------------------------------------------------------------------------

struct Config : public CmdLineProcessor
{
    std::string base_path;
    std::string perseus_path;
    std::string output_path;
    std::string hash_file;
    std::string perf_file;
    bool deinterleave_hash;
    bool disable_simd;
    std::string interleaving;
    int32_t pipeline_mode;
    float sfilter_strength;
    bool disable_dithering;
    bool use_approximate_pa;
    uint64_t dither_seed;
    int32_t num_threads;
    bool generate_cmdbuffers;
    bool logo_overlay_enable;
    int32_t logo_overlay_position_x;
    int32_t logo_overlay_position_y;
    int32_t logo_overlay_delay;
    bool format_filenames;
    bool dump_dpi_surfaces;
    bool simulate_padding;
    bool old_codes;
    uint32_t frame_count;
    bool highlight_residuals;
    bool parallel_decode;
    std::string internal_stats_path;
    int32_t isolate_frame;
    int32_t isolate_loop_count;

    Config(int32_t argc, const char* argv[])
        : CmdLineProcessor("P.Plus DPI Harness", ' ', "", CmdLineHelpType::Full)
    {
        CmdLineGroupID groupSettings = AddGroup("Settings");
        AddOption<std::string>(groupSettings, base_path, "b", "base", true, "",
                               "Path to a base recon YUV used in place of decoded video");
        AddOption<std::string>(groupSettings, perseus_path, "p", "perseus", true, "",
                               "Path to a Perseus 'bin' file representation");
        AddOption<std::string>(groupSettings, output_path, "o", "output", false, "",
                               "Optional path to output the decoded YUVs to");
        AddOption<std::string>(groupSettings, hash_file, "c", "hash-file", false, "",
                               "Optional path of where to write the hashes json file");
        AddOption<std::string>(groupSettings, perf_file, "", "perf-file", false, "",
                               "Optional path of where to write performance states to");
        AddOption(groupSettings, deinterleave_hash, "", "hash-deinterleave", false,
                  false, "This will perform de-interleaving during hash calculation when calc-hash is supplied and interleaving is not 'none'");
        AddOption(groupSettings, disable_simd, "d", "disable-simd", false,
                  false, "Override the SIMD behaviour of the DPI to explicitly disable it, otherwise it will auto-detect depending on the platform");
        AddOption<std::string>(
            groupSettings, interleaving, "", "interleaving", false, "none",
            "Specify the interleaving mode of the YUV files for input/output [none, nv12]");
        AddOption(groupSettings, pipeline_mode, "", "pipeline-mode", false,
                  0, "Specify the pipeline mode for the Perseus decoder (DPI), 0 for speed (default), or 1 for high precision");
        AddOption(groupSettings, sfilter_strength, "", "s-strength", false, 0.0f, "Strength of the s");
        AddOption(groupSettings, disable_dithering, "", "disable-dithering", false, true,
                  "Whether to override dithering");
        AddOption(groupSettings, dither_seed, "", "dither-seed", false, (uint64_t)0,
                  "Value used to seed the dither buffer, 0 uses current time");
        AddOption(groupSettings, use_approximate_pa, "", "use-approximate-pa", false, false,
                  "If enabled, approximate pre-baked predicted-average will be used");
        AddOption(groupSettings, num_threads, "j", "num-threads", false, -1,
                  "How many threads to use when decoding");
        AddOption(groupSettings, generate_cmdbuffers, "g", "generate-cmdbuffers", false,
                  false, "When enabled will instead of producing surfaces and hashes produce command buffers from the decoder");
        AddOption(groupSettings, logo_overlay_enable, "O", "logo-overlay-enable", false, false,
                  "Enable overlay watermark");
        AddOption(groupSettings, logo_overlay_position_x, "x", "logo-overlay-position-x", false,
                  LOGO_OVERLAY_POSITION_X_DEFAULT, " Specify displacement in pixels of left edge of overlay watermark from left edge of video");
        AddOption(groupSettings, logo_overlay_position_y, "y", "logo-overlay-position-y", false,
                  LOGO_OVERLAY_POSITION_Y_DEFAULT, " Specify displacement in pixels of top edge of overlay watermark from top edge of video");
        AddOption(groupSettings, logo_overlay_delay, "D", "logo-overlay-delay", false,
                  LOGO_OVERLAY_DELAY_DEFAULT,
                  " Specify number of frames to delay before displaying logo overlay");
        AddOption(groupSettings, format_filenames, "", "format-filenames", false,
                  true, "When enabled this will format output YUV filenames based upon Vooya specifications");
        AddOption(groupSettings, dump_dpi_surfaces, "", "dump-dpi-surfaces", false,
                  false, "Can be used to dump internal surfaces to disk, will use -o path if it is specified, otherwise they will be output to current directory");
        AddOption(groupSettings, simulate_padding, "", "simulate-padding", false,
                  false, "When specified all surfaces passed to the DPI will be padded with their strides set to the next power of 2 from their width. And the input file will be read in accounting for the padding");
        AddOption(groupSettings, old_codes, "", "old-codes", false, false,
                  "The input stream uses the 'old' entropy coding codes");
        AddOption(groupSettings, frame_count, "n", "frame-count", false,
                  0u, "Set to non-zero to run up to this number of frames, if the number is bigger than the number of frames on the input then this value is ignored.");
        AddOption(groupSettings, highlight_residuals, "", "highlight-residuals", false, false,
                  "Highlights all residuals");
        AddOption(groupSettings, parallel_decode, "", "parallel-decode", false, false,
                  "Enable parallel decoding");
        AddOption<std::string>(groupSettings, internal_stats_path, "", "internal-stats", false, "", "");

        AddOption(groupSettings, isolate_frame, "", "isolate-frame", false, -1, "");
        AddOption(groupSettings, isolate_loop_count, "", "isolate-loop-count", false, 1000, "");
        ApplyValuesFromCmdLine(argc, argv, true);
    }

    bool WriteSurfaces() const { return !generate_cmdbuffers && !output_path.empty(); }
    bool WriteCmdBuffers() const { return generate_cmdbuffers && !output_path.empty(); }
    bool CalcSurfaceHashes() const { return !generate_cmdbuffers && !hash_file.empty(); }
    bool CalcCmdBufferHashes() const { return generate_cmdbuffers && !hash_file.empty(); }
};

// -----------------------------------------------------------------------------

static void GetFormat(const Config& cfg, const perseus_global_config& lcevc_cfg, uint32_t loq_index,
                      YUVFormat::Enum& format, YUVInterleaving::Enum& interleaving)
{
    if (EndsWith(cfg.base_path, ".rgb")) {
        format = PictureFormat::RGB24;
        interleaving = YUVInterleaving::None;
        return;
    } else if (EndsWith(cfg.base_path, ".rgba")) {
        format = PictureFormat::RGBA32;
        interleaving = YUVInterleaving::None;
        return;
    }

    const perseus_bitdepth loq_depth = lcevc_cfg.bitdepths[(perseus_loq_index)loq_index];

    if (lcevc_cfg.colourspace == PSS_CSP_MONOCHROME) {
        format = (loq_depth == PSS_DEPTH_8) ? YUVFormat::RAW8 : YUVFormat::RAW16;
    } else {
        format = (YUVFormat::Enum)((loq_depth * 3) + lcevc_cfg.colourspace);
    }

    interleaving = YUVInterleaving::FromString2(cfg.interleaving);
}

// -----------------------------------------------------------------------------

class Deinterleaver
{
public:
    bool Initialise(const YUVDesc& desc)
    {
        m_desc.Initialise(desc.GetFormat(), desc.GetWidth(), desc.GetHeight(), YUVInterleaving::None);
        m_func = GetDeinterleaveFunction(desc, m_desc);

        if (m_func == nullptr) {
            VNLogError("Deinterleave function not implemented");
            return false;
        }

        m_buffer.resize(m_desc.GetMemorySize());
        m_bEnabled = true;

        return true;
    }

    const std::vector<uint8_t>& operator()(const std::vector<uint8_t>& data)
    {
        m_func(data, m_buffer);
        return m_buffer;
    }

    bool IsEnabled() const { return m_bEnabled; }

private:
    bool m_bEnabled = false;
    YUVDesc m_desc;
    DeinterleaveFn m_func;
    std::vector<uint8_t> m_buffer;
};

// -----------------------------------------------------------------------------

struct Stage
{
    enum Enum
    {
        UpscaleLOQ2,
        Base,
        UpscaleLOQ1,
        High,
        Sfilter,
        Overlay,
        ConformanceWindow,
        Count,
    };

    bool m_bEnabled = false;
    Enum m_id;
    YUVDesc m_desc;
    Deinterleaver m_deinterleaver;
    YUVFile m_file;
    HasherPtr m_hasher;
    uint32_t m_loqIndex = 0;

    static const char* ToString(Enum stage)
    {
        switch (stage) {
            case UpscaleLOQ2: return "upscale_loq2";
            case Base: return "base";
            case UpscaleLOQ1: return "upscale_loq1";
            case High: return "high";
            case Sfilter: return "sfilter";
            case Overlay: return "overlay";
            case ConformanceWindow: return "conformance_window";
            case Count: return "count";
        }

        return "";
    }

    bool Initialise(Enum id, const Config& cfg, const YUVDesc& desc)
    {
        m_bEnabled = true;
        m_id = id;
        m_desc = desc;

        const bool bWriteSurfaces = cfg.WriteSurfaces();
        const bool bCalcHashes = cfg.CalcSurfaceHashes();
        const bool bDeinterleave = cfg.deinterleave_hash;

        if (bWriteSurfaces) {
            const auto format = desc.GetFormat();
            const auto interleaving = desc.GetInterleaving();

            const char* extension = [&format] {
                if (PictureFormat::IsYUV(format))
                    return "yuv";
                else if (PictureFormat::IsRaw(format))
                    return "y";
                else if (format == PictureFormat::RGB24)
                    return "rgb";

                return "rgba";
            }();

            std::string name(256, 0);

            if (cfg.format_filenames) {
                const char* type = (interleaving == YUVInterleaving::None)
                                       ? YUVFormat::ToString2(format)
                                       : YUVInterleaving::ToString2(interleaving);

                if (YUVFormat::IsRaw(format)) {
                    snprintf(&name[0], name.size(), "%s_%ux%u_%s.%s", ToString(m_id),
                             desc.GetPlaneStridePixels(0), desc.GetHeight(),
                             BitDepthToString(desc.GetBitDepth()), extension);
                } else {
                    snprintf(&name[0], name.size(), "%s_%ux%u_%s_%s.%s", ToString(m_id),
                             desc.GetPlaneStridePixels(0), desc.GetHeight(), type,
                             BitDepthToString(desc.GetBitDepth()), extension);
                }
            } else {
                snprintf(&name[0], name.size(), "%s.%s", ToString(m_id), extension);
            }

            const std::string path = cfg.output_path + name;

            if (m_file.Open(path, desc, true) != YUVFile::Success) {
                VNLogError("Unable to open output file \"%s\"\n", path.c_str(), strerror(errno));
                return false;
            }
        }

        if (bCalcHashes) {
            // NOTE: The hash used must match the one used by the DIL harness.
            // some tests use expect the same hash between the two
            // The DIL has the hashing code built in to it (dec_il_utils.cpp), the DIL harness
            // supports a command line option to change the hasher
            m_hasher = Hasher::Factory(HasherType::XXH3);

            if (!m_hasher) {
                VNLogError("Unable to initialise hash for %s surface\n", ToString(m_id));
                return false;
            }
        }

        if (bDeinterleave && !m_deinterleaver.Initialise(m_desc)) {
            VNLogError("Failed to initialise deinterleaver for stage");
        }

        return true;
    }
};

// -----------------------------------------------------------------------------

class SurfaceWriter
{
public:
    bool Initialise(const Config& cfg, const perseus_decoder_stream& stream)
    {
        YUVInterleaving::Enum interleaving = YUVInterleaving::None;
        YUVFormat::Enum format = YUVFormat::Count;

        const perseus_global_config& global_cfg = stream.global_config;

        // Determine surface descriptions for each LOQ.
        const bool has_loq2 = global_cfg.scaling_modes[PSS_LOQ_1] != PSS_SCALE_0D;
        uint32_t width = global_cfg.width;
        uint32_t height = global_cfg.height;

        for (uint32_t loq_index = 0; loq_index < 3; ++loq_index) {
            GetFormat(cfg, global_cfg, loq_index, format, interleaving);

            if (cfg.deinterleave_hash) {
                if (interleaving == YUVInterleaving::None) {
                    VNLogError("De-interleaving requested but surfaces are not interleaved\n");
                    return false;
                }
            }

            const uint32_t bitdepth = (global_cfg.colourspace == PSS_CSP_MONOCHROME)
                                          ? BitDepthToValue(global_cfg.bitdepths[loq_index])
                                          : 0;

            uint32_t planeStrides[4] = {};
            const uint32_t* planeStridesPtr = nullptr;

            if (cfg.simulate_padding) {
                YUVDesc tmpDesc;
                tmpDesc.Initialise(format, width, height, interleaving, Colorspace::Auto, bitdepth);

                for (uint32_t i = 0; i < tmpDesc.GetPlaneCount(); ++i) {
                    const auto planeWidth = tmpDesc.GetPlaneWidth(i);
                    planeStrides[i] = NextPow2(planeWidth + 1);
                }

                planeStridesPtr = planeStrides;
            }

            if (!m_loqDesc[loq_index].Initialise(format, width, height, interleaving,
                                                 Colorspace::Auto, bitdepth, planeStridesPtr)) {
                VNLogError("Failed to initialise LOQ desc\n");
                return false;
            }

            // Drop resolution by scaling mode for this LOQ.
            const perseus_scaling_mode loq_scaling =
                global_cfg.scaling_modes[(loq_index == 0) ? PSS_LOQ_0 : PSS_LOQ_1];

            if ((loq_scaling == PSS_SCALE_1D) || (loq_scaling == PSS_SCALE_2D)) {
                width = (width + 1) >> 1;
            }

            if (loq_scaling == PSS_SCALE_2D) {
                height = (height + 1) >> 1;
            }
        }

        // Setup each stage.
        if (has_loq2)
            m_stages[Stage::UpscaleLOQ2].Initialise(Stage::UpscaleLOQ2, cfg, m_loqDesc[PSS_LOQ_2]);

        m_stages[Stage::Base].Initialise(Stage::Base, cfg, m_loqDesc[PSS_LOQ_1]);
        m_stages[Stage::UpscaleLOQ1].Initialise(Stage::UpscaleLOQ1, cfg, m_loqDesc[PSS_LOQ_0]);
        m_stages[Stage::High].Initialise(Stage::High, cfg, m_loqDesc[PSS_LOQ_0]);
        m_stages[Stage::Sfilter].Initialise(Stage::Sfilter, cfg, m_loqDesc[PSS_LOQ_0]);
        if (cfg.logo_overlay_enable)
            m_stages[Stage::Overlay].Initialise(Stage::Overlay, cfg, m_loqDesc[PSS_LOQ_0]);

        // Check conformance window
        auto& conformanceWindow = stream.conformance_window;

        if (conformanceWindow.enabled) {
            auto& planeWindow = conformanceWindow.planes[0];
            YUVDesc& loq0Desc = m_loqDesc[PSS_LOQ_0];

            const uint32_t conformanceWidth = global_cfg.width - (planeWindow.left + planeWindow.right);
            const uint32_t conformanceHeight = global_cfg.height - (planeWindow.top + planeWindow.bottom);

            m_conformanceDesc.Initialise(loq0Desc.GetFormat(), conformanceWidth, conformanceHeight,
                                         loq0Desc.GetInterleaving());

            m_stages[Stage::ConformanceWindow].Initialise(Stage::ConformanceWindow, cfg, m_conformanceDesc);
        }

        m_cfg = &cfg;

        return true;
    }

    bool Update(Stage::Enum stage_id, const DataBuffer& img)
    {
        auto& stage = m_stages[stage_id];

        if (!stage.m_bEnabled)
            return true;

        auto& file = stage.m_file;
        auto& deinterleaver = stage.m_deinterleaver;
        auto& hasher = stage.m_hasher;

        if (file.IsOpen()) {
            const auto& yuv_data = deinterleaver.IsEnabled() ? deinterleaver(img) : img;

            if (file.Write(yuv_data.data()) != YUVFile::Success) {
                VNLogError("Could not write frame to file\n");
                return false;
            }
        }

        if (hasher) {
            const auto& hash_data =
                (deinterleaver.IsEnabled() && m_cfg->deinterleave_hash) ? deinterleaver(img) : img;

            if (!hasher->Update(hash_data.data(), (uint32_t)hash_data.size())) {
                VNLogError("Could not update hash\n");
                return false;
            }
        }

        return true;
    }

    std::vector<std::tuple<std::string, Hasher*>> GetHashers()
    {
        std::vector<std::tuple<std::string, Hasher*>> res;

        if (m_cfg->CalcSurfaceHashes()) {
            for (auto& stage : m_stages) {
                if (stage.m_bEnabled) {
                    res.push_back({Stage::ToString(stage.m_id), stage.m_hasher.get()});
                }
            }
        }

        return res;
    }

    inline const YUVDesc& GetLOQDesc(uint32_t loq_index) const
    {
        assert(loq_index < static_cast<uint32_t>(m_loqDesc.size()));
        return m_loqDesc[loq_index];
    }

    inline const YUVDesc& GetConformanceDesc() const { return m_conformanceDesc; }

private:
    const Config* m_cfg;
    std::array<YUVDesc, 3> m_loqDesc;
    YUVDesc m_conformanceDesc;
    std::array<Stage, Stage::Count> m_stages;
};

// -----------------------------------------------------------------------------

class CmdBufferWriter
{
    struct Output
    {
        FILE* file;
        HasherPtr hasher;
    };

public:
    bool Initialise(const Config& cfg)
    {
        m_cfg = &cfg;

        if (cfg.WriteCmdBuffers()) {
            const char* output_filenames[] = {"loq0_intra_cmdbuffer.bin", "loq0_inter_cmdbuffer.bin",
                                              "loq0_clear_cmdbuffer.bin", "loq1_intra_cmdbuffer.bin"};

            for (uint32_t i = 0; i < 4; ++i) {
                const std::string path = cfg.output_path + output_filenames[i];
                m_output[i].file = fopen(path.c_str(), "wb");

                if (!m_output[i].file) {
                    VNLogError("Unable to open cmdbuffer output file: %s\n", path.c_str());
                    return false;
                }
            }
        }

        if (cfg.CalcCmdBufferHashes()) {
            for (auto& output : m_output) {
                output.hasher = Hasher::Factory(HasherType::XXH3);

                if (!output.hasher) {
                    VNLogError("Unable to initialise hasher for cmdbuffer\n");
                    return false;
                }
            }
        }

        return true;
    }

    bool Update(perseus_decoder decoder, perseus_loq_index loq)
    {
        std::vector<std::tuple<perseus_cmdbuffer_id, int32_t>> updateList;

        if (m_cfg->generate_cmdbuffers) {
            if (loq == PSS_LOQ_0) {
                updateList.push_back({PSS_CBID_Intra, 0});
                updateList.push_back({PSS_CBID_Inter, 1});
                updateList.push_back({PSS_CBID_Clear, 2});
            } else {
                updateList.push_back({PSS_CBID_Intra, 3});
            }
        }

        for (auto& updateItem : updateList) {
            Output& output = m_output[std::get<1>(updateItem)];

            // @todo: Per-plane cmd buffers.

            perseus_cmdbuffer cmdBuffer = {};
            if (perseus_decoder_get_cmd_buffer(decoder, std::get<0>(updateItem), loq, 0, &cmdBuffer) != 0) {
                VNLogError("Failed to retrieve command buffer\n");
                return false;
            }

            int32_t dataElementSize = 0;
            switch (cmdBuffer.type) {
                case PSS_CBT_2x2: dataElementSize = sizeof(perseus_cmdbuffer_2x2); break;
                case PSS_CBT_4x4: dataElementSize = sizeof(perseus_cmdbuffer_4x4); break;
                case PSS_CBT_Clear: break;
            }

            if (m_cfg->WriteCmdBuffers()) {
                fwrite(&cmdBuffer.count, sizeof(int32_t), 1, output.file);
                fwrite(&dataElementSize, sizeof(int32_t), 1, output.file);

                if (cmdBuffer.count) {
                    fwrite(cmdBuffer.coords, sizeof(perseus_cmdbuffer_coords), cmdBuffer.count,
                           output.file);

                    if (cmdBuffer.data) {
                        fwrite(cmdBuffer.data, dataElementSize, cmdBuffer.count, output.file);
                    }
                }
            }

            if (m_cfg->CalcCmdBufferHashes()) {
                output.hasher->Update(cmdBuffer.count);
                output.hasher->Update(dataElementSize);

                output.hasher->Update(cmdBuffer.coords, sizeof(perseus_cmdbuffer_coords) * cmdBuffer.count);
                output.hasher->Update(cmdBuffer.data, dataElementSize * cmdBuffer.count);
            }
        }

        return true;
    }

    std::vector<std::tuple<std::string, Hasher*>> GetHashers()
    {
        std::vector<std::tuple<std::string, Hasher*>> res;

        if (m_cfg->CalcCmdBufferHashes()) {
            res.push_back({"cmdbuffer_loq_0_intra", m_output[0].hasher.get()});
            res.push_back({"cmdbuffer_loq_0_inter", m_output[1].hasher.get()});
            res.push_back({"cmdbuffer_loq_0_clear", m_output[2].hasher.get()});
            res.push_back({"cmdbuffer_loq_1_intra", m_output[3].hasher.get()});
        }

        return res;
    }

private:
    std::array<Output, 4> m_output;
    const Config* m_cfg;
};

// -----------------------------------------------------------------------------

bool ApplyConformanceWindow(const lcevc_conformance_window& window, const DataBuffer& srcData,
                            const YUVDesc& srcDesc, DataBuffer& dstData, const YUVDesc& dstDesc)
{
    if (srcDesc.GetFormat() != dstDesc.GetFormat()) {
        VNLogError("Both src and dst must have same format\n");
        return false;
    }

    const uint8_t* srcPtrs[3] = {};
    uint32_t srcStrides[3] = {};

    uint8_t* dstPtrs[3] = {};
    uint32_t dstStrides[3] = {};

    srcDesc.GetPlanePointers(srcData.data(), (const uint8_t**)srcPtrs, srcStrides);
    dstDesc.GetPlanePointers(dstData.data(), (uint8_t**)dstPtrs, dstStrides);

    for (uint32_t planeIndex = 0; planeIndex < srcDesc.GetPlaneCount(); ++planeIndex) {
        const auto& planeWindow = window.planes[planeIndex];

        // Scale the conformance window accordingly to select from the source.
        const uint32_t planeWindowX = planeWindow.left;
        const uint32_t planeWindowY = planeWindow.top;
        const uint32_t planeWindowWidth =
            srcDesc.GetPlaneWidth(planeIndex) - (planeWindow.left + planeWindow.right);
        const uint32_t planeWindowHeight =
            srcDesc.GetPlaneHeight(planeIndex) - (planeWindow.top + planeWindow.bottom);

        if (planeWindowWidth != dstDesc.GetPlaneWidth(planeIndex)) {
            VNLogError("Expected dst plane width to be the same as the scaled conformance window "
                       "width. [Expected: %u, Got: %u]\n",
                       dstDesc.GetPlaneWidth(planeIndex), planeWindowWidth);
            return false;
        }

        if (planeWindowHeight != dstDesc.GetPlaneHeight(planeIndex)) {
            VNLogError("Expected dst plane height to be the same as the scaled conformance window "
                       "height. [Expected: %u, Got: %u]\n",
                       dstDesc.GetPlaneHeight(planeIndex), planeWindowHeight);
            return false;
        }

        const uint32_t pixelStride = srcDesc.GetPlanePixelStride(planeIndex);
        const uint32_t srcByteStride = srcDesc.GetPlaneStrideBytes(planeIndex);
        const uint32_t dstByteStride = dstDesc.GetPlaneStrideBytes(planeIndex);
        const uint8_t* srcPtr = srcPtrs[planeIndex] + (planeWindowY * srcByteStride) +
                                (srcDesc.GetByteDepth() * planeWindowX * pixelStride);
        uint8_t* dstPtr = dstPtrs[planeIndex];

        // Line by line memcpy to dst.
        for (uint32_t y = 0; y < planeWindowHeight; ++y) {
            memcpy(dstPtr, srcPtr, dstByteStride);

            srcPtr += srcByteStride;
            dstPtr += dstByteStride;
        }
    }

    return true;
}

bool ReadInputFrame(Config& cfg, YUVFile& inputYUV, DataBuffer& inputBuffer, YUVDesc& inputYUVDesc,
                    perseus_image* loqImages, const uint32_t inputLOQ)
{
    if (cfg.simulate_padding) {
        inputBuffer.resize(inputYUVDesc.GetMemorySize());

        if (inputYUV.ReadFrame(inputBuffer.data()) != YUVFile::Success) {
            VNLogError("Failed to read from base YUV file: %s\n", cfg.base_path.c_str());
            return false;
        }

        // Copy into strided memory directly.
        uint8_t* inputPlanes[3];
        uint32_t inputStrides[3];

        inputYUVDesc.GetPlanePointers(inputBuffer.data(), inputPlanes, inputStrides);

        for (uint32_t planeIdx = 0; planeIdx < 3; ++planeIdx) {
            const uint32_t planeHeight = inputYUVDesc.GetPlaneHeight(planeIdx);

            const uint8_t* planeSrc = inputPlanes[planeIdx];
            const uint32_t planeSrcStride = inputStrides[planeIdx];
            uint8_t* planeDst = (uint8_t*)loqImages[inputLOQ].plane[planeIdx];
            const uint32_t planeDstStride = loqImages[inputLOQ].stride[planeIdx];

            assert(planeDstStride > planeSrcStride);

            for (uint32_t y = 0; y < planeHeight; y++) {
                memcpy(planeDst, planeSrc, planeSrcStride);

                planeDst += planeDstStride;
                planeSrc += planeSrcStride;
            }
        }
    } else {
        if (inputYUV.ReadFrame(static_cast<uint8_t*>(loqImages[inputLOQ].plane[0])) != YUVFile::Success) {
            VNLogError("Failed to read from base YUV file: %s\n", cfg.base_path.c_str());
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------

class PerfStats
{
public:
    void initialise(const std::string& filePath)
    {
        if (!filePath.empty()) {
            m_file.open(filePath.c_str(), std::ofstream::out);
        }
    }

    void beginFrame() { m_times.clear(); }

    void endFrame()
    {
        if (m_file.is_open()) {
            if (m_index == 0) {
                m_file << "\n";
            }

            for (const auto time : m_times) {
                m_file << time << ",";
            }
            m_file << "\n";

            m_file.flush();
        }

        m_index++;
    }

    template <typename Fn, typename... ArgsT>
    int32_t profileFunction(const char* label, Fn func, ArgsT... args)
    {
        Timer<NanoSecond> timer;
        timer.Start();
        VNRet(func(args...), "Failed to invoke profiled function\n");
        m_times.push_back(timer.Stop());

        if (m_index == 0 && m_file.is_open()) {
            m_file << label << ",";
        }

        return 0;
    }

private:
    std::ofstream m_file;
    uint64_t m_index = 0;
    std::vector<NanoSecond::rep> m_times;
};

// -----------------------------------------------------------------------------

void decoderLogCallback(void* userData, perseus_decoder_log_type type, const char* msg, size_t msgLength)
{
    VNUnused(userData);
    VNUnused(msgLength);

    LogType kTable[] = {LogType_Error, LogType_Info, LogType_Warning, LogType_Debug, LogType_Debug};

    VNLog(kTable[type], "%s", msg);
}

int32_t main(int32_t argc, const char* argv[])
{
    // Setup logging
    log_set_enable_stdout(true);
    log_set_verbosity(LogType_Debug);
    log_set_enable_function_names(false);

    // Load up config
    Config cfg(argc, argv);

    // Open Perseus bin file and read first frame for stream cfg
    LCEVCBinReader reader;
    BlockLoader loader(reader);

    if (!loader.Initialise(cfg.perseus_path)) {
        VNLogError("Failed to initialise LCEVC reader\n");
        return -1;
    }

    uint32_t frame = 0;
    LCEVCBinCacheBlock block;
    if (!loader.GetNextReorderedBlock(block)) {
        VNLogError("Failed to read initial block of LCEVC dat\n");
        return -1;
    }

    // Open Perseus decoder
    perseus_decoder_config perseusCfg;
    VNRet(perseus_decoder_config_init(&perseusCfg),
          "Failed to initialise Perseus decoder config\n");
    perseusCfg.simd_type = cfg.disable_simd ? PSS_SIMD_DISABLED : PSS_SIMD_AUTO;
    perseusCfg.pipeline_mode = cfg.pipeline_mode ? PPM_PRECISION : PPM_SPEED;
    if (cfg.IsOptionSet("s-strength")) {
        VNLogInfo("Overriding signalled S strength with %f\n", cfg.sfilter_strength);
        perseusCfg.s_strength = cfg.sfilter_strength;
    }
    perseusCfg.disable_dithering = cfg.disable_dithering;
    perseusCfg.use_approximate_pa = cfg.use_approximate_pa;
    perseusCfg.dither_seed = cfg.dither_seed;
    perseusCfg.num_worker_threads = cfg.num_threads;
    perseusCfg.generate_cmdbuffers = cfg.generate_cmdbuffers;
    perseusCfg.logo_overlay_enable = cfg.logo_overlay_enable;
    perseusCfg.logo_overlay_position_x = cfg.logo_overlay_position_x;
    perseusCfg.logo_overlay_position_y = cfg.logo_overlay_position_y;
    perseusCfg.logo_overlay_delay = cfg.logo_overlay_delay;
    perseusCfg.dump_path = cfg.output_path.empty() ? NULL : cfg.output_path.c_str();
    perseusCfg.dump_surfaces = cfg.dump_dpi_surfaces;
    perseusCfg.use_old_code_lengths = cfg.old_codes;
    perseusCfg.log_callback = &decoderLogCallback;
    perseusCfg.use_parallel_decode = cfg.parallel_decode ? 1 : 0;
    perseusCfg.debug_internal_stats_path =
        cfg.internal_stats_path.empty() ? nullptr : cfg.internal_stats_path.c_str();

    VNLogDebug("SIMD type: %s\n", ToString(perseusCfg.simd_type));
    VNLogDebug("Pipeline mode: %s\n", ToString(perseusCfg.pipeline_mode));
    VNLogDebug("S strength %f\n", perseusCfg.s_strength);
    VNLogDebug("Dithering: %s\n", perseusCfg.disable_dithering ? "disabled" : "enabled");
    VNLogDebug("Dither seed: %" PRIu64 "\n", perseusCfg.dither_seed);
    VNLogDebug("Approximate PA: %s\n", perseusCfg.use_approximate_pa ? "enabled" : "disabled");
    VNLogDebug("Generate cmdbuffers: %s\n", perseusCfg.generate_cmdbuffers ? "enabled" : "disabled");
    VNLogDebug("Overlay: %s\n", perseusCfg.logo_overlay_enable ? "enabled" : "disabled");
    VNLogDebug("Dump DPI surfaces: %s [%s]\n", perseusCfg.dump_surfaces ? "enabled" : "disabled",
               perseusCfg.dump_path ? perseusCfg.dump_path : "default");
    VNLogDebug("Old-codes: %s\n", perseusCfg.use_old_code_lengths ? "enabled" : "disabled");
    VNLogDebug("Parallel decode: %s\n", perseusCfg.use_parallel_decode ? "enabled" : "disabled");
    VNLogDebug("DPI Stats path: %s\n",
               perseusCfg.debug_internal_stats_path ? perseusCfg.debug_config_path : "empty");

    perseus_decoder decoder = nullptr;
    VNRet(perseus_decoder_open(&decoder, &perseusCfg), "Failed to open decoder\n");

    if (cfg.highlight_residuals) {
        perseus_decoder_debug(decoder, HIGHLIGHT_RESIDUALS);
    }

    // Parse first frame for stream cfg
    perseus_decoder_stream streamCfg;

    VNRet(perseus_decoder_parse(decoder, block.payload.data, block.payload.dataSize, &streamCfg),
          "Failed to parse first frame of Perseus data\n");

    SurfaceWriter surfaceWriter;

    if (!surfaceWriter.Initialise(cfg, streamCfg)) {
        VNLogError("Unable to initialise surface writer\n");
        return -1;
    }

    // Open input YUV file.
    const perseus_global_config& globalCfg = streamCfg.global_config;
    const uint32_t inputLOQ = (globalCfg.scaling_modes[PSS_LOQ_1] == PSS_SCALE_0D) ? 1 : 2;

    YUVFile inputYUV;
    const YUVDesc& inputLOQDesc = surfaceWriter.GetLOQDesc(inputLOQ);
    YUVDesc inputYUVDesc;
    inputYUVDesc.Initialise(inputLOQDesc.GetFormat(), inputLOQDesc.GetWidth(),
                            inputLOQDesc.GetHeight(), inputLOQDesc.GetInterleaving(),
                            inputLOQDesc.GetColorspace(), inputLOQDesc.GetBitDepth());

    if (inputYUV.Open(cfg.base_path, inputYUVDesc, false) != YUVFile::Success) {
        VNLogError("Failed to open base yuv file: %s\n", cfg.base_path.c_str());
        return -1;
    }

    // Setup data buffers for input & each LOQ.
    DataBuffer inputBuffer = {};
    perseus_image loqImages[3] = {};
    DataBuffer loqBuffers[3] = {};

    for (uint32_t loqIndex = 0; loqIndex <= inputLOQ; ++loqIndex) {
        const YUVDesc& loqDesc = surfaceWriter.GetLOQDesc(loqIndex);

        loqBuffers[loqIndex].resize(loqDesc.GetMemorySize(), 0);

        perseus_image& loqImage = loqImages[loqIndex];
        loqImage.ilv = GetPSSILVFromInterleaving(loqDesc.GetFormat(), loqDesc.GetInterleaving());
        loqImage.depth = globalCfg.bitdepths[loqIndex];

        loqDesc.GetPlanePointers(loqBuffers[loqIndex].data(), (const uint8_t**)loqImage.plane,
                                 loqImage.stride);

        if (PictureFormat::IsRGB(loqDesc.GetFormat())) {
            for (uint32_t i = 0; i < loqDesc.GetPlaneCount(); ++i) {
                loqImage.stride[i] = loqDesc.GetPlaneStridePixels(i);
            }
        }
    }

    const auto& conformanceWindow = streamCfg.conformance_window;
    DataBuffer conformanceBuffer;

    if (conformanceWindow.enabled) {
        const YUVDesc& conformanceDesc = surfaceWriter.GetConformanceDesc();
        conformanceBuffer.resize(conformanceDesc.GetMemorySize(), 0);
    }

    CmdBufferWriter cmdbufferWriter;
    if (!cmdbufferWriter.Initialise(cfg)) {
        VNLogError("Unable to initialise cmdbuffer writer\n");
        return -1;
    }

    PerfStats perfStats;
    perfStats.initialise(cfg.perf_file);

    // Process loop
    perseus_decoder_stream strm = {};

    Timer<NanoSecond> totalTime;
    totalTime.Start();

    int64_t peakFrameTime = 0;

    bool bIsolatingFrame = (cfg.isolate_frame != -1);
    int32_t loopCount = bIsolatingFrame ? cfg.isolate_loop_count : 1;

    do {
        printf("Frame: %d\n", (int)frame);

        Timer<NanoSecond> frameTime;
        frameTime.Start();

        perfStats.beginFrame();

        if (!ReadInputFrame(cfg, inputYUV, inputBuffer, inputYUVDesc, loqImages, inputLOQ)) {
            return -1;
        }

        VNRet(perfStats.profileFunction("Parse", perseus_decoder_parse, decoder, block.payload.data,
                                        block.payload.dataSize, &strm),
              "Failed to parse frame %u\n", frame);

        if (bIsolatingFrame && (frame != static_cast<uint32_t>(cfg.isolate_frame))) {
            // Just parse and skip decoding if we're isolating a single frame.
            ++frame;
            continue;
        }

        if (strm.global_config.scaling_modes[PSS_LOQ_1] != PSS_SCALE_0D) {
            // Perform initial upscale if the stream is configured as such.
            VNRet(perfStats.profileFunction("Upscale to LOQ-1", perseus_decoder_upscale, decoder,
                                            &loqImages[1], &loqImages[2], PSS_LOQ_2),
                  "Failed to upscale from LOQ-2 on frame %u\n", frame);

            if (!surfaceWriter.Update(Stage::UpscaleLOQ2, loqBuffers[1])) {
                VNLogError("Failed to update upscale loq-2 frame %u\n", frame);
                return -1;
            }
        }

        VNRet(perfStats.profileFunction("Decode LOQ-1", perseus_decoder_decode_base, decoder,
                                        &loqImages[1]),
              "Failed to decode base layer on frame %u\n", frame);

        if (!surfaceWriter.Update(Stage::Base, loqBuffers[1])) {
            VNLogError("Failed to update base frame %u\n", frame);
            return -1;
        }

        if (!cmdbufferWriter.Update(decoder, PSS_LOQ_1)) {
            VNLogError("Failed to update LOQ-1 cmd buffers\n");
            return -1;
        }

        VNRet(perfStats.profileFunction("Upscale to LOQ-0", perseus_decoder_upscale, decoder,
                                        &loqImages[0], &loqImages[1], PSS_LOQ_1),
              "Failed to upscale from LOQ-1 on frame %u\n", frame);

        if (!surfaceWriter.Update(Stage::UpscaleLOQ1, loqBuffers[0])) {
            VNLogError("Unable to update upscale frame %u\n", frame);
            return -1;
        }

        // If isolating then loop hard.
        for (int32_t i = 0; i < loopCount; ++i) {
            if (bIsolatingFrame) {
                VNLogDebug("Isolate: %d\n", i);
            }

            VNRet(perfStats.profileFunction("Decode LOQ-0", perseus_decoder_decode_high, decoder,
                                            &loqImages[0]),
                  "Failed to decode high layer on frame %u\n", frame);
        }

        if (!surfaceWriter.Update(Stage::High, loqBuffers[0])) {
            VNLogError("Unable to update high frame %u\n", frame);
            return -1;
        }

        if (!cmdbufferWriter.Update(decoder, PSS_LOQ_0)) {
            VNLogError("Failed to update LOQ-1 cmd buffers\n");
            return -1;
        }

        VNRet(perfStats.profileFunction("Apply S-Filter", perseus_decoder_apply_s, decoder, &loqImages[0]),
              "Failed to apply sfilter on frame %u\n", frame);

        if (!surfaceWriter.Update(Stage::Sfilter, loqBuffers[0])) {
            VNLogError("Unable to update sfilter frame %u\n", frame);
            return -1;
        }

        if (cfg.logo_overlay_enable) {
            VNRet(perfStats.profileFunction("Apply Overlay", perseus_decoder_apply_overlay, decoder,
                                            &loqImages[0]),
                  "Failed to apply overlay on frame %u\n", frame);

            if (!surfaceWriter.Update(Stage::Overlay, loqBuffers[0])) {
                VNLogError("Unable to update overlay frame %u\n", frame);
                return -1;
            }
        }

        // Build the conformance window
        if (conformanceWindow.enabled) {
            const YUVDesc& loqDesc = surfaceWriter.GetLOQDesc(0);
            if (!ApplyConformanceWindow(conformanceWindow, loqBuffers[0], loqDesc,
                                        conformanceBuffer, surfaceWriter.GetConformanceDesc())) {
                VNLogError("Unable to apply conformance window frame %u\n", frame);
                return -1;
            }

            if (!surfaceWriter.Update(Stage::ConformanceWindow, conformanceBuffer)) {
                VNLogError("Unable to update conformance window frame %u\n", frame);
                return -1;
            }
        }

        ++frame;
        perfStats.endFrame();

        const auto frameTimeNS = frameTime.Stop();
        peakFrameTime = std::max(peakFrameTime, frameTimeNS);

        if ((cfg.frame_count > 0) && (frame >= cfg.frame_count)) {
            break;
        }

    } while (loader.GetNextReorderedBlock(block));

    perseus_decoder_close(decoder);

    if (cfg.CalcSurfaceHashes() || cfg.CalcCmdBufferHashes()) {
        auto hashers = surfaceWriter.GetHashers();
        auto cmdbuffer_hashers = cmdbufferWriter.GetHashers();
        hashers.insert(hashers.end(), cmdbuffer_hashers.begin(), cmdbuffer_hashers.end());

        auto* file = fopen(cfg.hash_file.c_str(), "w");

        if (file == nullptr) {
            VNLogError("Unable to open hash file: %s\n", cfg.hash_file.c_str());
            return -1;
        }

        fprintf(file, "{\n");

        for (size_t i = 0; i < hashers.size(); ++i) {
            auto& output = hashers[i];

            const std::string& id = std::get<0>(output);
            Hasher* hasher = std::get<1>(output);
            const std::string digest = hasher->GetDigestHex();

            fprintf(file, "    \"%s\": \"%s\"%s\n", id.c_str(), digest.c_str(),
                    (i == (hashers.size() - 1)) ? "" : ",");
        }

        fprintf(file, "}\n");
        fclose(file);
    }

    const auto decodeTime = static_cast<double>(totalTime.Stop()) / 1000000000.0;
    const auto decodeFPS = frame / decodeTime;
    const auto peakTimeMS = static_cast<double>(peakFrameTime) / 1000000.0;

    printf("Decoding took: %.4fs, FPS: %.2f, Peak Time (%.4fms)\n", decodeTime, decodeFPS, peakTimeMS);

    return 0;
}

// ----------------------------------------------------------------------------
