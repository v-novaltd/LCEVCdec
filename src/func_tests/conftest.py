import json
import os
import platform
import shutil
from pathlib import Path
import subprocess
from datetime import datetime

import pytest

from util.csv_report import CsvReport
from util.runner import ADBRunner

conan_api = None
ConanFileReference = None

pytest_plugins = ["plugins.reporter"]


def get_executable(path, name):
    if platform.system() == "Windows":
        name += ".exe"

    executable = os.path.join(path, name)

    if not os.path.exists(executable):
        pytest.exit(f'"{executable}" does not exist')

    return os.path.abspath(executable)


def abspath(path):
    if path:
        path = os.path.expanduser(path)
        path = os.path.expandvars(path)
        return os.path.abspath(path)
    else:
        return None


def pytest_addoption(parser):
    parser.addoption("--bin-dir", action="store", default=None,
                     help="Directory of the binaries")
    parser.addoption("--resource-dir", action="store",
                     default=None, help="Directory of test resources")
    parser.addoption("--data-dir", action="store", default=None,
                     help="Directory of the test data files")
    parser.addoption("--conformance-assets-dir", action="store", default=None,
                     help="Directory of the cloned lcevc-conformance-assets repo")
    parser.addoption("--arch", action="store", default=platform.machine(),
                     help="Include tests for specified architecture")
    parser.addoption("--regen-encodes", action="store_true",
                     default=False, help="Regenerate encode for test. Requires --erp-path.")
    parser.addoption("--regen-hashes", action="store_true",
                     default=False, help="Regenerate hashes for tests.")
    parser.addoption("--erp-path", action="store",
                     default=None, help="Path of the ERP executable, required for --regen-encode flag")
    parser.addoption("--ltm-decoder-path", action="store",
                     default=None, help="Path to the LTM ModelDecoder executable, required GPU for conformance tests")
    parser.addoption("--adb-path", action="store",
                     default=None, help="Path to the ADB executable, specifying also enables android testing")
    parser.addoption("--ffmpeg", action="store_true", default=False,
                     help="Enable ffmpeg func tests")
    parser.addoption("--conan-profile", action="store", default="default",
                     help="Conan profile to use when installing packages")
    parser.addoption("--conan-options", default=[], action="store", nargs='+',
                     help="Conan options to use when installing packages")
    parser.addoption("--conan-settings", default=[], action="store", nargs='+',
                     help="Conan settings to use when installing packages")
    parser.addoption("--ffmpeg-pkg", action="store", default='auto',
                     help="FFmpeg conan package to use for tests")
    parser.addoption("--ffmpeg-build", action="store", default=None,
                     help="Path to build of FFmpeg to use for tests")
    parser.addoption("--conan-update", action="store_true", default=False,
                     help="Set to force conan checking for newer versions / revisions of packages. This greatly "
                          "slows down the conan install process but is sometimes required if there are "
                          "package updates in remote (which is rarely the case).")
    parser.addoption("--benchmark-runs", default=3, type=int,
                     help="Set number of benchmark runs to perform for "
                          "performance tests")
    parser.addoption("--warmup-duration", default=2, type=int,
                     help="Set benchmark warmup duration (seconds)")
    parser.addoption("--benchmark-duration", default=5, type=int,
                     help="Set total time duration of benchmark run for"
                          "performance tests (seconds) (including warmup)")
    parser.addoption("--csv-output", default=None, type=str,
                     help="Specify CSV output filename")
    parser.addoption("--suite", default="full", type=str,
                     help="Specify test suite to run: [full, merge_request]")


# We need to raise usage errors here instead of making the options required as
# a workaround for pytest-xdist. Otherwise the workers fail thinking that the
# required options haven't been set.
@pytest.hookimpl(trylast=True)
def pytest_configure(config):
    if config.getoption("--bin-dir") is None:
        raise pytest.UsageError("Must provide --bin-dir")

    if config.getoption("--resource-dir") is None:
        raise pytest.UsageError("Must provide --resource-dir")


