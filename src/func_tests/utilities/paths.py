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
import shutil
import platform

from utilities.config import config, logger
from utilities.runner import ADB_PLATFORMS


def format_path(template_path):
    bases_path = os.path.join(config.get('CACHE_PATH', 'test_asset_cache'), "bases")
    path = template_path.format(BASE=bases_path)
    if platform.system() == "Windows":
        path = path.replace('/', '\\')
    path = os.path.abspath(path)
    return path


def get_executable(name):
    if platform.system() == "Windows" and config.get('PLATFORM') not in ADB_PLATFORMS:
        name += ".exe"
    executable = os.path.join(config.get('BIN_DIR'), name)
    if not os.path.exists(executable):
        raise RuntimeError(f'"{executable}" does not exist')

    return os.path.abspath(executable)


def initialize_tempdirs():
    root = config.get('WORKDIR')
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(root)
    cache = config.get('CACHE_PATH', 'test_asset_cache')
    logger.info(f"Initialising tempdirs. root {root}, cache {cache}")
    if config.getboolean('RESET_LOCAL_CACHE', False):
        logger.info("RESET_LOCAL_CACHE is enabled, deleting all locally cached assets")
        if os.path.isdir(cache):
            shutil.rmtree(cache)
    if not os.path.isdir(cache):
        os.makedirs(cache, exist_ok=True)


def get_tempdir(uid, root=config.get('WORKDIR'), make_dir=True):
    tempdir = os.path.join(root, f"test_{uid}")
    if make_dir:
        os.mkdir(tempdir)
    return tempdir
