# Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
# software may be incorporated into a project under a compatible license provided the requirements
# of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
# licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
# ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
# THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

import os
import re
import git
import json
import time
import hashlib
import requests
import multiprocessing as mp
import shutil

from utilities.config import config, logger


DEFAULT_HERP_URL = 'http://herp'


def get_encode(input_erp_params):
    erp_params = input_erp_params.copy()
    cache_path = os.path.join(config.get('CACHE_PATH'), 'encodes')
    os.makedirs(cache_path, exist_ok=True)
    build_hash = erp_params.get('hash', config.get('ENCODER_VERSION'))
    if 'hash' in erp_params:
        del erp_params['hash']
    bitstream_type = erp_params.get('type', 'ts')
    if 'type' in erp_params:
        del erp_params['type']
    param_hash = hashlib.md5(json.dumps(erp_params, sort_keys=True).encode('utf-8')).hexdigest()
    encode_filepath = os.path.join(cache_path, f"{build_hash}-{param_hash}.{bitstream_type}")
    if not os.path.isfile(encode_filepath):
        start_time = time.perf_counter()
        url = config.get('HERP_URL', DEFAULT_HERP_URL) + '/encode'
        assert build_hash, "An ENCODER_VERSION must be specified in the config"
        req = requests.get(url, params=dict(type=bitstream_type, hash=build_hash, **erp_params))
        if not req.ok:
            try:
                # Log errors with newlines formatted correctly
                logger.error(req.json()['error_msg'])
                logger.error(erp_params)
            except Exception:
                pass
            raise RuntimeError(f"Error while getting encode ({req.status_code}): {req.text}")
        save_file_safe(req, cache_path, encode_filepath)
        logger.info(f"Retrieved encoded asset from HERP in {(time.perf_counter() - start_time) * 1000:.3f}ms. Asset"
                    f" location: {os.path.join(cache_path, encode_filepath)}")
    return os.path.abspath(encode_filepath)


def get_conformance(version, conf_id, content_name, stream_type='bit', get_config=True):
    conformance_dir = os.path.join(config.get('CACHE_PATH', 'test_asset_cache'), "conformance")
    os.makedirs(conformance_dir, exist_ok=True)
    assert isinstance(version, str) and version in ('v2', 'v3', 'v4'), \
        "Conformance version must be either v2, v3 or v4"
    os.makedirs(os.path.join(conformance_dir, version), exist_ok=True)
    conformance_stream_path = os.path.abspath(os.path.join(
        conformance_dir, version, f"{conf_id}_{content_name}.{stream_type}"))
    conformance_config_path = os.path.abspath(os.path.join(
        conformance_dir, version, f"{conf_id}_{content_name}.cfg"))
    missing = dict()
    if not os.path.isfile(conformance_stream_path):
        missing['stream'] = conformance_stream_path
    if get_config and not os.path.isfile(conformance_config_path):
        missing['config'] = conformance_config_path
    if missing:
        start_time = time.perf_counter()
        for asset_type, path in missing.items():
            url = config.get('HERP_URL', DEFAULT_HERP_URL) + f'/conformance_{asset_type}'
            type_param = dict(type=stream_type) if asset_type == 'stream' else dict()
            req = requests.get(url, params=dict(version=version, id=conf_id,
                               content=content_name, **type_param))

            if not req.ok:
                raise RuntimeError(f"Error while getting conformance {asset_type} {conf_id}_{content_name}"
                                   f" ({req.status_code}): {req.text}")
            save_file_safe(req, conformance_dir, path)
        logger.info(f"Retrieved conformance asset {version} {conf_id}_{content_name}.{stream_type} from HERP in "
                    f"{(time.perf_counter() - start_time) * 1000:.3f}ms. Asset location:"
                    f"{os.path.join(conformance_dir, path)}")
    return conformance_stream_path, conformance_config_path


