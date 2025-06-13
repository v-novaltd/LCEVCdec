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

import os
import glob
import argparse
import datetime
import platform
import subprocess

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
COPYRIGHT_TYPES = ('*.cpp', '*.c', '*.h', '*.py', '*.js',
                   '*.cmake', 'CMakeLists.txt', 'build_config.h.in', '*.yml')
CLANG_FORMAT_TYPES = ('*.cpp', '*.c', '*.h')
CMAKE_TYPES = ('*.cmake', 'CMakeLists.txt')
PYTHON_TYPES = ('*.py',)
WORKFLOW_TYPES = ('*.yml', '*.yaml')
GLOB_DIRS = ('src/**/', 'cmake/**/', 'conan/lcevc_dec_headers/*', 'conan/ffmpeg/[!conanfile.py]*'
             'include/**/', 'docs/sphinx/**', '.github/**/', '')
WIN_CLANG_FORMAT_ENV_VAR = 'CLANG_FORMAT_PATH'
WIN_DEFAULT_CLANG_FORMAT_PATH = r'C:\Program Files\LLVM\bin\clang-format.exe'
# These files are copied from other sources and have their own copyrights,
# find associated licenses in the licenses folder
EXCLUDED_GLOBS = ('cmake/toolchains/ios.toolchain.cmake',
                  'cmake/toolchains/Emscripten.*', '.github/workflows/cla.yml')
TRAILING_SPACE_GLOB_DIRS = ('src/**/', 'cmake/**/', 'conan/**/', 'include/**/',
                            'docs/', 'docs/sphinx/**', '.github/**/', 'licenses/**/', '')


