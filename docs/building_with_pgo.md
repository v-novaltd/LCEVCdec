# Building the LCEVC Decoder SDK with Profile-Guided Optimization

Profile-Guided Optimization is the process of using past runs of a program to generate more optimal
code at the compile stage.

Compilers perform several tricks to make code more optimal, usually based on "guesses" about how
the code will be used. For example, consider inlining: if a function is small, and usually used in
one place, then it's worth inlining; if a function is large, and often used in many places, then
it's better to have it as a regular function. The compiler already knows if a function is small,
and if it's used in many place, but how can it know if it's "often" used in many places, or
"usually" used in one place?

Using PGO, you can gather profiles of actual runs of your code, to see exactly how often each
branch goes which way, how often each function is called, etc.

Nightly builds are already optimized in this way, based on the set of test scenarios found in
`src/func_tests/test_definitions/generate_pgo.csv`.

## Known incompatible configurations and hardware

There are some known issues with PGO. In particular:

- **gcc**: It is possible, but not advisable, to use gcc for PGO. Gcc's PGO involves producing
  profiles for *each* source file (rather than each executable or library). This ultimately means
  that your profiling scenarios are required to somehow use each source file, even if there are
  some files that are never used in a typical or representative scenario.
- **Cross-architecture builds**: In general, we have found that it's not possible to produce PGO
  builds for different architectures on the same operating system. For example, macos aarch64
  doesn't seem able to produce PGO-generating builds for macos x86_64.
- **Centos7, Debian10, and Ubuntu older than 20**: These Linux targets were not considered
  important enough to get working fully.
- **Apple mobile devices**: This is theoretically possible, but Apple mobile devices don't provide
  command line access, so our existing testing scripts can't be used to generate profiles.
- **Platforms without ffmpeg**: We use ffmpeg as a base decoder for our testing harness, and there
  are many platforms for which it's difficult to get a working ffmpeg dependency via conan. If you
  cannot get ffmpeg working on your platform, you won't be able to run the tests.
- **Debug builds**: This may go without saying, but PGO is for release builds. It is essentially a
  way to supercharge the optimizations normally done in release builds.
- **Speed**: Generating PGO can be quite slow. For our set of 72 typical use cases, running on CI,
  times range from 30 minutes (MacOS), to 4 hours (Windows), to 8 hours (driving a VIM4 from a
  Ubuntu machine). These devices are shared, so you may get better results, but it's still
  recommended to set aside plenty of time for PGO generation.

Conversely, there are some platforms that definitely *do* work, and for which we produce PGO-
optimised builds in our weekly CI pipeline. These are:

- Android APIs 21, 26, and 30, on both arm64 and armeabi architectures (with clang)
- Apple MacOS aarch64 (with clang)
- Linux Ubuntu20, Ubuntu22, and Debian11 (with clang)
- Windows (with MSVC, compiled with /MT)

## Steps

As a summary, the steps are:
Build (1-3)
Run (4)
Combine profiles (5)
Build again, using profiles (6)

In more detail:

1. Follow the usual build instructions, up to (but excluding) the `cmake -G '...' ..` step
2. Take your `cmake ..` command line and append `-DVN_CORE_GENERATE_PGO=ON`, and run this. If you
have the choice between a Debug and a Release build at this point, choose Release (on Windows,
this choice is deferred until the next step). The "core generate PGO" flag will create a build
which generates PGO profiles every time it's run.
3. Run `cmake --build .` as usual.
NOTE: for Windows, we have found that it's necessary to locate a file called `pgort140.dll` and
either copy it into the binary directory (`bin/Release`), or add it to your PATH environment
variable. You may find this file in somewhere like:
`C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64`.
4. Run several representative scenarios with your code. The simplest way to do this is to adapt the
existing `run_tests.py` test suite to your particular needs, like so:
   1. In `src/func_tests`, modify `config.ini` so that `THREADS` is 1 (many profile-generating
   tools are not threadsafe across multiple instances of a program).
   2. Modify other parts of `config.ini` to match your target hardware. For example, if you want to
   build for your current laptop, you can leave the configuration as-is. If you want to build for a
   device that you've connected to by adb over ethernet, then set `ADB_SERIAL` to the full IP
   address (including the `:5555` suffix). Many devices have small storage capacities with large
   SD cards; if this describes your device, then you'll also want to change `DEVICE_BASE_DIR` to
   something in the large SD card (a common directory name among our test devices is
   `/storage/0123-4567`).
   3. Set `DELETE_TEMP_DIR` to `NEVER`. This is because some PGO tools will place profiles into
   the program's current working directory, in which case you'll find your profiles spread across
   the several temporary directories generated by these tests. Note: other PGO tools will place
   profiles in the same directory as the relevant library or executable, but it's best not to risk
   deleting your temporary directories until you're sure.
   4. Select your test cases. There are three ways to do this:
      1. A broad range of tests is already available in `generate_pgo.csv`. You can select all of
      these by setting `FILTER_GROUP` to `generate_pgo`, and setting `LEVEL` to `PGO`.
      OR
      2. If you want a particular subset of those tests, you can also manually modify either the
      csv or the `config.ini` file to narrow things down. For example, you can manually exclude
      certain clips by changing `PGO` to `SKIP` in the csv. Or, you can filter by the test name,
      e.g. by setting `FILTER_NAME` to `cmdbuffer_on` to only include tests whose name includes
      `cmdbuffer_on`.
      OR
      3. If you want to take some of our numerous existing CI tests, you can also do that. For
      example, if know that your build will only be used for tiled encodes, you can set
      `FILTER_GROUP` to `serial_tiled_decoding` in `core_serial_harness_hash`. Likewise, there's
      `serial_cmdbuffer` and `serial_cmdbuffer_resolution` for command buffers. The only downside
      to this is that the profile may overestimate the importance of generating hashes (since
      these tests generally do generate hashes).
   5. Now simply call the test script: `python3 src/func_tests/run_tests.py`.
5. Now that you've run several tests, you should see either a bunch of `.profraw` files
(Unix-like), or a bunch of `.pgc` files (Windows). You'll need to merge these.
   1. For `.profraw` files, navigate to the directory where all your `.profraw` files are, and run
   `llvm-profdata merge -output="default.profdata" ./*.profraw` from the command line. This will
   generate a single merged profile for later use.
   NOTE: you may find that your `.profraw` files are all in different temporary directories. The
   llvm profdata utility doesn't seem to be able to recurse into directories, so you'll want to
   move them first. [This python snippet will do that for you](#appendix-a-handy-python-script-for-copying-profraw-files-into-the-current-directory).
   2. For `.pgc` files, you'll want to use the Visual Studio "Developer Command Prompt" (this
   saves the hassle of modifying your PATH variable). In the Developer Command Prompt, again
   navigate to the directory with all your `.pgc`s (this will almost certainly be
   `your_build_directory/bin/Release`). Then, run: `pgomgr /merge lcevc_dec_core.pgd`. MSVC
   requires your profile to have the same name as the executable/library that was profiled, hence
   "lcevc_dec_core".
6. You should now have a *single file* which encapsulates all the data from all your runs. *Copy*
this file into root of the repo (that is, into `LCEVCdec`). Now, repeat steps 1 and 2 for a *new*
*directory* (e.g. `build_use_pgo`), BUT, replace `-DVN_CORE_GENERATE_PGO=ON` with
`-DVN_CORE_USE_PGO=ON`. Finally, run `cmake --build .` as usual, and you should have a pgo-
optimized build ready to go. Note that the optimizations are now contained in the compiled code
itself, rather than just in the `.pgd` or `.profdata` files.

NOTE: For Windows, this last step always seems to fail at the `cmake --build` step. The simplest
way we've found to accommodate this is to build unsuccessfully, then copy and paste the `.pgd` file
into `bin/Release`, and build again. This second attempt should succeed.

## Appendix: a handy python script for copying .profraw files into the current directory

```python
import glob
import os

def main():
    for i, file in enumerate(glob.glob("./test_outputs/test_*/*.profraw")):
        print(f"Moving {i}th version of {file} here")
        os.rename(file,f"./{i}.profraw")

if __name__ == '__main__':
    main()
```
