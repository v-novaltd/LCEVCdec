# Emscripten

**The Emscripten port of `enhancement` and `pixel_processing` functions is not complete yet however the structure has been moved out of legacy to here**

This build target packages the core LCEVCdec functions into a web assembly (wasm) package. Primarily to support the [lcevc_dec.js](https://www.npmjs.com/package/lcevc_dec.js?activeTab=readme) project for decoding LCEVC streams in web players.

## Build the WASM

Install the emsdk to get the emscripten build tools. The latest tested version of emsdk is 4.0.6.

```bash
EMSDK_VERSION=4.0.6
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install ${EMSDK_VERSION}
./emsdk activate ${EMSDK_VERSION}
export EMSCRIPTEN_ROOT_PATH=$(pwd)
export PATH=PATH:$(pwd)
```

Build LCEVCdec with Emscripten:

```bash
cd LCEVCdec
mkdir build_emscripten && cd build_emscripten
cmake -DCMAKE_TOOLCHAIN_FILE="../cmake/toolchains/Emscripten.cmake" -DCMAKE_BUILD_TYPE="Release" ..
cmake --build .
cmake --install . --prefix install
```

Note that a 'Debug' build is also supported for `CMAKE_BUILD_TYPE` which will adjust the output javascript wrapper to make it easier to debug.

## Run the sample

You now have the LCEVCdec wasm and javascript wrapper built and installed in `LCEVCdec/build_emscripten/install/sample`, this includes a direct copy of `sample.html` from this dir. Simply serve the folder with the built-in python webserver and navigate to http://localhost:8000/sample.html to view the running decoder in your browser.

```bash
cd LCEVCdec/build_emscripten/
python3 -m http.server 8000 -d install/sample
```

Note that the webserver is required, and you cannot simply open `index.html` due to CORS restrictions on modern browsers.
