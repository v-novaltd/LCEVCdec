# Getting started - Build Tools for macOS

## Git

Via homebrew or a package manager in your distro:

```shell
brew install git git-lfs
```

## Xcode

Xcode is required for Apple. It should also include things that are needed in other steps. In particular, it should include python, pip, and python-venv. Beware: your system may use either `python3` and `pip3`, or `python` and `pip`. Python versions 3.7 - 3.11 are known to be supported.

## Python

Once installed with xcode, install requirements with:

```shell
pip install -r requirements.txt
pip install -r cmake/tools/lint_requirements.txt
pip install -r src/func_tests/test_requirements.txt
```

## Installing and Configuring Conan

Conan is software that uses python to manage C/C++ packages. You will need to have Conan installed to get V-Nova's package recipes and prebuilt packages, which will then be fed into CMake to come up with build configurations. Most steps involving Conan will require access to the VPN, or an in-office ethernet connection.

Conan is installed and configured from the `lcevc_dec` repo, using pip. You may wish to use a virtualenv (python-venv or python3-venv) for this.

That step will also get other required python modules, as a happy little bonus.

Now, you may have received warnings during that last step, such as `The script conan is installed in ~/Library/Python/3.9/bin/, which is not on PATH. Consider adding this directory to PATH`. Obey these warnings like so:

```shell
export PATH=$PATH:~/Library/Python/3.9/bin/ # or add this to /etc/paths
```

Finally, install the conan profiles:
```shell
conan config install "https://gitlab.com/v-nova-public/conan-profiles.git" --type=git
python3 [CONAN_HOME]/create_conan_profiles.py --platform apple
```

the `--platform apple` option covers macOS as well as iOS (iOS profiles also include tvOS profiles).

## CMake

Version 3.18.1 or newer is required for a full build.