def run_cmd(cmd):
    return subprocess.run(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def get_changed_files(diff_master=False):
    if diff_master:
        if os.environ.get('SOURCE_BRANCH'):
            current_branch = os.environ.get('SOURCE_BRANCH')
        else:
            process = run_cmd(['git', 'rev-parse', '--abbrev-ref', 'HEAD'])
            current_branch = process.stdout.decode('utf-8').strip()
        target_branch = os.environ.get('TARGET_BRANCH', 'master')
        print(f'Getting file diff from "{current_branch}" to "{target_branch}"')
        process = run_cmd(['git', 'diff', '--name-only', '--diff-filter=ACMRTUX',
                           f'{current_branch}', f'origin/{target_branch}'])
        changed_files = process.stdout.decode('utf-8').splitlines()
    else:
        process = run_cmd(['git', 'ls-files', '--modified'])
        changed_files = process.stdout.decode('utf-8').splitlines()
        process = run_cmd(['git', 'diff', '--name-only', '--cached'])
        changed_files.extend(process.stdout.decode('utf-8').splitlines())
    changed_files = [path for path in changed_files if os.path.isfile(path)]
    print(f'Linting {len(changed_files)} changed files only: {", ".join(changed_files)}')
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


def get_start_year(path):
    process = run_cmd(["git", "log", "--follow", "--format='%as'", path])
    dates = process.stdout.decode('utf-8')
    if not dates:
        return THIS_YEAR
    return dates.splitlines()[-1].strip("'")[:4]


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
    if file_extension in ('cpp', 'c', 'h', 'in', 'js'):
        ret = format_cpp_comment(COPYRIGHT_MSG).splitlines(keepends=True)
    elif file_extension in ('py', 'cmake', 'yml') or filename == 'CMakeLists.txt':
        for line in COPYRIGHT_MSG.splitlines():
            ret.append('# ' + line + '\n')

    return ret


def copyright_file(path, check_only=False):
    start_year = get_start_year(path)
    years = THIS_YEAR if start_year == THIS_YEAR else f'{start_year}-{THIS_YEAR}'
    copyright_msg = format_comment(path)
    copyright_msg[0] = copyright_msg[0].format(years=years)
    with open(path, 'r') as f:
        file_contents = f.readlines()

    if len(file_contents) < len(COPYRIGHT_MSG.splitlines()):
        if check_only:
            print(f'\033[0;33m!>>\033[0m Incorrect copyright header in {path}')
            return False
        print(f'Adding copyright header to {path}')
        with open(path, 'w', newline='') as f:
            f.writelines(copyright_msg)
            f.writelines(file_contents)
        return

    body_lines_match = 0
    for line_no, (file_line, correct_line) in enumerate(zip(file_contents, copyright_msg)):
        if line_no == 0:
            continue
        if file_line.strip() == correct_line.strip():
            body_lines_match += 1
    if body_lines_match == len(copyright_msg) - 1:
        if file_contents[0] != copyright_msg[0]:
            if check_only:
                print(f'\033[0;33m!>>\033[0m Incorrect copyright year in {path}, '
                      f'should be {years}')
                return False
            print(f'Updating copyright year in {path} to {years}')
            with open(path, 'w', newline='') as f:
                f.writelines(copyright_msg)
                f.writelines(file_contents[len(copyright_msg):])
    elif body_lines_match != 0:
        print(f'Unknown partial copyright match in {path}, '
              f'please manually delete incomplete notice and re-commit')
        return False
    else:
        if check_only:
            print(f'\033[0;33m!>>\033[0m Incorrect copyright header in {path}')
            return False
        print(f'Adding copyright header to {path}')
        leading_newlines = 0
        for line in file_contents:
            if line != '\n':
                break
            leading_newlines += 1
        with open(path, 'w', newline='') as f:
            f.writelines(copyright_msg)
            f.write('\n')
            f.writelines(file_contents[leading_newlines:])

    return True


def find_clang_format():
    if platform.system() == 'Windows':
        env_value = os.environ.get(WIN_CLANG_FORMAT_ENV_VAR)
        if env_value and os.path.isfile(env_value):
            exe = env_value
        elif os.path.isfile(WIN_DEFAULT_CLANG_FORMAT_PATH):
            exe = WIN_DEFAULT_CLANG_FORMAT_PATH
        else:
            print(f"Could not find clang format executable, either set it's location to '{WIN_CLANG_FORMAT_ENV_VAR}' "
                  f"environment variable or install it to the default location '{WIN_DEFAULT_CLANG_FORMAT_PATH}'")
            exit(1)
    elif os.environ.get('CLANG_FORMAT_PATH'):
        exe = os.environ.get('CLANG_FORMAT_PATH')
    else:
        exe = 'clang-format-14'

    try:
        process = run_cmd([exe, '--version'])
    except FileNotFoundError:
        print(f"Clang format executable '{exe}' not found, please install with"
              f" 'apt install clang-format-14' or similar")
        exit(1)
    if 'version 14.0.' not in process.stdout.decode('utf-8'):
        print(f"Incorrect version of clang-format installed at '{exe}', version 14.0.x is required")
        exit(1)
    return exe


def find_formatter(exe, version):
    try:
        process = run_cmd([exe, '--version'])
    except FileNotFoundError:
        print(f"{exe} not found, ensure lint_requirements.txt are installed")
        exit(1)
    if version not in process.stdout.decode('utf-8'):
        print(f"Incorrect version of {exe} is installed please use the version "
              f"specified in lint_requirements.txt")
        exit(1)
    return exe


def is_binary_file(file_path):
    result = run_cmd(["git", "grep", "-qIl", ".", file_path])
    if result.returncode != 0:
        return True
    return False


def remove_tailing_space(file, check_only=False):
    if not is_binary_file(file) and os.path.isfile(file):
        try:
            with open(file, 'r') as f:
                original_content = f.read()
            stripped_content = '\n'.join([line.rstrip() for line in original_content.splitlines()])

            if original_content != stripped_content:
                if check_only:
                    diff = []
                    for i, (original, stripped) in enumerate(zip(original_content.splitlines(), stripped_content.splitlines()), start=1):
                        if original != stripped:
                            diff.append(f"Line {i}: '{repr(original)}'")
                    if diff:
                        print(f'\033[0;33m!>>\033[0m Tailing spaces in {file}:\n' + '\n'.join(diff))
                        return False
                else:
                    with open(file, 'w') as f:
                        f.write(stripped_content + '\n')
            return True
        except Exception as e:
            print(f"Failed to remove tailing spaces on {file}: {e}")
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
            print("\033[0;33m!>>\033[0m README copyright \033[0;31mFAILED\033[0m")
            return False
        else:
            readme_content = readme_content[:str_loc] + THIS_YEAR + readme_content[str_loc + 4:]
            with open(readme_path, 'w') as f:
                f.write(readme_content)
            print('Updated README.md copyright year')
    return True


def parse_args():
    parser = argparse.ArgumentParser(description="Lint all file types in the repo")
    parser.add_argument("--check-only", action="store_true",
                        help="Don't fix issues in-place, return non-zero exit code if errors are found")
    parser.add_argument("--all-files", action="store_true",
                        help="Lint every file, not just git diff")
    parser.add_argument("--diff-master", action="store_true",
                        help="Get file diff from master branch rather than locally changed files")

    return parser.parse_args()


def main():
    args = parse_args()
    changed_files = get_changed_files(args.diff_master) if not args.all_files else False
    errors = 0

    clang_format_exe = find_clang_format()
    for path in get_paths(CLANG_FORMAT_TYPES, changed_files):
        cmd = [clang_format_exe, path]
        if args.check_only:
            cmd.insert(1, '-n')
            cmd.insert(2, '--Werror')
        else:
            cmd.insert(1, '-i')
        process = run_cmd(cmd)
        if args.check_only and process.returncode != 0:
            print(f"\033[0;33m!>>\033[0m clang-format \033[0;31mFAILED\033[0m "
                  f"on {process.stderr.decode('utf-8')}")
            errors += 1

    cmake_format_exe = find_formatter('cmake-format', '0.6.')
    for path in get_paths(CMAKE_TYPES, changed_files):
        cmd = [cmake_format_exe, path]
        if args.check_only:
            cmd.insert(1, '--check')
        else:
            cmd.insert(1, '--in-place')
        process = run_cmd(cmd)
        if process.returncode != 0:
            print(f"\033[0;33m!>>\033[0m cmake-format \033[0;31mFAILED\033[0m on {path}")
            errors += 1

    if args.check_only:
        flake_exe = find_formatter('flake8', '7.1.0')
        process = run_cmd([flake_exe])
        if process.returncode != 0:
            print(f"\033[0;33m!>>\033[0m flake8 \033[0;31mFAILED\033[0m:\n"
                  f"{process.stdout.decode('utf-8')}")
            errors += 1
    else:
        autopep_exe = find_formatter('autopep8', '2.3')
        python_files = get_paths(PYTHON_TYPES, changed_files)
        if python_files:
            process = run_cmd([autopep_exe, '-i', '--global-config', '.flake8'] + python_files)
            if process.returncode != 0:
                print(f"autopep8 failed: {process.stderr.decode('utf-8')}")
                errors += 1

    for path in get_paths(COPYRIGHT_TYPES, changed_files):
        if not copyright_file(path, args.check_only):
            errors += 1
    if not lint_readme():
        errors += 1

    for path in get_paths('*.*', changed_files, global_dirs=TRAILING_SPACE_GLOB_DIRS, excluded_dirs=None):
        if not remove_tailing_space(path, args.check_only):
            errors += 1

    zizmor_exe = find_formatter('zizmor', '1.3.0')
    for path in get_paths(WORKFLOW_TYPES, changed_files, global_dirs=['.github/workflows/**'], excluded_dirs=None):
        cmd = [zizmor_exe, path]
        process = run_cmd(cmd)
        if process.returncode != 0:
            print(f"\033[0;33m!>>\033[0m zizmor \033[0;31mFAILED\033[0m "
                  f"on {process.stdout.decode('utf-8')}")
            errors += 1

    if errors == 0:
        print("~~~ Linting \033[0;32mPASSED\033[0m ~~~")
    else:
        print("~~~ Linting \033[0;31mFAILED\033[0m ~~~")

    return errors


if __name__ == '__main__':
    exit(main())
