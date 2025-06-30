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
import csv
import copy
import platform

from utilities.config import config, logger


def load_tests(definitions_dir):
    test_platform = config.get('PLATFORM', platform.system())
    logger.info(f"Loading tests from CSVs for platform '{test_platform}'...")
    assert os.path.isdir(definitions_dir), f"Cannot locate test definitions from '{definitions_dir}', " \
        f"please set TEST_DEFINITIONS_DIR in the config"
    if filter_group := os.environ.get('FILTER_GROUP', config.get('FILTER_GROUP')):
        logger.info(f"Filtering tests to group '{filter_group}'")
    tests = list()
    for definition_csv in os.listdir(definitions_dir):
        if not definition_csv.endswith('.csv'):
            continue
        definition_path = os.path.join(definitions_dir, definition_csv)
        with open(definition_path, 'r') as csv_file:
            csv_reader = csv.DictReader(csv_file)
            for csv_row, raw_test in enumerate(csv_reader):
                test = parse_test(raw_test, filter_group)
                test['id'] = len(tests)
                test['definition_csv'] = definition_csv
                test['csv_row'] = csv_row
                tests.append(test)
    auto_gen_names(tests)
    check_duplicates(tests)
    if filter_name := os.environ.get('FILTER_NAME', config.get('FILTER_NAME')):
        logger.info(f"Filtering tests by name substring '{filter_name}'")
        for test in tests:
            if test['enabled'] and filter_name not in test['name']:
                test['enabled'] = False
    enabled_count = len([t for t in tests if t['enabled']])
    logger.info(f"Loaded {len(tests)} {test_platform} {config.get('LEVEL')} tests"
                f" ({enabled_count} enabled)")
    return tests


def parse_test(test, filter_group):
    assert 'Test Function' in test and 'Group' in test, \
        "Test is missing required parameters: 'Test Function' and 'Group'"
    params = dict(function=test['Test Function'], group=test['Group'], notes=test.get('Notes'))
    enabled_platform = f"Enabled-{config.get('PLATFORM', platform.system())}"
    assert enabled_platform in test, f"Test is missing an 'Enabled' parameter for the current platform " \
        f"'{config.get('PLATFORM', platform.system())}' ensure all tests have a defined " \
        f"enabled column and the correct 'PLATFORM' is set in the config"
    level = config.get('LEVEL', '').lower()
    assert level in ('mr', 'nightly', 'manual', 'pgo'), "Test level must be either 'MR'," \
        " 'Nightly', 'Manual', or 'PGO' - set with 'LEVEL' config item"

    assert test[enabled_platform] in ('MR', 'Nightly', 'BROKEN', 'SKIP', 'PGO'), f"Test CSV column '{enabled_platform}' " \
        f"must be either: 'Nightly', 'MR', 'BROKEN', 'SKIP', or 'PGO', not '{test.get(enabled_platform)}'"
    params['enabled_type'] = test[enabled_platform].lower()
    enabled = (level == 'nightly' and params['enabled_type'] in ('nightly', 'mr')) or \
              (level in ('mr', 'manual') and params['enabled_type'] == 'mr') or \
              (level == 'pgo' and params['enabled_type'] == 'pgo')
    enabled &= (not filter_group or filter_group == params['group'])
    params['enabled'] = enabled

    if 'Name' in test:
        params['name'] = test['Name']
    if "Performance Group" in test:
        params['performance_group'] = test['Performance Group']
    for key, value in test.items():
        if ':' not in key:
            continue
        group, param = key.split(':')
        if group not in params:
            params[group] = dict()
        if value != '':
            params[group][param] = value
    return params


def auto_gen_names(tests):
    groups = set([test['group'] for test in tests])
    for group in groups:
        group_tests = list(filter(lambda d: d['group'] == group, tests))
        all_params = set()
        for test in group_tests:
            for param_name, param_group in test.items():
                if isinstance(param_group, dict) and param_name != 'meta':
                    [all_params.add(f"{param_name}:{param}") for param in param_group
                     if test[param_name][param] != '']
        iterated_params = set()
        for param in all_params:
            param_group, param_name = param.split(':')
            if 'base' not in param_name and 'lcevc' not in param_name and \
                    len(set([test[param_group][param_name] for test in group_tests
                             if test.get(param_group, {}).get(param_name)])) > 1:
                iterated_params.add(param)
        for test in group_tests:
            if not test.get('name'):
                name = list()
                for param in iterated_params:
                    param_group, param_name = param.split(':')
                    value = test[param_group][param_name]
                    if '/' in value:
                        value = value.split('/')[-1]
                    if '\\' in value:
                        value = value.split('\\')[-1]
                    if value != '':
                        name.append(f"{param_name.replace('-', '')}:{value}")
                test['name'] = '-'.join(name)


def check_duplicates(tests):
    for ref_test in tests:
        for check_test in tests:
            if ref_test['id'] != check_test['id'] and ref_test['name'] == check_test['name']:
                raise AssertionError(f"Test {ref_test['id']} ({ref_test['definition_csv']}:{ref_test['csv_row']}) has "
                                     f"the same name as {check_test['id']} ({check_test['definition_csv']}:{check_test['csv_row']})")
    dupe_tests = copy.deepcopy(tests)
    for test in dupe_tests:
        del test['id']
        del test['definition_csv']
        del test['csv_row']
        del test['name']
        del test['group']
    for ref_id, ref_test in enumerate(dupe_tests):
        for check_id, check_test in enumerate(dupe_tests):
            if ref_id != check_id and ref_test == check_test:
                full_ref_test = tests[ref_id]
                full_check_test = tests[check_id]
                raise AssertionError(f"Test {full_ref_test['id']} '{full_ref_test['name']}' ({full_ref_test['definition_csv']}:"
                                     f"{full_ref_test['csv_row']}) has the same parameters as test {full_check_test['id']} "
                                     f"'{full_check_test['name']}' ({full_check_test['definition_csv']}:{full_check_test['csv_row']})")


def csv_json_to_dict(csv_args):
    json_config = dict()
    for param, value in csv_args.items():
        if value == '':
            value = None
        elif value == 'TRUE' or value == 'FALSE':
            value = (value == 'TRUE')
        else:
            try:
                if '.' in value:
                    value = float(value)
                elif value.startswith('0x'):
                    value = int(value, 16)
                else:
                    value = int(value)
            except (TypeError, ValueError):
                pass

        if value is not None:
            json_config[param] = value

    return json_config
