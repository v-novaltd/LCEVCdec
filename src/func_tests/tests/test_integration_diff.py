import pytest
from util.constants import DIL, Sequence
from util.helpers import bitdepth_from_filename, generate_sequences
from util.runner import OpenGLRunner, Runner, ADBRunner


@pytest.fixture
def erp(erp_path, tmpdir):
    runner = Runner(erp_path, tmpdir)

    # Settings that remain constant between all runs
    config = {
        "base_encoder": "baseyuv_h264",
        "n": 10,
        "dc_dithering_type": "none",
        "g": 8,
        "m_ad_mode": "disabled",
        "lcevc_tune": "disabled",
        "lcevc_preset": -1,

        # Force a fixed step width so that rate control does not introduce a change
        "encoding_step_width_loq_0_min": 300,
        "encoding_step_width_loq_0_max": 300,
        "encoding_step_width_loq_1_min": 300,
        "encoding_step_width_loq_1_max": 300,
    }

    runner.update_config(config)

    return runner


@pytest.fixture
def comparer(dil_harness_path, adb_path, tmpdir):
    if adb_path:
        runner = ADBRunner(dil_harness_path, tmpdir, adb_path)
    else:
        runner = OpenGLRunner(dil_harness_path, tmpdir)
    runner.set_timeout(60)

    return runner


@pytest.fixture
def base(request, resource):
    return resource.get_path(request.param)


@pytest.fixture
def perseus_dict(test_data, request,regen_encode, regen_hash):
    return test_data.get(request, regen_encode and regen_hash, ["base", "disable_simd"])


def pytest_generate_tests(metafunc):
    # Generate YUV configurations (I420 and NV12):
    input_names = ["yuv", "base"]
    inputs = [{8: Sequence.Input['YUV'][8]}, {10: Sequence.Input['YUV'][10]}]
    bases = [Sequence.Base, Sequence.BaseNV12]
    depths = [8, 10]
    with_scaling_modes = None

    if "scaling_mode_level0" in metafunc.fixturenames:
        input_names += ["scaling_mode_level0", "scaling_mode_level1"]
        with_scaling_modes = "simple"

    params = []

    for name, depth, sequence in generate_sequences(inputs, bases, depths, with_scaling_modes):
        nv12 = "nv12" in sequence.base

        if nv12:
            name += "_nv12"
        else:
            name += "_420p"
        params.append(pytest.param(*sequence, id=name))

    # Generate RGB base inputs
    input_names = ["yuv", "base"]
    inputs = [{8: Sequence.Input['RGB'][8]['rgb']}]
    bases = [Sequence.BaseRGB]
    depths = [8]
    with_scaling_modes = None

    if "scaling_mode_level0" in metafunc.fixturenames:
        input_names += ["scaling_mode_level0", "scaling_mode_level1"]
        with_scaling_modes = "simple"

    for name, depth, sequence in generate_sequences(inputs, bases, depths, with_scaling_modes):
        name += "_rgb"
        params.append(pytest.param(*sequence, id=name))

    # Finally, "parametrize" using all the params we've gathered
    metafunc.parametrize(input_names, params, indirect=["yuv", "base"])


def static_vars(**kwargs):
    def decorate(func):
        for k in kwargs:
            setattr(func, k, kwargs[k])
        return func
    return decorate


@static_vars(regen_index=0)
def run_regen_encode(erp, record_property, data, perseus_dict, yuv, base):
    perseus = f"perseus{run_regen_encode.regen_index}.bin"
    erp.set_param("input", yuv)
    erp.set_param("base_recon", base)
    erp.set_param("output", data.get_path(perseus, True))

    assert erp.get_executable(), "Must provide --erp-path when using --regen-encodes"
    record_property("enc_command", " ".join(erp.get_command_line()))
    record_property("enc_config", erp.get_config())

    process = erp.run()
    assert process.returncode == 0, f"ERP exited with non-zero return code {process.returncode}"
    run_regen_encode.regen_index += 1
    perseus_dict["perseus"] = perseus


@pytest.mark.mr
@pytest.mark.skip  # AW: Lots of bugs here and this functionality is now covered by test_integration_gpu_tolerance,
# skipping as test is inherently flawed comparing the DPI against the DIL
@pytest.mark.parametrize("compare_mode", DIL.CompareModes)
@pytest.mark.parametrize("upscaling", DIL.Upscaling)
def test_difference(erp, regen_encode, comparer, record_property, yuv, base, data, perseus_dict,
                    scaling_mode_level0, scaling_mode_level1, compare_mode, upscaling):
    if '.rgb' in base:
        pytest.xfail('RGB input not supported in CPU mode')

    # 2D comparisons are not-quite-passing (at ALL bitrates). They only work with either linear
    # upsampling, or easy inputs. Easy inputes are identified by having "_contrastLimited" in their
    # file names, before the ".yuv".
    if scaling_mode_level0 == '2D' and compare_mode == 1:
        if upscaling == 0:
            pass
        else:
            base = base[:base.rfind(".")] + "_contrastLimited" + base[base.rfind("."):]
            yuv = yuv[:yuv.rfind(".")] + "_contrastLimited" + yuv[yuv.rfind("."):]
    elif upscaling == 0:
        pytest.skip(
            'Only CPU vs GPU tests of 2D 8bit input uses the "Linear" upsample option, because '
            'that is the only configuration which cannot meet diff requirements with ordinary '
            'input and ordinary (AdaptiveCubic) upsampling.')

    if int(bitdepth_from_filename(base)) > 8:
        # TODO (VEN-1762): For now, 10bit inputs have errors in the range of -4 to +2. It would be
        # nice to bring this into the +/-3 tolerance that 8bit inputs obey.
        comparer.set_param("diff_tolerance", 4)
    else:
        # Reset to defaults for 8bit.
        comparer.unset_param("diff_tolerance")

    if regen_encode:
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        run_regen_encode(erp, record_property, data, perseus_dict, yuv, base)

    comparer.set_param("base", base)
    comparer.set_param("perseus", data.get_path(perseus_dict["perseus"]))
    comparer.set_param("compare_mode", compare_mode)
    comparer.set_param("log_level", 1)

    record_property("comparer_command", " ".join(comparer.get_command_line()))
    record_property("comparer_config", comparer.get_config())

    result = comparer.run()
    assert result.returncode == 0, f"DIL harness exited with non-zero return code {result.returncode}"
