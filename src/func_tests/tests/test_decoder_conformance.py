# The JSON file is generated from RedPill
# this spreadsheet
# https://vnovaltd.sharepoint.com/:x:/s/RedPill/EarCEAvgjr1FpbsJoYGxhmYBVcmZK04speaS5BFdC23uqg
# details the different test's being run
#

import pytest
import os
import json
import numpy as np

from util.finders import DataFinder, ConformanceAssets
from util.runner import OpenGLRunner, Runner, ADBRunner
from util.constants import DIL
from util.helpers import generate_decodes

# some tests are expected to fail due to un-implemented features in the DIL
# SD  - Scaling tests: SD-1 = None/None and SD-2 = None/2D, works.1D/1D, 1D/2D, 2D/2D fail not supported, fails with 4294967293
# CST - Chroma Sampling Type: CST-2 = 4:2:0 works. CST-1 = Monochrome also works.
EXPECTED_FAILURES = (
    "Cactus_SD-3_v-nova_v03",
    "Cactus_SD-4_v-nova_v03",
    "Cactus_SD-5_v-nova_v03",
    "Cactus_SD-6_v-nova_v01",
    "CanYouReadThis_SD-3_v-nova_v03",
    "CanYouReadThis_SD-4_v-nova_v03",
    "CanYouReadThis_SD-5_v-nova_v03",
    "CanYouReadThis_SD-6_v-nova_v01",
    "ParkRunning3_10bit_SD-3_v-nova_v03",
    "ParkRunning3_10bit_SD-4_v-nova_v03",
    "ParkRunning3_10bit_SD-5_v-nova_v03",
    "ParkRunning3_10bit_SD-6_v-nova_v01",
    "ParkRunning3_12bit_SD-3_v-nova_v03",
    "ParkRunning3_12bit_SD-4_v-nova_v03",
    "ParkRunning3_12bit_SD-5_v-nova_v03",
    "ParkRunning3_12bit_SD-6_v-nova_v01",
    "ParkRunning3_14bit_SD-3_v-nova_v03",
    "ParkRunning3_14bit_SD-4_v-nova_v03",
    "ParkRunning3_14bit_SD-5_v-nova_v03",
    "ParkRunning3_14bit_SD-6_v-nova_v01"
)
GPU_PIXEL_BOUNDS = 1  # Pixel value difference (+/-)
GPU_PERCENT_TOLERANCE = 95
EXPECTED_GPU_TOLERANCE_FAILURES = (
    "Cactus_L1D-1_v-nova_v03",
    "Cactus_L1D-2_v-nova_v03",
    "Cactus_CONF-3_v-nova_v03",
    "Cactus_QUANT-3-1_v-nova_v05",
    "Cactus_QUANT-6-4_v-nova_v02",
    "Cactus_QUANT-5-1_v-nova_v03",
    "CanYouReadThis_CONF-3_v-nova_v03",
    "Cactus_UPS-5_v-nova_v03"
)


def pytest_generate_tests(metafunc):
    data = DataFinder(metafunc)
    config_path = data.get_path("conformance_data.json")
    config_json = json.loads(config_path.read())

    params = []
    for bitstream_prefix in config_json:
        hash = config_json[bitstream_prefix]["hash"]
        hash_high = config_json[bitstream_prefix]["hash_high"]
        base = config_json[bitstream_prefix]["base"]

        params.append(pytest.param(bitstream_prefix,
                      hash, hash_high, base, id=bitstream_prefix))

    metafunc.parametrize(["bitstream_prefix", "ltm_hash", "ltm_hash_high",
                          "base_yuv_filename"], params)


@pytest.fixture
def dec_harness(dec_harness_path, tmpdir):
    runner = Runner(dec_harness_path, tmpdir)

    return runner


@pytest.fixture
def dil_harness(dil_harness_path, adb_path, tmpdir):
    if adb_path:
        runner = ADBRunner(dil_harness_path, tmpdir, adb_path)
    else:
        runner = OpenGLRunner(dil_harness_path, tmpdir)
    return runner


@pytest.fixture
def ltm_decoder(ltm_decoder_path, tmpdir):
    assert ltm_decoder_path, "Must specify --ltm-decoder-path to run GPU conformance"
    runner = Runner(ltm_decoder_path, tmpdir)
    return runner


