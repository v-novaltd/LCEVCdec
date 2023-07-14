import pytest
from collections import namedtuple


class EILPytestParam(object):
    def __init__(self, name: str, v: str, eil_params: dict):
        self.name = name
        self.v = v
        self.eil_params = eil_params
        self.id = f'{name}_{v}'

    def __str__(self) -> str:
        return self.v

    def eil_params_str(self) -> str:
        return ';'.join(
            map(lambda kv: f'{kv[0]}={kv[1]}', self.eil_params.items()))


class Sequence:
    Input = {
        'YUV': {
            8: "input_1280x720_8bit_420p.yuv",
            10: "input_1280x720_10bit_420p.yuv",
        },
        'RGB': {
            8: {
                'rgb': "input_1280x720_8bit_I444.rgb",
                'bgr': "input_1280x720_8bit_I444.bgr",
                'rgba': "input_1280x720_8bit_I444.rgba",
                'bgra': "input_1280x720_8bit_I444.bgra",
                'argb': "input_1280x720_8bit_I444.argb",
                'abgr': "input_1280x720_8bit_I444.abgr",
            },
            10: "input_1280x720_10bit_I444.rgb",
        },
        'YUV_360p': {
            8: 'input_640x360_8bit_420p.yuv',
        },
        'YUV_540p': {
            8: 'Cactus_960x540_8bit_v3.yuv',
        },
        'YUV_720p': {
            8: 'input_1280x720_8bit_420p.yuv'
        },
        'YUV_1080p': {
            8: 'input_1920x1080_8bit_420p.yuv',
            10: 'input_1920x1080_10bit_420p.yuv'
        },
        'YUV_1920p': {
            8: 'input_3680x1920_8bit_420p.yuv',
        },
        'YUV_4k': {
            8: 'input_3840x2160_8bit_420p.yuv',
        }
    }

    Base = {
        8: {
            "2D": {
                "0D": "base_640x360_8bit_420p.yuv",
                "1D": "base_320x360_8bit_420p.yuv",
                "2D": "base_320x180_8bit_420p.yuv"
            },
            "1D": {
                "0D": "base_640x720_8bit_420p.yuv",
                "1D": "base_320x720_8bit_420p.yuv",
                "2D": "base_320x360_8bit_420p.yuv"
            },
            "0D": {
                "0D": "input_1280x720_8bit_420p.yuv",
                "1D": "base_640x720_8bit_420p.yuv",
                "2D": "base_640x360_8bit_420p.yuv"
            }
        },
        10: {
            "2D": {
                "0D": "base_640x360_10bit_420p.yuv",
                "1D": "base_320x360_10bit_420p.yuv",
                "2D": "base_320x180_10bit_420p.yuv"
            },
            "1D": {
                "0D": "base_640x720_10bit_420p.yuv",
                "1D": "base_320x720_10bit_420p.yuv",
                "2D": "base_320x360_10bit_420p.yuv"
            },
            "0D": {
                "0D": "input_1280x720_10bit_420p.yuv",
                "1D": "base_640x720_10bit_420p.yuv",
                "2D": "base_640x360_10bit_420p.yuv",
            }
        },
    }

    BaseRGB = {
        8: {
            "2D": {"0D": "base_640x360_8bit_I444.rgb"},
            "1D": {"0D": "base_640x720_8bit_I444.rgb"},
        }
    }

    BaseNV12 = {
        8: {
            "2D": {"0D": "base_640x360_8bit_nv12.yuv"},
            "1D": {"0D": "base_640x720_8bit_nv12.yuv"},
        },
    }

    # TODO: These are all I420 inputs, maybe some NV12 or RGB inputs would be good?
    Input_Dithering = [
        "lcevc_Cactus_1920x1080_8bit_yuv",
        "lcevc_FootballerNoDithering_640x360_8bit_420p_yuv"
    ]

    # TODO: These are all I420 inputs, maybe some NV12 or RGB inputs would be good?
    Input_Sample = [
        "lcevc_Cactus_1920x1080_8bit_yuv.ts",
        "lcevc_base_640x360_8bit_420p_yuv.ts"
    ]

    Basic = pytest.param(Input['YUV'][8], Base[8]["2D"]["0D"],
                         "2D", "0D", True, id="2D_0D_8bit_speed")

    # Set of sequences and modes that should hit every code path
    # Param 1: Input sequence
    # Param 2: Base sequence
    # Param 3: Scaling mode level 0
    # Param 4: Scaling mode level 1
    # Param 5: Speed/Precision mode
    #          in encoder: encoding_upsample_decoder_compatible_8b
    #          in decoder: pipeline-mode
    FullCoverage = [
        Basic,
        pytest.param(Input['YUV'][8], Base[8]["2D"]["0D"],
                     "2D", "0D", False, id="2D_0D_8bit_precision"),
        pytest.param(Input['YUV'][8], Base[8]["1D"]["0D"],
                     "1D", "0D", False, id="1D_0D_8bit_precision"),
        pytest.param(Input['YUV'][8], Base[8]["0D"]["0D"],
                     "0D", "0D", False, id="0D_0D_8bit_precision"),
        pytest.param(Input['YUV'][10], Base[10]["2D"]["0D"],
                     "2D", "0D", False, id="2D_0D_10bit"),
    ]

    ScalingModeCoverage = [
        pytest.param(Input["YUV"][8], Base[8]["2D"]["1D"],
                     "2D", "1D", True, id="2D_1D_8bit_speed"),
        pytest.param(Input["YUV"][8], Base[8]["2D"]["2D"],
                     "2D", "2D", True, id="2D_2D_8bit_speed"),
        pytest.param(Input["YUV"][8], Base[8]["1D"]["2D"],
                     "1D", "2D", True, id="1D_2D_8bit_speed"),
        pytest.param(Input["YUV"][8], Base[8]["1D"]["1D"],
                     "1D", "1D", True, id="1D_1D_8bit_speed"),

        pytest.param(Input["YUV"][8], Base[8]["0D"]["0D"],
                     "0D", "0D", True, id="0D_0D_8bit_speed"),
        pytest.param(Input["YUV"][8], Base[8]["1D"]["0D"],
                     "0D", "1D", True, id="0D_1D_8bit_speed"),
        pytest.param(Input["YUV"][8], Base[8]["2D"]["0D"],
                     "0D", "2D", True, id="0D_2D_8bit_speed"),

        pytest.param(Input["YUV"][8], Base[8]["2D"]["1D"],
                     "2D", "1D", False, id="2D_1D_8bit_precision"),
        pytest.param(Input["YUV"][8], Base[8]["2D"]["2D"],
                     "2D", "2D", False, id="2D_2D_8bit_precision"),
        pytest.param(Input["YUV"][8], Base[8]["1D"]["2D"],
                     "1D", "2D", False, id="1D_2D_8bit_precision"),
        pytest.param(Input["YUV"][8], Base[8]["1D"]["1D"],
                     "1D", "1D", False, id="1D_1D_8bit_precision"),

        pytest.param(Input["YUV"][8], Base[8]["0D"]["0D"],
                     "0D", "0D", False, id="0D_0D_8bit_precision"),
        pytest.param(Input["YUV"][8], Base[8]["1D"]["0D"],
                     "0D", "1D", False, id="0D_1D_8bit_precision"),
        pytest.param(Input["YUV"][8], Base[8]["2D"]["0D"],
                     "0D", "2D", False, id="0D_2D_8bit_precision"),

        pytest.param(Input["YUV"][10], Base[10]["2D"]["1D"],
                     "2D", "1D", False, id="2D_1D_10bit"),
        pytest.param(Input["YUV"][10], Base[10]["2D"]["2D"],
                     "2D", "2D", False, id="2D_2D_10bit"),
        pytest.param(Input["YUV"][10], Base[10]["1D"]["2D"],
                     "1D", "2D", False, id="1D_2D_10bit"),
        pytest.param(Input["YUV"][10], Base[10]["1D"]["1D"],
                     "1D", "1D", False, id="1D_1D_10bit"),

        pytest.param(Input["YUV"][10], Base[10]["0D"]["0D"],
                     "0D", "0D", False, id="0D_0D_10bit"),
        pytest.param(Input["YUV"][10], Base[10]["1D"]["0D"],
                     "0D", "1D", False, id="0D_1D_10bit"),
        pytest.param(Input["YUV"][10], Base[10]["2D"]["0D"],
                     "0D", "2D", False, id="0D_2D_10bit"),
    ]

    ScalingModeCoverageVulkan = [
        pytest.param("2D", "0D", 8, id="2D_0D_8bit"),
        pytest.param("2D", "1D", 8, id="2D_1D_8bit"),
        pytest.param("2D", "2D", 8, id="2D_2D_8bit"),

        pytest.param("1D", "0D", 8, id="1D_0D_8bit"),
        pytest.param("1D", "1D", 8, id="1D_1D_8bit"),
        pytest.param("1D", "2D", 8, id="1D_2D_8bit"),

        pytest.param("0D", "0D", 8, id="0D_0D_8bit"),
    ]

    CornerCases = [
        pytest.param("input_856x480_8bit_420p.yuv",
                     "base_428x240_8bit_420p.yuv"),
        pytest.param("input_704x576_8bit_420p.yuv",
                     "base_352x288_8bit_420p.yuv"),
        pytest.param("input_600x420_8bit_420p.yuv",
                     "base_300x210_8bit_420p.yuv"),
    ]

    DesyncDepthsCoverage = [
        pytest.param(Input["YUV"][10], Base[8]["2D"]["0D"], "2D",
                     "0D", False, id="2D_0D_10bit_base_8bit_precision"),
        pytest.param(Input["YUV"][10], Base[8]["1D"]["0D"], "1D",
                     "0D", False, id="1D_0D_10bit_base_8bit_precision"),
        pytest.param(Input["YUV"][10], Base[8]["0D"]["0D"], "0D",
                     "0D", False, id="0D_0D_10bit_base_8bit_precision"),

        pytest.param(Input["YUV"][10], Base[8]["2D"]["1D"], "2D",
                     "1D", False, id="2D_1D_10bit_base_8bit_precision"),
        pytest.param(Input["YUV"][10], Base[8]["2D"]["2D"], "2D",
                     "2D", False, id="2D_2D_10bit_base_8bit_precision"),
        pytest.param(Input["YUV"][10], Base[8]["1D"]["2D"], "1D",
                     "2D", False, id="1D_2D_10bit_base_8bit_precision"),
        pytest.param(Input["YUV"][10], Base[8]["1D"]["1D"], "1D",
                     "1D", False, id="1D_1D_10bit_base_8bit_precision"),
        pytest.param(Input["YUV"][10], Base[8]["0D"]["2D"], "0D",
                     "2D", False, id="0D_2D_10bit_base_8bit_precision"),
        pytest.param(Input["YUV"][10], Base[8]["0D"]["1D"], "0D",
                     "1D", False, id="0D_1D_10bit_base_8bit_precision"),

        pytest.param(Input["YUV"][10], Base[8]["2D"]["0D"],
                     "2D", "0D", True, id="2D_0D_10bit_base_8bit_speed"),
        pytest.param(Input["YUV"][10], Base[8]["1D"]["0D"],
                     "1D", "0D", True, id="1D_0D_10bit_base_8bit_speed"),
        pytest.param(Input["YUV"][10], Base[8]["0D"]["0D"],
                     "0D", "0D", True, id="0D_0D_10bit_base_8bit_speed"),

        pytest.param(Input["YUV"][10], Base[8]["2D"]["1D"],
                     "2D", "1D", True, id="2D_1D_10bit_base_8bit_speed"),
        pytest.param(Input["YUV"][10], Base[8]["2D"]["2D"],
                     "2D", "2D", True, id="2D_2D_10bit_base_8bit_speed"),
        pytest.param(Input["YUV"][10], Base[8]["1D"]["2D"],
                     "1D", "2D", True, id="1D_2D_10bit_base_8bit_speed"),
        pytest.param(Input["YUV"][10], Base[8]["1D"]["1D"],
                     "1D", "1D", True, id="1D_1D_10bit_base_8bit_speed"),
        pytest.param(Input["YUV"][10], Base[8]["0D"]["2D"],
                     "0D", "2D", True, id="0D_2D_10bit_base_8bit_speed"),
        pytest.param(Input["YUV"][10], Base[8]["0D"]["1D"],
                     "0D", "1D", True, id="0D_1D_10bit_base_8bit_speed"),
    ]

