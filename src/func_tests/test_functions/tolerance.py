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
import json

from utilities.assets import get_encode
from utilities.config import config, logger
from utilities.histogram import pixel_deviation_histogram
from utilities.load_tests import csv_json_to_dict
from utilities.ltm import ltm_generate_regen_image
from utilities.paths import get_executable
from utilities.runner import get_runner
from utilities.test_runner import BaseTest


class Test(BaseTest):
    def test(self, test, test_dir):
        hash_file = 'hashes.json'
        output_prefix = 'harness'
        json_config = csv_json_to_dict(test.get('json', {}))

        encode_path = get_encode(test['erp'])
        self.log_erp_command(encode_path)
        runner = get_runner(get_executable('lcevc_dec_test_harness'), test_dir)
        if json_config:
            runner.set_json_param('--configuration', json_config)
        runner.set_param('--input', encode_path, path=True)
        if config.getboolean('REGEN', False):
            runner.set_param('--output', output_prefix, path=True)
        runner.set_param('--output-hash', hash_file)
        self.record_cmd('dec_test_harness', runner.get_command_line())
        logger.debug(f"Running test harness: {runner.get_command_line()}")
        runner.run(assert_rc=True)

        with open(os.path.join(test_dir, hash_file), 'r') as json_file:
            generated_hashes = json.load(json_file)

        if config.getboolean('REGEN', False):
            output_files = [path for path in os.listdir(test_dir) if path.startswith(output_prefix)]
            assert len(output_files) == 1, "Incorrect files in test folder"
            harness_dump = os.path.join(test_dir, output_files[0])
            self.validate_regen(test, test_dir, encode_path, harness_dump)
            self.regenerated_hashes = generated_hashes
        else:
            for hash_name, regression_hash in test['meta'].items():
                if hash_name.startswith('hash') and regression_hash:
                    hash_name = hash_name.replace('hash_', '')
                    assert regression_hash == generated_hashes.get(hash_name), \
                        f"Harness '{hash_name}' hash {generated_hashes.get(hash_name)} does not match {regression_hash}"

    def validate_regen(self, test, test_dir, encode_path, harness_dump):
        width = int(test['meta']['width'])
        height = int(test['meta']['height'])
        bitdepth = int(test['meta']['bitdepth'])
        subsampling = test['meta'].get('subsampling', '420')

        ltm_dec_path = ltm_generate_regen_image(self, subsampling, test_dir, encode_path)

        y, u, v = pixel_deviation_histogram(ltm_dec_path, harness_dump,
                                            bitdepth, width, height, subsampling_ratio=subsampling)

        tolerance = int(test['meta']['tolerance'])
        threshold = float(test['meta']['threshold'])
        for plane_name, plane in zip(('Y', 'U', 'V'), (y, u, v)):
            good_pixels = round(sum([plane.get(bucket, 0)
                                for bucket in range(-tolerance, tolerance + 1, 1)]), 3)
            assert good_pixels >= threshold, \
                f"Cannot regen - too many pixels out of threshold in {plane_name}, target {threshold}% within " \
                f"+/- {tolerance} value, actual {good_pixels}%"
