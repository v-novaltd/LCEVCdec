import ffmpeg
import platform
import os
from subprocess import TimeoutExpired
import re
import copy
import math


VSTATS_FILENAME = 'vstats.log'

FFMPEG_LOG_FRAME_REGEXP = re.compile(r'.*frame=\s*([0-9]+).*')
FFMPEG_LOG_FPS_REGEXP = re.compile(r'.*fps=\s*([0-9]+(?:.[0-9]+)?).*')
FFMPEG_LOG_PERIOD_REGEXP = re.compile(r'.*period=\s*([0-9]+).*')

FFMPEG_RESOLUTIONS_DEF = {
    '360p': ('640', '360'),
    '480p': ('854', '480'),
    '540p': ('960', '540'),
    '720p': ('1280', '720'),
    '1080p': ('1920', '1080'),
    '1920p': ('3680', '1920'),
    '4k': ('3840', '2160'),
}

FFMPEG_RESOLUTIONS = dict(map(lambda kv: (
    kv[0], f'{kv[1][0]}x{kv[1][1]}'), FFMPEG_RESOLUTIONS_DEF.items()))

FFMPEG_SCALE_FILTERS = dict(map(lambda kv: (
    kv[0], f'w={kv[1][0]}:h={kv[1][1]}'), FFMPEG_RESOLUTIONS_DEF.items()))

RAW_CPU_INPUT = 'raw_cpu'
RAW_CUDA_INPUT = 'raw_cuda'
RAW_VULKAN_INPUT = 'raw_vulkan'
COMPRESSED_CUDA_INPUT = 'compressed_cuda'
COMPRESSED_VULKAN_INPUT = 'compressed_vulkan'

GPU_INPUT_TYPES = [
    COMPRESSED_CUDA_INPUT,
    COMPRESSED_VULKAN_INPUT,
    RAW_CUDA_INPUT,
    RAW_VULKAN_INPUT
]

COMPRESSED_INPUT_TYPES = [
    COMPRESSED_CUDA_INPUT,
    COMPRESSED_VULKAN_INPUT
]

VULKAN_INPUT_TYPES = [
    RAW_VULKAN_INPUT,
    COMPRESSED_VULKAN_INPUT
]

FFMPEG_INPUT_TYPES = [
    RAW_CPU_INPUT,
    *GPU_INPUT_TYPES
]


def input_is_gpu(input_type):
    return input_type in GPU_INPUT_TYPES


def input_is_compressed(input_type):
    return input_type in COMPRESSED_INPUT_TYPES


def input_is_vulkan(input_type):
    return input_type in VULKAN_INPUT_TYPES


#
# FPS values reported by FFmpeg are rounded
# and not reported for first second of processing
# To get a more reliable statistic we calculate
# FPS ourselves based upon number of frames processed
# and period durations reported on each log line
#
# Before returning statistics we discard a number
# of initial measurements according to
# duration of warmup period of the benchmark
#


def fps_from_ffmpeg_log(ffmpeg_log, warmup_duration):
    fps_log_lines = list(
        filter(lambda l: 'fps=' in l, ffmpeg_log.split('\n')))

    running_period = []
    running_frame = []
    running_fps = []

    for i, fps_log_line in enumerate(fps_log_lines):
        frame_reported_by_ffmpeg = int(
            FFMPEG_LOG_FRAME_REGEXP.match(fps_log_line).group(1))
        running_frame.append(frame_reported_by_ffmpeg)
        period_reported_by_ffmpeg = int(
            FFMPEG_LOG_PERIOD_REGEXP.match(fps_log_line).group(1))
        running_period.append(period_reported_by_ffmpeg)
        fps_reported_by_ffmpeg = float(
            FFMPEG_LOG_FPS_REGEXP.match(fps_log_line).group(1))
        running_fps.append(fps_reported_by_ffmpeg)

    moment_fps = []
    moment_t = []

    last_frame = 0
    last_t = 0
    first_idx = None
    for idx in range(len(running_frame)):
        frame = running_frame[idx]
        period = running_period[idx]
        period_t = period / 1000000.0

        if idx == 0 or period_t == 0.0:
            continue

        t = round(last_t + period_t, 2)
        fps = round((frame - last_frame) / period_t, 2)
        moment_fps.append(fps)
        moment_t.append(t)
        if t > warmup_duration and first_idx is None:
            first_idx = idx
        last_frame = frame
        last_t = t

    # print(f'FPS reported by FFmpeg: {running_fps}')

    moment_fps = moment_fps[first_idx:-1]
    moment_t = moment_t[first_idx:-1]

    # print(f'Moment FPS calculated: {moment_fps}')

    return moment_fps


VSTATS_LATENCY_RE = re.compile(r'.*enc_latency=\s*([0-9]*)us.*')