@pytest.mark.mr
def test_core_conformance(dec_harness, bitstream_prefix, ltm_hash, ltm_hash_high, base_yuv_filename, resource, record_property, tmpdir):

    lcevc_bin_filename = f'{bitstream_prefix}.bin'
    lcevc_bin = resource.get_path(lcevc_bin_filename)
    base_yuv = resource.get_path(base_yuv_filename)
    hash_path = tmpdir.join('hashes.json')

    dec_harness.set_param('perseus', lcevc_bin)
    dec_harness.set_param('base', base_yuv)
    # NOTE: it's important to ensure the hasher type used for this test matches
    # the one used by the DIL test.
    dec_harness.set_param('hash-file', hash_path)
    dec_harness.set_param('format-filenames', False)
    dec_harness.set_param('pipeline-mode', 1)

    for parallel_decode, decode_name in generate_decodes():
        dec_harness.set_param("parallel-decode", parallel_decode)
        record_property(f'dec_harness_command_{decode_name}', ' '.join(dec_harness.get_command_line()))
        process = dec_harness.run()
        assert process.returncode == 0, f'Harness exited with non-zero return code {process.returncode} for decode run {decode_name}.'

        try:
            with open(hash_path) as f:
                hashes = json.load(f)

                if 'conformance_window' in hashes:
                    harness_hash = hashes['conformance_window']
                else:
                    harness_hash = hashes['high']
        except Exception as err:
            pytest.fail(f'Error reading harness hash {err}')

        record_property(f'harness_hash_{decode_name}: ', harness_hash)
        record_property(f'ltm_hash_{decode_name}: ', ltm_hash)

        # Clean up .yuvs from tmpdir as they use a lot of disk space
        for filename in os.listdir(tmpdir):
            if filename.endswith('.yuv'):
                if os.path.exists(tmpdir.join(filename)):
                    os.remove(tmpdir.join(filename))

        assert harness_hash == ltm_hash, f"mismatching hashes for {decode_name} decode. dec_harness: {harness_hash} ltm: {ltm_hash}"


def run_dil_test(dil_harness, bitstream_prefix, ltm_hash, ltm_hash_high, base_yuv_filename, resource, record_property, tmpdir, precalc, cpu_gpu):
    if bitstream_prefix in EXPECTED_FAILURES:
        pytest.xfail(f"Test {bitstream_prefix}, is not expected to work yet")

    lcevc_bin_filename = f'{bitstream_prefix}.bin'
    lcevc_bin = resource.get_path(lcevc_bin_filename)
    base_yuv = resource.get_path(base_yuv_filename)
    hash_path = 'hashes.json'

    dil_harness, process = decode_dil_harness(dil_harness, lcevc_bin, base_yuv, record_property, cpu_gpu, precalc, hash_path)

    assert process.returncode == 0, f'Test {bitstream_prefix} failed, harness exited with non-zero return code {process.returncode}'

    try:
        with open(tmpdir.join(hash_path)) as f:
            hashes = json.load(f)
            harness_hash = hashes['Output']
    except Exception as err:
        pytest.fail(f'Error reading harness hash {err}')

    record_property('harness_hash: ', harness_hash)
    record_property('ltm_hash: ', ltm_hash)

    assert harness_hash == ltm_hash, f"Test {bitstream_prefix} failed, mismatching hashes. dil_harness: {harness_hash} ltm: {ltm_hash} base: {base_yuv_filename}"


def decode_dil_harness(dil_harness, lcevc_bin, base_yuv, record_property, gpu=0, precalc=0, hash_path=None, output_path=None):
    dil_harness.set_param("perseus", lcevc_bin, path=True)
    dil_harness.set_param("base", base_yuv, path=True)
    dil_harness.set_param("use_gl_decode", gpu)
    dil_harness.set_param("can_precalculate", precalc)
    dil_harness.set_param("use_u8_surfaces", 0)
    if hash_path:
        dil_harness.set_param("hash-file", hash_path)
        dil_harness.set_param("hasher-type", "XXH3")
    if output_path:
        dil_harness.set_param("output", output_path)
    dil_harness.set_param('dpi_pipeline_mode', 1)
    dil_harness.set_param('sfilter_mode', 0)

    record_property("dil_harness_command", " ".join(dil_harness.get_command_line()))
    process = dil_harness.run()
    return dil_harness, process


def decode_ltm(ltm_decoder, lcevc_bin, config, output_path):
    ltm_decoder.set_param('input_file', lcevc_bin)
    ltm_decoder.set_param('base_encoder', config['base_encoder'])
    if not config['format'].startswith("yuv") or config['format'].endswith("12") or config['format'].endswith("14"):
        ltm_decoder.set_param('base_external', 'true')
    ltm_decoder.set_param('encapsulation', 'nal')
    ltm_decoder.set_param('output_file', output_path)

    process = ltm_decoder.run()
    return ltm_decoder, process


def pixel_deviation_histogram(bits, reference_file, sample_file):
    np_int_type = np.int8 if bits <= 8 else np.int16
    bin_range = range(int((-2 ** bits) / 2), int(((2 ** bits) / 2) - 1))

    assert os.path.getsize(reference_file) == os.path.getsize(sample_file), f"YUVs are different sizes - cannot compare"
    reference = np.fromfile(reference_file, dtype=np_int_type)
    sample = np.fromfile(sample_file, dtype=np_int_type)

    histo = np.histogram(reference-sample, bins=bin_range, density=False)

    # Remove empty head and tail of histogram
    zh = list(zip(histo[1], histo[0]))
    while zh and zh[0][1] == 0:
        zh = zh[1:]
    while zh and zh[-1][1] == 0:
        zh.pop()

    total = sum(histo[0])
    ret = dict()
    for level, value in zh:
        percentage = value * 100.0 / total
        ret[level] = percentage
    return ret


