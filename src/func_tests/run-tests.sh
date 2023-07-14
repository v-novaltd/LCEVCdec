#!/bin/bash
set -e
# set -x

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
lcevc_root="$( cd "$( dirname "${script_dir}/../../../" )" >/dev/null 2>&1 && pwd )"

n_workers=$(nproc)
max_workers=4
if [[ "${TARGET}" == "ubuntu20-aarch64" ]]; then
    max_workers=8
fi

if [ "${n_workers}" -gt "${max_workers}" ]; then
    n_workers=${max_workers}
fi

default_marks="not slow and not xvfb and not conformance "

if [[ "$*" == *--ffmpeg* ]]; then
    ffmpeg_version=$(jq -r ".ffmpeg" "${script_dir}/../../version.json")
    ffmpeg_conan_package="ffmpeg/${ffmpeg_version}@"
    # shellcheck disable=SC2086
    conan install ${CONAN_UPDATE_ARG} "${ffmpeg_conan_package}"\
        -o license=gpl -o pthread-win32:shared=True -r vnova-conan

    extra_marks="and ffmpeg"
    n_workers=2
else
    extra_marks="and not ffmpeg"
fi

if [[ "${JOB}" == "test-plugins-nvenc" ]]; then
    # Strong chance of running out of memory otherwise
    n_workers=2
fi

test_suite="full"
if [[ "${PIPELINE_TYPE}" == "merge_request" ]]; then
    test_suite="merge_request"
fi

echo "Chosen test suite: ${test_suite}"


if [[ "${TARGET}" == "ubuntu20-x64-asan" ]]; then
    stop_on_failure="no"
    export ASAN_OPTIONS="log_path=${lcevc_root}/asan.log"
else
    stop_on_failure="yes"
fi
if [[ "${JOB}" == "test-dilbert" ]]; then
    extra_marks="${extra_marks} and dilbert and not vulkan and not performance"
elif [[ "${JOB}" == "test-venus" ]]; then
    extra_marks="${extra_marks} and venus and not performance"
elif [[ "${JOB}" == "test-vulkan" ]]; then
    extra_marks="${extra_marks} and dilbert and vulkan and not performance"
    n_workers=3
elif [[ "${JOB}" == "test-plugins" ]]; then
    extra_marks="${extra_marks} and plugin_validation and not hardware_special and not performance"
elif [[ "${JOB}" == "test-plugins-nvenc" ]]; then
    extra_marks="${extra_marks} and plugin_validation and hardware_nvenc and not performance"
elif [[ "${JOB}" == "test-functional" ]]; then
    extra_marks="${extra_marks} and not vulkan and not performance"
elif [[ "${JOB}" == "test-gpu-sanity" ]]; then
    n_workers=0
    extra_marks="${extra_marks} and dilbert and vulkan and not performance"
elif [[ "${JOB}" == "test-gpu-perf" ]]; then
    n_workers=0
    extra_marks="${extra_marks} and dilbert and vulkan and performance"
    # if performance regressed it would be usefull to see the delta for all of the tests, not just first failure
    stop_on_failure="no"
fi

if [[ "$*" == *--regen* ]]; then
    n_workers=0
    test_suite="full"
    echo "Overriding test suite: ${test_suite} due to regen"
fi

echo "DISPLAY is DISPLAY=$DISPLAY"

if [[ "${n_workers}" == "0" ]]; then
    workers_arg=""
else
    workers_arg="-n ${n_workers}"
fi

stop_on_failure_arg=""
if [[ "${stop_on_failure}" == "yes" ]]; then
    stop_on_failure_arg="-x"
fi

echo "Running pytest test suite: ${test_suite} marks: \"${default_marks}${extra_marks}\" workers: ${n_workers} stop on failure: ${stop_on_failure} extra args:" "$@"

ret=0
# shellcheck disable=SC2086
# shellcheck disable=SC2068
python3 -m pytest "${script_dir}/tests" ${RUN_TESTS_CONAN_UPDATE_ARG}\
    --resource-dir "${ASSETS}"\
    --suite "${test_suite}"\
    -rfE ${workers_arg} ${stop_on_failure_arg} --basetemp cpu -m "${default_marks}${extra_marks}"\
    $@ || ret=1

if [[ "${TARGET}" == "ubuntu20-x64-asan" ]]; then
    ASAN_LOG="${TARGET}-${JOB}.log"
    cat "${lcevc_root}"/asan.log.* > "${lcevc_root}/${ASAN_LOG}"
    rm -rf "${lcevc_root}"/asan.log.*
    python3 "${lcevc_root}/tools/asan/unique-asan-errors.py" "${lcevc_root}/${ASAN_LOG}"
    echo "${ASAN_LOG}" >> .jenkins-artifacts
fi

exit ${ret}