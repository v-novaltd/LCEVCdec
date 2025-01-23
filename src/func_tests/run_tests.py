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
import time
import signal
import platform
import multiprocessing as mp

import test_functions
from utilities.adb import initialise_adb_device
from utilities.assets import get_bases, herp_is_accessible, download_assets_externally
from utilities.config import config, logger
from utilities.load_tests import load_tests
from utilities.ltm import initialise_ltm
from utilities.paths import initialise_tempdirs, get_tempdir
from utilities.results import dump_results, save_regenerated_hashes
from utilities.runner import ADB_PLATFORMS


def setup_tests_get_threads(test_definitions):
    if not any([test['enabled'] for test in test_definitions]):
        logger.info("No tests are enabled, exiting with success")
        quit(0)
    if config.getboolean('REGEN'):
        initialise_ltm()
    initialise_tempdirs()
    assert os.path.exists(config.get('BIN_DIR')), \
        f"Specified binary dir '{config.get('BIN_DIR')}' doesn't exist"
    if config.getboolean('ENABLE_VALGRIND'):
        assert platform.system() == 'Linux', "Valgrind option only works on Linux"
    if not herp_is_accessible():
        download_assets_externally()
        return True
    else:
        get_bases(test_definitions)
    threads = min(config.getint('THREADS', 1), mp.cpu_count())
    if config.get('PLATFORM') in ADB_PLATFORMS:
        assert not config.getboolean('ENABLE_VALGRIND'), \
            "Valgrind option is not available on Android tests"
        initialise_adb_device()
        if threads > 1:
            threads = 1
            logger.warning("An Android platform is selected so tests are forced single threaded")
    return threads


def main():
    # Setup
    start_time = time.perf_counter()
    test_definitions = load_tests(config.get('TEST_DEFINITIONS_DIR'))
    threads = setup_tests_get_threads(test_definitions)
    # Run
    if threads > 1:
        pool = mp.Pool(threads, initializer=worker_initializer)
        logger.info(f"Running tests on {threads} threads")
        test_threads = list()
    test_results = list()
    for test in test_definitions:
        if threads > 1:
            test_threads.append(pool.apply_async(test_wrapper, args=(test,)))
        else:
            try:
                test_results.append(test_wrapper(test))
            except KeyboardInterrupt:
                break
    if threads > 1:
        pool.close()
        try:
            test_results = [test.get() for test in test_threads]
        except KeyboardInterrupt:
            logger.critical("Stopping all tests from CTRL+C")
            pool.terminate()
            test_results = [test.get() for test in test_threads if test.ready()][:-threads]
    # Aggregate results
    count_results = test_results
    if filter_group := os.environ.get('FILTER_GROUP', config.get('FILTER_GROUP')):
        count_results = [result for result in count_results if result['group'] == filter_group]
    if filter_name := os.environ.get('FILTER_NAME', config.get('FILTER_NAME')):
        count_results = [result for result in count_results if filter_name in result['name']]
    num_passed = len([test for test in count_results if test['enabled']
                     and test['results'].get('pass')])
    num_disabled = len([test for test in count_results if not test['enabled']
                       or test['results'].get('ignored')])
    num_skipped = len([test for test in count_results if test['enabled_type']
                      == 'skip' or test.get('results', {}).get('ignored')])
    num_broken = len([test for test in count_results if test['enabled_type'] == 'broken'])
    overall_pass = (num_passed == (len(count_results) - num_disabled))
    logger.info(f"Passed {num_passed}/{len(count_results) - num_disabled} tests in "
                f"{time.perf_counter() - start_time:.2f}s (skipped {num_skipped}, broken {num_broken}), "
                f"overall result: {'PASS' if overall_pass else 'FAILED'}")
    if config.getboolean('REGEN', False):
        save_regenerated_hashes(test_results, config.get('TEST_DEFINITIONS_DIR'))
    dump_results(test_results)
    quit(0 if overall_pass else 1)


def test_wrapper(test):
    test_dir = get_tempdir(test.get('id'), make_dir=test['enabled'])
    test_class = getattr(test_functions, test['function']).Test
    test_runner = test_class(test=test, test_dir=test_dir)
    return test_runner.run()


def worker_initializer():
    """Ignore CTRL+C in the worker process - allows all tests to be stopped quickly with CTRL+C"""
    signal.signal(signal.SIGINT, signal.SIG_IGN)


if __name__ == '__main__':
    main()
