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
import git
import sys
import logging
import platform
import configparser
import multiprocessing as mp

LOG_LEVEL_MAP = dict(DEBUG=logging.DEBUG, INFO=logging.INFO,
                     WARNING=logging.WARNING, ERROR=logging.ERROR, CRITICAL=logging.CRITICAL)
LOG_FORMAT = '[%(asctime)s %(hostname)s] %(levelname)s in %(processName)s %(module)s - %(message)s'


def read_config():
    default_path = os.path.join(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))), 'config.ini')
    config_path = os.environ.get("TEST_CONFIG") if os.environ.get("TEST_CONFIG") else default_path
    parsed_config = configparser.ConfigParser()
    if len(parsed_config.read(config_path)) == 0:
        raise FileNotFoundError(
            f"Couldn't locate a config at '{config_path}', set with TEST_CONFIG environment variable")
    for config_name in parsed_config["OPTIONS"]:
        config_name = config_name.upper()
        if os.environ.get(config_name.upper()):
            if mp.current_process().name == "MainProcess":
                print(f"Overwriting config value for '{config_name}' "
                      f"with environment variable: {os.environ[config_name]}")
            parsed_config.set("OPTIONS", config_name, os.environ[config_name])
    return parsed_config["OPTIONS"]


class HostnameFilter(logging.Filter):
    hostname = platform.node()

    def filter(self, record):
        record.hostname = HostnameFilter.hostname
        return True


def setup_app_logger():
    app_logger = logging.getLogger(str(os.getpid()))
    if not app_logger.hasHandlers():
        config_level = config.get("LOG_LEVEL", "INFO")
        if config_level not in LOG_LEVEL_MAP:
            raise RuntimeError(f"'LOG_LEVEL' config param must be "
                               f"one of {list(LOG_LEVEL_MAP.keys())}")
        app_logger.setLevel(LOG_LEVEL_MAP[config_level])
        formatter = logging.Formatter(LOG_FORMAT)
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.addFilter(HostnameFilter())
        console_handler.setFormatter(formatter)
        app_logger.addHandler(console_handler)

    return app_logger


def get_githash():
    try:
        repo = git.Repo(search_parent_directories=True)
    except git.exc.GitError:
        return "Unknown"
    return repo.head.object.hexsha[:8]


config = read_config()
logger = setup_app_logger()
GIT_HASH = os.environ.get('GIT_HASH')[:8] if os.environ.get('GIT_HASH') else get_githash()
