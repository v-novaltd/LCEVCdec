# LCEVCdec Functional Tests
The functional tests are designed to give coverage of the high level features of the LCEVC decoder. This is achieved
with a range of test cases defined within CSV files to parameterize the encoder (ERP) and the decoder using the
lcevc_dec_test_harness to interface with the library. Some tests also run using the lcevc_dec_sample for coverage.

## Basic usage
You will need to build LCEVCdec with `VN_SDK_EXECUTABLES=ON` before running the tests. Additionally, there is no
command-line interface for the tests; they use `config.ini` instead. Therefore, you'll need to set `BIN_DIR` to the
location of your binaries, in that file.

Then, run the full default test suite with
```shell
cd lcevc_dec
pip install -r src/func_tests/test_requirements.txt
python3 src/func_tests/run_tests.py
```

## Test definitions
Tests are defined in CSVs within the `test_definitions` folder, each CSV maps to a test function and each test
function runs a different type of test. Each CSV defines the parameters of the test. Parameters are grouped into
encoder, decoder and 'meta' groups. Parameter groups also define the definition location, eg. CLI or JSON parameters.
Some columns of the CSV are mandatory:
* Test Function - the exact name of the python file within `test_functions` that the test is for
* Group - each test needs a simple group name for reporting and to identify the area it is testing
* Enabled-`<platform>` - set to either 'Nightly', 'MR' or 'FALSE' this parameter is for CI to know if the test should
be run for a given platform eg. `ubuntu`
* Name - it is recommended to give each test a unique name so it can be referenced individually.
If a name is not provided it will be auto generated

The remaining fields in the CSV definitions are grouped using a key, value syntax delimited by a colon, eg. `group:param`.
The groups are passed to the test function in a dictionary format and have no strict format but there are some examples
of parameters that are already in use:
* `meta` - used by the python test function itself, eg. `meta:hash` is used to compare as a reference hash
from hashing functions within the test_harness
* `cli` - command line parameters for the harness under test. These are directly passed through so `--` dashes
must be included correctly. For positional arguments, use a number e.g. `cli:0` (positional arguments always come first)
* `json` - these are formatted to a JSON string to be added to the command line
* `erp` - these parameters are for the ERP encoder via HERP, see below

## Test functions
Test functions are python files with a single class that inherits from the base class in
`utilities.test_runner.BaseTest`, the single method `test` must be populated. The `test` method takes two positional
arguments, the first is a dictionary containing all the details from the definition CSV and the second is a path to a
temporary directory where test-specific files can be written to. If adding a new test function file, ensure it is added
to `__init__.py`.

Test functions have no return value and will fail if any exceptions are raised within them. Several methods from
`BaseTest` can be used to record command lines, parameters, and extra results for output to the final results JSON. The
base class also has functionality for regenerating hashes for the hash tests when required.