def get_compliance(version, conf_id, content_name, stream_type='bit', get_config=True):
    compliance_dir = os.path.join(config.get('CACHE_PATH'), 'assets')
    os.makedirs(os.path.join(compliance_dir, version), exist_ok=True)
    compliance_stream_path = os.path.abspath(os.path.join(
        compliance_dir, version, f"{conf_id}_{content_name}.{stream_type}"))
    compliance_config_path = os.path.abspath(os.path.join(
        compliance_dir, version, f"{conf_id}_{content_name}.cfg"))
    missing = dict()
    if not os.path.isfile(compliance_stream_path):
        missing['bit'] = compliance_stream_path
    if get_config and not os.path.isfile(compliance_config_path):
        missing['cfg'] = compliance_config_path
    if missing:
        start_time = time.perf_counter()
        for asset_type, path in missing.items():
            url = config.get('HERP_URL', DEFAULT_HERP_URL) + '/asset'
            req = requests.get(url, params=dict(
                path=f"compliance_{stream_type}/{conf_id}/{content_name}_{conf_id}.{asset_type}"))
            if not req.ok:
                raise RuntimeError(f"Error while getting compliance {asset_type} {conf_id}_{content_name}"
                                   f" ({req.status_code}): {req.text}")
            save_file_safe(req, compliance_dir, path)
        logger.info(f"Retrieved compliance asset {conf_id}_{content_name}.{stream_type} from HERP in "
                    f"{(time.perf_counter() - start_time) * 1000:.3f}ms. Asset location:"
                    f"{os.path.join(compliance_dir, path)}")
    return compliance_stream_path, compliance_config_path


def get_asset(asset):
    asset_root = os.path.join(config.get('CACHE_PATH'), 'assets')
    os.makedirs(asset_root, exist_ok=True)
    asset_path = os.path.join(asset_root, asset)
    if not os.path.isfile(asset_path):
        start_time = time.perf_counter()
        url = config.get('HERP_URL', DEFAULT_HERP_URL) + '/asset'
        req = requests.get(url, params=dict(path=asset))
        if not req.ok:
            raise RuntimeError(f"Error while getting lcevc_dec_asset {asset} "
                               f"({req.status_code}): {req.text}")
        os.makedirs(os.path.dirname(asset_path), exist_ok=True)
        save_file_safe(req, asset_root, asset_path)
        logger.info(f"Retrieved lcevc_dec_asset from HERP in {(time.perf_counter() - start_time) * 1000:.3f}ms. Asset"
                    f" location: {os.path.abspath(asset_path)}")
    return os.path.abspath(asset_path)


def save_file_safe(req, dir_name, file_path):  # Save a download in thread safe way without locking
    temp_filepath = os.path.join(dir_name, f"download_{mp.current_process().pid}")
    with open(temp_filepath, 'wb') as f:
        f.write(req.content)
    if os.path.isfile(file_path):  # Case where another thread has got here first
        os.remove(temp_filepath)
    else:
        try:
            os.rename(temp_filepath, file_path)
        except FileExistsError:
            pass  # Case where another thread has got here first - on Windows only


