# Shader Validator

## Introduction
A Python script that uses the Khronos GLSL reference compiler (`glslangvalidator`) to generate variants of shaders and validate their compilation on different GL and GLES versions.

## Requirements
- Python 3.7
- pyyaml
    - Usually packaged with Python 3.7. If not present, then install with `pip install pyyaml`
- glslangvalidator
    - Found [HERE](https://github.com/KhronosGroup/glslang/releases/tag/master-tot)
    - Must be present on the system's `$PATH`

## Usage
```
shader_validator.py [-h] [-m METADATA] [-f FILTER]
                           [-i IGNORE [IGNORE ...]] [-v VARIANTDIR]
                           [-r RESULTS] [-c CLEANUP]
```
| Option | Switch (Short) | Description | Default | Example |
|--------|--------|-------------|---------|---|
| Metadata file | `--metadata` (`-m`) | Path to shader metadata YAML file | *shader_metadata.yaml* | `shader_validator.py -m "../metadata.yaml"` |
| Filter | `--filter` (`-f`) | Name filter to only generate shader variants that satisfy this glob filter | *(empty)* | `shader_validator.py -f "*blit*.glsl"` |
| Ignore List | `--ignore` (`-i`) | Shader file paths to ignore. Can have multiple values | *(empty)* | `shader_validator.py -i "shaders/my_shader.glsl" "shaders/ignore_me.glsl"` |
| Variant Directory | `--variantdir` (`-v`) | Path to directory to output shader variant files | *"variants/"* | `shader_validator.py -v "my_v_dir"` |
| Results file | `--results` (`-r`) | Path to YAML file where results are output to | *results.yaml* | `shader_validator.py -r "../path/shader_results_file.yaml"` |
| Cleanup Mode | `--cleanup` (`-c`) | Cleanup operation of variant directory after variant generation has completed. Options: 0 - No removal, 1 - Remove only successfully validated variants, 2 - Remove everything | *1* | `shader_variants.py -c 0` |

## Notes
- *WARNING: If a GLSL shader is present in one of the requested directories and there is no metadata for that shader in the metadata file, then the validator will fail. **All** shaders that want validation need to have valid metadata added for them.*
- It is the user's responsibility to ensure that all `#define` switches and their possible values are provided in the metadata file, otherwise not all variants will be generated.