import json
import pytest
from util.constants import DIL, Sequence
from util.helpers import generate_sequences
from util.runner import OpenGLRunner, Runner, ADBRunner


@pytest.fixture
def erp(erp_path, tmpdir):
    runner = Runner(erp_path, tmpdir)

    # Settings that remain constant between all runs
    config = {
        "base_encoder": "baseyuv_h264",
        "n": 25,
        "dc_dithering_type": "none",
        "scaling_mode_level0": "2D",
        "scaling_mode_level1": "0D",
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
def dil_harness(dil_harness_path, adb_path, tmpdir):
    if adb_path:
        runner = ADBRunner(dil_harness_path, tmpdir, adb_path)
    else:
        runner = OpenGLRunner(dil_harness_path, tmpdir)
    config = {"hash-file": "hashes.json",
        "core_parallel_decode": True,
    }

    runner.update_config(config)

    return runner


@pytest.fixture(scope="function")
def hashes(test_data, regen_encode, regen_hash,  request):
    return test_data.get(request, regen_encode and regen_hash)

@pytest.fixture
def base(request, resource):
    return resource.get_path(request.param)


def get_interleaving_from_path(path):
    if path.endswith("_420p.yuv"):
        return "none"
    elif path.endswith("_nv12.yuv"):
        return "nv12"
    return "none"


def pytest_generate_tests(metafunc):
    input_names = ["yuv", "base"]
    inputs = [{8: Sequence.Input['YUV'][8]}]
    bases = [Sequence.Base]
    depths = [8, 10]
    with_scaling_modes = None

    bases.append(Sequence.BaseNV12)

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

    metafunc.parametrize(input_names, params, indirect=["yuv", "base"])


def static_vars(**kwargs):
    def decorate(func):
        for k in kwargs:
            setattr(func, k, kwargs[k])
        return func
    return decorate


@static_vars(regen_index=0)
def run_regen_encode(erp, record_property, data, hashes, yuv, base):
    perseus = f"perseus{run_regen_encode.regen_index}.bin"
    erp.set_param("input", yuv)
    erp.set_param("base_recon", base)
    erp.set_param("output", data.get_path(perseus, True))

    assert erp.get_executable(), "Must provide --erp-path when using --regen-encodes"
    record_property("enc_command", " ".join(erp.get_command_line()))
    record_property("enc_config", erp.get_config())
    process = erp.run()
    assert process.returncode == 0, f"ERP exited with non-zero return code {process.returncode}."
    run_regen_encode.regen_index += 1
    hashes["perseus"] = perseus


def run_test(dil_harness, record_property, data, hashes, base, regen_hash):
    dil_harness.set_param("base", base, path=True)
    dil_harness.set_param("perseus", data.get_path(hashes["perseus"]), path=True)
    dil_harness.set_param("interleaving", get_interleaving_from_path(str(base)))

    if dil_harness.get_param("interleaving") != "none":
        dil_harness.set_param("hash-deinterleave", True)

    record_property("harness_command", " ".join(dil_harness.get_command_line()))

    result = dil_harness.run()
    assert result.returncode == 0, f"DIL harness exited with non-zero return code {result.returncode}."

    hash_file = dil_harness.get_param("hash-file")

    json_path = dil_harness.get_working_directory().join(hash_file)
    with open(json_path) as f:
        generated = json.load(f)

    generated_hash = generated.get("Output")
    if regen_hash:
        hashes["hash"] = generated_hash
    correct_hash = hashes.get("hash")

    assert generated_hash, "DIL did not generate a hash"
    assert correct_hash, "No reference hash exists for comparison"

    assert generated_hash == correct_hash, f"DIL hash and reference lcevc_dec_assets hashes do not match"


@pytest.mark.mr
def test_dil_outputs_cpu(dil_harness, erp, record_property, hashes, data, regen_encode, regen_hash, yuv, base,
                         scaling_mode_level0, scaling_mode_level1):
    if '.rgb' in base:
        pytest.xfail('RGB input not supported in CPU mode')

    if regen_encode:
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    # To get better stats, you can tell the DIL what the input format is
    input_format = DIL.format_from_filename(base)
    dil_harness.set_param("input_format_hint", input_format)

    run_test(dil_harness, record_property, data, hashes, base, regen_hash)


@pytest.mark.mr
def test_dil_outputs_gpu(dil_harness, erp, record_property, hashes, data, regen_encode, regen_hash, yuv, base,
                         scaling_mode_level0, scaling_mode_level1):
    if 'I444' in base:
        pytest.xfail('I444 input not supported')

    if regen_encode:
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    # To get better stats, you can tell the DIL what the input format is
    input_format = DIL.format_from_filename(base)
    dil_harness.set_param("input_format_hint", input_format)

    dil_harness.set_param("use_gl_decode", 1)

    run_test(dil_harness, record_property, data, hashes, base, regen_hash)


@pytest.mark.mr
def test_dil_outputs_passthrough(dil_harness, erp, record_property, hashes, data, regen_encode, regen_hash, yuv, base,
                                 scaling_mode_level0, scaling_mode_level1):
    if regen_encode:
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    # To get better stats, you can tell the DIL what the input format is
    input_format = DIL.format_from_filename(base)
    dil_harness.set_param("input_format_hint", input_format)

    dil_harness.set_param("passthrough_mode", 1)

    run_test(dil_harness, record_property, data, hashes, base, regen_hash)