class DIL:
    RGB = 0
    I420 = 1
    NV12 = 2
    Formats = [
        pytest.param(RGB, id="RGB"),
        pytest.param(I420, id="I420"),
        pytest.param(NV12, id="NV12"),
    ]

    # Makes a crude guess at the format
    def format_from_filename(filename):
        if "rgb" in filename or "RGB" in filename:
            return DIL.RGB
        if "nv12" in filename or "NV12" in filename:
            return DIL.NV12
        return DIL.I420


    BIT_DEPTH_16 = 0
    BIT_DEPTH_8 = 1
    SurfaceBitdepths = [
        pytest.param(BIT_DEPTH_16, id="16bit"),
        pytest.param(BIT_DEPTH_8, id="8bit"),
    ]

    # TODO: It would probably be a good thing to decouple the specification
    #       for the 2 instances of the DIL that have to be compared
    #       and have them as 2 different parameters, to be more future proof.
    #       This means that also the lcevc_dil_harness has to be
    #       modified to take 2 separate parameters for the 2 instance
    #       of the DIL rather than a single one that includes both.
    #
    # TODO: Enable cpu_precalc tests when VEN-1382 is fixed.
    CompareModes = [
        pytest.param(1, id="cpu_vs_gpu"),
        # pytest.param(2, id="cpu_precalc_vs_gpu"),
        pytest.param(3, id="cpu_precalc_vs_cpu"),
    ]

    Upscaling = [
        pytest.param(0, id="Linear"),
        pytest.param(1, id="AdaptiveCubic"),
    ]

    ResidualType = (
        pytest.param({"loq_residual_image_type": 0}, id="buffer-residuals"),
        pytest.param({"loq_residual_image_type": 1}, id="texture-residuals"),
        # pytest.param({"loq_residual_image_type": 2}, id="hardware-residuals"),
        pytest.param({"geometry_residuals": True}, id="geometry-residuals"),
    )


