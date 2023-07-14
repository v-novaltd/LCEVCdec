# Enabling Language Server Protocol

## Using `clangd`:

Add `-DCMAKE_EXPORT_COMPILE_COMMANDS=1` to the cmake configuration line.

Pass `--compile-commands-dir=_build` to clangd.
