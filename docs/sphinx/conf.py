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

from datetime import datetime

project = 'LCEVC Decoder SDK'
copyright = f'V-Nova International Limited {datetime.now().year}. All rights reserved.'
author = 'V-Nova Ltd.'
# 'release` and `version' are set via sphinx-build command line
release = 'Unknown'
version = 'Unknown'

# General configuration
extensions = [
    'breathe',
    'sphinx_rtd_theme',
    'sphinxcontrib.plantuml',
    'sphinx.ext.autosectionlabel'
]
# Path to plant UML executable for CI - might need modification when running locally, doesn't seem to work well on Windows
plantuml = 'java -jar -Djava.net.useSystemProxies=true -Djava.awt.headless=true /usr/share/plantuml/plantuml.jar'
plantuml_output_format = 'svg'

templates_path = ['_templates']
exclude_patterns = []

html_theme = 'sphinx_rtd_theme'
html_static_path = ['.']
html_logo = 'lcevc-logo.png'

# Breathe Configuration
breathe_default_project = "LCEVC_DEC"


def setup(app):
    app.add_css_file('custom.css')


nitpick_ignore = [
    ('cpp:identifier', 'int8_t'),
    ('cpp:identifier', 'int16_t'),
    ('cpp:identifier', 'int32_t'),
    ('cpp:identifier', 'int64_t'),
    ('cpp:identifier', 'uint8_t'),
    ('cpp:identifier', 'uint16_t'),
    ('cpp:identifier', 'uint32_t'),
    ('cpp:identifier', 'uint64_t'),
    ('cpp:identifier', 'intptr_t'),
    ('cpp:identifier', 'uintptr_t'),
]
