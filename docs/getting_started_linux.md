# Getting started - Build Tools for Linux

NOTE FOR WINDOWS DEVELOPERS: if you are hoping to use these instructions with WSL (Windows Subsystem for Linux), you will be disappointed, because certain steps simply will not work. This is especially true if building for Android. Instead, ask IT for a Linux machine to SSH into.

## Git

Via apt or a package manager in your distro:

```shell
sudo apt install git git-lfs
```

## Python

Via apt or a package manager in your distro. You will also need pip, and it is recommended to install venv support.

```shell
sudo apt install python3 pip python3-venv
```

## Compiler

It is up to you which compiler you want to use for development. However, be aware that CI is using **gcc-11** (as well as 7 and 9) for Linux jobs. So it is handy to have it installed in your system as well. Note, however, that you'll probably need Ubuntu 22.04 or higher for gcc-11 or higher.

Via apt:

```shell
sudo apt install gcc-11 g++-11
```

If you want to cross-compile for ARM64, you'll also need gnu compilers for that:

```shell
sudo apt install gcc-11-aarch64-linux-gnu g++-11-aarch64-linux-gnu
```

These will be needed when running `create_all_linux_conan_profiles.py`.

## Installing and Configuring Conan

Conan is software that uses python to manage C/C++ packages.

Conan is installed and configured from the `LCEVCdec` repo, like so:

```shell
pip install -r requirements.txt
export PATH=$PATH:~/.local/bin # or add this to the bottom of your ~/.bashrc
```

The `pip install` step will also get other required python modules, as a happy little bonus.

## CMake

Version 3.18.1 or newer is required for a full build. It should be available via `sudo apt install cmake`. 

If not, you can always install via installer script
```shell
sudo mkdir /opt/cmake
wget https://github.com/Kitware/CMake/releases/download/v3.18.1/cmake-3.18.1-Linux-x86_64.sh -O cmake.sh
sudo /bin/bash cmake.sh --skip-license --prefix=/opt/cmake
rm cmake.sh
```

Add the following line at the end of ~/.bashrc to use cmake 3.18 by default
```shell
export PATH=/opt/cmake/bin:${PATH}
```

## Ninja (Optional)

`sudo apt install ninja-build`

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
sdkmanager "ndk;21.4.7075529"
```

We use 21.4.7075529 here, as that's an NDK version which is known to work.