def get_bases(tests):
    bases_path = os.path.join(config.get('CACHE_PATH', 'test_asset_cache'), "bases")
    if not os.path.isdir(bases_path):
        os.makedirs(bases_path, exist_ok=True)
    required_bases = set([test['cli']['--base'].replace('{BASE}/', '') for test in tests
                          if test.get('cli', {}).get('--base') and '{BASE}' in test['cli']['--base'] and test['enabled']])
    missing_bases = required_bases - set(os.listdir(bases_path))
    if not missing_bases:
        return

    logger.info(f"Missing {len(missing_bases)} base YUVs from local bases cache, "
                f"retrieving from HERP...")
    url = config.get('HERP_URL', DEFAULT_HERP_URL) + '/content'
    req = requests.get(url)
    if not req.ok:
        raise RuntimeError(f"Error while getting content list ({req.status_code}): {req.text}")
    contents = set(req.json())
    assert not (missing_bases
                - contents), f"Some bases are not available on HERP: {missing_bases - contents}"
    for base in sorted(list(missing_bases)):
        base_path = os.path.join(bases_path, base)
        if os.path.exists(base_path):
            continue
        req = requests.head(url, params=dict(file=base))
        if not req.ok:
            raise RuntimeError(f"Error while getting base YUV ({req.status_code}): {req.text}")
        size = int(req.headers.get('content-length')) // (1024 ** 2) \
            if req.headers.get('content-length') else 'unknown'
        logger.info(f"Downloading base '{base}' ({size} MB)...")
        req = requests.get(url, params=dict(file=base), stream=True)
        if req.status_code == 200:
            with open(base_path, 'wb') as f:
                for chunk in req.iter_content(chunk_size=(128 * 1024)):  # Download in 128KiB chunks
                    f.write(chunk)
        else:
            raise RuntimeError(f"Error while getting base YUV ({req.status_code}): {req.text}")


def herp_is_accessible():
    url = config.get('HERP_URL', DEFAULT_HERP_URL)
    try:
        requests.get(url)
        return True
    except (requests.exceptions.ConnectionError, requests.exceptions.Timeout):
        return False


def download_assets_externally(platform):
    repo = git.Repo(search_parent_directories=True)
    tags = sorted(repo.tags, key=lambda t: t.commit.committed_date)

    # Filter tags that match the numerical format (e.g., 1.2.3)
    numeric_tags = [tag for tag in tags if re.match(r'^\d+(\.\d+)*$', str(tag))]
    version = numeric_tags[-1] if numeric_tags else None
    assert version, f"Cannot determine git version to download the correct test assets: {version}"
    variant = 'external' if platform == 'External' else 'full'
    external_url = config.get('EXTERNAL_ASSET_URL').format(VERSION=version, VARIANT=variant)
    test_cache_path = config.get('CACHE_PATH', 'test_cache')
    test_cache_zip = os.path.join(test_cache_path, "test_cache.zip")

    existing = None
    if os.path.isfile(os.path.join(test_cache_path, 'version.txt')):
        with open(os.path.join(test_cache_path, 'version.txt'), 'r') as f:
            existing = f.read()
        existing_version, existing_platform = existing.split('-')

    # If external URL exists and the correct assets are not already downloaded
    if external_url and (not existing or existing_version != version
                         or (existing_platform == 'External' and platform != 'External')):
        req = requests.head(external_url)
        if not req.ok:
            raise RuntimeError(
                f"Error while getting test assets externally from {external_url} ({req.status_code}): {req.text}")
        size = round(float(req.headers.get('content-length')) / (1024 ** 3), 2) \
            if req.headers.get('content-length') else 'unknown'
        logger.info(f"Downloading test assets from '{external_url}' ({size} GB)...")
        if os.listdir(test_cache_path):
            shutil.rmtree(test_cache_path)
            os.makedirs(test_cache_path)
        req = requests.get(external_url, stream=True)
        if req.status_code == 200:
            with open(test_cache_zip, 'wb') as f:
                # Download in 128KiB chunks
                for chunk in req.iter_content(chunk_size=(128 * 1024)):
                    f.write(chunk)
        else:
            raise RuntimeError(
                f"Error while downloading test cache ({req.status_code}): {req.text}")
        logger.info("Extracting test assets zip...")
        shutil.unpack_archive(test_cache_zip, extract_dir=test_cache_path, format='zip')
        for dir in os.listdir(os.path.join(test_cache_path, 'test_cache')):
            shutil.move(os.path.join(test_cache_path, 'test_cache', dir), test_cache_path)
        os.rmdir(os.path.join(test_cache_path, 'test_cache'))
        with open(os.path.join(test_cache_path, 'version.txt'), 'w') as f:
            f.write(f"{version}-{platform}")
