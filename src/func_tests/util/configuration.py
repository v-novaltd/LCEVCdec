import re
import pytest

from .base_type import BaseType


# Widely advertised operating points for Vanilla and LCEVC-enhanced codecs
# mapping resolution+framerate: [(codec, bitrate)]+
VanillaOperatingPoints = {
    '1080p30': [(BaseType.H264, '6M'), (BaseType.HEVC, '5M')],
    '1080p60': [(BaseType.H264, '9M'), (BaseType.HEVC, '7.5M')],
    '4k60': [(BaseType.H264, '25M'), (BaseType.HEVC, '20M')],
    '1920p72': [(BaseType.H264, '40M'), (BaseType.HEVC, '35M')],
}

LCEVCOperatingPoints = {
    '1080p30': [(BaseType.H264, '4M'), (BaseType.HEVC, '3.5M')],
    '1080p60': [(BaseType.H264, '6M'), (BaseType.HEVC, '4.5M')],
    '4k60': [(BaseType.H264, '14M'), (BaseType.HEVC, '13M')],
    '1920p72': [(BaseType.H264, '28M'), (BaseType.HEVC, '25M')],
}


OPERATING_POINT_REGEXP = r'^([0-9]+[kp])([0-9\.]+)at([0-9\.]+[MmKk])$'


class EncOperatingPoint(object):
    def __init__(self, resolution: str, framerate: float, bitrate: str):
        self.resolution = resolution
        self.framerate = framerate
        self.bitrate = bitrate

    def __str__(self) -> str:
        return f'{self.resolution}{self.framerate}at{self.bitrate}'

    @classmethod
    def from_string(cls, op_string: str):
        match = re.compile(OPERATING_POINT_REGEXP).match(op_string)
        if not match:
            raise RuntimeError(
                f'Unable to parse operating point from string: {op_string}')
        resolution = match.group(1)
        framerate = float(match.group(2))
        bitrate = match.group(3)
        return cls(resolution=resolution, framerate=framerate, bitrate=bitrate)


VanillaCodecs = {
    BaseType.H264: 'nvenc_h264',
    BaseType.HEVC: 'nvenc_hevc',
}
LCEVCVulkanCodecs = {
    BaseType.H264: 'lcevc_h264_vulkan_nvenc',
    BaseType.HEVC: 'lcevc_hevc_vulkan_nvenc',
}
LCEVCCPUCodecs = {
    BaseType.H264: 'lcevc_h264_nvenc',
    BaseType.HEVC: 'lcevc_hevc_nvenc',
}


class EncConfiguration:

    @staticmethod
    def guess_codec(base_type: int, is_lcevc: bool = False, is_vulkan: bool = False):
        if is_lcevc:
            return LCEVCVulkanCodecs[base_type] if is_vulkan else LCEVCCPUCodecs[base_type]
        else:
            return VanillaCodecs[base_type]

    @staticmethod
    def get_pytest_marks(base_type: int, is_lcevc: bool = False, is_vulkan: bool = False):
        regen_only_marks = [pytest.mark.suite("full")]
        mr_and_regen_marks = [pytest.mark.suite("full", "merge_request")]
        if is_lcevc:
            marks = regen_only_marks if base_type == BaseType.HEVC else mr_and_regen_marks
        else:
            marks = regen_only_marks
        return marks

    @staticmethod
    def generate(operating_points, targets, is_vanilla: bool = False, is_lcevc: bool = False, is_vulkan: bool = False):
        if not is_vanilla and not is_lcevc:
            raise RuntimeError(f'One of is_vanilla or is_lcevc has to be set')
        if is_vanilla and is_lcevc:
            raise RuntimeError(f'Both is_vanilla and is_lcevc cannot be set')

        guessed_codecs = {}
        pytest_marks = {}
        for base_type in [BaseType.H264, BaseType.HEVC]:
            guessed_codecs[base_type] = EncConfiguration.guess_codec(
                base_type, is_lcevc, is_vulkan)
            pytest_marks[base_type] = EncConfiguration.get_pytest_marks(
                base_type, is_lcevc, is_vulkan)

        encoder_confs = []
        for res_framerate in targets:
            if res_framerate not in operating_points:
                raise RuntimeError(
                    f'No encoder operating points defined for {res_framerate}')
            for base_type, bitrate in operating_points[res_framerate]:
                if base_type not in guessed_codecs:
                    raise RuntimeError(f'Unsupported base type: {base_type}')
                codec = guessed_codecs[base_type]
                if codec is None:
                    raise RuntimeError(
                        f'No guessed codec for base type: {base_type} is_lcevc={is_lcevc} is_vulkan={is_vulkan}')
                marks = pytest_marks[base_type]
                op_str = f'{res_framerate}at{bitrate}'
                encoder_confs.append(
                    pytest.param(codec, EncOperatingPoint.from_string(op_str), id=f'{codec}-{op_str}', marks=marks))

        return encoder_confs


# configurations tested for Vulkan encoder pipeline
class Vulkan:

    perf_targets = ['1080p30', '1080p60', '4k60', '1920p72']
    # skip 1920p for density tests
    density_targets = ['1080p30', '1080p60', '4k60']
    latency_targets = ['1920p72']

    VanillaPerformance = EncConfiguration.generate(
        VanillaOperatingPoints, perf_targets, is_vanilla=True)

    LCEVCPerformanceVulkan = EncConfiguration.generate(
        LCEVCOperatingPoints, perf_targets, is_lcevc=True, is_vulkan=True)
    LCEVCPerformanceCPU = EncConfiguration.generate(
        LCEVCOperatingPoints, perf_targets, is_lcevc=True, is_vulkan=False)
    LCEVCPerformance = LCEVCPerformanceVulkan + LCEVCPerformanceCPU

    VanillaUtilization = VanillaPerformance
    LCEVCUtilization = LCEVCPerformanceVulkan

    LCEVCSanity = LCEVCPerformance

    VanillaDensity = EncConfiguration.generate(VanillaOperatingPoints,
                                               density_targets, is_vanilla=True)
    LCEVCDensity = EncConfiguration.generate(LCEVCOperatingPoints,
                                             density_targets, is_lcevc=True, is_vulkan=True)

    LCEVCLatency = EncConfiguration.generate(LCEVCOperatingPoints,
                                             latency_targets, is_lcevc=True, is_vulkan=True) +\
        EncConfiguration.generate(LCEVCOperatingPoints, latency_targets,
                                  is_lcevc=True, is_vulkan=False)
