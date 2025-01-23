# Copyright (c) V-Nova International Limited 2024. All rights reserved.
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

from utilities.paths import format_path, get_executable
from utilities.runner import get_runner
from utilities.test_runner import BaseTest
from utilities.assets import get_encode
from utilities.config import config, logger


class Test(BaseTest):
    def test(self, test, test_dir):
        asset_root = os.path.join(config.get('CACHE_PATH'), 'assets')
        os.makedirs(asset_root, exist_ok=True)

        encode_path = get_encode(test['erp'])
        self.log_erp_command(encode_path)

        runner = get_runner(get_executable('lcevc_dec_test_harness'), test_dir)
        runner.set_params_from_config(test['cli'])
        runner.set_param('lcevc', encode_path, path=True)
        runner.set_param('base', format_path(test['cli']['--base']), path=True)
        self.record_cmd('dec_test_harness', runner.get_command_line())
        logger.debug(f"Running test harness: {runner.get_command_line()}")
        runner.run(assert_rc=True)
