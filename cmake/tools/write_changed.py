#!/usr/bin/env python3
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
