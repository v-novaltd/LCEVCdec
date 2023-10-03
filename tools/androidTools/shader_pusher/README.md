# Shader Pusher
Tool to make Android shader development easier and faster.

## Prerequisites
- Python 3
- Watchdog
    - Install with: `pip3 install watchdog`
- ADB available on system path

## How to use
1. This can be enabled by setting the config option `load_local_shaders` to `true`.
2. Build the DIL.
3. Run `python shader_pusher.py`.
4. Select device from devices listed.
5. Modify shaders, save and restart stream to observe the shader changes.

## Command line options
- `--cleanup` - Runs an `adb shell rm /sdcard/*.glsl` command on the device to cleanup any GLSL files remaining on the device when the script exits.