class ERP:
    Transforms = [
        "dd",
        "dds"
    ]

    ScalingModeLevel0 = [
        "2D", "1D", "0D"
    ]

    ScalingModeLevel1 = [
        "0D"
    ]

    RGBBitdepths = [
        8
    ]

    RGBFormats = [
        pytest.param("rgb", marks=pytest.mark.baseline),
        "bgr",
        "rgba",
        "bgra",
        "argb",
        "abgr",
    ]

    PipelineFormats = [
        "yuv420p"
    ]

    Downsamplers = [
        "area",
        "lanczos",
        "lanczos2",
        "lanczos3",
        "larea3"
    ]

    Upsamplers = [
        "nearest",
        "linear",
        "cubic",
        "modifiedcubic"
    ]

    CustomKernels = [
        "1382 14285 3942 461",
        "2360 15855 4165 1276"
    ]

    UpsampleCubicKernels = {
        pytest.param("cubic", "1382 14285 3942 461", id="cubic"),
        pytest.param(
            "modifiedcubic", "2360 15855 4165 1276", id="modifiedcubic")
    }

    # Param 1: Temporal Enabled,
    # Param 2: Reduced Signalling
    # Param 3: Feauture Extraction Enabled
    # Param 4: Priority Map Enabled
    # Both 3 and 4 must be on to enable the priority map
    Temporal = [
        pytest.param(True, True, False, False,
                     id="tiled_temporal_priority_off"),
        pytest.param(True, False, False, False,
                     id="temporal_on_priority_off"),
        pytest.param(False, False, False, False,
                     id="temporal_off_priority_off"),
        pytest.param(True, False, True, True, id="temporal_on_priority_on"),
        pytest.param(True, True, True, True,
                     id="tiled_temporal_priority_on"),
    ]

    # Params that effect the decoder's temporal
    TemporalDecode = [
        pytest.param(False, False, id="temporal_off"),
        pytest.param(True, False, id="full_signalling"),
        pytest.param(True, True, id="reduced_signalling")
    ]

    BaseDepthShiftDithering = [
        pytest.param(True, id="depth_shift_dithered"),
        pytest.param(False, id="depth_shift_not_dithered")
    ]

    Level1DepthFlag = [
        pytest.param(True, id="loq1_enh_depth"),
        pytest.param(False, id="loq1_base_depth")
    ]

    TemporalReduced = [
        pytest.param(True, id="reduced_signalling"),
        pytest.param(False, id="full_signalling")
    ]

    TemporalSinglePass = [
        pytest.param(True, False, False, id="tiled_temporal_priority_off"),
        pytest.param(False, False, False, id="temporal_on_priority_off"),
        pytest.param(False, True, True, id="temporal_on_priority_on"),
        pytest.param(True, True, True, id="tiled_temporal_priority_on"),
    ]

    TemporalSinglePassId = [
        pytest.param(0, id="with_rdo"),
        pytest.param(1, id="temporal_single_pass"),
    ]

    TemporalOnOff = [
        pytest.param(False, False, id="temporal_off"),
        pytest.param(True, True, id="reduced_signalling"),
    ]

    SSE = pytest.param(
        "sse", marks=[pytest.mark.arch("x86_64", "i386", "AMD64"), pytest.mark.suite("full", "merge_request")])
    NEON = pytest.param(
        "neon", marks=[pytest.mark.arch("arm", "aarch64", "arm64"), pytest.mark.suite("full", "merge_request")])

    Simd = [
        pytest.param("none", id="simd_off", marks=[
                     pytest.mark.baseline, pytest.mark.suite("full")]),
        SSE,
        NEON,
    ]

    SimdOnly = [
        SSE,
        NEON,
    ]

    EncoderPipeline = [
        "cpu",
        "vulkan",
    ]

    DeadzoneOffset = [
        pytest.param(True, id="deadzone_offset_on"),
        pytest.param(False, id="deadzone_offset_off")
    ]

    TemporalSWModifier = [
        pytest.param(0, id="temporal_sw_modifier_off"),
        pytest.param(20, id="temporal_sw_modifier_on_20")
    ]

    DequantOffset = [
        pytest.param(-1, id="dequant_offset_not_signalled"),
        pytest.param(0, id="dequant_offset_0"),
        pytest.param(64, id="dequant_offset_64"),
        pytest.param(127, id="dequant_offset_127")
    ]

    DequantOffsetMode = [
        pytest.param(-1, id="dequant_offset_mode_minus_1"),
        pytest.param(0, id="dequant_offset_mode_0"),
        pytest.param(1, id="dequant_offset_mode_1")
    ]

    PredictedAverage = [
        pytest.param(True, id="predicted_average_on"),
        pytest.param(False, id="predicted_average_off")
    ]

    StepWithQuant = [
        pytest.param("none", id="sw_dep_quant_off"),
        pytest.param("toploqonly", id="sw_dep_quant_top_loq"),
        pytest.param("both", id="sw_dep_quant_both_loqs"),
    ]

    # Param 1: Deblock_loq1_c
    # Param 2: Deblock_loq1_s
    Deblock = [
        pytest.param(0, 0, id="deblock_off"),
        pytest.param(2, 5, id="deblock_on"),
    ]

    Preallocate = [
        pytest.param(True, id="preallocate", marks=pytest.mark.baseline),
        pytest.param(False, id="no_preallocation"),
    ]

    LoqEncodeScalingModeParameters = [
        pytest.param("2D", id="scaling_mode_level0_2D"),
        pytest.param("1D", id="scaling_mode_level0_1D")
    ]

    LoqEncodeForce8BitPipeline = [
        pytest.param(True, id="encoding_upsample_decoder_compatible_8b_on"),
        pytest.param(False, id="encoding_upsample_decoder_compatible_8b_off")
    ]

    ExternalInput = [
        pytest.param(True, id="external_input"),
        pytest.param(False, id="internal_input", marks=pytest.mark.baseline),
    ]

    # If the fixture name is a key in this dictionary that fixture's
    # value won't be used when creating the hash ids of tests that
    # use this fixture. This is generally used for fixtures who should
    # not modify the output of the encoder in any way and so all
    # values should produce the same the hashes.
    #
    # If a test uses one of these fixutres but does not specify parameters
    # for it the dictioanry value will be used to parameterize that fixture.
    #
    # When a test's data is being regened the value marked with "baseline"
    # in the key's list will be used, so that data for a test, that should
    # produce the same output, is not generated multiple
    # times.
    NonModifyingParmeters = {
        "simd": Simd,
        "rgb_format": RGBFormats,
        "preallocate": Preallocate,
        "external_input": ExternalInput,
        "enc_pipeline": EncoderPipeline
    }

    StepWidthLOQ1 = [
        pytest.param(4, id="rc_pcrf_sw_loq1=4"),
        pytest.param(32767, id="rc_pcrf_sw_loq1=32767")
    ]

    PlaneModes = [
        "y",
        "yuv"
    ]

    ChromaStepwidthMultipliers = [
        32,
        64,
        128,
        200
    ]
    SFilterModes = [
        'disabled',
        pytest.param('in_loop', marks=pytest.mark.skip('not implemented')),
        'out_of_loop',
    ]

    SFilterStrengths = [
        0.1,
        0.25,
        0.32
    ]

    MFilterModes = [
        'replace',
        'separate'
    ]

    MFilterStrengths = [
        0.0,
        0.3,
        0.5,
        0.7,
    ]

    MFilterLFStrengths = [
        0.0,
        3.0,
        5.0,
        7.0,
    ]

    TilingModes = [
        pytest.param(512, 256, id="TilingMode512x256"),
        pytest.param(1024, 512, id="TilingMode1024x512"),
        pytest.param(256, 256, id="TilingModeCustom256x256"),
    ]

    CompressionEntropyEnabledPerTile = [
        pytest.param(True, id="RLE"),
        pytest.param(False, id="none"),
    ]

    CompressionTypeSizePerTile = [
        'none',
        'prefix',
        'prefix_diff'
    ]

    ConformanceWindows = [
        pytest.param("10 0 0 0", id="left"),
        pytest.param("0 10 0 0", id="right"),
        pytest.param("0 0 10 0", id="top"),
        pytest.param("0 0 0 10", id="bottom"),
        pytest.param("50 12 30 21", id="all"),
    ]

    VulkanBase = [
        pytest.param("baseyuv_h264", id="baseyuv"),
        pytest.param("nvenc_h264", id="nvenc_h264"),
        pytest.param("nvenc_hevc", id="nvenc_hevc"),
    ]

    # temporal_enabled, temporal_single_pass_id, temporal_use_priority_map, residual_mode_priority_enabled
    VulkanTemporal = [
        pytest.param(True, 1, False, False, id="temporal_single_pass"),
        pytest.param(False, 1, False, False, id="temporal_off"),
    ]

    # gpu_parallel_transfer
    VulkanParallelTransfer = [
        pytest.param(True, id="parallel_transfer_on"),
        pytest.param(False, id="parallel_transfer_off")
    ]

    # upsampler, upsampler_kernel_coefficients
    VulkanUpsamplers = [
        pytest.param("nearest", ""),
        pytest.param("linear", ""),
        pytest.param("cubic", ""),
        pytest.param("modifiedcubic", ""),
        pytest.param("adaptivecubic", "1382 14285 3942 461"),
        pytest.param("adaptivecubic", "2360 15855 4165 1276")
    ]


