import json

import pytest
from util.constants import ERP, DecHarness, Sequence
from util.helpers import generate_sequences, generate_decodes
from util.runner import Runner


@pytest.fixture
def erp(erp_path, tmpdir):
    runner = Runner(erp_path, tmpdir)

    # Settings that remain constant between all runs
    config = {
        "base_encoder": "baseyuv_h264",
        "n": 10,
        "dc_dithering_type": "none",
        "scaling_mode_level0": "2D",
        "scaling_mode_level1": "0D",
        "g": 8,
        "m_ad_mode": "disabled",
        "lcevc_tune": "disabled",
        "lcevc_preset": -1,

        # Force a fixed step width so that rate control does not introduce change
        "encoding_step_width_loq_0_min": 300,
        "encoding_step_width_loq_0_max": 300,
        "encoding_step_width_loq_1_min": 300,
        "encoding_step_width_loq_1_max": 300,
    }

    runner.update_config(config)

    return runner


@pytest.fixture
def dec_harness(dec_harness_path, tmpdir):
    runner = Runner(dec_harness_path, tmpdir)
    config = {
        "hash-file": "hashes.json",
        "pipeline-mode": 0,
        "parallel-decode": 0,
    }

    runner.update_config(config)

    return runner


@pytest.fixture(scope="function")
def hashes(test_data, regen_encode, regen_hash, request):
    return test_data.get(request, regen_encode and regen_hash, ["base", "disable_simd"])


def get_interleaving_from_path(path):
    if path.endswith("_420p.yuv"):
        return "none"
    elif path.endswith("_nv12.yuv"):
        return "nv12"
    return "none"


def pytest_generate_tests(metafunc):
    regen_hashes = metafunc.config.getoption("--regen-hashes")

    # Why are we doing this?
    if "disable_simd" in metafunc.fixturenames:
        if regen_hashes:
            metafunc.parametrize("disable_simd", [DecHarness.SimdOff])
        else:
            metafunc.parametrize("disable_simd", DecHarness.Simd)

    input_names = ["yuv", "base"]
    inputs = [Sequence.Input['YUV']]
    bases = [Sequence.Base]
    depths = [8, 10]
    with_pipeline_modes = False
    with_scaling_modes = None
    ignore_nv12 = "ignore_nv12" in metafunc.fixturenames

    if not regen_hashes and not ignore_nv12:
        bases.append(Sequence.BaseNV12)

    if "scaling_mode_level0" in metafunc.fixturenames:
        input_names += ["scaling_mode_level0", "scaling_mode_level1"]
        with_scaling_modes = "simple"

    if "scaling_mode_complex" in metafunc.fixturenames:
        metafunc.parametrize("scaling_mode_complex", [True])
        with_scaling_modes = "complex"

    if "pipeline_mode" in metafunc.fixturenames:
        input_names += ["pipeline_mode"]
        with_pipeline_modes = True

    params = []

    for name, depth, sequence in generate_sequences(inputs, bases, depths, with_scaling_modes):
        nv12 = "nv12" in sequence.base

        if nv12:
            name += "_nv12"

        if with_pipeline_modes:
            speed = DecHarness.PipelineModes["speed"]
            precision = DecHarness.PipelineModes["precision"]

            if not nv12:
                params.append(pytest.param(
                    *sequence, speed, id=name + "_speed"))
                params.append(pytest.param(
                    *sequence, precision, id=name + "_precision"))
            else:
                params.append(pytest.param(*sequence, speed, id=name))
        else:
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

    assert erp.get_executable(), "Must provide --erp-path when using --regen_encode"
    record_property("enc_command", " ".join(erp.get_command_line()))
    record_property("enc_config", erp.get_config())

    process = erp.run()
    assert process.returncode == 0, f"ERP exited with non-zero return code {process.returncode}."
    run_regen_encode.regen_index += 1
    hashes["perseus"] = perseus


