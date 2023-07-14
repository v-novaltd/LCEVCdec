# Getting started - Build Tools for Windows

## Git

The executable to download Git for Windows can be found [here](https://git-scm.com/download/win) (the download should start automatically from there).

## Python

Install python as a standalone package, that way you can control the version. Ensure that the path is set correctly to be able to run from a shell, and that pip is installed.

Python for Windows is available for download [here](https://www.python.org/downloads/windows/). You should use version 3.7 or greater. Follow the installation instructions and add installed location to `Path` for it be found by other tools.

Good instructions on how to install Python virtual environments on Windows machines are available
[here](http://timmyreilly.azurewebsites.net/python-pip-virtualenv-installation-on-windows/).

## CMake

CMake can be used to produce .sln files for Visual Studio, ninja files for windows or android, or even good old makefiles for windows or android. It can be installed from [here](https://github.com/Kitware/CMake/releases). Find the one ending in "windows-x86_64.msi", then download and run it. The current recommended version is `3.25.2`, as that's what we use for our CI (nightly builds, etc).

## Installing and Configuring Conan

Conan is software that uses python to manage C/C++ packages. You will need to have Conan installed to get V-Nova's package recipes and prebuilt packages, which will then be fed into CMake to come up with build configurations. Most steps involving Conan will require access to the VPN, or an in-office ethernet connection.

Conan is installed and configured like so:

```shell
pip install -r requirements.txt
conan config install "https://gitlab.com/v-nova-public/conan-profiles.git" --type=git
```

The `pip install` step will also get other required python modules, as a happy little bonus.

## Visual Studio

You will need to install Visual Studio to build the software on windows. Currently we are building with Visual Studio 2019, you will need at least version 16.9.

Ensure that **Desktop development with C++** is selected to install. Check for updates after installing. Be prepared for a few reboots :(

Do **NOT** install Visual Studio's version of Python. Doing so will ruin your day at some point when its set of packages diverge from the external python's set.

Do **NOT** install Visual Studio's version of CMake. There must be only 1 version of CMake used in the system and that will be the one installed above.

## Clang-format and Clang-tidy

Clang-format is our linter: it formats the code. It is mandatory to obey clang-format, but you can use `// clang-format off` and `// clang-format on` before and after code that you want clang-format to ignore.

Clang-tidy provides useful warnings. It is *not* currently mandatory to obey clang-tidy, but it is helpful. If you do violate clang-tidy, you should generally add a comment to disable that particular rule. For example, you can add `NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)` if the next line has a `reinterpret_cast`.

We use LLVM version 14.0.0 for both these, which is available [here](https://github.com/llvm/llvm-project/releases/tag/llvmorg-14.0.0). As always, once downloaded, add its location to `Path`.

It is receommended to integrate both clang-format and clang-tidy into your IDE (this is easy for Visual Studio, and can be done in VS Code if you export compile commands, as described in the build instructions)

You can also use clang-format from the command line, like so

```shell
clang-format -i changed_file.cpp
```

In PowerShell, the following is a handy command to format every file for which you've committed a change:
```shell
clang-format -i @(git diff master HEAD --name-only *.cpp *.h *.c)
```

## Ninja (optional)

Ninja is a build system (similar to make), see [here](https://ninja-build.org/) for more details. We typically use Ninja when building for Android on Windows, but not typically when building for Windows itself.

We are currently using version 1.8.2 which can be found [here](https://github.com/ninja-build/ninja/releases/tag/v1.8.2). Note that Ninja doesn't have any special installer: it's just a single file (ninja.exe). So, download the zip, take ninja.exe out of the zipped folder, and put it in a folder somewhere sensible (like Program Files).

You will need to add this newly created Ninja folder to the `Path` Environment Variable.

If you intend to build for Android, and use Android Studio (which is recommended), be aware that the ninja executable must be in the same directory as the cmake executable. This is due to a bug/feature in the IntelliJ software (used by AndroidStudio). You can either copy the ninja.exe into the same directory as cmake.exe OR create a sym link using `mklink`.

## Android Development (optional)

These instructions are based on Android Studio. It is also possible to develop for Android on Windows with command-line tools, but you will not find instructions for that here.

Download Android Studio [here](https://developer.android.com/studio).

Open the SDK Manager, via `Tools/SDK Manager`. On the left, it should say that you're in the `Android SDK` section. At the top of this window, there will be a text bar labelled `Android SDK Location`. Click `edit` to make a folder (for example, `C:/Users/your.name/AppData/Android/Sdk`) and set this as the SDK location. Below, we call this `ANDROID_HOME`. Android Studio should, at this point, force you to download the SDK, and the latest SDK Platform, so do that.

Now, the most important part: download the Android NDK. Return to the `SDK Manager` tool. Again, in the `Android SDK` section, switch from your current tab (which will typically be `SDK Platforms`) to the `SDK Tools` tab. On the lower right, tick "show package details". Now, in the main window, find `NDK (side-by-side)`, expand it, and select version 21.4.7075529. Click `Apply`, and the NDK should start downloading immediately.

Finally, set the following environment variables:

`ANDROID_HOME` = whatever you used as the SDK location earlier.  
`ANDROID_NDK_PATH` = `%ANDROID_HOME%\ndk\21.4.7075529`  
`ANDROID_NDK` = `%ANDROID_NDK%`

`ANDROID_HOME` is used by Android Studio, `ANDROID_NDK_PATH` is used by our conan-related files (`create_all_android_conan_profiles.py`), and `ANDROID_NDK` is used by our cmake-related files (`android_ndk.toolchain.cmake`).

NDK 21.4.7075529 is the minimum NDK, but newer ones should, in theory, also work.

## Emscripten (optional)

If you plan to integrate the Decoder into web applications, then you'll need Emscripten. Emscripten is available as a git repo. Setup instructions can be found [here](https://emscripten.org/docs/getting_started/downloads.html). If you are using PowerShell, you may find that `emsdk` violates your default script permissions, so you can just use `emsdk.bat` instead.

The following environment variables are optional for the cmake generation step, but required for the ninja build step:

`EMSCRIPTEN_ROOT_PATH` = the location of `emsdk\upstream\emscripten` (a folder in the emsdk repo).  
`EMSCRIPTEN` = `%EMSCRIPTEN_ROOT_PATH`

Now, add this newly-set variable to your Path environment variable. You can simply add `%EMSCRIPTEN%`.
