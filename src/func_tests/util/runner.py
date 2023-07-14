import py
import pytest
import subprocess
import sys
import os
import xml.etree.ElementTree as ET
import time
import random
from pathlib import Path

from .vooya_specifiers import resolution_from_filename, bitdepth_from_filename, yuv_format_from_filename


class Runner:
    def __init__(self, executable, cwd, env=None, record_property=None):
        self._executable = executable
        self._config = {}
        self._cwd = cwd
        self._timeout = None
        self._xvfb = False
        self._env = env
        self.args = []
        self._record_property_func = record_property

    def set_working_directory(self, cwd):
        self._cwd = cwd

    def set_timeout(self, timeout):
        self._timeout = timeout

    def set_xvfb(self, xvfb):
        self._xvfb = xvfb

    def get_config(self):
        return self._config

    def get_executable(self):
        return self._executable

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

    def set_param(self, param, value="", **kwargs):
        # required because xdist doesn't like serializing these objects
        if isinstance(value, py._path.local.LocalPath):
            value = str(value)

        while param.startswith("-"):
            param = param[1:]

        self._config[param] = value

    def unset_param(self, param):
        while param.startswith("-"):
            param = param[1:]

        if param in self._config:
            del self._config[param]

    def get_param(self, param, default=None):
        return self._config[param] if param in self._config else default

    def get_working_directory(self):
        return self._cwd

    def get_command_line(self):
        params = [self._executable] if not self._xvfb else ['xvfb-run', self._executable]
        params += self.args

        for param, param_value in self._config.items():
            prefix = "-" if len(param) == 1 else "--"

            if isinstance(param_value, bool):
                value = "1" if param_value else "0"
            else:
                value = str(param_value)

            params.append(f"{prefix}{param}")
            params.append(value)

        return params

    def record_properties(self, prefix: str):
        self._record_property_func(
            prefix + "_command", " ".join(self.get_command_line()))
        self._record_property_func(prefix + "_config", self.get_config())

    def run(self, **kwargs):
        try:
            return subprocess.run(self.get_command_line(), cwd=self._cwd, **kwargs, timeout=self._timeout, env=self._env)
        except subprocess.TimeoutExpired:
            pytest.fail(f"Command did not exit within {self._timeout}s")

    def run_and_check(self, **kwargs):
        p = self.run(**kwargs)
        p.check_returncode()


GLFW_INIT_FAILED_ERROR_MESSAGE = 'Failed to initialise GLFW 0x10008'


class OpenGLRunner(Runner):
    def __init__(self, executable, cwd, max_retries: int = 10):
        super().__init__(executable, cwd)
        self._max_retries = max_retries

    def _glfw_init_failed(self, result):
        stderr_output = result.stderr.decode()
        if GLFW_INIT_FAILED_ERROR_MESSAGE in stderr_output:
            return True
        return False

    def run(self, **kwargs):
        # Need to capture the output so that we can inspect it in _glfw_init_failed.
        result = super().run(**kwargs, capture_output=True)

        n_retries = 1
        while n_retries < self._max_retries:
            # Because we captured the output, we prevented stderr and stdout from getting printed
            # However, obviously we do actually want to see the program output, so we print it
            # manually here.
            print(f'{result.stderr.decode()}')
            print(f'{result.stdout.decode()}')
            if self._glfw_init_failed(result):
                result = super().run(**kwargs, capture_output=True)
                n_retries += 1
                # \033[91m=red text, and \033[0m=reset text to default colour
                print(
                    f'\033[91m\n\n==========================================='
                    'Retrying command because of GLFW init failure, attempt {n_retries} of {self._max_retries}\n\n\033[0m')
                time.sleep(random.random())
            else:
                break

        return result


