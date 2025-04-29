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

import json
import os

from utilities.assets import get_asset, get_compliance
from utilities.config import config, logger
from utilities.load_tests import csv_json_to_dict
from utilities.ltm import LOCAL_LTM_PATH, ltm_vs_harness_hash_match
from utilities.paths import format_path, get_executable
from utilities.runner import get_runner, ADBRunner
from utilities.test_runner import BaseTest


class Test(BaseTest):
    def test(self, test, test_dir):
        hash_file = 'hashes.json'
        asset_path = f"compliance_bin/{test['compliance']['id']}" \
            f"/{test['compliance']['content']}.bin"
        stream_path = get_asset(asset_path)

        runner = get_runner(get_executable('lcevc_dec_test_harness'), test_dir)
        runner.set_param('--lcevc', stream_path, path=True)
        runner.set_param('--base', format_path(test['cli']['--base']), path=True)
        runner.set_json_param('--configuration', csv_json_to_dict(test['json']))
        runner.set_param('--output-hash', hash_file)
        if config.getboolean('REGEN', False):
            runner.set_param('--output', "harness")
        self.record_cmd('harness', runner.get_command_line())
        logger.debug(f"Running API harness: {runner.get_command_line()}")
        runner.run(assert_rc=True)

        with open(os.path.join(test_dir, hash_file), 'r') as json_file:
            generated_hashes = json.load(json_file)

        if config.getboolean('REGEN', False):
            assert self.validate_regen(test, test_dir), \
                "Harness does not match LTM, cannot regenerate hash to this value"
            self.regenerated_hashes = generated_hashes
        else:
            for hash_name, regression_hash in test['meta'].items():
                if hash_name.startswith('hash') and regression_hash:
                    hash_name = hash_name.replace('hash_', '')
                    assert regression_hash == generated_hashes.get(hash_name), \
                        f"Harness {hash_name} hash does not match"

    def validate_regen(self, test, test_dir):
        es_path, stream_config_path = get_compliance(
            test['compliance']['version'], test['compliance']['id'], test['compliance']['content'])
        with open(stream_config_path, 'r') as json_file:
            stream_config = json.load(json_file)
        ltm_runner = get_runner(os.path.abspath(os.path.join(
            LOCAL_LTM_PATH, 'ModelDecoder')), test_dir, force_local=True)
        assert not isinstance(ltm_runner, ADBRunner), "Cannot regen compliance hashes on Android"
        ltm_runner.set_param('--input_file', es_path)
        if not stream_config['format'].startswith("yuv") or stream_config['format'].endswith("12") or \
                stream_config['format'].endswith("14"):
            ltm_runner.set_param('--base_external', 'true')
        ltm_runner.set_param('--base', stream_config['base_encoder'])
        ltm_runner.set_param('--encapsulation', 'nal')
        ltm_runner.set_param('--output_file', 'ltm.yuv')
        self.record_cmd('ltm', ltm_runner.get_command_line())
        ltm_runner.run(assert_rc=True)

        return ltm_vs_harness_hash_match(test_dir)
