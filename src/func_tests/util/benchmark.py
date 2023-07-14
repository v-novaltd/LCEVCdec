import numpy as np
import json

from .stats import StatType, AllStatTypes, StatDefs, calc_stats, two_sample_t_test, TTestType, TTestStats

from functools import reduce

csv_blacklist = [StatType.NumSamples]


class BenchmarkMetric:
    def __init__(self, stats: dict,
                 name: str = None, units: str = None, raw_data=None,
                 filter_outliers: bool = False):
        if StatType.NumSamples not in stats:
            raise RuntimeError(
                f'Required stat NumSamples is missing for metric {name}')
        self.stats = stats
        self.name = name
        self.units = units
        self.raw_data = raw_data
        self.filter_outliers = filter_outliers

    def has_stat(self, stat_type):
        return stat_type in self.stats

    def get_stat(self, stat_type):
        return self.stats[stat_type] if self.has_stat(stat_type) else None

    @property
    def stat_types(self):
        return list(filter(lambda stat_type: self.has_stat(stat_type), AllStatTypes))

    @property
    def mean(self):
        return self.get_stat(StatType.Mean)

    @property
    def n(self):
        return self.get_stat(StatType.NumSamples)

    @property
    def variance(self):
        return self.get_stat(StatType.Variance)

    @property
    def min(self):
        return self.get_stat(StatType.Min)

    @property
    def max(self):
        return self.get_stat(StatType.Max)

    @property
    def ci_half(self):
        return self.get_stat(StatType.ConfidenceInterval)

    @property
    def ci(self):
        if self.mean is None or self.ci_half is None:
            return None
        return [self.mean - self.ci_half, self.mean + self.ci_half]

    def to_string(self, short=False):
        str = f'{self.name}: ' if self.name and not short else ''
        for stat_type in self.stat_types:
            str += f' {StatDefs[stat_type].id}={self.get_stat(stat_type)}'
        str += f' units={self.units}' if self.units else ''
        return str

    def to_dict(self):
        v = {}
        if self.name is not None:
            v['name'] = self.name
        for stat_type in self.stat_types:
            v[StatDefs[stat_type].id] = self.get_stat(stat_type)
        if self.units:
            v['units'] = self.units
        return v

    def to_json(self):
        return json.dumps(self.to_dict())

    def get_ttest_stats(self):
        return TTestStats(self.mean, self.n, self.variance)

    @staticmethod
    def from_string(str):
        if ':' in str:
            name, val_str = tuple(str.split(':'))
            val_str = val_str.strip()
        else:
            name = None
            val_str = str

        units = None
        stats = {}

        for token in val_str.split(' '):
            if token.startswith('units='):
                units = token.replace('units=', '')
                continue
            for stat_type in AllStatTypes:
                stat_token_prefix = f'{StatDefs[stat_type].id}='
                if token.startswith(stat_token_prefix):
                    stats[stat_type] = float(
                        token.replace(stat_token_prefix, ''))

        return BenchmarkMetric(stats=stats, name=name, units=units)

    @staticmethod
    def from_dict(v):

        def lookup_default(v, k, default):
            if k in v:
                return v[k]
            return default

        name = lookup_default(v, 'name', None)
        units = lookup_default(v, 'units', None)

        stats = {}
        for stat_type in AllStatTypes:
            stat_id = StatDefs[stat_type].id
            if stat_id in v:
                stats[stat_type] = v[stat_id]

        return BenchmarkMetric(stats, name=name, units=units)

    @staticmethod
    def from_json(json_str):
        return BenchmarkMetric.from_dict(json.loads(json_str))

    @staticmethod
    def from_raw(values: list, stat_types: int, name: str = None, units: str = None, filter_outliers: bool = False):
        stats, raw_data = calc_stats(
            values, stat_types, filter_outliers=filter_outliers)

        return BenchmarkMetric(stats=stats,
                               name=name, units=units, raw_data=raw_data,
                               filter_outliers=filter_outliers)

    @staticmethod
    def aggregate(runs: list):
        name = runs[0].name
        units = runs[0].units

        if not all([r.name == name for r in runs]):
            raise RuntimeError(f'Name mismatch during metric aggregation')

        if not all([r.units == units for r in runs]):
            raise RuntimeError(f'Units mismatch during metric aggregation')

        aggregate_stat_types = 0
        for stat_type in AllStatTypes:
            if all([r.has_stat(stat_type) for r in runs]):
                aggregate_stat_types = aggregate_stat_types | stat_type
        filter_outliers = all([r.filter_outliers is None for r in runs])

        def aggregate_if_not_none(pick, aggregate):
            vals = np.array(list(map(pick, runs)))
            if all([v is None for v in vals]):
                return None
            return aggregate(vals)

        have_raw_data = runs[0].raw_data is not None
        if have_raw_data:
            if isinstance(runs[0].raw_data, set):
                raw_data_concat = set()
                for r in runs:
                    for v in r.raw_data:
                        raw_data_concat.add(v)
            else:
                raw_data_concat = np.concatenate(
                    [r.raw_data for r in runs])
            stats, raw_data = calc_stats(
                raw_data_concat, stat_types=aggregate_stat_types, filter_outliers=filter_outliers)
        else:
            stats = {}
            for stat_type in AllStatTypes:
                stat_def = StatDefs[stat_type]
                if aggregate_stat_types & stat_type:
                    stats[stat_type] = aggregate_if_not_none(
                        lambda r: r.get_stat(stat_type), stat_def.aggregate)
            raw_data = None

        return BenchmarkMetric(stats, name=name, units=units, raw_data=raw_data)

    def equal_to(self, other):
        # Null hypothesis
        # Mean of current measurements is the same as mean of other measurements
        # Alternative hypothesis
        # Means of current measurements and other measurements are not the same
        return not two_sample_t_test(self.get_ttest_stats(), other.get_ttest_stats(), test=TTestType.DoubleTailed)

    def worse_than(self, other):
        # Null hypothesis
        # Mean of current measurements is not less than mean of other measurements
        # Alternative hypothesis
        # Means of current measurements is less than mean of other measurements
        return two_sample_t_test(self.get_ttest_stats(), other.get_ttest_stats(), test=TTestType.LeftTailed)

    def better_than(self, other):
        # Null hypothesis
        # Mean of current measurements is not greater than mean of other measurements
        # Alternative hypothesis
        # Means of current measurements is greater than mean of other measurements
        return two_sample_t_test(self.get_ttest_stats(), other.get_ttest_stats(), test=TTestType.RightTailed)

    def csv_header(self):
        unit_suffix = f' ({self.units})' if self.units else ''

        header = []
        for stat_type in self.stat_types:
            if stat_type in csv_blacklist:
                continue
            header.append(
                f'{self.name} {StatDefs[stat_type].name}{unit_suffix}')
        return header

    def csv_values(self):
        values = []
        for stat_type in self.stat_types:
            if stat_type in csv_blacklist:
                continue
            stat_def = StatDefs[stat_type]
            if stat_def.value_type is float:
                value = round(self.get_stat(stat_type), 2)
            else:
                value = self.get_stat(stat_type)
            values.append(value)
        return values

    def csv(self):
        return list(zip(self.csv_header(), self.csv_values()))

    def raw_csv_values(self):
        return list(self.raw_data)