class DecHarness:
    PipelineModes = {
        "precision": 1,
        "speed": 0,
    }

    SimdOff = pytest.param(True, id="simd_off",
                           marks=pytest.mark.suite("full"))

    Simd = [
        SimdOff,
        pytest.param(False, id="simd_on",
                     marks=pytest.mark.suite("full", "merge_request")),
    ]


class FFmpeg:
    TargetResolutions = [
        "640x360",
        "768x432",
        "864x480",
        "960x540",
        "960x544",
        "1280x720",
        "1920x1080",
        "3840x2160"
    ]

    def IsResolutionSlow(resolution, codec):
        width = int(resolution.split('x')[0])
        height = int(resolution.split('x')[1])
        max_side = max(width, height)

        if codec == 'lcevc_hevc' and max_side > 800:
            return True

        return False

    def GetTargetResolutions(codec):
        return list(
            map(lambda r: r if not FFmpeg.IsResolutionSlow(r, codec)
                else pytest.param(r, marks=pytest.mark.slow),
                FFmpeg.TargetResolutions))

    BitDepths = [
        pytest.param(8, id="8bit"),
        pytest.param(10, id="10bit")
    ]

    MainCodecs = [
        'lcevc_h264',
        'lcevc_hevc'
    ]

    AllCodecs = [
        'lcevc_h264',
        'lcevc_hevc',
        'lcevc_vp8',
        'lcevc_vp9',
        'lcevc_aom_av1',
        'lcevc_svt_av1',
        'lcevc_aom_av1_dav1d',
        'lcevc_svt_av1_dav1d',
    ]

    BaseCodecs = {
        'lcevc_h264': 'libx264',
        'lcevc_hevc': 'libx265',
        'lcevc_vp8': 'vpx_vp8',
        'lcevc_vp9': 'vpx_vp9',
        'lcevc_aom_av1': 'aom',
        'lcevc_svt_av1': 'aom',
        'lcevc_aom_av1_dav1d': 'aom',
        'lcevc_svt_av1_dav1d': 'aom',
    }

    Args = {
        'lcevc_h264': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_h264', '-base_encoder', 'x264'],
                'eil_params': [
                    'deterministic=1',
                ]
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_h264']
            }
        },

        'lcevc_hevc': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_hevc', '-base_encoder', 'x265'],
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_hevc']
            }
        },

        'lcevc_vp8': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_vp8', '-base_encoder', 'vpx_vp8'],
                'eil_params': [
                    'lcevc_tune=disabled',
                    'lcevc_preset=-1',
                ]
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_vp8']
            }
        },

        'lcevc_vp9': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_vp9', '-base_encoder', 'vpx_vp9'],
                'eil_params': [
                    'lcevc_tune=disabled',
                    'lcevc_preset=-1',
                ]
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_vp9']
            }
        },

        'lcevc_aom_av1': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_av1', '-base_encoder', 'aom'],
                'eil_params': [
                    'lcevc_tune=disabled',
                    'lcevc_preset=-1',
                ]
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_av1']
            }
        },

        'lcevc_svt_av1': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_av1', '-base_encoder', 'svt_av1'],
                'eil_params': [
                    'lcevc_tune=disabled',
                    'lcevc_preset=-1',
                ]
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_av1']
            }
        },

        'lcevc_aom_av1_dav1d': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_av1', '-base_encoder', 'aom'],
                'eil_params': [
                    'lcevc_tune=disabled',
                    'lcevc_preset=-1',
                ]
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_av1', '-base_decoder', 'libdav1d']
            }
        },

        'lcevc_svt_av1_dav1d': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'lcevc_av1', '-base_encoder', 'svt_av1'],
                'eil_params': [
                    'lcevc_tune=disabled',
                    'lcevc_preset=-1',
                ]
            },
            'dec': {
                'input_format_args': ['-vcodec', 'lcevc_av1', '-base_decoder', 'libdav1d']
            }
        },

        'libx264': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'libx264']
            }
        },

        'libx265': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'libx265']
            }
        },

        'vpx_vp8': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'libvpx_vp8']
            }
        },

        'vpx_vp9': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'libvpx_vp9']
            }
        },

        'aom': {
            'enc': {
                'ffmpeg_args': ['-c:v', 'libaom_av1']
            }
        },

        # Codec args below will only work with FFmpegRunnerV2
        'nvenc_h264': {
            'enc': {
                'output_args': {'c:v': 'h264_nvenc', 'preset': 'llhp', 'coder': 'cavlc'}
            }
        },

        'nvenc_hevc': {
            'enc': {
                'output_args': {'c:v': 'hevc_nvenc', 'preset': 'llhp', 'coder': 'cavlc'}
            }
        },

        'lcevc_h264_nvenc': {
            'enc': {
                'output_args': {
                    'c:v': 'lcevc_h264',
                    'base_encoder': 'nvenc_h264'
                },
            },
            'dec': {
                'input_args': {'vcodec': 'lcevc_h264'}
            }
        },

        'lcevc_hevc_nvenc': {
            'enc': {
                'output_args': {
                    'c:v': 'lcevc_hevc',
                    'base_encoder': 'nvenc_hevc'
                },
            },
            'dec': {
                'input_args': {'vcodec': 'lcevc_hevc'}
            }
        },

        'lcevc_h264_vulkan_nvenc': {
            'enc': {
                'output_args': {
                    'c:v': 'lcevc_h264_vulkan',
                    'base_encoder': 'nvenc_h264'
                },
            },
            'dec': {
                'input_args': {'vcodec': 'lcevc_h264'}
            }
        },

        'lcevc_hevc_vulkan_nvenc': {
            'enc': {
                'output_args': {
                    'c:v': 'lcevc_hevc_vulkan',
                    'base_encoder': 'nvenc_hevc'
                },
            },
            'dec': {
                'input_args': {'vcodec': 'lcevc_hevc'}
            }
        },
    }

    GOPSizes = [
        None,  # default behaviour, when -g is not specified
        -1,    # infinite GOP
        0,     # intra only, only I-frames
        300,   # some GOP size
    ]

    # This is a list of all resolutions which are listed in the standard
    FullResolutionSet = [
        pytest.param((360, 200), id="360x200"),
        pytest.param((400, 240), id="400x240"),
        pytest.param((480, 320), id="480x320"),
        pytest.param((640, 360), id="640x360"),
        pytest.param((640, 480), id="640x480"),
        pytest.param((768, 480), id="768x480"),
        pytest.param((800, 600), id="800x600"),
        pytest.param((852, 480), id="852x480"),
        pytest.param((854, 480), id="854x480"),
        pytest.param((856, 480), id="856x480"),
        pytest.param((960, 540), id="960x540"),
        pytest.param((1024, 576), id="1024x576"),
        pytest.param((1024, 600), id="1024x600"),
        pytest.param((1024, 768), id="1024x768"),
        pytest.param((1152, 864), id="1152x864"),
        pytest.param((1280, 720), id="1280x720"),
        pytest.param((1280, 800), id="1280x800"),
        pytest.param((1280, 1024), id="1280x1024"),
        pytest.param((1360, 768), id="1360x768"),
        pytest.param((1400, 1050), id="1400x1050"),
        pytest.param((1440, 900), id="1440x900"),
        pytest.param((1600, 1200), id="1600x1200"),
        pytest.param((1680, 1050), id="1680x1050"),
        pytest.param((1920, 1080), id="1920x1080"),
        pytest.param((1920, 1200), id="1920x1200"),
        pytest.param((2048, 1080), id="2048x1080"),
        pytest.param((2048, 1152), id="2048x1152"),
        pytest.param((2048, 1536), id="2048x1536"),
        pytest.param((2160, 1440), id="2160x1440"),
        pytest.param((2560, 1440), id="2560x1440"),
        pytest.param((2560, 1600), id="2560x1600"),
        pytest.param((2560, 2048), id="2560x2048"),
        pytest.param((3200, 1800), id="3200x1800"),
        pytest.param((3200, 2048), id="3200x2048"),
        pytest.param((3200, 2400), id="3200x2400"),
        pytest.param((3440, 1440), id="3440x1440"),
        pytest.param((3840, 1600), id="3840x1600"),
        pytest.param((3840, 2160), id="3840x2160"),
        pytest.param((3840, 2400), id="3840x2400"),
        pytest.param((4096, 2160), id="4096x2160"),
        pytest.param((4096, 3072), id="4096x3072"),
        pytest.param((5120, 3200), id="5120x3200"),
        pytest.param((5120, 4096), id="5120x4096"),
        pytest.param((6400, 4096), id="6400x4096"),
        pytest.param((6400, 4800), id="6400x4800"),
        pytest.param((7680, 4320), id="7680x4320"),
        pytest.param((7680, 4800), id="7680x4800")
    ]

    # 0D not supported with enhancement
    SupportedScalingModeLevel0 = [
        "2D",
        "1D",
        "0D"
    ]

    GPULCEVCTemporal = [
        pytest.param(EILPytestParam('temporal', 'off', eil_params={'temporal_enabled': 0, 'temporal_single_pass_id': 0,
                                                                   'temporal_use_refresh': 0}), id='temporal_off'),
        pytest.param(EILPytestParam('temporal', 'on', eil_params={'temporal_enabled': 1, 'temporal_single_pass_id': 1,
                                                                  'temporal_use_refresh': 1}), id='temporal_on'),
    ]

    GPULCEVCMFilter = [
        pytest.param(EILPytestParam('mfilter', 'off', eil_params={'m_ad_mode': 'disabled', 'm_lf_strength': 0.0,
                                                                  'm_hf_strength': 0.0}), id='mfilter_off'),
        pytest.param(EILPytestParam('mfilter', 'on', eil_params={'m_ad_mode': 'replace', 'm_lf_strength': 3.0,
                                                                 'm_hf_strength': 0.3}), id='mfilter_on'),
    ]

    LCEVCFrameTrace = EILPytestParam('', '', eil_params={
                                     'debug_enable_frame_trace': 1})

    # whether or not single GPU upload is performed and shared across all encoding sessions
    InputType = [
        pytest.param('compressed'),
        pytest.param('raw'),
    ]