class ADBRunner(Runner):
    BASE_WORKDIR = "/data/local/pytest_workdir"
    ASSETS_DIR = "/data/local/pytest_all_assets"
    BUILD_DIR = "/data/local/pytest_build"
    ABD_SERVER_HOST = "localhost"  # Used to switch when debugging from a different machine

    def __init__(self, executable, cwd, adb_path, env=None, record_property=None):
        super().__init__(executable, cwd, env, record_property)
        self.adb_path = adb_path
        self._config = {}
        self._xvfb = False
        self.args = []
        self._record_property_func = record_property
        self._executable = f"{self.BUILD_DIR}/bin/{os.path.basename(executable)}"
        self.workdir = f"{self.BASE_WORKDIR}/{Path(self._cwd).stem}"
        self.run_adb(['shell', f'rm -rf {self.workdir} && mkdir {self.workdir}'])

    def run(self, **kwargs):
        ret = self.run_adb(['shell', f'cd {self.workdir} && export LD_LIBRARY_PATH={self.BUILD_DIR}/lib/ '
                           f'&& {" ".join(self.get_command_line())}'])
        self._copy_back_results()
        return ret

    def run_adb(self, cmd, assert_rc=False, **kwargs):
        formatted_cmd = [self.adb_path, '-H', self.ABD_SERVER_HOST, '-s', '30e4a0c7'] + cmd
        try:
            process = subprocess.run(formatted_cmd, **kwargs, timeout=self._timeout,
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except subprocess.TimeoutExpired:
            pytest.fail(f"Command did not exit within {self._timeout}s")
        if assert_rc:
            assert process.returncode == 0, f"ADB command '{cmd}' returned {process.returncode}: {process.stderr.decode('utf-8')}"
        return process

    def set_param(self, param, value="", **kwargs):
        if kwargs.get('path', False):
            value = self._translate_path(value)
        return super().set_param(param, value)

    def _translate_path(self, host_path):
        process = self.run_adb(['shell', f'ls {self.ASSETS_DIR}'], assert_rc=True)
        resource_folders = process.stdout.decode('utf-8').replace('\r', '').split('\n')[:-1]
        last_shortened = None
        host_folders = host_path
        while host_folders != last_shortened:
            last_shortened = host_folders
            host_folders = os.path.dirname(host_folders)
            folder = os.path.basename(host_folders)
            if folder in resource_folders:
                folder_index = list(Path(host_path).parts).index(folder)
                sub_path = '/'.join(Path(host_path).parts[folder_index + 1:])
                return f"{self.ASSETS_DIR}/{folder}/{sub_path}"
        else:
            raise RuntimeError(f"Cannot convert path '{host_path}' to an android resource folder: {resource_folders}")

    def _copy_back_results(self):
        self.run_adb(['pull', self.workdir, self._cwd], assert_rc=True)
        inner_folder = os.path.join(self._cwd, Path(self.workdir).stem)
        for file in os.listdir(inner_folder):
            os.rename(os.path.join(inner_folder, file), os.path.join(self._cwd, file))


class FFmpegSource:
    def __init__(self, path: str, format_args=[], size_args=[]):
        self._path = path
        extension = path.split('.')[-1]

        if size_args == []:
            if extension == 'yuv':
                width, height = resolution_from_filename(path)
                self._size_args = ['-s', f'{width}x{height}']
            else:
                self._size_args = []
        else:
            self._size_args = size_args

        if format_args == []:
            if extension == 'yuv':
                yuv_format = yuv_format_from_filename(path)
                bitdepth = bitdepth_from_filename(path)
                if bitdepth == 10:
                    yuv_format += "10"
                elif bitdepth == 12:
                    yuv_format += "12"
                self._format_args = ['-f', 'rawvideo',
                                     '-pix_fmt', f'yuv{yuv_format}']
            else:
                self._format_args = []
        else:
            self._format_args = format_args

    def set_format_args(self, format_args):
        self._format_args = format_args

    def get_path(self) -> str:
        return self._path

    def get_ffmpeg_args(self, is_input: bool) -> list:
        return self._size_args + self._format_args + (['-i'] if is_input else []) + [self._path]


class FFmpegRunner:
    def __init__(self, bin_dir, cwd, env={}):
        self._executable = 'ffmpeg'
        self._input = None
        self._output_name = None
        self._output_container = None
        self._args = []
        self._eil_params = []
        self._cwd = cwd
        self._env = env

    def set_working_directory(self, cwd):
        self._cwd = cwd

    def set_input(self, input: FFmpegSource):
        self._input = input

    def get_input(self) -> FFmpegSource:
        return self._input

    def set_output_name(self, output_name: str):
        self._output_name = output_name

    def set_output_container(self, output_container: str):
        self._output_container = output_container

    def get_output(self) -> FFmpegSource:
        return FFmpegSource(f'{self._output_name}.{self._output_container}')

    def get_args(self):
        return self._args

    def set_args(self, args):
        self._args = args

    def set_eil_params(self, eil_params):
        self._eil_params = eil_params

    def get_eil_params(self):
        return self._eil_params

    def add_eil_param(self, param):
        self._eil_params.append(param)
        return

    def get_working_directory(self):
        return self._cwd

    def get_command_line(self):
        full_args = self._args.copy()
        if self._eil_params:
            full_args += [
                "-eil_params", f"{';'.join(self._eil_params)}"
            ]
        return [
            self._executable,
            *self._input.get_ffmpeg_args(is_input=True),
            *full_args,
            *self.get_output().get_ffmpeg_args(is_input=False),
        ]

    def update(self, conf):
        if 'eil_params' in conf:
            self._eil_params = conf['eil_params'] + self._eil_params

        if 'ffmpeg_args' in conf:
            self._args = conf['ffmpeg_args'] + self._args

        if 'input_format_args' in conf:
            self._input.set_format_args(conf['input_format_args'])

    def run(self, **kwargs):
        is_shell = sys.platform == "win32"
        return subprocess.run(self.get_command_line(), cwd=self._cwd, env=self._env, shell=is_shell, **kwargs)

    def wait(self, **kwargs):
        is_shell = sys.platform == "win32"
        p = subprocess.Popen(self.get_command_line(
        ), cwd=self._cwd, env=self._env, shell=is_shell, **kwargs)
        return p.wait()


class BasicFFmpegRunner:
    def __init__(self, ffmpeg_env):
        self._env = ffmpeg_env
        self.args = []

    def run(self, **kwargs):
        is_shell = sys.platform == "win32"
        return subprocess.run(['ffmpeg'] + self.args, env=self._env, shell=is_shell, **kwargs)


class EilpValidatorRunner:
    DEFAULT_TIMEOUT_SECONDS = 15 * 60

    def __init__(self, executable, cwd, record_property, env=None):
        self._executable = executable
        self.validator_args = []
        self.gtest_args = []
        self.base_plugin_config = {}
        self.record_property = record_property
        self.enable_properties_passthru = True
        self.working_directory = cwd
        self._env = env
        self.timeout_seconds = self.DEFAULT_TIMEOUT_SECONDS

    def setup(self, plugin_name, test_name):
        self.gtest_args = [
            f'--gtest_filter={test_name}'
        ]

        if sys.platform == "win32":
            plugin_lib_name = f'lcevc_eilp_{plugin_name}.dll'
        else:
            plugin_lib_name = f'liblcevc_eilp_{plugin_name}.so'

        self.validator_args = ['--plugin', plugin_lib_name]

        self.record_property('eilp_validator_plugin_name', plugin_name)
        self.record_property('eilp_validator_test_name', test_name)

    @property
    def output_filename(self):
        return os.path.join(self.working_directory, 'gtest-output.xml')

    @property
    def command_line(self):
        base_plugin_args = []
        for k in self.base_plugin_config:
            base_plugin_args.append(f'--{k}')
            base_plugin_args.append(str(self.base_plugin_config[k]))

        return [self._executable] \
            + self.validator_args \
            + ['--'] \
            + [f'--gtest_output=xml:{self.output_filename}'] \
            + self.gtest_args \
            + base_plugin_args

    def run(self, **kwargs):
        is_shell = sys.platform == "win32"
        command_line = self.command_line
        self.record_property('eilp_validator_command', ' '.join(command_line))
        try:
            p = subprocess.run(command_line, cwd=self.working_directory, env=self._env,
                               shell=is_shell, timeout=self.timeout_seconds, **kwargs)
            p.check_returncode()
        finally:
            self.record_eilp_validator_properties()

    def record_eilp_validator_properties(self):
        if not self.enable_properties_passthru:
            return

        if not os.path.exists(self.output_filename):
            # Can happen if the test executable crashes, e.g. in which case
            # we don't want the error to be "FileNotFound"
            return

        tree = ET.parse(self.output_filename)
        root = tree.getroot()
        test_cases = root.findall('.//testcase')
        if len(test_cases) > 1:
            raise ValueError(
                'Cannot record properties: Too many test cases run. Please configure gtest to run only one at a time.')
        elif len(test_cases) == 0:
            # Do not raise an error here - we should already be processing an
            # exception because the test executable should return bad exit
            # code in this case
            return

        for prop in test_cases[0].findall('./properties/property'):
            name = prop.attrib['name']
            value = prop.attrib['value']
            self.record_property(f'eilp_validator_{name}', value)