def run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd, with_parallel_decode=False):
    dec_harness.set_param("base", base)
    dec_harness.set_param("perseus", data.get_path(hashes["perseus"]))
    dec_harness.set_param("interleaving", get_interleaving_from_path(str(base)))
    dec_harness.set_param("disable-simd", regen_hash or disable_simd)

    if dec_harness.get_param("interleaving") != "none":
        dec_harness.set_param("hash-deinterleave", True)


    parallel_decode_params =  list(set([False, with_parallel_decode]))
    parallel_decode_names = {False: "serial", True: "parallel"}

    for parallel_decode, decode_name in generate_decodes(with_parallel_decode):
        dec_harness.set_param("parallel-decode", parallel_decode)

        record_property(f"harness_command_{decode_name}", " ".join(dec_harness.get_command_line()))

        process = dec_harness.run()
        assert process.returncode == 0, f"Harness exited with non-zero return code {process.returncode} with {decode_name} decoding"

        hash_file = dec_harness.get_param("hash-file")
        failures = []

        with open(dec_harness.get_working_directory().join(hash_file)) as f:
            generated = json.load(f)

        for surface in generated:
            surface_hash = generated[surface]

            if regen_hash:
                hashes[surface] = surface_hash
            elif surface not in hashes or surface_hash != hashes[surface]:
                failures.append((f'{surface}_{decode_name}', surface_hash))

        record_property(f"mismatching_surfaces_{decode_name}", failures)

        assert not failures, f"{len(failures)} hashes do not match when running decoder with {decode_name} decoding"


@pytest.mark.mr
@pytest.mark.parametrize("upsample", ERP.Upsamplers)
@pytest.mark.parametrize("predicted_average", ERP.PredictedAverage)
def test_upsample(dec_harness, erp, record_property, hashes, data, regen_encode, regen_hash, upsample, predicted_average,
                  pipeline_mode, yuv, base, scaling_mode_level0, scaling_mode_level1, disable_simd):
    if regen_encode:
        erp.set_param("encoding_upsample", upsample)
        erp.set_param("encoding_upsample_decoder_compatible_8b", pipeline_mode == 0)
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        erp.set_param("encoding_use_predicted_residuals", predicted_average)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    dec_harness.set_param("pipeline-mode", pipeline_mode)
    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd)


@pytest.mark.mr
@pytest.mark.parametrize("upsample", ERP.Upsamplers)
@pytest.mark.parametrize("predicted_average", ERP.PredictedAverage)
def test_upsample_level1(dec_harness, erp, record_property, hashes, data, regen_encode,  regen_hash,upsample, predicted_average,
                         pipeline_mode, yuv, base, scaling_mode_level0, scaling_mode_level1, scaling_mode_complex, disable_simd):
    if regen_encode:
        erp.set_param("encoding_upsample", upsample)
        erp.set_param("encoding_upsample_decoder_compatible_8b", pipeline_mode == 0)
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        erp.set_param("encoding_use_predicted_residuals", predicted_average)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    dec_harness.set_param("pipeline-mode", pipeline_mode)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd)


@pytest.mark.mr
@pytest.mark.parametrize("transform", ERP.Transforms)
@pytest.mark.parametrize("temporal, with_reduced", ERP.TemporalDecode)
def test_temporal(dec_harness, erp, record_property, hashes, data, regen_encode,  regen_hash,transform, temporal, with_reduced,
                  pipeline_mode, yuv, base, scaling_mode_level0, scaling_mode_level1, disable_simd):
    if regen_encode:
        erp.set_param("encoding_transform_type", transform)
        erp.set_param("temporal_enabled", temporal)
        erp.set_param("temporal_use_reduced_signalling", with_reduced)
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)
        record_property("erp_command", " ".join(erp.get_command_line()))

    dec_harness.set_param("pipeline-mode", pipeline_mode)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd, True)


@pytest.mark.mr
@pytest.mark.parametrize("transform", ERP.Transforms)
@pytest.mark.parametrize("temporal_sw_modifier", ERP.TemporalSWModifier)
@pytest.mark.parametrize("dequant_offset_mode", ERP.DequantOffsetMode)
@pytest.mark.parametrize("dequant_offset", ERP.DequantOffset)
def test_dequant_offset(dec_harness, erp, record_property, hashes, data, regen_encode, regen_hash, transform, temporal_sw_modifier,
                        dequant_offset_mode, dequant_offset, yuv, base):

    if regen_encode:
        erp.set_param("encoding_transform_type", transform)
        erp.set_param("temporal_enabled", True)
        erp.set_param("temporal_sw_modifier", temporal_sw_modifier)
        erp.set_param("dequant_offset_mode", dequant_offset_mode)
        erp.set_param("dequant_offset", dequant_offset)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, False, True)


@pytest.mark.mr
@pytest.mark.parametrize("sfilter_strength", [0, 0.5, 1.0])
def test_sfilter(dec_harness, erp, record_property, hashes, data, regen_encode,  regen_hash,sfilter_strength, yuv, base, disable_simd):

    if regen_encode:
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    dec_harness.set_param("s-strength", sfilter_strength)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd)


