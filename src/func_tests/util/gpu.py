import subprocess
import pynvml
import threading
import time
from datetime import datetime

from functools import reduce

from .benchmark import BenchmarkMetric, BenchmarkMetricError, BenchmarkResult, StatType


GPU_INFO_KEYS = [
    'uuid',
    'index',
    'name',
    'memory.total',
]

GPU_SM_UTILIZATION = 'gpu_util_01_sm'
GPU_ENC_UTILIZATION = 'gpu_util_02_enc'
GPU_DEC_UTILIZATION = 'gpu_util_03_dec'
GPU_MEM_UTILIZATION = 'gpu_util_04_mem'
GPU_MEM_USAGE = 'gpu_util_05_mem_usage'
GPU_TEMPERATURE = 'gpu_util_06_temp'
GPU_CLOCK_SPEED = 'gpu_util_08_clock_speed'
GPU_THROTTLE_REASONS = 'gpu_util_09_throttle_reasons'

GPU_UTILIZATION_KEYS = [
    GPU_SM_UTILIZATION,
    GPU_ENC_UTILIZATION,
    GPU_DEC_UTILIZATION,
    GPU_MEM_UTILIZATION,
    GPU_MEM_USAGE,
    GPU_TEMPERATURE,
    GPU_CLOCK_SPEED,
    GPU_THROTTLE_REASONS,
]

GPU_METRICS_DEF = {
    GPU_SM_UTILIZATION: ('GPU SM Utilization', '%', StatType.Mean),
    GPU_ENC_UTILIZATION: ('GPU Encode Utilization', '%', StatType.Mean),
    GPU_DEC_UTILIZATION: ('GPU Decode Utilization', '%', StatType.Mean),
    GPU_MEM_UTILIZATION: ('GPU Memory Utilization', '%', StatType.Mean),

    # tracking maximum memory usage during the run as the most extreme scenario
    GPU_MEM_USAGE: ('GPU Memory Usage', 'MB', StatType.Max),

    # to check difference in min and max temperature during the run
    GPU_TEMPERATURE: ('GPU Temperature', 'C', StatType.Min | StatType.Max),


    # to check that clock speed was stable during the run
    # if min clock speed != max clock speed then results may be not so reliable
    GPU_CLOCK_SPEED: ('GPU Clock Speed', 'MHz', StatType.Min | StatType.Max),

    # to check if at any moment during the run GPU was throttled and why
    GPU_THROTTLE_REASONS: ('GPU Throttle Reasons', '', StatType.Set)
}


def metric_error(m):
    name, units, stat_types = GPU_METRICS_DEF[m]
    return BenchmarkMetricError(name=name, stat_types=stat_types, units=units)


GPU_METRIC_ERRORS = dict(
    map(lambda m: (m, metric_error(m)), GPU_METRICS_DEF.keys()))


