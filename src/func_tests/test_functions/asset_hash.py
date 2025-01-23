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
import json
import hashlib

from utilities.paths import get_executable
from utilities.runner import get_runner, ADBRunner
from utilities.test_runner import BaseTest
from utilities.assets import get_asset
from utilities.config import config, logger


class Test(BaseTest):
    def test(self, test, test_dir):
        hash_file = 'hashes.json'
        executable = test['meta']['executable']
        hash_flag = test['meta']['filepath_hash']
        stream_path = get_asset(test['meta']['asset_path'])
        hash_output_key = test['meta'].get('json_hash_key', 'high')

        runner = get_runner(get_executable(executable), test_dir)
        if 'sample' in executable:
            config_file = 'config.json'
            hash_config = dict()
            hash_config[hash_flag] = hash_file
            with open(os.path.join(test_dir, config_file), "w") as jf:
                json.dump(hash_config, jf)
            runner.set_positional_arg(0, stream_path, path=True)
            runner.set_positional_arg(1, 'dump.yuv')
            runner.set_positional_arg(2, config_file)
        else:
            runner.set_param('--input', stream_path, path=True)
            runner.set_param(hash_flag, hash_file)

        self.record_cmd(f'{executable}', runner.get_command_line())
        logger.debug(f"Running {executable}: {runner.get_command_line()}")
        runner.run(assert_rc=True)

        if isinstance(runner, ADBRunner):  # Stop the android devices filling up with YUVs
            runner.run_adb(['shell', f'rm {runner.workdir}/dump.yuv'])

        if executable == 'lcevc_dec_sample':
            with open(os.path.join(test_dir, 'dump.yuv'), "rb") as yuv_dump:
                generated_hashes = dict(hash=hashlib.md5(yuv_dump.read()).hexdigest())
        else:
            with open(os.path.join(test_dir, hash_file), 'r') as json_file:
                generated_hashes = json.load(json_file)

        if config.getboolean('REGEN', False):
            assert generated_hashes.get(
                hash_output_key), "No 'Output' hash in hash-file from sample executable"
            self.record_regenerated_hash('hash', generated_hashes[hash_output_key])
        else:
            assert test['meta'].get('hash_hash') == generated_hashes.get(hash_output_key), \
                "Hash does not match"