## Test assets
The LCEVCdec test suite aims to pull *all* required test assets over HTTP at runtime from [HERP](http://herp/) - an HTTP
wrapper for the ERP. On the initial local run, asset downloading will take some time depending on your connection however
assets will be cached to the `CACHE_PATH` for any subsequent runs. From time to time you may want to clear your
local cache to save space and remove any assets that are no longer in use, this can be done by manually deleting the folder
or enabling `RESET_LOCAL_CACHE`. Assets are only downloaded for the tests that are enabled. The SDK version of the encoder
is set with `ENCODER_VERSION`, please use a hash from the [dashboard](http://dashboard/sdk_builds).

Please note that HERP is V-Nova's internal tool for generating and managing LCEVC encodes and it is not accessible to
external users. For external users a zip file of all the required test assets for a given version is downloadable so that
the tests can be run as they are, although development is limited as you cannot create any new test streams. The
`EXTERNAL_ASSET_URL` config param sets this URL and the tests will automatically download this file if it cannot reach
HERP. Two variants are available, `external` and `full` following the `External` and `cpu` platforms from the CSVs.
The external zip runs most tests with a ~3GB download and the full zip runs all tests with a ~20GB download - notably
including the performance group that requires some large base YUVs.

## Config
The test suite takes many config options, these are all defined in `config.ini` and the path to the config is set by an
environment variable `TEST_CONFIG` - when running tests locally, you may want to take a copy of the ini outside of the
repo so it is not overwritten by git. Each parameter in the test config can be overwritten using environment variables
of exactly the same name.

Description of config parameters (default parameters should work on Windows):
* `BIN_DIR` - path to your `/bin` directory within your build directory, differs by OS, can be locally referenced to the
top-level `lcevc_dec` dir
* `TEST_DEFINITIONS_DIR` - path to the CSV definitions, can be changed if you want to run a subset of tests from a
different folder
* `RESULTS_PATH` - file or path to dump the JSON results of the tests, JSON results have greater detail than the logs
* `THREADS` - number of threads to run tests in, android tests cannot be multithreaded, THREADS value cannot be greater
than your logical core count
* `WORKDIR` - path to root of temporary folders for test, this is cleared on each re-run
* `LOG_LEVEL` - set the python log level, setting to 'DEBUG' will show command lines in the logs


* `PLATFORM` - changes the test platform in the CSV to a different 'Enabled' column. Currently 'ubuntu' and
'Qualcomm_devkit' are supported. When running on Windows, please use the 'ubuntu' platform.
* `LEVEL` - usually taken from the Jenkins `BUILD_TYPE`, when set to 'MR', a limited test set is selected
* `FILTER_GROUP` - uncomment to filter by an exact group name as defined in the CSV 'Group' column
* `FILTER_NAME` - uncomment to filter by the substring of a test name, can be used in conjunction with `FILTER_GROUP`
* `DELETE_TEMP_DIR` - either 'ALWAYS', 'ON_PASS' or 'NEVER' - specifies whether temp folders for each test in `WORKDIR`
are removed after each test run, 'ON_PASS' will only keep folders of tests that fail. The `WORKDIR` will always be cleared
when tests are re-run regardless of this parameter
* `ENABLE_VALGRIND` - Run with Valgrind's memcheck tool using `error-exitcode` so that tests with memory leaks will fail
and the stderr captured by the test will contain valgrind's report


* `ADB_PATH` - path to the Android Debug Bridge executable, must be set when running android tests on Windows
* `ADB_SERIAL` - the serial number of the target device within ADB, must be set even when only one device is connected.
Can be a networked device.
* `ABD_SERVER_HOST` - the hostname of the node with the connected android device
* `DEVICE_BASE_DIR` - base location to copy assets, executables and run tests on the android device
* `DEVICE_BUILD_DIR` - optionally specify a separate location for executables as some devices don't allow running
executables from the SD card however internal storage may not be large enough for the assets


* `REGEN` - regenerates hashes and adds them back to the test definition CSVs, beware when committing updated hashes
* `CACHE_PATH` - path to store encoded assets locally, this folder can become large when certain tests are selected
* `ENCODER_VERSION` - an 8-character hash of an SDK build, please stick to release builds, caution when changing as the
HERP cache will have to be re-built
* `HERP_URL` - URL for herp - connection to the V-Nova network is required
* `RESET_LOCAL_CACHE` - deletes the contents of `ENCODE_CACHE_PATH` at the start of the tests
* `EXTERNAL_ASSET_URL` - URL for downloading test assets outside V-Nova


* `LTM_URL` - URL to the exact LTM build to download from the [Nexus](nexus.dev.v-nova.com), can use the Nexus API
features to download the latest rather than hardcode a specific hash - connection to the V-Nova network is required
* `NEXUS_USER` - Hardcode the user for authentication to the Nexus API, use 'PROMPT' for an interactive prompt at runtime
* `NEXUS_PASSWORD` - Hardcode the password for authentication to the Nexus API - not recommended to set in the ini file,
primarily for use via environment vars in CI, use 'PROMPT' for an interactive prompt at runtime
* `CACHE_LTM_DECODES` - Some tests decode reference streams with the LTM, these are usually very slow to decode so
caching them yields a performance improvement in certain cases. This does result in 100s of YUVs being left on the
disk so use with care

## Content attribution
The functional tests use a modified version of the 'ElFuenteTunnel' clip, which is part of Netflix's Open Content and is
licensed under the [Creative Commons Attribution 4.0 International License (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/).
The clip has been trimmed from frame 18061 and re-encoded. All rights are retained by [Netflix](https://opencontent.netflix.com/). The
performance test group uses the 'Performance' clip. Copyright © V-Nova International Limited 2025. All rights reserved.