def pytest_collection_modifyitems(session, config, items):
    def runnable_on_arch(test):
        test_arch = [arg for mark in test.iter_markers(
            name="arch") for arg in mark.args]
        return not test_arch or config.getoption("--arch") in test_arch

    def disabled_without_ffmpeg(test):
        ffmpeg_mark = len(
            list(filter(lambda mark: mark.name == 'ffmpeg', test.iter_markers()))) > 0
        if not ffmpeg_mark:
            return False
        return not config.getoption("--ffmpeg")

    def disabled_by_suite(test):
        suite_marks = [arg for mark in test.iter_markers(
            name="suite") for arg in mark.args]
        if len(suite_marks) == 0:
            suite_marks = ['full', 'merge_request']
        return config.getoption("--suite") not in suite_marks

    items[:] = [item for item in items if runnable_on_arch(
        item) and not disabled_without_ffmpeg(item) and not disabled_by_suite(item)]


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    report = outcome.get_result()

    if report.when == "call" and report.outcome == "passed":
        if "tmpdir" in item.funcargs:
            tmpdir = item.funcargs['tmpdir']

            if tmpdir.exists():
                tmpdir.remove()


@pytest.fixture(scope="session")
def bin_dir(request):
    return abspath(request.config.getoption("--bin-dir"))


@pytest.fixture(scope="session")
def resource_dir(request):
    return abspath(request.config.getoption("--resource-dir"))


@pytest.fixture(scope="session")
def erp_path(request):
    return abspath(request.config.getoption("--erp-path"))


@pytest.fixture(scope="session")
def ltm_decoder_path(request):
    if os.environ.get('LTM_DECODER_PATH'):
        return os.environ.get('LTM_DECODER_PATH')
    else:
        return abspath(request.config.getoption("--ltm-decoder-path"))


@pytest.fixture(scope="session", autouse=True)
def adb_path(request):
    if os.environ.get('ADB_PATH'):
        adb_path = os.environ.get('ADB_PATH')
    else:
        adb_path = abspath(request.config.getoption("--adb-path"))
    if adb_path:
        bin_dir = request.config.getoption("--bin-dir")
        resource_dir = request.config.getoption("--resource-dir")
        data_dir = os.environ.get('LCEVC_TEST_DATA_DIR') if os.environ.get('LCEVC_TEST_DATA_DIR') else request.config.getoption("--data-dir")
        initialise_adb_device(adb_path, bin_dir, resource_dir, data_dir)

    return adb_path


def initialise_adb_device(adb_path, bin_dir, resource_dir, data_dir):
    process = run_adb(adb_path, ['devices'])
    assert process.returncode == 0, f"Cannot connect to ADB client at {ADBRunner.ABD_SERVER_HOST}: {process.stderr}"
    process_stdout = process.stdout.decode("utf-8")
    assert len(process_stdout.split('\n')) >= 4, f"Cannot find a single device connected to " \
                                                 f"{ADBRunner.ABD_SERVER_HOST}, device list: {process_stdout}"
    # Setup clean workdir
    run_adb(adb_path, ['root'], assert_rc=True)
    # process = run_adb(adb_path, ['shell', 'mount /dev/block/nvme0n1p2 /mnt/nvme'])
    # err = process.stderr.decode('utf-8')
    # if process.returncode != 0 and "need -t" not in err:
    #     raise AssertionError(f"Could not mount NVMe drive to android device {process.returncode}: {err}")
    run_adb(adb_path, ['shell', f'rm -rf {ADBRunner.BASE_WORKDIR} && mkdir {ADBRunner.BASE_WORKDIR}'], assert_rc=True)
    run_adb(adb_path, ['shell', f'rm -rf {ADBRunner.BUILD_DIR} && mkdir {ADBRunner.BUILD_DIR}'], assert_rc=True)
    # Push executables and libs
    host_build_path = os.path.dirname(bin_dir)
    if host_build_path.endswith('bin'):
        host_build_path = os.path.dirname(host_build_path)
    host_bin_dir = os.path.join(host_build_path, 'bin')
    host_lib_dir = os.path.join(host_build_path, 'lib')
    run_adb(adb_path, ['push', host_bin_dir, ADBRunner.BUILD_DIR], assert_rc=True)
    run_adb(adb_path, ['push', host_lib_dir, ADBRunner.BUILD_DIR], assert_rc=True)
    run_adb(adb_path, ['shell', f'chmod +x {ADBRunner.BUILD_DIR}/bin/*'])
    # Push assets with --sync flag
    run_adb(adb_path, ['push', '--sync', resource_dir, ADBRunner.ASSETS_DIR], assert_rc=True, timeout=7200)
    run_adb(adb_path, ['push', '--sync', data_dir, ADBRunner.ASSETS_DIR], assert_rc=True, timeout=3600)


