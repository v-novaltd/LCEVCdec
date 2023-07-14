import numpy as np
import scipy.stats
import math
from enum import Enum

from collections import namedtuple
from functools import reduce


class StatType:
    # special case - always required
    NumSamples = 0x0000000000000001

    # optional stats
    Mean = 0x0000000000000002
    Variance = 0x0000000000000004
    Min = 0x0000000000000008
    Max = 0x0000000000000010
    ConfidenceInterval = 0x0000000000000020
    Set = 0x0000000000000040


AllStatTypes = [StatType.NumSamples, StatType.Mean,
                StatType.Variance, StatType.Min, StatType.Max,
                StatType.ConfidenceInterval, StatType.Set]


class StatDef:
    def __init__(self, id, name, value_type, aggregate):
        self.id = id
        self.name = name
        self.value_type = value_type
        self.aggregate = aggregate


StatDefs = {
    StatType.NumSamples: StatDef('n', 'Number of Samples', int, lambda arr: np.sum(arr)),
    StatType.Mean: StatDef('mean', 'Mean', float, lambda arr: arr.mean()),
    StatType.Variance: StatDef('variance', 'Variance', float, lambda arr: arr.mean()),
    StatType.Min: StatDef('min', 'Min', float, lambda arr: arr.min()),
    StatType.Max: StatDef('max', 'Max', float, lambda arr: arr.max()),
    StatType.ConfidenceInterval: StatDef(
        'ci_half', 'Confidence Interval Half', float, lambda arr: arr.mean()),
    StatType.Set: StatDef(
        'set', 'Seen', str, lambda arr: ' '.join(set(reduce(lambda a, b: a + b, map(lambda a: a.split(' '), arr))))),
}


class TTestType(Enum):
    LeftTailed = 0
    RightTailed = 1
    DoubleTailed = 2


def test_name(test: TTestType):
    if test == TTestType.LeftTailed:
        return 'left tailed'
    elif test == TTestType.RightTailed:
        return 'right tailed'
    elif test == TTestType.DoubleTailed:
        return 'double tailed'
    return 'unknown'


TTestStats = namedtuple('Stats', ['mean', 'n', 'variance'])


# Statistical Two-Sample t-Test for unequal variances
# Source: https://www.jmp.com/en_gb/statistics-knowledge-portal/t-test/two-sample-t-test.html
# returns:
# - True if Null Hypothesis is rejected and Alternate Hypothesis is assumed
# - False if Null Hypothesis is not rejected
def two_sample_t_test(lhs: TTestStats, rhs: TTestStats, test: TTestType = TTestType.DoubleTailed, alpha: float = 0.05) -> float:
    # print(f'{lhs} vs {rhs} test: {test_name(test)}')

    lhs_s = lhs.variance / lhs.n
    rhs_s = rhs.variance / rhs.n

    df_nom = math.pow(lhs_s + rhs_s, 2)
    df_denom = lhs_s**2 / (lhs.n - 1) + rhs_s**2 / (rhs.n - 1)

    df = df_nom / df_denom

    mean_diff = lhs.mean - rhs.mean
    denom = np.sqrt(lhs_s + rhs_s)

    t = mean_diff / denom

    if test == TTestType.LeftTailed:
        q = alpha
    elif test == TTestType.RightTailed:
        q = 1 - alpha
    elif test == TTestType.DoubleTailed:
        t = abs(t)
        q = 1 - alpha / 2.0

    critical_t = scipy.stats.t.ppf(q=q, df=df)

    if test == TTestType.LeftTailed:
        result = t < critical_t
    else:
        result = t > critical_t

    # print(f't={t} critical t={critical_t} result={result}')

    return result


def mean_confidence_interval(data, confidence=0.95):
    flat_data = data.flatten()
    mean, se = np.mean(flat_data), scipy.stats.sem(flat_data)
    confidence_interval_half = se * \
        scipy.stats.t.ppf((1 + confidence) / 2., len(flat_data) - 1)
    return mean, confidence_interval_half


def calc_stats(values, stat_types: int, filter_outliers=False):
    if isinstance(values, set):
        stats = {StatType.NumSamples: len(values)}
        for stat_type in AllStatTypes:
            if stat_type == StatType.NumSamples:
                continue
            if stat_type & stat_types:
                if stat_type != StatType.Set:
                    raise RuntimeError(
                        f'Only know how to aggregate set for StatType.Set, not {stat_type}')
                else:
                    stats[StatType.Set] = ' '.join(values)
        return stats, values

    if len(values) == 0:
        return {StatType.NumSamples: 0}, []

    np_data = to_numpy(values)
    n = len(np_data)
    mean, ci_half = mean_confidence_interval(np_data)
    variance = np.var(np_data, ddof=1)
    minv = np.min(np_data)
    maxv = np.max(np_data)

    if filter_outliers:
        sigma = np.sqrt(variance)
        valid_range = [mean - 3 * sigma, mean + 3 * sigma]

        filtered_data = np.array(
            list(filter(lambda v: inside_interval(v, valid_range), np_data)))

        n = len(filtered_data)
        mean, ci_half = mean_confidence_interval(filtered_data)
        variance = np.var(filtered_data, ddof=1)
        minv = np.min(filtered_data)
        maxv = np.max(filtered_data)

    stats = {StatType.NumSamples: n}
    if stat_types & StatType.Mean:
        stats[StatType.Mean] = mean
    if stat_types & StatType.ConfidenceInterval:
        stats[StatType.ConfidenceInterval] = ci_half
    if stat_types & StatType.Variance:
        stats[StatType.Variance] = variance
    if stat_types & StatType.Min:
        stats[StatType.Min] = minv
    if stat_types & StatType.Max:
        stats[StatType.Max] = maxv

    return stats, np_data


def intervals_overlap(interval_lhs, interval_rhs):
    no_overlap = interval_lhs[1] < interval_rhs[0] or interval_lhs[0] > interval_rhs[1]
    return not no_overlap


def inside_interval(val, interval):
    return val >= interval[0] and val <= interval[1]


def to_numpy(data):
    if len(data) > 0 and type(data[0]) is list:
        min_len = len(data[0])
        for row in data:
            if len(row) < min_len:
                min_len = len(row)

        np_data = np.array([np.array(row[:min_len], dtype='float')
                            for row in data])
    else:
        np_data = np.array(data, dtype='float')
    return np_data
