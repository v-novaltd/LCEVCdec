# Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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
import re
import glob
import argparse
import datetime
import platform
import shutil
import subprocess
from pathlib import Path

COPYRIGHT_MSG = '''Copyright (c) V-Nova International Limited {years}. All rights reserved.
This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
No patent licenses are granted under this license. For enquiries about patent licenses,
please contact legal@v-nova.com.
The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
software may be incorporated into a project under a compatible license provided the requirements
of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE.'''
THIS_YEAR = str(datetime.datetime.now().year)
COPYRIGHT_TYPES = ('*.cpp', '*.c', '*.h', '*.py', '*.js', '*.java', '*.gradle', '*.cmake',
                   'CMakeLists.txt', 'build_config.h.in', '*.yml', '*.glsl', '*.comp', '*.frag', '*.vert')
CLANG_FORMAT_TYPES = ('*.cpp', '*.c', '*.h')
CMAKE_TYPES = ('*.cmake', 'CMakeLists.txt')
PYTHON_TYPES = ('*.py',)
JAVA_TYPES = ('*.java',)
WORKFLOW_TYPES = ('*.yml', '*.yaml')
GLOB_DIRS = ('src/**/', 'cmake/**/', 'conan/lcevc_dec_headers/*', 'conan/ffmpeg/[!conanfile.py]*',
             'include/**/', 'docs/sphinx/**', '.github/**/', 'wrappers/**/', '')
CLANG_FORMAT_ENV_VAR = 'CLANG_FORMAT_PATH'
WIN_CLANG_FORMAT_ENV_VAR = 'CLANG_FORMAT_PATH'
WIN_DEFAULT_CLANG_FORMAT_PATH = r'C:\Program Files\LLVM\bin\clang-format.exe'
# These files are copied from other sources and have their own copyrights,
# find associated licenses in the licenses folder
EXCLUDED_GLOBS = ('cmake/toolchains/ios.toolchain.cmake',
                  'cmake/toolchains/Emscripten.*', '.github/workflows/cla.yml',
                  'src/pipeline_vulkan/src/vma/vk_mem_alloc.h',
                  'src/pipeline_vulkan/src/shaders/src/specialization_ids.h')
INCLUDE_GUARD_OVERRIDES = {'src/api/include/LCEVC/lcevc_dec.h': 'LCEVC_DEC_H',
                           'include/LCEVC/api_defs.h': 'API_DEFS_H',
                           'wrappers/jni/library/src/main/jni/lcevc_dec_jni.h': 'LCEVC_DEC_JNI_H',
                           'wrappers/jni/library/src/main/jni/surface_presenter.h': 'LCEVC_DEC_JNI_SURFACE_PRESENTER_H',
                           'wrappers/jni/library/src/main/jni/weak_api29.h': 'LCEVC_DEC_JNI_WEAK_API29_H',
                           'src/pixel_processing/src/upscale_macros': None}
TRAILING_SPACE_GLOB_DIRS = ('src/**/', 'cmake/**/', 'conan/**/', 'include/**/',
                            'docs/', 'docs/sphinx/**', '.github/**/', 'licenses/**/', 'wrappers/', '')
COLOR_RESET = '\033[0m'
COLOR_RED = '\033[0;31m'
COLOR_GREEN = '\033[0;32m'
COLOR_YELLOW = '\033[0;33m'


