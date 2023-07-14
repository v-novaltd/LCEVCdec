import glob
import json
import os
from typing import List

import py
import pytest
from _pytest.fixtures import FixtureRequest
from util.helpers import get_test_name, get_unique_name


class ResourceFinder(object):
    def __init__(self, directory):
        self._directory = directory
        self._cache = {}

    def get_path(self, name, use_cache=True):
        if name is None:
            return None

        if use_cache and name in self._cache:
            return self._cache[name]

        pattern = os.path.join(self._directory, "**", name)

        for result in glob.iglob(pattern, recursive=True):
            if use_cache:
                self._cache[name] = result

            return result

        pytest.fail(f'Unable to find resource \"{pattern}\"')


class DataFinder(object):
    def __init__(self, obj):
        module = obj.module.__name__

        # Get the path from the commandline, then the environment. If neither is present, guess.
        data_path = obj.config.getoption("--data-dir")

        if not data_path:
            data_path = os.environ.get('LCEVC_TEST_DATA_DIR')

        if not data_path:
            data_path = os.path.join(os.path.dirname(__file__), "../../../../lcevc_dec_assets/func_tests")
        
        p = os.path.expanduser(data_path)
        p = os.path.expandvars(p)
        p = os.path.abspath(p)
        self._module_directory = os.path.join(p, module)
        self._yuv_directory = os.path.join(p, "yuv")

    def get_path(self, name, ignore_exist=False) -> py._path.local.LocalPath:
        return self._get_data(os.path.join(self._module_directory, name), ignore_exist)

    def get_yuv(self, name) -> py._path.local.LocalPath:
        return self._get_data(os.path.join(self._yuv_directory, name), False)

    def _get_data(self, path, ignore_exist) -> py._path.local.LocalPath:
        if ignore_exist or os.path.exists(path):
            return py._path.local.LocalPath(path, True)

        pytest.fail(f'Unable to find data file: "{path}"')


class TestData:
    def __init__(self, request: FixtureRequest, test_name=None):
        self._finder = DataFinder(request)
        self._data = {}
        self._test_name = test_name

    def get_data(self, request: FixtureRequest, full_regen: bool, blacklist: List[str] = [], test_name: str = None):
        if not test_name:
            test_name = get_test_name(request)
        name = f'{test_name}.json'
        path = self._finder.get_path(name, ignore_exist=full_regen)
        test_key = str(path)

        # Test_key will typically be a file in json format, with mappings from test names to
        # dictionaries of an encoded .bin file and a decode hash.
        if test_key not in self._data:
            if full_regen:
                self._data[test_key] = {}
            else:
                self._data[test_key] = json.loads(path.read())

        data = self._data[test_key]
        return data

    def get(self, request: FixtureRequest, full_regen: bool, blacklist: List[str] = [], test_name: str = None):
        data = self.get_data(request, full_regen, blacklist, test_name)
        hashes_key = get_unique_name(request, blacklist)

        # Hashes_key will typically be the name of a particular test (including params)
        if hashes_key not in data:
            if full_regen:
                data[hashes_key] = {}
            else:
                pytest.fail(f"No hashes for {hashes_key}")

        return data[hashes_key]

    def save(self):
        for path, contents in self._data.items():
            with open(path, "w") as f:
                json.dump(contents, f, indent=4)


class ConformanceAssets:
    def __init__(self, request: FixtureRequest, bitstream_prefix):
        self.bitstream_prefix = bitstream_prefix

        asset_root = request.config.getoption("--conformance-assets-dir")
        if not asset_root:
            asset_root = os.environ.get('LCEVC_CONFORMANCE_ASSETS_DIR')
        if not asset_root:
            asset_root = os.path.join(os.path.dirname(__file__), "../../../../lcevc-conformance-assets")
        assert asset_root, "Cannot locate lcevc-conformace-assets directory, set with --conformance-assets-dir"
        self.asset_root = os.path.abspath(asset_root)
        with open("test_name", 'w') as f:
            f.write(self.bitstream_prefix)

    def get_bitstream(self):
        bitstream_path = glob.glob(os.path.join(self.asset_root, '*', f"{self.bitstream_prefix}.bit"))
        assert bitstream_path, f"Cannot find conformance bitstream for {self.bitstream_prefix} in {self.asset_root}"
        return os.path.abspath(bitstream_path[0])

    def get_config(self):
        config_path = glob.glob(os.path.join(self.asset_root, '*', f"{self.bitstream_prefix}.cfg"))
        assert config_path, f"Cannot find conformance JSON config for {self.bitstream_prefix} in {self.asset_root}"
        return os.path.abspath(config_path[0])