def enc_latency_from_vstats_log(vstats_filename):
    if not os.path.exists(vstats_filename):
        return []

    with open(vstats_filename, 'rt') as vstats_file:
        vstats_output = vstats_file.read()

    enc_latency = []
    for line in vstats_output.split('\n'):
        if not line:
            continue
        match = VSTATS_LATENCY_RE.match(line)
        if not match:
            continue
        enc_latency_measurement = float(
            match.group(1)) / 1000.0  # convert from us -> ms
        enc_latency.append(enc_latency_measurement)

    return enc_latency


class FFmpegError(Exception):
    def __init__(self, stdout, stderr):
        super(FFmpegError, self).__init__(
            '{} ffmpeg error (see stderr output for detail)'
        )
        self.stdout = stdout if stdout else None
        self.stderr = stderr if stderr else None

    def __repr__(self):
        return f'FFmpeg stdout:\n{self.stdout}\nFFmpeg stderr:\n{self.stderr}'


class CustomSelectorMultiOutputGraphBuilder:
    def __init__(self, input_selector):
        """input_selector should be a function accepting:
         a ffmpeg input node, an index, and the number of outputs and returning a stream"""
        self._input_selector = input_selector

    def __call__(self, inputs, outputs, filters, global_args):
        if len(inputs) != 1:
            raise ValueError('Wrong number of inputs')
        res = ffmpeg.input(inputs[0][0], **inputs[0][1])

        for f, filter_args in filters:
            res = res.filter(f, **filter_args)

        output_nodes = []
        # Copy the input selector; if it stores data it should be fresh for
        # every call to the graph builder
        input_selector = copy.deepcopy(self._input_selector)
        for idx, (output_file, output_args) in enumerate(outputs):
            stream = input_selector(res, idx, len(outputs))
            output_nodes.append(stream.output(output_file, **output_args))
        runner = ffmpeg.merge_outputs(*output_nodes)

        runner = runner.global_args(*global_args)

        return runner


class SingleInputGraphBuilder(CustomSelectorMultiOutputGraphBuilder):
    def __init__(self):
        super().__init__(self.select)
        self._split = None

    def select(self, stream, idx, count):
        if count > 1:
            if not self._split:
                self._split = stream.split()
            return self._split[idx]
        else:
            return stream


class MultiInputSingleOutputGraphBuilder:
    def __init__(self, input_selector=None):
        self._input_selector = input_selector if input_selector else lambda v, idx, count: v

    def __call__(self, inputs, outputs, filters, global_args):
        if len(outputs) != 1:
            raise ValueError('Wrong number of outputs')

        streams = []
        # Copy the input selector; if it stores data it should be fresh for
        # every call to the graph builder
        input_selector = copy.deepcopy(self._input_selector)
        for inp in inputs:
            stream = input_selector(ffmpeg.input(inp[0], **inp[1]), 0, 1)
            for f, filter_args in filters:
                stream = stream.filter(f, **filter_args)
            streams.append(stream)

        output_file, output_args = outputs[0]
        runner = ffmpeg.output(*streams, filename=output_file, **output_args)
        return runner.global_args(*global_args)


class OsEnvironment:
    """Context manager for updating and resetting os.environ"""

    # any environment update passed for the
    # following variables will be appended to,
    # not overwritten
    ENV_VARS_TO_APPEND_TO = [
        'PATH',
        'LD_LIBRARY_PATH',
        'DYLD_LIBRARY_PATH',
        'DYLD_FRAMEWORK_PATH'
    ]

    def __init__(self, new_env, new_cwd=None):
        self._new_env = new_env
        self._new_cwd = new_cwd
        self._old_env = None
        self._old_cwd = None

    def __enter__(self):
        if self._new_env:
            self._old_env = dict(os.environ)
            OsEnvironment.set_environ(self._new_env)

        if self._new_cwd:
            self._old_cwd = os.getcwd()
            os.chdir(self._new_cwd)

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._old_env:
            OsEnvironment.set_environ(self._old_env)
            self._old_env = None

        if self._old_cwd:
            os.chdir(self._old_cwd)
            self._old_cwd = None

    @classmethod
    def set_environ(cls, env):
        # We can't just do os.environ = X, it does nothing
        for k in os.environ:
            del os.environ[k]
        for k, v in env.items():
            os.environ[k] = v

    @classmethod
    def merge(cls, env, vars_to_append_to=None):
        if not vars_to_append_to:
            vars_to_append_to = OsEnvironment.ENV_VARS_TO_APPEND_TO

        environ = dict(os.environ)
        for k, v in env.items():
            # append to current values for some env vars
            if k in os.environ and k in vars_to_append_to:
                environ[k] = v + os.pathsep + environ[k]
            else:
                environ[k] = v
        return environ

    @classmethod
    def merged_with(cls, env, new_cwd=None, vars_to_append_to=None):
        return OsEnvironment(OsEnvironment.merge(env, vars_to_append_to), new_cwd)


