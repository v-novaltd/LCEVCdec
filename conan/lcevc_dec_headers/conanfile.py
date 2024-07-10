# Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
# This software is licensed under the BSD-3-Clause-Clear License.
# No patent licenses are granted under this license. For enquiries about patent licenses,
# please contact legal@v-nova.com.
# The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
# If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
# AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
# SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
# DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
# EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.

from conans import ConanFile
import os
import shutil


class LCEVCDecHeadersConan(ConanFile):
    name = 'lcevc_dec_headers'
    url = 'https://github.com/v-novaltd/LCEVCdec'
    description = 'V-Nova LCEVC Decoder SDK Headers'
    license = 'BSD-3-Clause-Clear License'
    header_only = True

    def build_requirements(self):
        self.build_requires(f'lcevc_dec/{self.version}@')

    def build(self):
        lcevc_dec_includes_root = 'include'
        lcevc_dec_includes = self.deps_cpp_info['lcevc_dec'].includedirs
        for src in lcevc_dec_includes:
            full_src = os.path.join(
                self.deps_cpp_info['lcevc_dec'].rootpath, src)
            dst = os.path.relpath(src, lcevc_dec_includes_root)
            self.output.info(f'Copying {src} from LCEVC Decoder SDK headers to {dst}')
            shutil.copytree(
                full_src,
                os.path.join(self.build_folder, dst), dirs_exist_ok=True)

    def package(self):
        self.copy('*.h', dst='include', keep_path=True)
