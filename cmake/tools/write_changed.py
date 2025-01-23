# Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

"""
Usage: write_change.py <output_file> <command ...>

Runs command and captures output into a file.

If existing contents of file is identical, file is untouched.
"""

import os
import sys
import subprocess


def capture_output(cmd):
    '''Run `cmd` as a subprocess, returning stdout.'''
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if not proc or proc.returncode != 0:
        raise RuntimeError(f"Failed to run command: \"{cmd}\"")
    return proc.stdout.decode().rstrip()


def write_if_changed(path, content):
    '''Write content to a file, only if file would change.'''

    if os.path.isfile(path):
        with open(path, 'r') as f:
            if f.read() == content:
                return False

    # Actually output the content now
    with open(path, "w") as f:
        f.write(content)
        return True


if __name__ == "__main__":
    if write_if_changed(sys.argv[1], capture_output(" ".join(sys.argv[2:]))):
        print(f"Updated \"{sys.argv[1]}\"")
