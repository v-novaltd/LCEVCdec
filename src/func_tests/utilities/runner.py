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
import time
import json
import platform
import subprocess
from pathlib import Path, PurePosixPath

from utilities.config import config, logger

ADB_PLATFORMS = ("Qualcomm_devkit", "VIM4", "VIM4_1", "VIM3L_0", "PIXEL5_0", "PIXEL5_1")


def get_runner(*args, force_local=False, **kwargs):
    if config.get('PLATFORM') in ADB_PLATFORMS and not force_local:
        return ADBRunner(*args, **kwargs)
    else:
        return Runner(*args, **kwargs)


class Runner:
    def __init__(self, executable, cwd=config.get('WORKDIR'), env=None):
        self._executable = executable
        self._config = {}
        self._cwd = cwd
        self._timeout = None
        self.args = []
        self._positional_args = {}
        self.env = dict(os.environ)
        if env:
            self.env.update(env)
        if platform.system() != "Windows" and 'LD_LIBRARY_PATH' not in self.env:
            self.env['LD_LIBRARY_PATH'] = os.path.abspath(
                os.path.join(config.get('BIN_DIR'), '../lib'))
        elif platform.system() == "Windows" and not self._executable.endswith('ModelDecoder') and not self._executable.endswith('ModelEncoder'):
            if 'bin' in self._executable:
                self.env['PATH'] += ";" + self._executable[:self._executable.find("bin") + 3]
            else:
                raise RuntimeError(f"Cannot set library paths executable: {self._executable}")

    def get_config(self):
        return self._config

    def update_config(self, params):
        if isinstance(params, dict):
            for param, value in params.items():
                self.set_param(param, value)
        elif isinstance(params, list):
            it = iter(params)

            for param, value in zip(it, it):
                self.set_param(param, value)
        else:
            raise ValueError("Params must be a dict or list")

    def set_params_from_config(self, test):
        for key, value in test.items():
            try:
                key = int(key)
            except (ValueError, TypeError):
                pass
            if value != '':
                if isinstance(key, int):  # Any arg that is just a number is treated as a positional arg
                    self._positional_args[key] = value
                else:
                    self.set_param(key, value)

    def set_positional_arg(self, position, value, **kwargs):
        assert isinstance(position, int), \
            "Position of the positional arg must be a zero-indexed integer"
        self._positional_args[position] = value

    def set_param(self, param, value="", **kwargs):
        while param.startswith("-"):
            param = param[1:]

        self._config[param] = value

    def set_json_param(self, param, value: dict, **kwargs):
        self.set_param(param, json.dumps(value, separators=(",", ":")), **kwargs)

    def get_param(self, param, default=None):
        return self._config[param] if param in self._config else default

    # separate_param_from_value is necessary for some command-running interfaces, but it will mess
    # up parameters that are formatted as '--param=value' as opposed to '--param value'
    def command_as_list(self, separate_param_from_value=False):
        params = list()
        for param, param_value in self._config.items():
            prefix = "-" if len(param) == 1 else "--"
            if separate_param_from_value:
                params.append(f"{prefix}{param}")
                if param_value != 'FLAG':
                    params.append(f"{str(param_value)}")
            else:
                value = str(param_value) if param_value != 'FLAG' else ""
                if '=' not in value:
                    value = " " + value
                params.append(''.join([f"{prefix}{param}", f"{value}"]))
        return params

    def get_command_line(self, separate_param_from_value=False, as_string=True):
        params = list()
        if config.getboolean('ENABLE_VALGRIND'):
            params = ['valgrind', '--error-exitcode=1', '--trace-children=yes',
                      '--leak-check=full', '--leak-resolution=med']
        params.append(self._executable)
        params += [value for value in dict(sorted(self._positional_args.items())).values()]
        params += self.command_as_list(separate_param_from_value)
        if as_string:
            return ' '.join(params)
        return params

    def run(self, assert_rc=False, **kwargs):
        kwargs['env'] = self.env
        try:
            process = self.call_subprocess(kwargs)
            if assert_rc:
                assert process.returncode == 0, \
                    f"FAILED. {self._executable} returned {process.returncode}:\nSTDOUT:\n{process.stdout.decode('utf-8')}\n" \
                    f"STDERR:\n{process.stderr.decode('utf-8')}"
            return process
        except subprocess.TimeoutExpired:
            raise RuntimeError(f"Command did not exit within {self._timeout}s")

    def call_subprocess(self, kwargs):
        return subprocess.run(self.get_command_line(separate_param_from_value=True, as_string=False), cwd=self._cwd, **kwargs, timeout=self._timeout,
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


class ADBRunner(Runner):
    DEVICE_BASE_DIR = config.get('DEVICE_BASE_DIR')
    DEVICE_BUILD_DIR = config.get('DEVICE_BUILD_DIR', DEVICE_BASE_DIR)
    WORK_DIR = str(PurePosixPath(DEVICE_BASE_DIR) / "test_workdir")
    ASSETS_DIR = str(PurePosixPath(DEVICE_BASE_DIR) / "test_asset_cache")
    BUILD_DIR = str(PurePosixPath(DEVICE_BUILD_DIR) / "test_build")
    # Used to switch when debugging from a different machine
    ADB_SERVER_HOST = config.get('ADB_SERVER_HOST', 'localhost')
    ADB_SERIAL = config.get('ADB_SERIAL')
    DEVICE_ENCODE_CACHE_SIZE = 1000

    def __init__(self, executable, cwd, **kwargs):
        super().__init__(executable, cwd, **kwargs)
        self.adb_path = config.get('ADB_PATH', 'adb')
        self._config = {}
        self.args = []
        self._executable = f"{self.BUILD_DIR}/bin/{os.path.basename(executable)}"
        self.workdir = f"{self.WORK_DIR}/{Path(self._cwd).stem}"
        self.run_adb(['shell', f'rm -rf {self.workdir} && mkdir {self.workdir}'])
        self.run_adb(['shell', f'chmod +x {self.BUILD_DIR}/*'])

    def run(self, **kwargs):
        ret = self.run_adb(['shell', f'cd {self.workdir} '
                            f'&& export LD_LIBRARY_PATH={self.BUILD_DIR}/lib/:{self.BUILD_DIR}/bin/:{self.BUILD_DIR}/ '
                            f'&& {" ".join(self.get_command_line(separate_param_from_value=True, as_string=False))}'])
        self._copy_back_results()
        return ret

    def run_adb(self, cmd, assert_rc=False, **kwargs):
        formatted_cmd = [self.adb_path, '-H', self.ADB_SERVER_HOST, '-s', self.ADB_SERIAL]
        formatted_cmd.extend(cmd)
        if 'timeout' not in kwargs:
            kwargs['timeout'] = self._timeout
        try:
            process = subprocess.run(formatted_cmd, **kwargs,
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except subprocess.TimeoutExpired:
            raise RuntimeError(f"Command did not exit within {self._timeout}s")
        if assert_rc:
            assert process.returncode == 0, \
                f"ADB command '{cmd}' returned {process.returncode} STDERR:\n{process.stderr.decode('utf-8')}\n" \
                f"STDOUT:\n{process.stdout.decode('utf-8')}"
        return process

    def set_positional_arg(self, position, value, **kwargs):
        if kwargs.get('path', False):
            value = self._translate_path(value)
        return super().set_positional_arg(position, value)

    def set_param(self, param, value="", **kwargs):
        if kwargs.get('path', False):
            value = self._translate_path(value)
        return super().set_param(param, value)

    def set_json_param(self, param, value: dict, **kwargs):
        self.set_param(param, f"'{json.dumps(value, separators=(',', ':'))}'", **kwargs)

    def _translate_path(self, host_path):
        asset_cache = os.path.abspath(os.path.join(config.get('CACHE_PATH'), 'assets'))
        if os.path.samefile(os.path.commonpath([asset_cache, os.path.abspath(host_path)]), asset_cache):
            host_path_copy = host_path
            path_parts = list()
            while host_path_copy != asset_cache:
                path_parts.append(os.path.basename(host_path_copy))
                host_path_copy = os.path.dirname(host_path_copy)
            path_parts.reverse()
            file_name = os.path.join(*path_parts).replace('\\', '-').replace('/', '-')
        else:
            file_name = os.path.basename(host_path)

        if os.path.basename(os.path.dirname(host_path)) == 'bases':
            self.run_adb(['shell', f'mkdir -p {self.ASSETS_DIR}/bases'], assert_rc=True)
            process = self.run_adb(['shell', f'ls {self.ASSETS_DIR}/bases'], assert_rc=True)
            pushed_assets = process.stdout.decode(
                'utf-8').replace('\r', '').replace(' ', '').split('\n')[:-1]
            if file_name not in pushed_assets:
                logger.info(f"Pushing YUV to Android device '{file_name}'...")
                self.run_adb(['shell', f'mkdir -p {self.ASSETS_DIR}/bases'], assert_rc=True)
                self.run_adb(
                    ['push', host_path, f"{self.ASSETS_DIR}/bases/{file_name}"], assert_rc=True, timeout=3600)
            return f"{self.ASSETS_DIR}/bases/{file_name}"
        else:
            host_hash, host_extension = file_name.split('.')
            process = self.run_adb(['shell', f'ls {self.ASSETS_DIR}'], assert_rc=True)
            raw_pushed_assets = process.stdout.decode('utf-8').replace('\r', '').split('\n')[:-1]
            pushed_assets = list()
            for raw_name in raw_pushed_assets:
                raw_name = raw_name.strip()
                if raw_name == 'bases':
                    continue
                name, extension = raw_name.split('.')
                encode_hash = '_'.join(name.split('_')[:-1])
                push_time = name.split('_')[-1]
                pushed_assets.append(
                    dict(hash=encode_hash, time=int(push_time), extension=extension))
            existing = [asset for asset in pushed_assets
                        if asset['hash'] == host_hash and asset['extension'] == host_extension]
            if existing:
                return f"{self.ASSETS_DIR}/{existing[0]['hash']}_{existing[0]['time']}.{existing[0]['extension']}"
            else:
                translated_name = f"{host_hash}_{int(time.time())}.{host_extension}"
                logger.info(f"Pushing encode to Android device '{translated_name}'...")
                self.run_adb(
                    ['push', host_path, f"{self.ASSETS_DIR}/{translated_name}"], assert_rc=True, timeout=60)
                pushed_assets.sort(key=lambda k: k['time'])
                while len(pushed_assets) > self.DEVICE_ENCODE_CACHE_SIZE:
                    oldest_encode = pushed_assets.pop(0)
                    oldest_path = f"{self.ASSETS_DIR}/{oldest_encode[0]['hash']}_{oldest_encode[0]['time']}.{oldest_encode[0]['extension']}"
                    self.run_adb(['shell', f"rm {oldest_path}"], assert_rc=True)
                return f"{self.ASSETS_DIR}/{translated_name}"

    def _copy_back_results(self):
        local_workdir = config.get('WORKDIR')
        self.run_adb(['pull', self.workdir, local_workdir], assert_rc=True)
        inner_folder = os.path.join(local_workdir, Path(self.workdir).stem)
        for file in os.listdir(local_workdir):
            if not os.path.isfile(os.path.join(local_workdir, file)):
                continue
            os.rename(os.path.join(local_workdir, file), os.path.join(inner_folder, file))