def run_adb(adb_path, cmd, assert_rc=False, timeout=60):
    formatted_cmd = [adb_path, '-H', ADBRunner.ABD_SERVER_HOST, '-s', '30e4a0c7'] + cmd
    try:
        process = subprocess.run(formatted_cmd, timeout=timeout, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
    except subprocess.TimeoutExpired as e:
        raise TimeoutError(f"ADB setup command '{cmd}' did not exit within {timeout}s: {e}")
    if assert_rc:
        assert process.returncode == 0,\
            f"ADB command '{cmd}' returned {process.returncode}: {process.stderr.decode('utf-8')}"
    return process


@pytest.fixture
def validator_path(bin_dir):
    return get_executable(bin_dir, "lcevc_validator")


@pytest.fixture(scope="session")
def conan_profile(request):
    return request.config.getoption("--conan-profile")


def conan_args_list_to_dict(args_list):
    options_dict = {}
    for o in args_list:
        opt, v = tuple(o.split('='))
        options_dict[opt] = v
    return options_dict


def conan_args_dict_to_list(args_dict):
    return list(map(lambda o: f'{o[0]}={o[1]}', args_dict.items()))


@pytest.fixture(scope="session")
def conan_options(request):
    options_list = request.config.getoption("--conan-options")
    return conan_args_list_to_dict(options_list)


@pytest.fixture(scope="session")
def conan_settings(request):
    options_list = request.config.getoption("--conan-settings")
    return conan_args_list_to_dict(options_list)


@pytest.fixture(scope="session")
def conan_update_enabled(request):
    return request.config.getoption("--conan-update")


@pytest.fixture(scope="session")
def ffmpeg_package(request):
    ffmpeg_build = request.config.getoption("--ffmpeg-build")
    if ffmpeg_build is not None:
        return None

    res = request.config.getoption("--ffmpeg-pkg")
    if res == 'auto' and ffmpeg_build is None:
        version_json_path = os.path.join(
            os.path.dirname(__file__), '..', '..', 'version.json')
        if not os.path.exists(version_json_path):
            raise RuntimeError(
                f"Can't find version.json to lookup ffmpeg version, tried {version_json_path}")
        with open(version_json_path, 'rt') as version_json_file:
            version_json = json.load(version_json_file)
        res = f"ffmpeg/{version_json['ffmpeg']}@"

    return res


@pytest.fixture(scope="session")
def ffmpeg_build(request):
    return request.config.getoption("--ffmpeg-build")


@pytest.fixture(scope="session")
def ffmpeg_env(bin_dir, ffmpeg_package, conan_profile, conan_options, conan_settings, tmp_path_factory, conan_update_enabled, worker_id, ffmpeg_build):
    if worker_id == "master":
        # not executing in with multiple workers, just run the fixture implementation
        return ffmpeg_env_impl(bin_dir, ffmpeg_package, conan_profile, conan_options, conan_settings, tmp_path_factory, conan_update_enabled, ffmpeg_build)

    from filelock import FileLock

    # Prevent the expensive conan install invocations etc. from being executed in every
    # worker process; whichever worker wins the race for the lock will write the resulting
    # environment data to json file and other workers will load this
    shared_tmp_dir = tmp_path_factory.getbasetemp().parent
    data_file = shared_tmp_dir / "ffmpeg_env.json"
    lock_file = f'{data_file}.lock'

    with FileLock(lock_file):
        if data_file.exists():
            data = json.loads(data_file.read_text())
        else:
            data = ffmpeg_env_impl(
                bin_dir, ffmpeg_package, conan_profile, conan_options, conan_settings, tmp_path_factory, conan_update_enabled, ffmpeg_build)
            data_file.write_text(json.dumps(data))

    return data


def ffmpeg_env_impl(bin_dir, ffmpeg_package, conan_profile, conan_options, conan_settings, tmp_path_factory, conan_update_enabled, ffmpeg_build):
    if not ffmpeg_build:
        install_path = tmp_path_factory.getbasetemp()
        bin_paths, lib_paths, framework_paths = ffmpeg_env_conan_impl(
            install_path, bin_dir, ffmpeg_package, conan_profile, conan_options, conan_settings, conan_update_enabled)
        ffmpeg_bindir = os.path.join(install_path, 'ffmpeg', 'bin')
    else:
        install_path = ffmpeg_build
        bin_paths, lib_paths, framework_paths = ffmpeg_env_local_impl(
            ffmpeg_build)
        ffmpeg_bindir = ffmpeg_build

    bin_paths.append(bin_dir)
    lib_paths.append(bin_dir)

    lib_paths_key = 'DYLD_LIBRARY_PATH' if platform.system() == "Windows" else 'LD_LIBRARY_PATH'
    ffmpeg_env = {
        'PATH': os.pathsep.join(bin_paths),
        lib_paths_key: os.pathsep.join(lib_paths)
    }

    if framework_paths:
        ffmpeg_env['DYLD_FRAMEWORK_PATH'] = os.pathsep.join(framework_paths)

    if platform.system() == "Windows":
        all_dlls = list(Path(bin_dir).rglob('*.dll')) + \
            list(Path(install_path).rglob('*.dll'))
        print(f'all dlls: {all_dlls}')
        for dll_src in all_dlls:
            dll_basename = os.path.basename(dll_src)
            dll_dst = os.path.join(ffmpeg_bindir, dll_basename)
            if str(dll_src) == str(dll_dst):
                continue
            print(
                f'Copying {dll_basename} from bin_dir to FFmpeg binary dir: {ffmpeg_bindir}')
            try:
                shutil.copy(dll_src, dll_dst)
            except shutil.SameFileError:
                pass

    print(f'FFmpeg env: {ffmpeg_env}')
    return ffmpeg_env


def ffmpeg_env_conan_impl(conan_install_path, bin_dir, ffmpeg_package, conan_profile, conan_options, conan_settings, conan_update_enabled):
    global conan_api
    global ConanFileReference

    print(
        f'Using FFmpeg from conan: {ffmpeg_package} options: {conan_options} settings: {conan_settings} update: {conan_update_enabled}')

    if not conan_api:
        from conans.client.conan_api import ConanAPIV1 as conan_api
    if not ConanFileReference:
        from conans.model.ref import ConanFileReference

    conan, _, _ = conan_api.factory()
    conan.create_app()

    if 'license' not in conan_options:
        conan_options['license'] = 'gpl'
    if 'pthread-win32:shared' not in conan_options:
        conan_options['pthread-win32:shared'] = 'True'

    build = None
    if 'cuvid' in conan_options and conan_options['cuvid'] == 'True':
        build = ['cuda']

    conan_options_list = conan_args_dict_to_list(conan_options)
    conan_settings_list = conan_args_dict_to_list(conan_settings)

    conan_info = conan.install_reference(
        reference=ConanFileReference.loads(ffmpeg_package),
        profile_names=[conan_profile],
        options=conan_options_list,
        settings=conan_settings_list,
        generators=['deploy'],
        install_folder=str(conan_install_path),
        update=conan_update_enabled,
        remote_name='vnova-conan',
        build=build)

    lib_paths = []
    bin_paths = []
    framework_paths = []

    for package_info in conan_info['installed']:
        for package in package_info['packages']:
            cpp_info = package['cpp_info']
            package_root_path = cpp_info['rootpath']
            path_tokens = package_root_path.split(os.sep)
            for idx in range(len(path_tokens)):
                if path_tokens[idx] == cpp_info['version']:
                    package_name = path_tokens[idx - 1]
                    break
            else:
                if 'name' in cpp_info:
                    package_name = cpp_info['name']
                else:
                    if 'srt' in cpp_info['libs']:
                        package_name = 'srt'
                    elif 'harfbuzz' in cpp_info['libs']:
                        package_name = 'harfbuzz'
                    else:
                        raise RuntimeError(
                            f'Unable to find package name in package: {package} cpp_info: {cpp_info}')
            package_path = os.path.join(conan_install_path, package_name)
            bin_paths += list(map(lambda d: os.path.join(package_path,
                              d), cpp_info['bindirs']))
            lib_paths += list(map(lambda d: os.path.join(package_path,
                              d), cpp_info['libdirs']))

    return bin_paths, lib_paths, framework_paths


FFMPEG_LIB_DIRS = ['libavcodec', 'libavdevice', 'libavfilter', 'libavformat',
                   'libavresample', 'libavutil', 'libpostproc', 'libswresample', 'libswscale']


def ffmpeg_env_local_impl(ffmpeg_build):
    print(f'Using local build of FFmpeg: {ffmpeg_build}')

    bin_paths = [ffmpeg_build]
    lib_paths = list(map(lambda d: os.path.join(
        ffmpeg_build, d), FFMPEG_LIB_DIRS))
    framework_paths = []
    return bin_paths, lib_paths, framework_paths


@pytest.fixture
def dil_sample_path(bin_dir):
    return get_executable(bin_dir, "lcevc_dec_sample")


@pytest.fixture
def dec_harness_path(bin_dir):
    return get_executable(bin_dir, "lcevc_core_test_harness")


@pytest.fixture
def dil_harness_path(bin_dir):
    return get_executable(bin_dir, "lcevc_test_harness")


@pytest.fixture
def drp_playground_path(bin_dir):
    return get_executable(bin_dir, "lcevc_dec_playground")


@pytest.fixture
def eilp_validator_path(bin_dir):
    return get_executable(bin_dir, "lcevc_eilp_validator")


@pytest.fixture
def validator_path(bin_dir):
    return get_executable(bin_dir, "lcevc_validator")


@pytest.fixture(scope="session")
def regen_encode(request):
    return request.config.getoption("--regen-encodes")

@pytest.fixture(scope="session")
def regen_hash(request):
    return request.config.getoption("--regen-hashes")


@pytest.fixture(scope="session")
def resource(resource_dir):
    from util.finders import ResourceFinder
    return ResourceFinder(resource_dir)


@pytest.fixture(scope="module")
def data(request):
    from util.finders import DataFinder
    return DataFinder(request)


@pytest.fixture
def yuv(request, resource):
    return resource.get_path(request.param)


@pytest.fixture
def base(request, resource):
    return resource.get_path(request.param)


@pytest.fixture
def eilp_validator(eilp_validator_path, tmpdir, record_property):
    from util.runner import EilpValidatorRunner
    return EilpValidatorRunner(eilp_validator_path, tmpdir, record_property)


@pytest.fixture(scope="module")
def test_data(request, regen_encode, regen_hash):
    from util.finders import TestData
    data = TestData(request)

    yield data

    if regen_encode or regen_hash:
        if request.session.testsfailed > 0:
            pytest.exit("Some tests failed during regen, not overwriting test data")
            return

        data.save()


@pytest.fixture(scope="session")
def benchmark_runs(request):
    return request.config.getoption("--benchmark-runs")


@pytest.fixture(scope="session")
def warmup_duration(request):
    return request.config.getoption("--warmup-duration")


@pytest.fixture(scope="session")
def benchmark_duration(request):
    return request.config.getoption("--benchmark-duration")


def get_git_version():
    try:
        version = subprocess.check_output(['git', 'describe']).decode()
        print(f"Package verision from 'git describe': {version}")
    except Exception as err:
        if os.path.exists('package_info.txt'):
            with open('package_info.txt', 'rt') as package_info_file:
                package_version_prefix = 'Package version:'
                for line in package_info_file.read().strip().split('\n'):
                    if line.startswith(package_version_prefix):
                        version = line.replace(
                            package_version_prefix, '').rstrip()
                        print(
                            f'Package verision from package_info.txt: {version}')
                        return version
                raise RuntimeError(
                    f'Unable to find package version in package info file')
        else:
            print(f"No package_info.txt file found, unable to identify package version")
            raise err


@pytest.fixture(scope="session")
def csv_report(request):
    csv_report = CsvReport(path=request.config.getoption("--csv-output"))

    today = datetime.now()
    csv_report.add_header(['Run date', today.strftime("%Y-%m-%d %H:%M:%S")])
    csv_report.add_header(['Git version', get_git_version()])

    csv_report.add_header(
        ['Benchmark duration', request.config.getoption("--benchmark-duration")])
    csv_report.add_header(
        ['Warmup duration', request.config.getoption("--warmup-duration")])
    csv_report.add_header(
        ['Benchmark runs', request.config.getoption("--benchmark-runs")])

    csv_report.add_header([''])

    yield csv_report
    csv_report.dump()
