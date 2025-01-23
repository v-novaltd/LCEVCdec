# Getting started - Build Tools for Linux

NOTE FOR WINDOWS DEVELOPERS: if you are hoping to use these instructions with WSL (Windows Subsystem for Linux), you will be disappointed, because certain steps simply will not work. This is especially true if building for Android. Instead, ask IT for a Linux machine to SSH into.

## Git

Via apt or a package manager in your distro:

```shell
sudo apt install git git-lfs
```

Ensure the `pre-commit` hook script is copied from `cmake/tools` to your `.git/hooks` to preform automatic linting before committing.

## Python

Via apt or a package manager in your distro. You will also need pip, and it is recommended to install venv support.

```shell
sudo apt install python3 pip python3-venv
pip install -r requirements.txt
pip install -r cmake/tools/lint_requirements.txt
pip install -r src/func_tests/test_requirements.txt
```

## Ninja

Ninja is highly recommended to improve compile times for LCEVCdec and dependencies.

```shell
sudo apt install ninja-build
```

## Compiler

It is up to you which compiler you want to use for development. However, be aware that CI is using **gcc-11** (as well as 7 and 9) for Linux jobs. So it is handy to have it installed in your system as well. Note, however, that you'll probably need Ubuntu 22.04 or higher for gcc-11 or higher.

Via apt:

```shell
sudo apt install gcc-11 g++-11 build-essential
```

If you want to cross-compile for ARM64, you'll also need gnu compilers for that:

```shell
sudo apt install gcc-11-aarch64-linux-gnu g++-11-aarch64-linux-gnu
```

These will be needed if using conan when running `create_all_linux_conan_profiles.py`.

## CMake

Version 3.22.1 or newer is required for a full build. It should be available via `sudo apt install cmake`.

If not, you can always install via installer script
```shell
sudo mkdir /opt/cmake
wget https://github.com/Kitware/CMake/releases/download/v3.22.1/cmake-3.22.1-linux-x86_64.sh -O cmake.sh
sudo /bin/bash cmake.sh --skip-license --prefix=/opt/cmake
rm cmake.sh
```

Add the following line at the end of ~/.bashrc to use cmake 3.18 by default
```shell
export PATH=/opt/cmake/bin:${PATH}
```

## Gathering dependencies

The core and API compile targets of the repo are structured to have no dependencies to ensure the core functionality is as portable as possible. Components such as the test harness and unit tests require hashing libraries, JSON parsers and gtest for unit testing to allow for easier testing. An important dependency of the test harness are various libav components that allow base decoding and parsing of common container formats such as .ts and .mp4. By default, the repo uses `conan` to fetch these. `pkg-config` is an automatic fallback if `conan` fails or paths can be manually specified with the `-DVN_SDK_FFMPEG_LIBS_PACKAGE` cmake option.

Either follow Gathering pkg-config Dependencies OR Gathering conan Dependencies below.

## Gathering conan Dependencies

Conan is software that uses python to manage C/C++ packages. Internally at V-Nova we use conan to manage package recipes and prebuilt packages across platforms. The following steps involving Conan will require access to the V-Nova VPN, or an in-office ethernet connection.

After installing the repo, profiles are installed with the python script. Conan profiles set many constants about your environment such as the compiler, build type and architecture. For linux development `gcc-11-Release` and `gcc-11-Debug` are required. After installing conan profiles, they can be listed with `conan profile list`.

Conan is configured from the `lcevc_dec` repo, like so:

```shell
export PATH=$PATH:~/.local/bin # or add this to the bottom of your ~/.bashrc
conan config install "https://gitlab.com/v-nova-public/conan-profiles.git" --type=git

sudo apt install libx11-xcb-dev libxcb*
python3 ~/.conan/linux/create_all_linux_conan_profiles.py
```

## Gathering pkg-config Dependencies

All dependencies can be retrieved via `apt` on debian based systems.

```shell
sudo apt-get install -y pkg-config rapidjson-dev libfmt-dev libcli11-dev libavcodec-dev libavformat-dev libavutil-dev libavfilter-dev libgtest-dev libgmock-dev libxxhash-dev librange-v3-dev
```

## Android development

If you're building for Android, then you'll need Android's `sdkmanager` to download the Android NDK and related packages. This is found online, at the bottom of the download page for Android Studio, in the `command-line only tools` section [here](https://developer.android.com/studio#command-tools). If you're working strictly on the command line, you can copy the download location and `curl` it. For example, from your home directory:

```shell
mkdir android_sdk
cd android_sdk
curl https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip --output android_tools.zip
sudo apt install unzip # if you don't already have it
unzip android_tools.zip
cd cmdline-tools
mkdir latest
mv !(latest) latest

# Do this, and/or add them to your .bashsrc:
export PATH=$PATH:~/android_sdk/cmdline-tools/latest/bin
export ANDROID_SDK_HOME=~/android_sdk
```

Now, install the NDK (this also requires java, if you don't have it already):

```shell
sudo apt install openjdk-19-jre-headless
sdkmanager "platforms;android-33"
sdkmanager "ndk;25.2.9519653"
```

NDK version 25.2.9519653 is known to work.
