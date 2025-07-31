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

import getpass
import glob
import hashlib
import multiprocessing as mp
import os
import requests
import shutil
import sys
import zipfile

from utilities.config import config, logger
from utilities.runner import ADB_PLATFORMS, get_runner

LOCAL_LTM_PATH = os.path.join(config.get('CACHE_PATH'), 'ltm')
# The LTM is only pre-built for Windows and for Linux x64.
INVALID_TARGETS = ['aarch64', 'linux']


def is_platform_valid():
    if config.get('PLATFORM') in ADB_PLATFORMS:
        return False
    if os.environ.get('TARGET'):
        for invalid_arch in INVALID_TARGETS:
            if invalid_arch in os.environ.get('TARGET'):
                return False
    if sys.platform == 'darwin':
        return False
    return True


def is_os_valid(unzipped_path):
    return sys.platform in os.listdir(unzipped_path)


def initialize_ltm():
    if not is_platform_valid() and not os.path.exists(LOCAL_LTM_PATH):
        logger.error(
            f"Current platform {config.get('PLATFORM')} or target {os.environ.get('TARGET')} is"
            " not supported by the LTM pre-built executables")
        return ""

    existing_version = None
    if os.path.exists(LOCAL_LTM_PATH):
        with open(os.path.join(LOCAL_LTM_PATH, 'version.txt'), 'r') as f:
            existing_version = f.read().strip()
        requested_version = config.get('LTM_URL')[config.get(
            'LTM_URL').rfind('/') + 1:config.get('LTM_URL').rfind('.')]
        if existing_version != requested_version:
            logger.info(f"Existing LTM version '{existing_version}' doesn't match config version "
                        f"'{requested_version}', removing and re-downloading")
            shutil.rmtree(LOCAL_LTM_PATH)
            existing_version = None

    if not existing_version:
        temp_path = os.path.join(config.get('CACHE_PATH'), f'ltm_temp_{mp.current_process().pid}')
        os.makedirs(temp_path, exist_ok=True)
        logger.info("Cached LTM executable is required for tests but not present, "
                    "please enter your nexus credentials to download it")
        if config.get('NEXUS_USER') == 'PROMPT' or config.get('NEXUS_PASSWORD') == 'PROMPT':
            username = input("Nexus username (eg. first.last): ")
            password = getpass.getpass("Nexus password: ")
        else:
            username = config.get('NEXUS_USER')
            password = config.get('NEXUS_PASSWORD')
        authentication = requests.auth.HTTPBasicAuth(username, password)
        req = requests.head(config.get('LTM_URL'), auth=authentication)
        if req.status_code == 401:
            raise RuntimeError(f"Failed to authenticate with Nexus ({req.status_code})")
        if not req.ok:
            raise RuntimeError(f"Failed to download the LTM from Nexus "
                               f"({req.status_code}): {req.text}")
        size = int(req.headers.get('content-length')) // (1024 ** 2) \
            if req.headers.get('content-length') else 'unknown'
        logger.info(f"Authentication successful, downloading LTM ({size} MB)...")
        req = requests.get(config.get('LTM_URL'), auth=authentication)
        zip_path = os.path.join(temp_path, 'ltm.zip')
        with open(zip_path, 'wb+') as f:
            f.write(req.content)
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(temp_path)
        os.remove(zip_path)
        folder_contents = os.listdir(temp_path)
        assert len(folder_contents) == 1, "LTM did not correctly unzip"
        unzipped_name = folder_contents[0]
        unzipped_path = os.path.join(temp_path, unzipped_name)
        if not is_os_valid(unzipped_path):
            logger.error(
                f"Current OS {sys.platform} is not supported by the LTM pre-built executables")
            return ""
        if not os.path.exists(LOCAL_LTM_PATH):
            shutil.move(os.path.join(unzipped_path, sys.platform), LOCAL_LTM_PATH)
            if sys.platform == 'linux':
                for file in os.listdir(LOCAL_LTM_PATH):
                    path = os.path.join(LOCAL_LTM_PATH, file)
                    if os.path.isfile(path):
                        os.chmod(path, 0o777)
                codec_executables = (('HM', 'TApp*'), ('ETM', 'evca*'),
                                     ('VTM', '*App'), ('JM', 'l*'))
                for exe_path in codec_executables:
                    for file in glob.glob(os.path.join(LOCAL_LTM_PATH, 'external_codecs', *exe_path)):
                        os.chmod(file, 0o777)
            logger.info(f"Using downloaded LTM version: {unzipped_name}")
            with open(os.path.join(LOCAL_LTM_PATH, 'version.txt'), 'w') as f:
                f.write(unzipped_name)
        else:
            with open(os.path.join(LOCAL_LTM_PATH, 'version.txt'), 'r') as f:
                version = f.read()
            logger.info(f"Using cached LTM version: {version}")
        shutil.rmtree(temp_path)
    else:
        logger.info(f"Using cached LTM version: {existing_version}")

    return LOCAL_LTM_PATH


def ltm_generate_regen_image(test_runner, subsampling, test_dir, encode_path):
    output_path = 'ltm_ref.yuv'
    ltm_runner = get_runner(os.path.abspath(os.path.join(
        LOCAL_LTM_PATH, 'ModelDecoder')), test_dir, force_local=True)
    ltm_runner.set_param('--input_file', encode_path)
    ltm_runner.set_param('--base', 'hevc')
    ltm_runner.set_param('--encapsulation', 'sei_reg')
    ltm_runner.set_param('--format', f'yuv{subsampling}p')
    ltm_runner.set_param('--output_file', output_path)
    test_runner.record_cmd('ltm', ltm_runner.get_command_line())
    ltm_runner.run(assert_rc=True)

    return os.path.join(test_dir, output_path)


def ltm_vs_harness_hash_match(test_dir):
    harness_output = [f for f in os.listdir(test_dir) if f.startswith('harness')][0]
    harness_hash = hashlib.md5(
        open(os.path.join(test_dir, harness_output), 'rb').read()).hexdigest()
    ltm_hash = hashlib.md5(open(os.path.join(test_dir, 'ltm.yuv'), 'rb').read()).hexdigest()
    return harness_hash == ltm_hash