@pytest.mark.mr
def test_integration_conformance_cpu(dil_harness, bitstream_prefix, ltm_hash, ltm_hash_high, base_yuv_filename,
                                     resource, record_property, tmpdir):
    run_dil_test(dil_harness, bitstream_prefix, ltm_hash, ltm_hash_high,
                 base_yuv_filename, resource, record_property, tmpdir, 0, 0)


@pytest.mark.nightly
def test_integration_conformance_cpu_precalc(dil_harness, bitstream_prefix, ltm_hash, ltm_hash_high, base_yuv_filename,
                                             resource, record_property, tmpdir):
    pytest.xfail("Precalc doesn't match DIL CPU hash. Until it does we can't use this")
    run_dil_test(dil_harness, bitstream_prefix, ltm_hash, ltm_hash_high,
                 base_yuv_filename, resource, record_property, tmpdir, 1, 0)


@pytest.mark.nightly
@pytest.mark.parametrize("residual_type", DIL.ResidualType)
def test_integration_gpu_tolerance(dil_harness, bitstream_prefix, ltm_hash, ltm_hash_high, base_yuv_filename,
                                   ltm_decoder, residual_type, resource, record_property, tmpdir, request):
    if bitstream_prefix in EXPECTED_FAILURES:
        pytest.xfail(f"Test {bitstream_prefix}, is not expected to work yet")
    if bitstream_prefix in EXPECTED_GPU_TOLERANCE_FAILURES:
        pytest.xfail(f"Test {bitstream_prefix}, is known to be out of {GPU_PERCENT_TOLERANCE}% histogram tolerance - VEN-2266")
    if "12bit" in bitstream_prefix or "14bit" in bitstream_prefix:
        pytest.xfail(f"Test {bitstream_prefix}, is 12bit or 14bit - VEN-2265")
    if "10bit" in bitstream_prefix or "RBD-3" in bitstream_prefix or "RBD-4" in bitstream_prefix:
        pytest.xfail(f"Test {bitstream_prefix}, is 10bit - VEN-1129/1454")
    if "RBD-1" in bitstream_prefix or "RBD-2" in bitstream_prefix:
        pytest.xfail(f"Test {bitstream_prefix}, is 10-bit enhancement on 8-bit base - VEN-1731")
    if "geometry_residuals" in residual_type:
        pytest.xfail("The majority of 'geometry_residuals: true' tests fail due to OCD-185")

    conformance_assets = ConformanceAssets(request, bitstream_prefix)
    bitstream_path = conformance_assets.get_bitstream()
    with open(conformance_assets.get_config(), 'r') as json_file:
        config = json.load(json_file)

    with open(tmpdir.join("lcevc_dil_config.json"), 'w') as dil_config_file:
        json.dump({"config": residual_type}, dil_config_file)

    lcevc_bin_filename = f'{bitstream_prefix}.bin'
    lcevc_bin = resource.get_path(lcevc_bin_filename)
    base_yuv = resource.get_path(base_yuv_filename)
    dil_output_path = f"dil-{bitstream_prefix}.yuv"
    dil_harness, dil_process = decode_dil_harness(dil_harness, lcevc_bin, base_yuv, record_property,
                                                  output_path=dil_output_path, gpu=1)
    assert dil_process.returncode == 0, \
        f'Test {bitstream_prefix} failed, harness exited with non-zero return code {dil_process.returncode}'

    ltm_output_path = tmpdir.join(f"ltm-{bitstream_prefix}.yuv")
    ltm_decoder, ltm_process = decode_ltm(ltm_decoder, bitstream_path, config, ltm_output_path)
    record_property('ltm_command: ', ' '.join(dil_harness.get_command_line()))
    assert ltm_process.returncode == 0, \
        f'Test {ltm_process} failed, LTM decoder exited with non-zero return code {ltm_process.returncode}'

    try:
        bit_depth = int(config['format'].split('p')[1])
    except Exception as e:
        raise ValueError(f"Cannot derive bit depth from LTM config format '{config.get('format')}': {e}")
    histogram = pixel_deviation_histogram(bit_depth, ltm_output_path, tmpdir.join(dil_output_path))
    total_in_bounds = sum(histogram.get(n, 0) for n in range(-GPU_PIXEL_BOUNDS, GPU_PIXEL_BOUNDS + 1))

    assert total_in_bounds > 0, f"DIL decode has no correct pixels"
    assert total_in_bounds > GPU_PERCENT_TOLERANCE, f"Less than {GPU_PERCENT_TOLERANCE}% of pixels are in bounds of " \
                                                    f"+/-{GPU_PIXEL_BOUNDS}% deviation - {round(total_in_bounds, 3)}%"
