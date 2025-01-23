# lcevc_dec Code Formatting Visual Studio Setup


## Prerequisites

You'll need to install clang manually. Visual Studio appears to have a built-in clang-format executable, but it's a few versions behind us. We need at least 13.0.0 (and there's no harm in going newer, like 14.0.0). The list of versions is [here](https://releases.llvm.org/download.html), you just need to click through to the github page for your chosen version. You'll want to download and run "LLVM-14.0.0-win64.exe" (if you're on Windows). When downloading, you should add it to your PATH environment variable, so that you can run clang-format from the command line if you'd like.


<br/>

## clang-format

### Setup
1) Go to Tools>Options.
2) Click on Text Editor>C/C++>Code Style>Formatting.
3) Check the box on "Enable ClangFormat support" and it'll automatically find your .clang-format file.
4) As mentioned above, Visual Studio has a built-in clang-format.exe, but this will be an old version. So, check the "Use custom clang-format.exe file" box, and navigate to wherever you've installed clang-format.exe (for example, "C:\Program Files\LLVM\bin\clang-format.exe".
5) (optional) Check any boxes for the auto-format triggers that you want. For example, it can be helpful to have a line auto-format whenever you put a semi-colon at the end of it, and you'll almost definitely want the auto-format with braces option (which formats everything inside the braces).

### Running
If you chose to do step 5 in the previous section, then your code will typically stay formatted. Beyond that, there may still be times when it's necessary to explicitly format a lot of files (for example, if you do a Rename, and dozens of files now have too-long lines). For cases like this, you're better off using clang-format.exe in command line (as long as you added it to PATH during the [Prerequisites](#prerequisites) step). An example command line: `clang-format -i src/integration/src/*` (Note that `src/*` won't recurse into the directories, hence the more specific `src/integration/src/*`).


<br/>

## clang-tidy

### Automatic setup

Clang-tidy should be turned on automatically, due to the way our CMakeLists.txt are setup. Specifically, these lines should be in the CMakeLists.txt for any project whose code needs to be clang-tidy'd:
```
set_target_properties(lcevc_decoder_sample PROPERTIES
    ...
    VS_GLOBAL_EnableMicrosoftCodeAnalysis false
    VS_GLOBAL_EnableClangTidyCodeAnalysis true
)
```
If you're making a new VS project, or fixing an existing one, then just add those two lines to `set_target_properties`, and replace `lcevc_decoder_sample` with the name of the project you're working on.

Besides that, it's also possible to manually enable or disable clang-tidy, instructions for which are below. Note that these settings will get overwritten any time you do a fresh cmake.

### Manual setup
1) Select all the projects in the Solution Explorer window on the left (under both "Executables" and "Libraries").
2) Right click and select "Properties".
3) Find "Code Analysis" at the bottom left.
4) Select "No" for "Enable Microsoft Code Analysis" and "Yes" for "Enable Clang-Tidy". It will automatically find your .clang-tidy file. I DO NOT recommend allowing "Enable Code Analysis on Build": this will prevent your build from succeeding when you have clang-tidy violations.

### Running
After setup, clang-tidy violations will get green squiggle underlines. If, however, you want to see all the clang-tidy violations all at once, then you can choose to manually trigger a Code Analysis by going to Analyze>Run Code Analysis. That'll show the violations all in the output window.