class FFmpegRunnerV2:
    def __init__(self, workdir='.', env=None, graph_builder=None):
        self._workdir = workdir
        self._env = env if env else {}
        self._inputs = []
        self._filters = []
        self._outputs = []
        self._global_args = ['-hide_banner', '-y', '-loglevel',
                             'info', '-vstats', '-vstats_file', VSTATS_FILENAME]
        self._graph_builder = graph_builder if graph_builder else SingleInputGraphBuilder()
        self._record_property = None
        self._record_property_prefix = None

        self._runner = None

        self._stdout = None
        self._stderr = None
        self._fps = None
        self._latency = None
        self._pid = None

    @property
    def pid(self):
        return self._pid

    @property
    def stdout(self):
        return self._stdout

    @property
    def stderr(self):
        return self._stderr

    @property
    def env(self):
        return self._env

    @property
    def working_directory(self):
        return self._workdir

    @property
    def fps(self):
        return self._fps

    @property
    def enc_latency(self):
        return self._enc_latency

    def invalidate_runner(self):
        self._runner = None

    def add_input(self, file, **kwargs):
        self._inputs.append((str(file), kwargs))
        self.invalidate_runner()
        return self

    def set_inputs(self, inputs):
        self._inputs = inputs
        self.invalidate_runner()
        return self

    def set_input(self, file, **kwargs):
        self._inputs = []
        return self.add_input(file, **kwargs)

    def get_inputs(self):
        return self._inputs

    def get_input(self):
        return self._inputs[0]

    def add_filter(self, filter, **kwargs):
        self._filters.append((filter, kwargs))
        self.invalidate_runner()
        return self

    def clear_filters(self):
        self._filters.clear()
        return self

    def add_output(self, output, **kwargs):
        self._outputs.append((str(output), kwargs))
        self.invalidate_runner()
        return self

    def set_outputs(self, outputs):
        self._outputs = outputs
        self.invalidate_runner()
        return self

    def set_output(self, output, **kwargs):
        self._outputs = []
        return self.add_output(output, **kwargs)

    def get_outputs(self):
        return self._outputs

    def add_global_args(self, *args):
        self._global_args += args
        self.invalidate_runner()

    def set_record_property(self, record_property, prefix: str = None):
        self._record_property = record_property
        self._record_property_prefix = prefix
        return self

    def build(self):
        self._runner = self._graph_builder(
            self._inputs, self._outputs, self._filters, self._global_args)

    def ensure_runner_valid(self):
        if not self._runner:
            self.build()

    @staticmethod
    def get_runner_command_line(runner):
        args = runner.compile()
        if platform.system() == 'Linux':
            cmdline = ' '.join(map(
                lambda arg: f'"{arg}"' if ';' in arg or '[' in arg or ']' in arg else arg, args))
        else:
            # todo quote necessary symbols for other platforms
            cmdline = ' '.join(args)
        return cmdline

    def get_command_line(self):
        self.ensure_runner_valid()
        return FFmpegRunnerV2.get_runner_command_line(self._runner)

    def record_properties(self):
        if not self._record_property:
            raise ValueError('No record_property function set')

        prefix = self._record_property_prefix
        if prefix:
            prefix += '_'

        self._record_property(f'{prefix}ffmpeg_command',
                              self.get_command_line())
        # Not recording env and stdout/stderr as they get rather big
        return self

    def run(self, warmup_duration: float = 0, timeout=None):
        self.ensure_runner_valid()

        if self._record_property:
            self.record_properties()

        with OsEnvironment.merged_with(self.env, self.working_directory):
            process = self._runner.run_async(
                pipe_stdout=True, pipe_stderr=True)
            killed_on_timeout = False
            try:
                stdout, stderr = process.communicate(timeout=timeout)
            except TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate(None)
                killed_on_timeout = True

        self._pid = process.pid
        self._stdout = stdout.decode().replace('\r', '\n')
        self._stderr = stderr.decode().replace('\r', '\n')
        self._fps = fps_from_ffmpeg_log(self.stderr, warmup_duration)
        self._enc_latency = enc_latency_from_vstats_log(
            os.path.join(self._workdir, VSTATS_FILENAME))

        if not killed_on_timeout:
            retcode = process.poll()
            if retcode:
                print(self.stderr)
                raise FFmpegError(self.stdout, self.stderr)

        return self

    def probe(self, input_path, **kwargs):
        with OsEnvironment.merged_with(self.env, self.working_directory):
            return ffmpeg.probe(input_path, **kwargs)

    def psnr(self):
        if len(self._inputs) != 1:
            raise ValueError(
                'psnr measurement only supports one input, currently')

        input = self._inputs[0]
        reference = ffmpeg.input(input[0], **input[1])

        metrics = []

        for output, output_args in self._outputs:
            if 'c:v' in output_args and 'lcevc' in output_args['c:v']:
                vcodec = output_args['c:v']
                if vcodec.endswith('_vulkan'):
                    vcodec = vcodec.split('_vulkan')[0]
                target = ffmpeg.input(output, vcodec=vcodec)
            else:
                target = ffmpeg.input(output)

            stats_filename = os.path.join(self._workdir, '_psnr.log')
            psnr_runner = ffmpeg\
                .filter([target, reference], 'psnr', stats_file=stats_filename)\
                .output('-', f='null')\
                .global_args('-hide_banner', '-y', '-loglevel', 'error')

            psnr_runner_cmdline = FFmpegRunnerV2.get_runner_command_line(
                psnr_runner)
            print(f'PSNR runner cmdline:\n{psnr_runner_cmdline}')

            with OsEnvironment.merged_with(self.env, self.working_directory):
                psnr_runner.run(quiet=True)

            with open(stats_filename, 'rt') as stats_file:
                stats = stats_file.read()

            raw_psnr_values = []
            stats_lines = stats.split('\n')
            for line in filter(lambda l: l != '', stats_lines):
                avg_frame_psnr_str = list(
                    filter(lambda w: w.startswith('psnr_avg:'), line.split()))[0]
                raw_psnr_values.append(float(avg_frame_psnr_str.split(':')[1]))

            metrics.append(raw_psnr_values)

        return metrics

    def prepare_input(self, yuv: str, size: str, input_type: str, gpu_id: int = None, duration: float = None, estimated_fps: float = None,
                      input_framerate: float = 25.0, realtime: bool = False):
        if input_type not in FFMPEG_INPUT_TYPES:
            raise RuntimeError(f'Unsupported input type: {input_type}')

        framerate_args = {'r': input_framerate,
                          'readrate': 1 if realtime else 0}
        raw_input_args = {'f': 'rawvideo', 's': size}

        if duration:
            ffprobe = FFmpegRunnerV2(env=self.env)
            probe_args = dict(raw_input_args)
            if 's' in probe_args:
                probe_args['video_size'] = probe_args['s']
                del probe_args['s']
            probe = ffprobe.probe(yuv, **probe_args)
            video_stream_info = next(
                filter(lambda s: s['codec_type'] == 'video', probe['streams']), None)
            source_duration = float(video_stream_info['duration'])
            source_fps_num, source_fps_den = tuple(
                video_stream_info['r_frame_rate'].split('/'))
            source_fps = float(source_fps_num) / float(source_fps_den)

            input_duration = round(duration / source_fps
                                   * (estimated_fps if estimated_fps else 500.0), 2)
            n_loops = int(math.ceil(input_duration / source_duration))
            if n_loops * source_duration < input_duration:
                n_loops += 1

            raw_input_args.update({'t': input_duration})
            if n_loops > 1:
                raw_input_args.update({'stream_loop': n_loops})

        input = None
        input_args = {}
        filters = []

        if input_is_gpu(input_type):
            gpu_id_suffix = f':{gpu_id}' if gpu_id is not None else ''
            if input_is_compressed(input_type):
                input_gen_runner = FFmpegRunnerV2(
                    workdir=self.working_directory, env=self.env)
                input_gen_runner.set_input(yuv, **raw_input_args)
                # Lossless encoding, we never want to lose vq on this step
                # -c:v h264_nvenc -preset lossless
                args = {'c:v': 'h264_nvenc', 'preset': 'lossless'}
                input_gen_runner.set_output('input.mp4', **args)
                # cmdline = input_gen_runner.get_command_line()
                # print(f'Input prepare cmdline:\n{cmdline}')
                input_gen_runner.run()
                # use compressed input.mp4
                input = 'input.mp4'
                input_args = {
                    'init_hw_device': f'cuda=cuda{gpu_id_suffix}',
                    'hwaccel': 'nvdec',
                    'hwaccel_output_format': 'cuda'
                }
                if input_is_vulkan(input_type):
                    w, h = tuple(size.split('x'))
                    filters = [
                        ('scale_npp', {'w': w, 'h': h, 'format': 'yuv420p'}),
                        ('hwupload', {'derive_device': 'vulkan'})
                    ]
            else:
                hwdevice = 'vulkan' if input_is_vulkan(input_type) else 'cuda'
                input = yuv
                input_args = {
                    **raw_input_args,
                    'init_hw_device': f'{hwdevice}={hwdevice}{gpu_id_suffix}',
                }
                filters = [
                    ('hwupload', {})
                ]

        else:
            input = yuv
            input_args = raw_input_args

        input_args.update(framerate_args)

        return input, input_args, filters
