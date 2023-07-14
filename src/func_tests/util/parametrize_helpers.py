#  Copyright (c) V-Nova International Limited 2022. All rights reserved.

import itertools
import pytest


class ParamMatcher:
    def __init__(self, values):
        self.values = values
        self.used = False

    def matches(self, other):
        return all(self.match(x, y) for x, y in zip(self.values, other))

    def match(self, against, y):
        if callable(against):
            return against(y)
        else:
            return against == y

    def __repr__(self):
        return str(self.values)


def find_and_mark_used(matchers, search_for):
    for m in matchers:
        if m.matches(search_for):
            m.used = True
            return True
    else:
        return False


def assert_all_matchers_used(list_type, matchers):
    for m in matchers:
        if not m.used:
            raise ValueError(
                f'No tests found matching {list_type} specification {m}')


def generate_marked_parameters(*args, xfail=None, skip=None):
    """
    Convert a list of lists of values into a list of pytest parameters, taking the inner product
    combination of the values lists.

    Values on the list may be instances of pytest ParameterSets, in which case the existing marks
    are preserved.
    :param args: list of lists of values
    :param xfail: list of tuples of combinations of values that should be marked as xfail
    :param skip: list of tuples of combinations of values that should be marked as skip
    :return: list of pytest parameters for passing into pytest.parametrize
    """

    if xfail is None:
        xfail = []
    else:
        # In case the user passes in list-of-lists instead of list-of-tuple
        xfail = [ParamMatcher(x) for x in xfail]

    if skip is None:
        skip = []
    else:
        # In case the user passes in list-of-lists instead of list-of-tuple
        skip = [ParamMatcher(x) for x in skip]

    combinations = itertools.product(*args)
    params = []
    for combination in combinations:
        marks = []
        values = []
        for v in combination:
            if hasattr(v, 'marks'):
                marks += v.marks

            if hasattr(v, 'values'):
                values += v.values
            else:
                values.append(v)

        tup = tuple(values)

        if find_and_mark_used(xfail, tup):
            marks.append(pytest.mark.xfail(strict=True))

        if find_and_mark_used(skip, tup):
            marks.append(pytest.mark.skip)

        params.append(pytest.param(*values, marks=marks, id=None))

    assert_all_matchers_used('xfail', xfail)
    assert_all_matchers_used('skip', skip)

    return params
