# LCEVC SDK functional tests

Please read this document thoroughly to understand how to run the functional test suite and what
sorts of tests it performs.

## Usage

These tests are powered by pytest. In order to be able to run them you must have
at least Python 3.7.

To install pytest and the various plugins run the following:

    python3.7 -m pip install -r requirements.txt

You can then execute ALL the tests using the following command:

    pytest src/func_tests/ --bin-dir <path to build binaries> --resource-dir <path to folder containing yuvs>

If you want to run a subset of the tests you can point pytest at the specific
python file, e.g:

    pytest src/func_tests/tests/test_recons.py ....

Some of these tests are slow to perform, such as the performance test. As such we
have marked these tests as slow allowing they can be omitted with the `-k` option:

    pytest ... -k "not slow"

We are also using the xdist plugin which allows you to run the tests in parallel
using the `-n <numprocesses>` option:

    pytest ... -n 8

**Temporarily**, the perseus bin files required for some of the tests are contained in a
separate repository, lcevc_dec_assets - you can specify the path to the func_tests/ directory inside
that repository with the `LCEVC_TEST_DATA_DIR` environment variable, or by passing
`--test_data_dir=...` on the pytest commandline. If neitehr is specified, pytest will automatically
try to use a directory named `lcevc_dec_assets/func_tests` on the same directory level as this repository.

If you'd like to execute the tests in the same way that they're executed on CI, you can also use the
provided `ctest` configuration. From your install directory, `ctest -C Release --output-on-failure`

For more detailed instructions on pytest do `pytest --help` or visit the [pytest
website](https://pytest.org)

---

## Test Groups
### Major

These are the major "groups" of tests that all engineers should be familiar with during
their day to day activities. These are outlined below

#### dpi_regression

* **NOTE**: Due to dependency on the encoder, tests that use --regen are currently disabled in the
  lcevc_dec repository. These tests will still be run in CI on the lcevc repo.
* Uses --regen to regenerate test data and hashes for the decoder, this invokes the encoder with
  several configuration permutations over a very short number of frames and stores them in the
  func_tests folder structure - this runs the encoder using the base_yuv EIL plugin.
* You must run regen when you change the end to end behaviour of the system - you then must run
  a rebase since your hashes will be garbage.

#### dil_regression

* **NOTE**: Due to dependency on the encoder, tests that use --regen are currently disabled in the
  lcevc_dec repository. These tests will still be run in CI on the lcevc repo.
* Uses --regen to regenerate test data and hashes for the decoder, this invokes the encoder with
  several configuration permutations over a very short number of frames and stores them in the
  func_tests folder structure - this runs the encoder using the base_yuv EIL plugin.
* You must run regen when you change the end to end behaviour of the system - you then must run
  a rebase since your hashes will be garbage.

#### dil_diff

* It compares the output of the DIL run with different settings. It fails if the difference between
  any pixel is greater than 3.

#### dil_dither

* Takes a .ts file containing a stream with dithering. Said stream will live in the folder pointed 
  by --resource-dir.
* It uses the dec_playground to extract a base and a binary file containing the lcevc data. These 
  2 files are then fed to the dil_harness that does a comparison between the output of a DIL 
  instance with dither off and another with dither on. To pass the test the maximum difference
  between the 2 outputs should be +/- dither strength.

#### dil_conformance

* It compares an MD5 generated from a stream decoded by the LTM to an MD5 generated from the same 
  stream decoded using the DIL. The stream and the file containing the MD5 should be contained 
  inside the folder pointed by --resource-dir, which is the only argument needed to run these tests.
* At the moment some of these tests are disabled since they don't yield positive results.

#### dpi_conformance

* Similar to dil_conformance above except uses dec_harness. The inputs to the dec_harness are the base_yuv and
  raw LCEVC data extracted from the conformance bitstream .lvc file to a bin file using the validator. MD5 hash
  of dec_harness and LTM are compared for conformance.

---

## Misc 

We have customized pytest with a few things to make it better suited for our needs 

### Tracebacks

By default pytest prints a stacktrace to where an assertion occurs. This isn't very useful in our
case and causes unnecessary noise so it has been disabled by `--tb=no` in the `pytest.ini`. This can
however make it hard to debug tests during their development, so whilst developing test you may want
to override it by adding `--tb=auto` to your command line.

### Temporary Data 

We use pytest's built in [tmpdir](https://docs.pytest.org/en/latest/tmpdir.html) fixture for
creating working directories for temporary data. As we're often working with raw YUVs the tests can
easily generate gigabytes of data, for this reason we have made the modification that if a test
passes its temporary data will be automatically deleted.

There's currently no way to toggle this behaviour so if, for some reason, you wish to disable this
look at `pytest_runtest_makereport` in `conftest.py`.

### Reporting

We have a custom reporting plugin to change the behaviour of the output. This makes the tests
display all of the properties recorded with the 
[record_property](https://docs.pytest.org/en/latest/usage.html#record-property-example) fixture.

By default the properties are only displays for failing tests. You can control this behaviour with
the `--print-properties` command line with one of the following values:

* `never` - never displays the properties regardless of test outcome
* `always` - always displays the proeprties regardless of test outcome
* `failed` - the default

### Encode/Decode Parameters

Under utility we have a special file for storing encode/decode parameters, these not only provide
easy lists of all options for some parameters but also give them pretty names so it's easy to
identify tests by their names (and filter for subsets of tests). You should prefer to use these for
your parameterization of tests.