def run_cmd(cmd):
    return subprocess.run(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def colour(text: str, color: str):
    return f'{color}{text}{COLOR_RESET}'


def print_normal(msg: str):
    print(f"lint.py: {msg}")


def print_warn(msg: str):
    print_normal(f"{colour('!>>', COLOR_YELLOW)} {msg}")


def print_success(msg: str):
    print_normal(f"{colour('!>>', COLOR_GREEN)} {msg}")


def print_fail(tool: str, msg: str = ''):
    body = f"{tool} {colour('FAILED', COLOR_RED)}"
    if msg:
        body = f"{body} {msg}"
    print_warn(body)


def get_changed_files(diff_main=False):
    if diff_main:
        remote_name = 'origin'
        if os.environ.get('SOURCE_BRANCH'):
            current_branch = os.environ.get('SOURCE_BRANCH')
        else:
            process = run_cmd(['git', 'rev-parse', '--abbrev-ref', '@{upstream}'])
            current_branch = process.stdout.decode('utf-8').strip()
            remote_name = current_branch[:current_branch.find('/')]
        target_branch = os.environ.get('TARGET_BRANCH', f'{remote_name}/main')
        print_normal(f'Getting file diff from "{current_branch}" to "{target_branch}"')
        process = run_cmd(['git', 'diff', '--name-only', current_branch, target_branch])
        assert process.returncode == 0, f"Failed to find changed files: {process.stderr.decode('utf-8')}"
        changed_files = process.stdout.decode('utf-8').splitlines()
    else:
        process = run_cmd(['git', 'ls-files', '--modified'])
        changed_files = process.stdout.decode('utf-8').splitlines()
        process = run_cmd(['git', 'diff', '--name-only', '--cached'])
        changed_files.extend(process.stdout.decode('utf-8').splitlines())
    changed_files = list(set([path for path in changed_files if os.path.isfile(path)]))
    print_normal(f'Linting {len(changed_files)} changed files only: {", ".join(changed_files)}')
    return changed_files


def get_excluded_files(exclude_dirs):
    excluded = list()
    for exclude_glob in exclude_dirs:
        excluded.extend(glob.glob(exclude_glob, recursive=True))
    return excluded


def get_paths(glob_types, changed_files, global_dirs=GLOB_DIRS, excluded_dirs=EXCLUDED_GLOBS):
    paths = list()
    for glob_dir in global_dirs:
        for glob_type in glob_types:
            paths.extend(glob.glob(glob_dir + glob_type, recursive=True))
    if excluded_dirs:
        to_exclude = [path for path in paths for exclude in get_excluded_files(excluded_dirs)
                      if os.path.samefile(path, exclude)]
        for exclude in to_exclude:
            paths.remove(exclude)
    if changed_files is not False:
        paths = [path for path in paths for changed in changed_files
                 if os.path.samefile(path, changed)]
    return paths


def get_start_year(first_line):
    match = re.search(r"(20\d\d)-20\d\d|(20\d\d)", first_line)
    if match:
        return match.groups()[0] or match.groups()[1]
    return THIS_YEAR


def format_cpp_comment(comment):
    ret = ''
    for line_no, line in enumerate(comment.splitlines()):
        if line_no == 0:
            ret += '/* ' + line + '\n'
        elif line_no == len(comment.splitlines()) - 1:
            ret += ' * ' + line + ' */\n'
        else:
            ret += ' * ' + line + '\n'
    return ret


def format_comment(path):
    filename = os.path.basename(path)
    file_extension = filename.split('.')[-1] if '.' in filename else None
    ret = list()
    if file_extension in ('cpp', 'c', 'h', 'in', 'js', 'java', 'glsl', 'comp', 'gradle', 'frag', 'vert'):
        ret = format_cpp_comment(COPYRIGHT_MSG).splitlines(keepends=True)
    elif file_extension in ('py', 'cmake', 'yml') or filename == 'CMakeLists.txt':
        for line in COPYRIGHT_MSG.splitlines():
            ret.append('# ' + line + '\n')

    return ret


def format_include_guard(path, check_only=False):
    path_parts = Path(path).parts
    if any(Path(path).is_relative_to(Path(override_path := override)) for override in INCLUDE_GUARD_OVERRIDES.keys()):
        correct_guard = INCLUDE_GUARD_OVERRIDES[override_path]
    elif path_parts[2] == 'include' and path_parts[3] == 'LCEVC':
        assert len(path_parts) >= 6, f"Invalid folder structure for interfaces in {path}, " \
            f"should be src/<target>/include/LCEVC/<target>/..."
        correct_guard = f"VN_LCEVC_{'_'.join(path_parts[4:]).replace('.', '_').upper()}"
    else:
        assert path_parts[0] == 'src', f"Header file {path} outside of 'src' dir, unsure how to format guards"
        correct_guard = f"VN_LCEVC_{path_parts[1].upper()}_{path_parts[-1].replace('.h', '').upper()}_H"

    if not correct_guard:
        print_warn(f'Auto include guard disabled in {path}')
        return 0

    with open(path, 'r') as f:
        file_contents = f.read()

    # Note that top_guard doesn't require the ifndef'd string to match the define'd one, in case
    # there's a typo. Beware: this is a bit too broad, and will overwrite code of the following form
    # "#ifndef PATH_MAX #define PATH_MAX MAX_PATH", (if such code is the first "ifndef" in the file)
    top_guard = re.search(r"#ifndef (.*)\n#define (.*)\n", file_contents)
    existing_guards = list(top_guard.groups()) if top_guard else []

    # We assume that the LAST "#endif" with a comment is the bottom guard
    commented_endifs = list(re.finditer(r"#endif +// +([^\n ]*)\n", file_contents))
    bottom_guard = (commented_endifs[-1].groups()[0] if commented_endifs else None)
    if bottom_guard:
        existing_guards.append(bottom_guard)

    if len(existing_guards) == 3 and all(guard == correct_guard for guard in existing_guards):
        return 0
    if check_only:
        existing_guards_str = "'" + "', '".join(existing_guards) + "'"
        print_warn(f"Incorrect include guards in {path}, found {len(existing_guards)} guards of "
                   f"{existing_guards_str} - should be '{correct_guard}'")
        return 1

    def save_file(new_contents):
        with open(path, 'w', newline='') as f:
            f.write(new_contents)
    if len(existing_guards) == 0:
        if (top_pos := file_contents.find('#include')) != -1:
            file_contents = file_contents[:top_pos] + f"#ifndef {correct_guard}\n#define {correct_guard}\n\n" + \
                file_contents[top_pos:] + f"\n#endif // {correct_guard}\n"
            save_file(file_contents)
            print_success(f'Added new include guards in {path}')
            return 0
        else:
            print_warn(f"Please add include guards to {path} with name '{correct_guard}'")
            return 1
    if len(existing_guards) == 2 and (not bottom_guard or bottom_guard.groups()[0] is None):
        file_contents = file_contents[:file_contents.rfind(
            '#endif')] + f'#endif // {correct_guard}\n'
        save_file(file_contents)
        print_success(f'Fixed bottom include guard comment in {path}')
    if any(guard != correct_guard for guard in existing_guards):
        for wrong_guard in set(existing_guards):
            if not wrong_guard:
                continue
            file_contents = re.sub(wrong_guard, correct_guard, file_contents)
        save_file(file_contents)
        print_success(f'Fixed incorrect include guards in {path}')
    return 0


def format_include_braces(path, check_only=False):
    with open(path, 'r') as f:
        file_contents = f.readlines()

    errors = 0
    for line_no, line in enumerate(file_contents):
        if line.startswith('#include "LCEVC/'):
            include_path = re.search('#include "(.*)"', line).groups()[0]
            if check_only:
                print_warn(f'Incorrect braces in {path}, should be #include <{include_path}>')
            else:
                print_success(f'Fixed include brace format in {path}')
                file_contents[line_no] = f'#include <{include_path}>\n'
            errors += 1
    if check_only:
        return errors
    else:
        with open(path, 'w', newline='') as f:
            f.writelines(file_contents)
        return 0


def copyright_file(path, check_only=False):
    with open(path, 'r') as f:
        file_contents = f.readlines()
    start_year = get_start_year(file_contents[0])
    years = THIS_YEAR if start_year == THIS_YEAR else f'{start_year}-{THIS_YEAR}'
    copyright_msg = format_comment(path)
    copyright_msg[0] = copyright_msg[0].format(years=years)

    if len(file_contents) < len(COPYRIGHT_MSG.splitlines()):
        if check_only:
            print_warn(f'Incorrect copyright header in {path}')
            return False
        print_success(f'Added copyright header to {path}')
        with open(path, 'w', newline='') as f:
            f.writelines(copyright_msg)
            f.writelines(file_contents)
        return True

    body_lines_match = 0
    for line_no, (file_line, correct_line) in enumerate(zip(file_contents, copyright_msg)):
        if line_no == 0:
            continue
        if file_line.strip() == correct_line.strip():
            body_lines_match += 1
    if body_lines_match == len(copyright_msg) - 1:
        if file_contents[0] != copyright_msg[0]:
            if check_only:
                print_warn(f'Incorrect copyright year in {path}, should be {years}')
                return False
            with open(path, 'w', newline='') as f:
                f.writelines(copyright_msg)
                f.writelines(file_contents[len(copyright_msg):])
            print_success(f'Updated copyright year in {path} to {years}')
    elif body_lines_match != 0:
        print_warn(f'Unknown partial copyright match in {path}, '
                   f'please manually delete incomplete notice and re-commit')
        return False
    else:
        if check_only:
            print_warn(f'Incorrect copyright header in {path}')
            return False
        leading_newlines = 0
        for line in file_contents:
            if line != '\n':
                break
            leading_newlines += 1
        with open(path, 'w', newline='') as f:
            f.writelines(copyright_msg)
            f.write('\n')
            f.writelines(file_contents[leading_newlines:])
        print_success(f'Added copyright header to {path}')

    return True


def find_clang_format():
    env_value = os.environ.get(CLANG_FORMAT_ENV_VAR)
    if env_value and os.path.isfile(env_value):
        exe = env_value
    elif platform.system() == 'Windows':
        if os.path.isfile(WIN_DEFAULT_CLANG_FORMAT_PATH):
            exe = WIN_DEFAULT_CLANG_FORMAT_PATH
        else:
            print_normal(f"Could not find clang format executable, either set it's location to '{CLANG_FORMAT_ENV_VAR}' "
                         f"environment variable or install it to the default location '{WIN_DEFAULT_CLANG_FORMAT_PATH}'")
            exit(1)
    elif os.environ.get('CLANG_FORMAT_PATH'):
        exe = os.environ.get('CLANG_FORMAT_PATH')
    else:
        exe = 'clang-format-14'

    try:
        process = run_cmd([exe, '--version'])
    except (FileNotFoundError, PermissionError):
        print_normal(f"Clang format executable '{exe}' not found, please install with"
                     f" 'apt install clang-format-14' or similar")
        exit(1)
    if 'version 14.0.' not in process.stdout.decode('utf-8'):
        print_normal(
            f"Incorrect version of clang-format installed at '{exe}', version 14.0.x is required")
        exit(1)
    return exe


def find_formatter(exe, version, version_arg: str = '--version', stderr: bool = False):
    try:
        process = run_cmd([exe, version_arg])
    except (FileNotFoundError, PermissionError):
        print_normal(
            f"{exe} not found, ensure lint_requirements.txt are installed (google-java-format installed separately)")
        exit(1)
    version_string = process.stderr.decode('utf-8') if stderr else process.stdout.decode('utf-8')
    if version not in version_string:
        # If it is found but has wrong version, even if skip is passed, error as it's more confusing
        # for it to be on the path (user would expect it to work) but silently pass
        print_normal(f"Incorrect version of {exe} is installed ({version_string})")
        print_normal(f"Please use end= version specified in lint_requirements.txt ({version})")
        exit(1)
    return exe


def is_binary_file(file_path):
    result = run_cmd(["git", "check-attr", "text", file_path])
    if b"unset" in result.stdout:
        return True
    return False


def remove_trailing_spaces(file, check_only=False):
    if os.path.isdir(file):
        return True
    assert os.path.isfile(file), f"Cannot find file: {file}"

    # Dont lint binary files
    if is_binary_file(file):
        return True

    try:
        with open(file, 'r') as f:
            original_content = f.read()
        stripped_content = '\n'.join([line.rstrip() for line in original_content.splitlines()])

        if original_content != stripped_content:
            if check_only:
                diff = []
                for i, (original, stripped) in enumerate(zip(original_content.splitlines(), stripped_content.splitlines()), start=1):
                    if original != stripped:
                        diff.append(f"Line {i}: '{original}'")
                if diff:
                    print_warn(f'Trailing spaces in {file}:\n' + '\n'.join(diff))
                    return False
            else:
                with open(file, 'w') as f:
                    f.write(stripped_content + '\n')
        return True
    except Exception as e:
        print_normal(f"Failed to remove trailing spaces on {file}: {e}")
        return False


def lint_readme(check_only=False):
    readme_copyright_match = 'V-Nova Limited 2014-'
    readme_path = 'README.md'
    assert os.path.isfile(readme_path), "Cannot find README.md file"
    with open(readme_path) as f:
        readme_content = f.read()
    str_loc = readme_content.find(readme_copyright_match) + len(readme_copyright_match)
    cur_year = readme_content[str_loc:str_loc + 4]
    if cur_year != THIS_YEAR:
        if check_only:
            print_fail('README copyright')
            return False
        else:
            readme_content = readme_content[:str_loc] + THIS_YEAR + readme_content[str_loc + 4:]
            with open(readme_path, 'w') as f:
                f.write(readme_content)
            print_normal('Updated README.md copyright year')
    return True


def parse_args():
    parser = argparse.ArgumentParser(description="Lint all file types in the repo")
    parser.add_argument("--check-only", action="store_true",
                        help="Don't fix issues in-place, return non-zero exit code if errors are found")
    parser.add_argument("--all-files", action="store_true",
                        help="Lint every file, not just git diff")
    parser.add_argument("--diff-main", action="store_true",
                        help="Get file diff from main branch rather than locally changed files")

    return parser.parse_args()


def main():
    args = parse_args()
    print_normal("check mode" if args.check_only else "fix mode")
    changed_files = get_changed_files(args.diff_main) if not args.all_files else False
    errors = 0

    for path in get_paths(CLANG_FORMAT_TYPES, changed_files):
        clang_format_exe = find_clang_format()
        format_include_braces(path, args.check_only)
        cmd = [clang_format_exe, path]
        if args.check_only:
            cmd.insert(1, '-n')
            cmd.insert(2, '--Werror')
        else:
            cmd.insert(1, '-i')
        process = run_cmd(cmd)
        if args.check_only and process.returncode != 0:
            print_fail('clang-format', f"on {process.stderr.decode('utf-8')}")
            errors += 1
        if path.endswith('.h'):
            errors += format_include_guard(path, args.check_only)

    for path in get_paths(CMAKE_TYPES, changed_files):
        cmake_format_exe = find_formatter('cmake-format', '0.6.')
        cmd = [cmake_format_exe, path]
        if args.check_only:
            cmd.insert(1, '--check')
        else:
            cmd.insert(1, '--in-place')
        process = run_cmd(cmd)
        if process.returncode != 0:
            print_fail('cmake-format', f'on {path}')
            errors += 1

    python_files = get_paths(PYTHON_TYPES, changed_files)
    if python_files:
        flake_exe = find_formatter('flake8', '7.1.0')
        autopep_exe = find_formatter('autopep8', '2.3')
        process = run_cmd([flake_exe, '--exclude', '.venv'] + python_files)
        if process.returncode != 0:
            print_fail('flake8', f":\n{process.stdout.decode('utf-8')}")
            errors += 1
        autopep_args = ['--diff', '--exit-code'] if args.check_only else ['-i']
        process = run_cmd([autopep_exe] + autopep_args
                          + ['--global-config', '.flake8'] + python_files)
        if process.returncode != 0:
            if args.check_only:
                print_fail('autopep8', f":\n{process.stdout.decode('utf-8')}")
            else:
                print_normal(f"autopep8 failed: {process.stderr.decode('utf-8')}")
            errors += 1

    java_files = get_paths(JAVA_TYPES, changed_files)
    if java_files:
        java_format_exe = find_formatter("google-java-format", "1.",
                                         version_arg="-version", stderr=True, )
        java_args = ['--dry-run', '--set-exit-if-changed'] if args.check_only else ['--replace']
        process = run_cmd([java_format_exe] + java_args + java_files)
        if process.returncode != 0:
            print_normal(f"google-java-format failed: {process.stdout.decode('utf-8')}")
            errors += 1

    if not args.all_files:
        for path in get_paths(COPYRIGHT_TYPES, changed_files):
            if not copyright_file(path, args.check_only):
                errors += 1
    if not lint_readme():
        errors += 1

    for path in get_paths(['*.*'], changed_files, global_dirs=TRAILING_SPACE_GLOB_DIRS, excluded_dirs=None):
        if not remove_trailing_spaces(path, args.check_only):
            errors += 1

    for path in get_paths(WORKFLOW_TYPES, changed_files, global_dirs=['.github/workflows/**'], excluded_dirs=None):
        cmd = ['zizmor', path] if shutil.which('zizmor') else ['uvx', 'zizmor', path]

        process = run_cmd(cmd)
        if process.returncode != 0:
            print_fail('zizmor', f"on {process.stdout.decode('utf-8')}")
            errors += 1

    if errors == 0:
        print_normal(f"~~~ Linting {colour('PASSED', COLOR_GREEN)} ~~~")
    else:
        print_normal(f"~~~ Linting {colour(f'FAILED - {errors} errors', COLOR_RED)} ~~~")

    return errors


if __name__ == '__main__':
    exit(main())
