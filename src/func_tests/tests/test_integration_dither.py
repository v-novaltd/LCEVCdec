import pytest
import os

from util.runner import OpenGLRunner, ADBRunner
from util.constants import Sequence, DIL


@pytest.fixture
def stream_file(stream, resource):
    return resource.get_path(stream)


@pytest.fixture
def harness(dil_harness_path, adb_path, tmpdir):
    if adb_path:
        runner = ADBRunner(dil_harness_path, tmpdir, adb_path)
    else:
        runner = OpenGLRunner(dil_harness_path, tmpdir)

    return runner


def run_test(harness, dither_stream, input_format, surface_bitdepth,
             record_property, resource, use_gl_decode):

    base_file_name = dither_stream + "_base.yuv"
    enhancement_file_name = dither_stream + "_enhancement.bin"
    directory = resource.get_path(base_file_name).replace(base_file_name, "")

    # Note that this is a harness test, so we don't have to specify a base decoder.

    base_file_path = os.path.join(str(directory), base_file_name)
    if not os.path.exists(base_file_path):
        pytest.fail(f'File {base_file_path} not found')

    enhancement_file_path = os.path.join(str(directory), enhancement_file_name)
    if not os.path.exists(enhancement_file_path):
        pytest.fail(f'File {enhancement_file_path} not found')

    harness.set_param("base", str(base_file_path), path=True)
    harness.set_param("perseus", str(enhancement_file_path), path=True)
    harness.set_param("compare_mode", 4)
    harness.set_param("use_u8_surfaces", surface_bitdepth)
    harness.set_param("use_gl_decode", use_gl_decode)
    harness.set_param("input_format_hint", input_format)
    harness.set_param("dump_output", 1)
    record_property("harness_command", " ".join(harness.get_command_line()))
    record_property("harness_config", " ".join(harness.get_config()))

    result = harness.run()

    assert result.returncode == 0, f"DIL harness exited with non-zero return code {result.returncode}"


@pytest.mark.mr
@pytest.mark.skip
@pytest.mark.parametrize("dither_stream", Sequence.Input_Dithering)
@pytest.mark.parametrize("surface_bitdepth", DIL.SurfaceBitdepths)
def test_dithering_cpu(harness, dither_stream, surface_bitdepth, record_property, resource):
    input_format = DIL.format_from_filename(dither_stream)
    if input_format == DIL.RGB:
        pytest.xfail("RGB input not supported in CPU mode")

    run_test(harness, dither_stream, input_format, surface_bitdepth,
             record_property, resource, 0)


@pytest.mark.mr
@pytest.mark.skip
@pytest.mark.parametrize("dither_stream", Sequence.Input_Dithering)
@pytest.mark.parametrize("surface_bitdepth", DIL.SurfaceBitdepths)
def test_dithering_gpu(harness, dither_stream, surface_bitdepth, record_property, resource):
    input_format = DIL.format_from_filename(dither_stream)
    run_test(harness, dither_stream, input_format, surface_bitdepth,
             record_property, resource, 1)
