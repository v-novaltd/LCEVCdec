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
import shutil
import multiprocessing as mp

from utilities.config import logger, config
from utilities.adb import run_adb


class BaseTest:
    def __init__(self, test, test_dir):
        self.test_definition = test
        self.test_id = test['id']
        self.test_dir = test_dir
        self.params = {key: value for key, value in test.items() if isinstance(value, dict)}
        self.results = dict()
        self.command_lines = dict()
        self.regenerated_hashes = dict()
        self.delete_test_dir = config.get('DELETE_TEMP_DIR')
        assert self.delete_test_dir in ('ALWAYS', 'ON_PASS', 'NEVER'), \
            "DELETE_TEMP_DIR config param must be one of 'ALWAYS', 'ON_PASS' or 'NEVER'"

    def run(self):
        start_time = time.perf_counter()
        if mp.current_process().name != "MainProcess":
            mp.current_process().name = f"Test-{self.test_id}"
        if self.test_definition['enabled']:
            logger.info(f"Running test ID {self.test_id} ({self.test_definition['name']})")
            try:
                self.test(self.test_definition, self.test_dir)
                self.record_result('pass', True)
                if self.delete_test_dir == 'ON_PASS':
                    shutil.rmtree(self.test_dir)
            except KeyboardInterrupt:
                logger.critical("Stopping tests from CTRL+C")
                raise KeyboardInterrupt
            except IgnoreFailure as e:
                logger.warning(f"Ignoring failure in test {self.test_id} "
                               f"({self.test_definition['name']}): {str(e)}")
                self.record_result('ignored', True)
            except Exception as e:
                logger.exception(e)
                logger.error(f"Test {self.test_id} ({self.test_definition['name']}) "
                             f"failed with exception")
                self.record_result('pass', False)
                self.record_result('exception', str(e))
            if self.delete_test_dir == 'ALWAYS':
                shutil.rmtree(self.test_dir)
        runtime = round(time.perf_counter() - start_time, 3)
        params = dict(runtime=runtime) if self.test_definition['enabled'] else dict()
        return self.get_result(**params)

    def test(self, test, test_dir):
        raise NotImplementedError(f"You must implement the 'test' function in 'Test' class within module "
                                  f"'{self.test_definition['function']}'")

    def record_param(self, param_name, param):
        if param_name in self.params:
            logger.warning(f"Overwriting existing param '{param_name}' in test ID {self.test_id}")
        self.params[param_name] = param

    def record_result(self, result_name, result):
        self.results[result_name] = result

    def record_cmd(self, cmd_name, command_line):
        self.command_lines[cmd_name] = command_line

    def record_regenerated_hash(self, hash_name, new_hash):
        self.regenerated_hashes[hash_name] = new_hash

    def log_erp_command(self, encode_path):  # Only logged when LOG_LEVEL = DEBUG in the config
        logger.debug(f"Using ERP encode: lcevc_erp {' '.join([f'{k} {v}' for k, v in self.test_definition['erp'].items()])}"
                     f" --output {os.path.basename(encode_path)}")

    def get_result(self, **kwargs):
        results = dict(id=self.test_id,
                       enabled=self.test_definition['enabled'],
                       enabled_type=self.test_definition['enabled_type'],
                       group=self.test_definition['group'],
                       name=self.test_definition['name'],
                       **kwargs,
                       params=self.params)
        if self.test_definition['enabled']:
            results['results'] = self.results
            results['command_lines'] = self.command_lines
        if self.test_definition['enabled_type'] == 'broken':
            results['broken_reason'] = self.test_definition['notes']
        if self.regenerated_hashes:
            results['regenerated_hashes'] = self.regenerated_hashes
            results['definition_csv'] = self.test_definition['definition_csv']
            results['csv_row'] = self.test_definition['csv_row']
        if self.test_definition.get("performance_group"):
            results['performance_group'] = self.test_definition["performance_group"]
        return results

    def get_device_temperature(self):
        proc_temp = run_adb(['shell', 'dumpsys battery | grep temperature'], assert_rc=True)
        output = proc_temp.stdout.decode('utf-8')
        temperature = float(output.split()[-1]) / 10
        logger.debug(f"Extracted device temperature:{temperature}C")
        return temperature

    def get_current_frequency(self):
        proc_freq = run_adb(
            ['shell', "cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq"], assert_rc=True)
        output = proc_freq.stdout.decode('utf-8')
        cpu_freq = [int(val) for val in output.split()]
        logger.debug(f"Extracted current clock speed:{cpu_freq}kHz")
        return cpu_freq


class IgnoreFailure(Exception):
    pass