@pytest.mark.mr
@pytest.mark.parametrize("dither_seed", [1, 100, 10000])
# Dither occurs in s-filter if s-filter is enabled, otherwise occurs in upscale
@pytest.mark.parametrize("sfilter_strength", [0, 1.0])
def test_dither(dec_harness, erp, record_property, hashes, data, regen_encode, regen_hash,dither_seed, sfilter_strength, yuv, base,
                disable_simd):
    if regen_encode:
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    dec_harness.set_param("dither-seed", dither_seed)
    dec_harness.set_param("num-threads", 1)
    dec_harness.set_param("disable-dithering", 0)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd)


# @pytest.mark.mr
# @pytest.mark.parametrize("transform", ERP.Transforms)
# @pytest.mark.parametrize("temporal, with_reduced", ERP.TemporalDecode)
# def test_cmdbuffer(dec_harness, erp, record_property, hashes, data, regen_encode, regen_hash, transform, temporal, with_reduced,
#                    yuv, base, disable_simd):
#     if regen_encode:
#         erp.set_param("encoding_transform_type", transform)
#         erp.set_param("temporal_enabled", temporal)
#         erp.set_param("temporal_use_reduced_signalling", with_reduced)
# 
#         run_regen(erp, record_property, data, hashes, yuv, base)
# 
#     dec_harness.set_param("generate-cmdbuffers", 1)
# 
#     run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd, True)


@pytest.mark.mr
@pytest.mark.parametrize("transform", ERP.Transforms)
@pytest.mark.parametrize("plane_mode", ERP.PlaneModes)
@pytest.mark.parametrize("temporal, with_reduced", ERP.TemporalOnOff)
@pytest.mark.parametrize("tile_width, tile_height", ERP.TilingModes)
@pytest.mark.parametrize("ignore_nv12", [True])
@pytest.mark.parametrize("compression_entropy_enabled_per_tile", ERP.CompressionEntropyEnabledPerTile)
@pytest.mark.parametrize("compression_type_size_per_tile", ERP.CompressionTypeSizePerTile)
def test_tiled_decoding(dec_harness, erp, record_property, hashes, data, regen_encode,  regen_hash,transform, plane_mode, temporal,
                        with_reduced, scaling_mode_level0, scaling_mode_level1, tile_width, tile_height,
                        compression_entropy_enabled_per_tile, compression_type_size_per_tile, yuv, base,
                        disable_simd, ignore_nv12):

    # Tracked by SDK-1353 - standards limitation
    if tile_width == 1024 and tile_height == 512 and compression_type_size_per_tile == "prefix_diff":
        pytest.xfail(
            "Known failing case - tile data byte size is too big to be signalled by the compression scheme")

    if regen_encode:
        erp.set_param("encoding_transform_type", transform)
        erp.set_param("encoding_plane_mode", plane_mode)
        erp.set_param("temporal_enabled", temporal)
        erp.set_param("temporal_use_reduced_signalling", with_reduced)
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        erp.set_param("encoding_tile_width", tile_width)
        erp.set_param("encoding_tile_height", tile_height)
        erp.set_param("compression_type_entropy_enabled_per_tile", compression_entropy_enabled_per_tile)
        erp.set_param("compression_type_size_per_tile", compression_type_size_per_tile)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd, True)


@pytest.mark.mr
@pytest.mark.parametrize("conformance_window", ERP.ConformanceWindows)
def test_conformance_window(dec_harness, erp, record_property, hashes, data, regen_encode,  regen_hash,yuv, base, conformance_window,
                            disable_simd):

    if regen_encode:
        erp.set_param("encoding_conformance_window", conformance_window)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd)


@pytest.mark.mr
def test_deblock(dec_harness, erp, record_property, hashes, data, regen_encode, regen_hash,yuv, base, disable_simd):

    if regen_encode:
        erp.set_param("encoding_transform_type", "DDS")
        erp.set_param("deblock_loq1_c", 8)
        erp.set_param("deblock_loq1_s", 7)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd, True)


@pytest.mark.mr
@pytest.mark.parametrize("ignore_nv12", [True])
def test_strided_surfaces(dec_harness, erp, record_property, hashes, data, regen_encode, regen_hash, yuv, base, disable_simd,
                          scaling_mode_level0, scaling_mode_level1, scaling_mode_complex, ignore_nv12):
    if regen_encode:
        erp.set_param("scaling_mode_level0", scaling_mode_level0)
        erp.set_param("scaling_mode_level1", scaling_mode_level1)
        run_regen_encode(erp, record_property, data, hashes, yuv, base)

    dec_harness.set_param("simulate-padding", 1)

    run_test(dec_harness, record_property, data, regen_hash, hashes, base, disable_simd)