class Plugins:
    PluginData = namedtuple(
        'PluginData', ['name', 'hardware_mark', 'base_type'])

    # Tuple of (plugin_name, hardware_requirements)
    _AllParamsData = (
        PluginData('aom', None, 'av1'),
        PluginData('harmonic_h264', None, 'h264'),
        PluginData('mainconcept_h264', None, 'h264'),
        PluginData('mainconcept_hevc', None, 'hevc'),
        PluginData('ngcodec_hdk_hevc', 'ngcodec_hdk', 'hevc'),
        PluginData('ngcodec_sdx_hevc', 'ngcodec_sdx', 'hevc'),
        PluginData('openh264', None, 'h264'),
        PluginData('svt_av1', None, 'av1'),
        PluginData('vc6', 'vc6', 'vc6'),
        PluginData('vega_bqb_h264', 'vega_bqb', 'h264'),
        PluginData('vega_bqb_hevc', 'vega_bqb', 'hevc'),
        PluginData('vpx_vp8', None, 'vp8'),
        PluginData('vpx_vp9', None, 'vp9'),
        PluginData('x264', None, 'h264'),
        PluginData('x265', None, 'hevc'),
        PluginData('mediacodec_h264', 'mediacodec', 'h264'),
        PluginData('mediacodec_hevc', 'mediacodec', 'hevc'),
        PluginData('nvenc_h264', 'nvenc', 'h264'),
        PluginData('nvenc_hevc', 'nvenc', 'hevc'),
        PluginData('qsv_h264', 'qsv', 'h264'),
        PluginData('qsv_hevc', 'qsv', 'hevc'),
        PluginData('jetson_h264', 'jetson', 'h264'),
        PluginData('jetson_hevc', 'jetson', 'hevc'),
        PluginData('netint_h264', 'netint', 'h264'),
        PluginData('netint_hevc', 'netint', 'hevc'),
        PluginData('vcu_omx_h264', 'vcu_omx', 'h264'),
        PluginData('vcu_omx_hevc', 'vcu_omx', 'hevc'),
    )

    AllParams = tuple(pytest.param(data.name) for data in _AllParamsData)

    ByName = {data.name: data for data in _AllParamsData}

    # Codec types currently supported by utility::BaseDecoder
    BaseTypesSupportedForDecode = ['h264', 'hevc', 'vvc']
