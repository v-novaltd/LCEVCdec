# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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
import time
import subprocess

from utilities.runner import ADBRunner
from utilities.config import config, logger

# Regex for an IP address with port number
IP_REGEX = r"^(?:[0-9]{1,3}\.){3}[0-9]{1,3}:[0-9]{3,4}$"


def initialise_adb_device():
    logger.info(f"Initialising ADB device {config.get('PLATFORM')}...")
    start_time = time.perf_counter()
    process = run_adb(['devices'])
    assert process.returncode == 0, f"Cannot connect to ADB client at {ADBRunner.ADB_SERVER_HOST} (serial: {ADBRunner.ADB_SERIAL}): {process.stderr}"
    process_stdout = process.stdout.decode("utf-8")
    assert ADBRunner.ADB_SERIAL, f"Specified PLATFORM '{config.get('PLATFORM')}' is an ADB platform, " \
        f"please specify an ADB_SERIAL parameter in the config for your device"
    device_offline = [line for line in process_stdout.splitlines()
                      if ADBRunner.ADB_SERIAL in line and 'offline' in line]
    if re.search(IP_REGEX, ADBRunner.ADB_SERIAL) and (ADBRunner.ADB_SERIAL not in process_stdout or device_offline):
        logger.debug(f"Networked ADB device {ADBRunner.ADB_SERIAL} is not connected to {ADBRunner.ADB_SERVER_HOST}, "
                     f"attempting to connect...")
        cmd = [config.get('ADB_PATH', 'adb'), '-H', ADBRunner.ADB_SERVER_HOST,
               'connect', ADBRunner.ADB_SERIAL]
        process = subprocess.run(cmd, timeout=5, stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert process.returncode == 0 and 'connected' in process.stdout.decode("utf-8"), \
            f"Could not connect to networked ADB device at {ADBRunner.ADB_SERIAL}: {process.stderr.decode('utf-8')}"
        process = run_adb(['devices'])
        process_stdout = process.stdout.decode("utf-8")
    assert len(process_stdout.split('\n')) >= 4, f"Cannot find any ADB devices connected to " \
        f"{ADBRunner.ADB_SERVER_HOST}, device list: {process_stdout}"
    assert ADBRunner.ADB_SERIAL in process_stdout, f"Specified ADB serial '{ADBRunner.ADB_SERIAL}' is not connected" \
        f" to the ADB server '{ADBRunner.ADB_SERVER_HOST}'"
    run_adb(['root'], assert_rc=True)
    process = run_adb(
        ['shell', 'su && echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor'])
    if process.returncode != 0:
        logger.warning(f"Failed to force device ({config.get('PLATFORM')}) into performance mode")

    logger.info("Cleaning Android workdir...")
    run_adb(['shell', f'rm -rf {ADBRunner.WORK_DIR} && mkdir {ADBRunner.WORK_DIR}'], assert_rc=True)
    run_adb(
        ['shell', f'rm -rf {ADBRunner.BUILD_DIR} && mkdir {ADBRunner.BUILD_DIR}'], assert_rc=True)
    run_adb(['shell', f'mkdir -p {ADBRunner.ASSETS_DIR}'], assert_rc=True)
    host_build_path = os.path.dirname(config.get('BIN_DIR'))
    if host_build_path.endswith('bin'):
        host_build_path = os.path.dirname(host_build_path)
    host_bin_dir = os.path.join(host_build_path, 'bin')
    host_lib_dir = os.path.join(host_build_path, 'lib')
    dest_bin_dir = ADBRunner.BUILD_DIR + '/bin'
    dest_lib_dir = ADBRunner.BUILD_DIR + '/lib'
    logger.info(
        f"Pushing build to Android device... {host_bin_dir} -> {dest_bin_dir}, {host_lib_dir} -> {dest_lib_dir}")
    run_adb(['shell', f'rm -rf {ADBRunner.WORK_DIR} && mkdir {ADBRunner.WORK_DIR}'], assert_rc=True)
    run_adb(
        ['shell', f'rm -rf {ADBRunner.BUILD_DIR} && mkdir {ADBRunner.BUILD_DIR}'], assert_rc=True)

    run_adb(['push', host_bin_dir, dest_bin_dir], assert_rc=True, timeout=600)
    if os.path.exists(host_lib_dir):  # Some compiler configs just shove everything in the bin
        run_adb(['push', host_lib_dir, dest_lib_dir], assert_rc=True, timeout=600)
    run_adb(['shell', f'chmod +x {ADBRunner.BUILD_DIR}/bin/*'])
    logger.info(f"Initialised Android device in {time.perf_counter() - start_time:.3f}s")


def run_adb(cmd, assert_rc=False, timeout=60):
    adb_path = config.get('ADB_PATH', 'adb')
    formatted_cmd = [adb_path, '-H', ADBRunner.ADB_SERVER_HOST, '-s', config.get('ADB_SERIAL')]
    formatted_cmd.extend(cmd)
    try:
        process = subprocess.run(formatted_cmd, timeout=timeout, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
    except subprocess.TimeoutExpired as e:
        raise TimeoutError(f"ADB setup command '{cmd}' did not exit within {timeout}s: {e}")
    if assert_rc:
        assert process.returncode == 0, \
            f"ADB command '{cmd}' returned {process.returncode} STDERR:\n{process.stderr.decode('utf-8')}\n" \
            f"STDOUT:\n{process.stdout.decode('utf-8')}"
    return process