def get_gpu_info():
    all_keys = ','.join(GPU_INFO_KEYS)

    sp = subprocess.Popen([
        'nvidia-smi',
        f'--query-gpu={all_keys}',
        '--format=csv'
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    out_str = sp.communicate()
    out_list = list(filter(lambda l: len(l) > 0,
                    out_str[0].decode('utf-8').split('\n')))

    all_gpus = []

    for gpu_csv_info in out_list[1:]:
        gpu_info_dict = dict(zip(GPU_INFO_KEYS, map(
            lambda w: w.strip(), gpu_csv_info.split(','))))
        all_gpus.append(gpu_info_dict)

    return all_gpus


GPU_THROTTLE_REASON_NAMES = {
    pynvml.nvmlClocksThrottleReasonNone: 'None',
    pynvml.nvmlClocksThrottleReasonApplicationsClocksSetting: 'ApplicationsClocksSetting',
    pynvml.nvmlClocksThrottleReasonDisplayClockSetting: 'DisplayClockSetting',
    pynvml.nvmlClocksThrottleReasonGpuIdle: 'GpuIdle',
    pynvml.nvmlClocksThrottleReasonHwPowerBrakeSlowdown: 'HwPowerBrakeSlowdown',
    pynvml.nvmlClocksThrottleReasonHwSlowdown: 'HwSlowdown',
    pynvml.nvmlClocksThrottleReasonHwThermalSlowdown: 'HwThermalSlowdown',
    pynvml.nvmlClocksThrottleReasonSwPowerCap: 'SwPowerCap',
    pynvml.nvmlClocksThrottleReasonSwThermalSlowdown: 'SwThermalSlowdown',
    pynvml.nvmlClocksThrottleReasonSyncBoost: 'SyncBoost',
}


class GpuMonitor(object):
    def __init__(self, gpu_index: int):
        pynvml.nvmlInit()
        self._gpu_index = gpu_index
        self._device = pynvml.nvmlDeviceGetHandleByIndex(self._gpu_index)
        self._started = False
        self._util = {}
        self._all_throttle_reasons = set()

    def get_current_temperature(self):
        return pynvml.nvmlDeviceGetTemperature(
            self._device, pynvml.NVML_TEMPERATURE_GPU)

    def get_slowdown_temperature_thresholds(self):
        d = {'shutdown': pynvml.NVML_TEMPERATURE_THRESHOLD_SHUTDOWN,
             'slowdown': pynvml.NVML_TEMPERATURE_THRESHOLD_SLOWDOWN,
             'mem_max': pynvml.NVML_TEMPERATURE_THRESHOLD_MEM_MAX,
             'gpu_max': pynvml.NVML_TEMPERATURE_THRESHOLD_GPU_MAX,
             'acoustic_min': pynvml.NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MIN,
             'acoustic_curr': pynvml.NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_CURR,
             'acoustic_max': pynvml.NVML_TEMPERATURE_THRESHOLD_ACOUSTIC_MAX}
        thresholds = {}
        for k, v in d.items():
            try:
                thresholds[k] = pynvml.nvmlDeviceGetTemperatureThreshold(
                    self._device, v)
            except pynvml.NVMLError_NotSupported:
                pass
        return thresholds

    def get_throttle_reasons(self):
        throttle_reasons = []
        throttle_reasons_mask = pynvml.nvmlDeviceGetCurrentClocksThrottleReasons(
            self._device)
        for reason, name in GPU_THROTTLE_REASON_NAMES.items():
            if throttle_reasons_mask & reason:
                # never allow None into the set so that it
                # doesn't get aggregated with actual throttle reasons
                if reason == pynvml.nvmlClocksThrottleReasonNone:
                    continue
                throttle_reasons.append(name)
        return throttle_reasons

    def set_clock(self, clock_type, clock_setter):
        max_frequency = pynvml.nvmlDeviceGetMaxClockInfo(
            self._device, clock_type)
        clock_setter(self._device, max_frequency, max_frequency)

    def set_clocks(self):
        self.set_clock(pynvml.NVML_CLOCK_SM,
                       pynvml.nvmlDeviceSetGpuLockedClocks)
        # self.set_clock(pynvml.NVML_CLOCK_MEM,
        #                pynvml.nvmlDeviceSetMemoryLockedClocks)

    def reset_clocks(self):
        pynvml.nvmlDeviceResetGpuLockedClocks(self._device)
        # pynvml.nvmlDeviceResetMemoryLockedClocks(self._device)

    def _thread_func(self, stop_event):
        last_ts = int(datetime.now().timestamp() * 1000000)
        while not stop_event.is_set():
            procs = pynvml.nvmlDeviceGetComputeRunningProcesses(
                self._device)
            try:
                temperature = pynvml.nvmlDeviceGetTemperature(
                    self._device, pynvml.NVML_TEMPERATURE_GPU)
                throttle_reasons = self.get_throttle_reasons()

                clock_speed = pynvml.nvmlDeviceGetClockInfo(
                    self._device, pynvml.NVML_CLOCK_SM)

                utils = pynvml.nvmlDeviceGetProcessUtilization(
                    self._device, last_ts)
                for util in utils:
                    pid = util.pid
                    if pid == 0:
                        continue
                    proc = next(
                        filter(lambda proc: proc.pid == pid, procs), None)
                    measurement = {GPU_SM_UTILIZATION: util.smUtil, GPU_ENC_UTILIZATION: util.encUtil,
                                   GPU_DEC_UTILIZATION: util.decUtil, GPU_MEM_UTILIZATION: util.memUtil,
                                   GPU_TEMPERATURE: temperature,
                                   GPU_THROTTLE_REASONS: throttle_reasons,
                                   GPU_CLOCK_SPEED: clock_speed,
                                   }
                    if proc:
                        used_gpu_mem = proc.usedGpuMemory if proc.usedGpuMemory is not None else 0
                        measurement[GPU_MEM_USAGE] = int(
                            used_gpu_mem / (1024.0 * 1024.0))
                    last_ts = util.timeStamp
                    if pid in self._util:
                        self._util[pid].append(measurement)
                    else:
                        self._util[pid] = [measurement]
            except pynvml.nvml.NVMLError_NotFound as err:
                pass
            time.sleep(0.5)

        # discarding first and last sample as they're partially collected when application is idle
        for pid in self._util:
            if len(self._util[pid]) > 1:
                self._util[pid] = self._util[pid][1:]
            if len(self._util[pid]) > 1:
                self._util[pid] = self._util[pid][:-1]

    def start(self):
        if self._started:
            raise RuntimeError(f'GPU monitor already started, failed to start')

        self._stop_event = threading.Event()
        self._thread = threading.Thread(
            target=self._thread_func, args=(self._stop_event,), daemon=True)
        self._started = True
        self._thread.start()

    def stop(self):
        if not self._started:
            raise RuntimeError(f'GPU monitor not started, failed to stop')

        self._stop_event.set()
        self._started = False
        self._thread.join()

    def clear(self):
        self._util = {}
        self._all_throttle_reasons = set()

    def get_utilization_metrics(self, pid: int):
        if pid not in self._util:
            print(
                f'PID {pid} not observed during monitoring, seen PIDs: {self._util.keys()}')
            return None

        prev_mem_usage = 0
        for u in self._util[pid]:
            if GPU_MEM_USAGE not in u:
                u[GPU_MEM_USAGE] = prev_mem_usage
            else:
                prev_mem_usage = u[GPU_MEM_USAGE]

        kwargs = {}
        for metric, metric_def in GPU_METRICS_DEF.items():
            name, units, stat_types = metric_def
            if metric == GPU_THROTTLE_REASONS:
                metric_values = set(
                    reduce(lambda a, b: a + b, map(lambda u: u[metric], self._util[pid])))
            else:
                metric_values = list(map(lambda u: u[metric], self._util[pid]))
            kwargs[metric] = BenchmarkMetric.from_raw(
                metric_values,
                name=name,
                units=units,
                stat_types=stat_types
            )

        return BenchmarkResult(**kwargs)
