# lcevc_dec Visual Studio Code Setup

## Prerequisites

All you should need is the C/C++ Extension Pack, installable from Extensions in VSCode. If you've ever opened a c++ workspace in VSCode, you probably(!) already have this.

## clang-format

### Setup

Should just work OOTB with the C/C++ Extension installed in vscode, which bundles clang-format. Follows the .clang_format file in the root of the repo without any issues. I found that I had problems if I had installed any of the clang* extensions in vscode; the microsoft C/C++ Extension Pack was enough.

### Running

You can manually format a document with Shift+Alt+F, or Format Document in the Command Palette. If you want to auto-format, you can enable Format on Save and (more heavy, sometimes annoying) Format on Type. These go in your VSCode User or Workspace settings, and are under Text Editor > Formatting > Format On Paste/Save/Type (or, in JSON, `editor.formatOnSave` / `editor.formatOnPaste` / `editor.formatOnType`).

## clang-tidy

### Setup

Possibly see: [here](language_server.md) (first instruction, ignore the clangd one)

Clang Tidy will only run if there are no errors present in the file you're editing. To get this to work, have to configure cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=1, which will cause a compilation database to be created when building. This is easier building using Ninja on Windows than MSVC build tools; just add the flag to the `cmake -G Ninja ..` commandline, and build the project once with `cmake --build .` (For more information, see [Windows](building_windows.md) or [Linux](building_linux.md)).

Once you've built the project, you should find a `compile_commands.json` in the root of the build directory. Point this at VSCode in your `c_cpp_properties.json`, by adding a line like the following to your C++ configuration. For example:
 `"compileCommands": "C:\\Users\\lucy.firman\\build\\lcevc_dec\\build_windows\\compile_commands.json"`. An excerpt of my c_cpp_properties.json is attached below; containing a new Configuration for "lcevc_dec". To add a new Configuration, you can open either "C/C++: Edit Configurations (JSON)", or - if you're not down to clown in JSON town - "C/C++: Edit Configurations (UI)". In the UI, Compile Commands is under 'Advanced' at the bottom. Once you've created a new Configuration, you can select this by opening the Command Palette (With a project file open) and heading to "C/C++: Select a Configuration..." and hitting Enter and selecting the new Configuration you just added.

This should make VSCode pick up all required dependencies and defines, which will allow clang-tidy to run.


### Running

You can run clang tidy with "Run Code Analysis on Active File" from the Command Palette. It takes a little bit to run, and there's a little flame icon in the bottom bar in VSCode whilst it's running. When it's complete, it'll highlight clang tidy issues with a yellow squiggle, on which you can press ctrl+. (or whatever your Quick Fix bind is) and allow it to fix the issue. I'm not yet sure how to run it across a whole directory (or workspace) in VSCode.


### Example c_cpp_properties.json configuration

```
        {
            "name": "lcevc_dec",
            "includePath": [
                "${workspaceFolder}/**",
            ],
            "defines": [
                "UNICODE",
                "_UNICODE"
            ],
            "windowsSdkVersion": "10.0.19041.0",
            "compilerPath": "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64/cl.exe",
            "cStandard": "c11",
            "cppStandard": "c++11",
            "intelliSenseMode": "windows-msvc-x64",
            "compileCommands": "C:\\Users\\lucy.firman\\build\\lcevc_dec\\build_windows\\compile_commands.json"
        }
```


## Adding Visual Studio 2019 Tools as a Terminal in VSCode

You can add the "VS2019 Build Tools" terminal profile to visual studio code, to allow you to build the codebase inside VSCode. To do so, add the following to either your User or Workspace settings.json, under `"terminal.integrated.profiles.windows":` :

```
        "Visual Studio 2019 Tools Command": {
            "path": [
                "${env:windir}\\Sysnative\\cmd.exe",
                "${env:windir}\\System32\\cmd.exe"
            ],
            "args": ["/k", "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"],
            "icon": "terminal-cmd"
        },
```

## Running the build with VSCode

It's possible to configure Tasks in VSCode to run arbitrary commandline tasks. We can use this to run the Build and Install tasks. In your .vscode directory inside your workspace, create `tasks.json` if it doesn't exist. Inside, you can place something like the below:

```
{
    "version": "2.0.0",
    "command": "cmd.exe",
    "type":"shell",
    "args": ["/K"],
    "tasks": [
        {
            "taskName": "Build All",
            "isBuildCommand": true,
            "options": {
                "cwd": "${workspaceFolder}/build_windows"
            },
            "args": ["C:\\Program^ Files^ ^(x86^)\\Microsoft^ Visual^ Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat && cmake --build ."],
        },
        {
            "taskName": "Install",
            "options": {
                "cwd": "${workspaceFolder}/build_windows"
            },
            "args": ["C:\\Program^ Files^ ^(x86^)\\Microsoft^ Visual^ Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat && cmake --install . --prefix=_install"],
        }

    ]
  }
  ```
  replacing paths where appropriate. This will add two Tasks named "Build All" and "Install" to your task list, that you can execute from Command Palette > Tasks: Run Task. These will just build for the current platform. This could possibly use some refinement as the build process evolves.
