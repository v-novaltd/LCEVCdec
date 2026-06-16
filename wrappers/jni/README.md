# Java Native Interface (JNI) Wrapper

The JNI wrapper is required to build the AndroidX Media LCEVC decoder integration. This enables LCEVC playback natively in Android, including full rendering support directly from the LCEVCdec library.

## Building the JNI

The JNI wrapper can be built either with Android Studio or with command line tools. First, please ensure your system is appropriately configured:

### Prerequisites

- generic LCEVCdec build dependencies must be installed - it is a good idea to test that the dependencies are configured by performing a generic LCEVCdec build before building the JNI wrapper, see `docs/building.md`
- the JNI is built with Ninja, which is not a requirement to build LCEVCdec generally
- the JNI build also depends on the `glslangValidator` tool.

### Android Studio

- [Download](https://developer.android.com/studio), install, open and run first time setup (default settings are acceptable) for Android Studio.

- Open the `wrappers/jni` directory as a project in Android Studio.

- Wait for the project to import/sync.

- Select the `jni [publish]` configuration in the top right area.

- Press `Run` in the top right area to build the project.

### Command Line Tools

In this section, on Windows, replace usage of `./gradlew` with `gradlew.bat`.

- [Download](https://developer.android.com/studio#command-line-tools-only), install and set up the command line tools. This package will enable auto-installation of any components used in the build without having to manually select them.

- Change to the `wrappers/jni` directory.

- Run the `publish` task.
    ```bash
    ./gradlew publish
    ```

### Local m2 Maven Repository, Snapshots and Versions

Building the project using either of the methods above will generate an `.aar` file named with the project name and a version, like all Maven-managed files. When building locally, these are pushed to a special directory on the system (the `.m2` directory) which is a local Maven repository. Any other projects which use Maven for dependency management (if configured) will look in this directory to find dependency objects.

See the [Configuring Maven](https://maven.apache.org/guides/mini/guide-configuring-maven.html#configuring-your-local-repository) guide for default `.m2` filesystem locations and user-configurable options.

The following behavior describes the configuration of the V-Nova `androidx/media3` project, and it is possible to configure any other consuming project in the same way for an easy development flow.

This repo generates a release `.aar` artifact on every public release, of the form `com.vnova.lcevc.decoder:dec:0.1.0-aaaaaaa`, where `0.1.0` is an example corresponding Git tag for the release and `aaaaaaa` is the exact short Git hash of the commit from which the release was generated.

When building the downstream project, by default, it will consume a hard-coded public release of the LCEVCdec `.aar` from the GitHub Maven repository. If the development you are undertaking is constrained entirely to the downstream app, there is no need to change this. However, it is possible to override the consumed version using a Gradle parameter `-Plcevcdec=<VERSION>`, e.g. `-Plcevcdec=0.2.0-bbbbbbb` will use a different public release.

The version when using `publish` locally is a `dev-SNAPSHOT` version, which has special handling in the Maven system. It is intended for use in the local development loop, and allows the developer to rebuild the project many times without bumping the true version number, whilst always consuming the last build in any other locally-built projects on the same system. Maven internally timestamps each build and consumes whichever has the newest timestamp, but this is automatic and hidden from the developer.

Calling `publish` from the terminal in this project prints a helpful hint on how to use the artifact you just built in your own app, of the following form:

```
--> Building LCEVCdec JNI wrapper with version dev-SNAPSHOT
--> Consume in V-Nova androidx/media3 fork using publish with parameter -Plcevcdec=dev-SNAPSHOT
```

You can then call `publish` in the **androidx/media3 project** (not in this repository) with that parameter, and the library artifact just built in this repo will be consumed by it. As you rebuild this project, a new snapshot is consumed, and as it has a newer timestamp than the previous build, the downstream project will use the new one.

```bash
./gradlew publish -Plcevcdec=dev-SNAPSHOT
```
