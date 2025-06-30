# Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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
import os.path
import re

from test_functions.harness_hash import Test as CoreTest
from utilities.config import logger, config
from utilities.runner import ADB_PLATFORMS
from utilities.assets import get_encode

CONTENT_FRAME_RATE = 30


class Test(CoreTest):
    def test(self, test, test_dir):

        if config.get('PLATFORM') in ADB_PLATFORMS:
            frequency = self.get_current_frequency()
            self.record_result('pre_test_frequency', frequency)
            temperature = self.get_device_temperature()
            self.record_result('pre_test_temperature', temperature)

        if test['cli'].get('--base'):
            test['cli']['--read-bin-linearly'] = 'FLAG'
        process = super().test(test, test_dir)
        output = process.stdout.decode('utf-8')

        # Extract bitrate
        encode_path = get_encode(test['erp'])
        file_size = os.path.getsize(encode_path)
        if test['erp'].get('--input'):
            get_frame_number_match = re.search(r'_(\d+)f\.yuv$', test['erp'].get('--input'))
            total_frames = int(get_frame_number_match.group(1))
            bitrate = (file_size * CONTENT_FRAME_RATE * 8) / (total_frames * 1024)
            self.record_result('bitrate', bitrate)

        # Extracting latency and throughput from the output from core
        logger.debug("Extracting Avg latency and throughput")

        latency_pattern = r"Average frame latency: ([\d\.]+)ms"
        latency_match = re.search(latency_pattern, output)
        latency = float(latency_match.group(1)) if latency_match else None
        self.record_result('latency', latency)

        frame_time_pattern = r"frame time \(1 / throughput\): ([\d\.]+)ms"
        frame_time_match = re.search(frame_time_pattern, output)
        frame_time_ms = float(frame_time_match.group(1)) if frame_time_match else None
        self.record_result('frame_time', frame_time_ms)
        if config.get('PLATFORM') in ADB_PLATFORMS:
            frequency = self.get_current_frequency()
            self.record_result('post_test_frequency', frequency)
            temperature = self.get_device_temperature()
            self.record_result('post_test_temperature', temperature)
        peak_memory_file = os.path.abspath(os.path.join(test_dir, 'memory.log'))
        file_exists = os.path.exists(peak_memory_file)
        if file_exists:
            with open(peak_memory_file, 'r') as mem_file:
                peak_memory = mem_file.read().strip()
                if peak_memory.isdigit():
                    self.record_result('peak_memory', int(peak_memory))
