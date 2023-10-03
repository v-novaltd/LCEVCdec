import os
import json
import argparse

WORKING_DIR = os.path.dirname(__file__)

HEADER_KEYS = [
    "TestInfo",
    "Version",
    "JSONConfig",
    "CPUInfo",
    "BaseDecoderInfo",
    "GLInfo",
]

FIELD_PROPERTY_KEYS = [
    "FieldNames",
    "FieldTypes",
    "FieldDefaults",
]

GPU_STAT_NAMES = [
    "GPUProgram",
    "GPUTextureCreate",
    "GPUTextureCreateProtected",
    "GPUTextureUpdate",
    "GPUTextureUpdateStrips",
]

GPU_FIELD_NAMES = [
    "GPUOp_Name",
    "GPUOp_Resource",
    "GPUOp_CPUDuration",
    "GPUOp_GPUDuration",
    "GPUOp_IssuedCompletedDelta",
]

DEFAULT_INPUT_FILE_NAME = os.path.join(
    WORKING_DIR, "Dec_Ref_Playground_Stats.json")
DEFAULT_OUTPUT_FILE_NAME = os.path.join(
    WORKING_DIR, "Dec_Ref_Playground_Stats_Transposed.json")

input_file = DEFAULT_INPUT_FILE_NAME
output_file = DEFAULT_OUTPUT_FILE_NAME

input_json = {}
output_json = {}

transposed_field_names = []
transposed_field_types = []
transposed_field_defaults = []


def parse_args():
    global input_file
    global output_file
    parser = argparse.ArgumentParser(
        description="Script to process raw GPU profiles.")
    parser.add_argument("-i", "--input", help="Path to input JSON file.",
                        default=DEFAULT_INPUT_FILE_NAME, required=False)
    parser.add_argument("-o", "--output", help="Path to transposed output JSON file.",
                        default=DEFAULT_OUTPUT_FILE_NAME, required=False)

    args = parser.parse_args()
    input_file = args.input
    output_file = args.output


def get_fields():
    lens = []
    arrs = {}

    # Validate all of the things
    for k in FIELD_PROPERTY_KEYS:
        arr = input_json.get(k)

        if arr is None:
            print(f"Key '{k}' does not exist.")
            return False, None

        if len(arr) == 0:
            print(f"Array for key '{k}' is empty.")
            return False, None

        lens.append(len(arr))
        arrs[k] = arr

    num_read = len(lens)
    num_needed = len(FIELD_PROPERTY_KEYS)
    if num_read != num_needed:
        print(
            f"Length of read arrays '{num_read}' don't match length of required arrays '{num_needed}'.")
        return False, None

    # Check that all of the read arrays have the same length
    val = lens[0]
    success = True
    for ln in lens:
        if ln != val:
            success = False
            break

    if not success:
        errStr = ",".join(["{}: {}".format(FIELD_PROPERTY_KEYS[i], lens[i])
                          for i in range(0, len(lens))])
        print(f"Field arrays have mismatching sizes - ({errStr}).")
        return False, None

    # Phew
    return True, arrs


def copy_header():
    global output_json

    for k in HEADER_KEYS:
        output_json[k] = input_json.get(k, {})


def write_fields():
    global output_json
    output_json[FIELD_PROPERTY_KEYS[0]] = transposed_field_names
    output_json[FIELD_PROPERTY_KEYS[1]] = transposed_field_types
    output_json[FIELD_PROPERTY_KEYS[2]] = transposed_field_defaults


def split_timehandle(timehandle):
    CC_MASK = 0xFFFF
    PTS_MASK = 0xFFFFFFFFFFFF
    cc = (timehandle >> 48) & CC_MASK
    pts = ((timehandle << 16) >> 16) & PTS_MASK
    return cc, pts


def set_field(name, value, field_lut, output_stat_row):
    if name in field_lut:
        output_stat_row[field_lut[name]] = value


def transpose_stats(fieldprops, stats):
    global output_json
    transposed_stats = []

    # Construct an index LUT from the field names
    lut = {}
    for i, n in enumerate(fieldprops["FieldNames"]):
        lut[n] = i

    # Populate a dictionary with all of the timehandles
    stats_dict = {}
    for s in stats:
        stats_dict.setdefault(s[0], []).append(s[1:])

    # Construct the row of stats
    for timehandle in stats_dict.keys():

        # Create empty stat array
        stat = [None] * len(fieldprops["FieldNames"])

        # Split timehandle
        cc, pts = split_timehandle(timehandle)
        set_field("CC", cc, lut, stat)
        set_field("PTS", pts, lut, stat)
        set_field("TIMEHANDLE", timehandle, lut, stat)

        statsForTimeHandle = stats_dict[timehandle]
        gpuStatNameSet = set(GPU_STAT_NAMES)

        for currentStat in statsForTimeHandle:
            # Iterate through the list of stat values
            # If we have GPU field, we treat it a little bit special
            name = currentStat[0]
            _ = currentStat[1]  # Calltime of the stat. Ignore this for now
            value = currentStat[2]

            if name in lut and name not in gpuStatNameSet:
                set_field(name, value, lut, stat)
            elif name in gpuStatNameSet:
                # print(f"GPU Stat Name: {name}.")
                pass
            else:
                print(f"Error: '{name}' not in LUT or GPU_STAT_NAMES list.")

        transposed_stats.append(stat)

    return transposed_stats


def main():
    global input_json

    parse_args()

    # Load input JSON
    with open(input_file, "r") as in_fp:
        input_json = json.load(in_fp)

    # Process stats
    result, field_properties = get_fields()
    if not result:
        exit(-1)

    copy_header()

    stats = input_json["stats"]
    gpu_stats = [[]] * len(GPU_STAT_NAMES)
    for i in range(0, len(GPU_STAT_NAMES)):
        gpu_stats[i] = [x for x in stats if x[1] == GPU_STAT_NAMES[i]]

    transposed_stats = transpose_stats(field_properties, stats)

    # Populate defaults
    default_values = field_properties["FieldDefaults"]
    for s in transposed_stats:
        for i, v in enumerate(s):
            if v is None:
                s[i] = default_values[i]

    for k in FIELD_PROPERTY_KEYS:
        output_json[k] = field_properties[k]

    output_json["stats"] = transposed_stats

    # Write output JSON, in a pretty format so it can be viewed in an editor
    with open(output_file, "w") as out_fp:
        json.dump(output_json, out_fp, indent=2, separators=(',', ': '))


if __name__ == "__main__":
    main()