class BenchmarkResult:
    def __init__(self, **kwargs):
        self._metrics = {}
        for key in kwargs.keys():
            self._metrics[key] = kwargs[key]

    def __getattr__(self, name):
        if name != '_metrics' and name in self._metrics:
            return self._metrics[name]
        else:
            return object.__getattribute__(self, name)

    # aggregates multiple runs of BenchmarkResults generated for same set of metrics
    @staticmethod
    def aggregate(runs: list):
        first_run = runs[0]
        seen_metrics = first_run._metrics.keys()
        aggregate_metrics = {}
        for metric in seen_metrics:
            aggregate_metrics[metric] = BenchmarkMetric.aggregate(
                list(map(lambda run: run._metrics[metric], runs)))
        return BenchmarkResult(**aggregate_metrics)

    # combines multiple BenchmarkResults generated for different sets of metrics
    @staticmethod
    def combine(results: list):
        combined_metrics = {}
        for result in results:
            for metric in result._metrics:
                if metric in combined_metrics:
                    raise RuntimeError(
                        f'Metric {metric} clash during BenchmarkResult.combine')
                combined_metrics[metric] = result._metrics[metric]
        return BenchmarkResult(**combined_metrics)

    def csv_header(self):
        sorted_metrics = sorted(self._metrics.keys())
        header = []
        for k in sorted_metrics:
            header += self._metrics[k].csv_header()
        return header

    def csv_values(self):
        sorted_metrics = sorted(self._metrics.keys())
        values = []
        for k in sorted_metrics:
            values += self._metrics[k].csv_values()
        return values

    def csv(self):
        return list(zip(self.csv_header(), self.csv_values()))

    def to_string(self):
        sorted_metrics = sorted(self._metrics.keys())
        str = ''
        for k in sorted_metrics:
            str += ', ' + self._metrics[k].to_string()
        return str[2:] if len(str) > 0 else str

    def to_dict(self):
        d = {}
        for k in self._metrics.keys():
            d[k] = self._metrics[k].to_dict()
        return d


class BenchmarkMetricError(BenchmarkMetric):
    def __init__(self, name: str, stat_types: int, units: str = None):
        stats = {StatType.NumSamples: 0}
        for stat_type in AllStatTypes:
            if stat_types & stat_type:
                stats[stat_type] = '-'
        super().__init__(
            stats, name=name, units=units)


class BenchmarkError(Exception):
    def __init__(self, err, **kwargs):
        self._error = err
        self._metrics = {}
        for key in kwargs.keys():
            self._metrics[key] = kwargs[key]

    @property
    def error(self):
        return self._error

    def csv_header(self):
        header = []
        for k in sorted(self._metrics.keys()):
            header += self._metrics[k].csv_header()
        return header

    def csv_values(self):
        return ['-'] * len(self.csv_header())

    def csv(self):
        return list(zip(self.csv_header(), self.csv_values()))
