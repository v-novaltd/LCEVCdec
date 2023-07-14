import json
import os
import platform
import pytest
from util.constants import DIL, Sequence
from util.runner import OpenGLRunner, ADBRunner


@pytest.fixture
def dil_sample(dil_sample_path, adb_path, tmpdir):
    if 'bin' in dil_sample_path:
        run_dir = dil_sample_path[:dil_sample_path.find("bin") + 3]
        if platform.system() != "Windows":
            os.environ["LD_LIBRARY_PATH"] = "../lib"
    else:
        raise RuntimeError("Cannot set library paths for DIL sample")
    if adb_path:  # Not supported while sample executable only has positional args
        runner = ADBRunner(dil_sample_path, tmpdir, adb_path)
    else:
        runner = OpenGLRunner(dil_sample_path, run_dir)
    runner.set_timeout(60)

    return runner


@pytest.fixture(scope="function")
def hashes(test_data, regen_encode, regen_hash, request):
    return test_data.get(request, regen_encode and regen_hash)

def run_test(dil_sample, sample_stream, tmp_path, hashes, regen_hash,
             record_property, resource, input_output_format, passthrough, gl_decode):
    input_file = resource.get_path(sample_stream)

    output_file = "dump." + ("rgb" if input_output_format == 0 else "yuv")
    output_file = os.path.join(str(tmp_path), output_file)
    config_file = os.path.join(str(tmp_path), "config.json")
    hash_file = os.path.join(str(tmp_path), "hash.json")

    config = {"passthrough_mode": passthrough,
              "gl_decode": gl_decode,
              "hashes_filename": hash_file,
              "dithering": False}
    with open(config_file, "w") as f:
        json.dump(config, f)

    dil_sample.args = [input_file, output_file, config_file]
    record_property("dil_sample_command", " ".join(dil_sample.get_command_line()))
    record_property("dil_sample_config", " ".join(config))

    result = dil_sample.run()
    if result.returncode != 0:
        pytest.fail(f"DIL sample exited with non-zero return code {result.returncode}")

    with open(hash_file) as f:
        generated = json.load(f)

    failures = []
    for surface in generated:
        surface_hash = generated[surface]
        if regen_hash:
            hashes[surface] = surface_hash
        elif surface not in hashes or surface_hash != hashes[surface]:
            failures.append(f"Generated hash '{surface_hash}' and lcevc_dec_assets hash '{hashes[surface]}' "
                            f"do not match, use --regen-hashes to update if a breaking change has been added")

    record_property("mismatching_surfaces", failures)

    assert not failures, f"{len(failures)} hashes do not match"


# NOTE: A quirk of the sample is that it forces the input type to match whatever output type you
# provide. As a result, you can't separately test input and output types with the sample code.
@pytest.mark.mr
@pytest.mark.parametrize("sample_stream", Sequence.Input_Sample)
@pytest.mark.parametrize("input_output_format", DIL.Formats)
def test_dil_sample_cpu(dil_sample, record_property, hashes, regen_hash, resource, tmp_path,
                        sample_stream, input_output_format):
    if input_output_format == DIL.RGB:
        pytest.xfail("Ignoring: can't do RGB inputs in CPU.")
    if input_output_format == DIL.NV12:
        pytest.xfail("Ignoring: the sample doesn't do NV12.")

    run_test(dil_sample, sample_stream, tmp_path, hashes, regen_hash,
             record_property, resource, input_output_format, 0, False)

@pytest.mark.mr
@pytest.mark.parametrize("sample_stream", Sequence.Input_Sample)
@pytest.mark.parametrize("input_output_format", DIL.Formats)
def test_dil_sample_passthrough(dil_sample, record_property, hashes, regen_hash, resource, tmp_path,
                                sample_stream, input_output_format):
    if input_output_format == DIL.RGB:
        pytest.xfail("Ignoring: can't do RGB inputs in CPU.")
    if input_output_format == DIL.NV12:
        pytest.xfail("Ignoring: the sample doesn't do NV12.")

    run_test(dil_sample, sample_stream, tmp_path, hashes, regen_hash,
             record_property, resource, input_output_format, 1, False)


# These are skipped because GPU decodes differ between GPUs/drivers. However, this isn't a problem
# for test_integration_regression, we just make sure to regenerate on the same system every time.
@pytest.mark.mr
@pytest.mark.skip
@pytest.mark.parametrize("sample_stream", Sequence.Input_Sample)
@pytest.mark.parametrize("input_output_format", DIL.Formats)
def test_dil_sample_gpu(dil_sample, record_property, hashes, regen_hash, resource, tmp_path,
                        sample_stream, input_output_format):
    if input_output_format == DIL.NV12:
        pytest.xfail("Ignoring: the sample doesn't do NV12.")

    run_test(dil_sample, sample_stream, tmp_path, hashes, regen_hash,
             record_property, resource, input_output_format, 0, True)
