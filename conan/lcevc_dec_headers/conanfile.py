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

from conan import ConanFile
import os
import shutil


class LCEVCDecHeadersConan(ConanFile):
    name = 'lcevc_dec_headers'
    url = 'https://github.com/v-novaltd/LCEVCdec'
    description = 'V-Nova LCEVC Decoder SDK Headers'
    license = 'BSD-3-Clause-Clear License'
    header_only = True

    def requirements(self):
        self.requires(f'lcevc_dec/{self.version}@')
        self.options['lcevc_dec'].base_decoder = 'none'
        self.options['lcevc_dec'].json_config = False

    def build(self):
        lcevc_dec_includes_root = 'include'
        lcevc_dec_includes = self.deps_cpp_info['lcevc_dec'].includedirs
        for src in lcevc_dec_includes:
            full_src = os.path.join(
                self.deps_cpp_info['lcevc_dec'].rootpath, src, 'LCEVC')
            dst = os.path.relpath(src, lcevc_dec_includes_root)
            os.mkdir(os.path.join(dst, 'LCEVC'))
            self.output.info(f'Copying {src} from LCEVC Decoder SDK headers to {dst}')
            for path in os.listdir(full_src):
                if path.endswith('.h'):
                    shutil.copyfile(os.path.join(full_src, path), os.path.join(dst, 'LCEVC', path))

    def package(self):
        self.copy('*.h', dst='include', keep_path=True)
