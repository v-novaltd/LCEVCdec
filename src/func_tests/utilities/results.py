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

import os
import csv
import glob
import json
import platform

from utilities.config import config, logger, GIT_HASH


def dump_results(results):
    dump_path = config.get('RESULTS_PATH', 'dec_test_results.json')
    logger.info(f"Dumping results to '{dump_path}'...")
    groups = set([test['group'] for test in results])
    summary = dict()
    if os.environ.get('BUILD_NUMBER'):
        summary['ci_build_number'] = os.environ.get('BUILD_NUMBER')
    if os.environ.get('TARGET'):
        summary['ci_target'] = os.environ.get('TARGET')
    summary['git_hash'] = GIT_HASH
    summary['platform'] = config.get('PLATFORM')
    summary['encoder_version'] = config.get('ENCODER_VERSION')
    summary['lib_sizes'] = get_library_sizes()
    for group in groups:
        summary[group] = get_result_summary(results, group=group)
    summary.update(get_result_summary(results))
    enabled_results = [result for result in results if result['enabled_type'] != 'skip']
    if filter_group := os.environ.get('FILTER_GROUP', config.get('FILTER_GROUP')):
        enabled_results = [result for result in enabled_results if result['group'] == filter_group]
    if filter_name := os.environ.get('FILTER_NAME', config.get('FILTER_NAME')):
        enabled_results = [result for result in enabled_results if filter_name in result['name']]
    final_dump = dict(summary=summary, tests=enabled_results)

    with open(dump_path, 'w') as jf:
        json.dump(final_dump, jf, indent=4)


def get_result_summary(results, group=None):
    group_summary = dict(passed=0, skipped=0, broken=0, failed=0)
    for test in results:
        if group and test['group'] != group:
            continue
        if (not test['enabled'] and test['enabled_type'] != 'broken') or test.get('results', {}).get('ignored', False):
            group_summary['skipped'] += 1
        elif test['enabled_type'] == 'broken':
            group_summary['broken'] += 1
        elif test['results'].get('pass', False):
            group_summary['passed'] += 1
        else:
            group_summary['failed'] += 1
    return group_summary


def save_regenerated_hashes(results, definitions_dir):
    logger.info(f"Saving CSVs with regenerated hashes to '{definitions_dir}'...")
    definitions = dict()
    definition_columns = dict()
    for definition_csv in os.listdir(definitions_dir):
        if not definition_csv.endswith('.csv'):
            continue
        definitions[definition_csv] = list()
        definition_path = os.path.join(definitions_dir, definition_csv)
        with open(definition_path, 'r') as csv_file:
            csv_reader = csv.DictReader(csv_file)
            definitions[definition_csv] = [row for row in csv_reader]
            definition_columns[definition_csv] = csv_reader.fieldnames

    for result in results:
        if result.get('regenerated_hashes'):
            definition = definitions[result['definition_csv']][result['csv_row']]
            params_to_remove = [param for param in definition if param.startswith('meta:hash')]
            for param in params_to_remove:
                del definition[param]
            for param, new_hash in result['regenerated_hashes'].items():
                definition[f"meta:hash_{param}"] = new_hash

    for definition_csv, rows in definitions.items():
        all_columns = set()
        [[all_columns.add(param) for param in row] for row in rows]
        new_columns = all_columns - set(definition_columns[definition_csv])
        columns = definition_columns[definition_csv] + list(new_columns)
        definition_path = os.path.join(definitions_dir, definition_csv)
        with open(definition_path, 'w', newline='', encoding='utf-8') as csv_file:
            csv_writer = csv.DictWriter(csv_file, columns)
            csv_writer.writeheader()
            csv_writer.writerows(rows)


def get_library_sizes():
    ret = dict()
    if platform.system() == 'Windows':
        paths = zip(('api', 'core'), ('lcevc_dec_api.dll', 'lcevc_dec_core.dll'))
    elif platform.system() == 'Darwin':
        paths = zip(('api', 'core'), ('lcevc_dec_api.framework/lcevc_dec_api',
                    'lcevc_dec_core.framework/lcevc_dec_core'))
    else:
        paths = zip(('api', 'core'), ('liblcevc_dec_api.*', 'liblcevc_dec_core.*'))
    for name, glob_path in paths:
        lib_path = glob.glob(os.path.join(config.get('BIN_DIR'), '..', '**', glob_path))
        if lib_path:
            lib_path = lib_path[0]
            lib_size = os.path.getsize(lib_path)
            ret[name] = lib_size

    return ret
