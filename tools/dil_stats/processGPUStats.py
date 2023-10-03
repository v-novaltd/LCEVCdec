import argparse
import csv
import json
import os
import statistics

expected_column_names = [
    "timehandle",
    "label",
    "cpu_duration_ns",
    "gpu_duration_ns",
]

program_prefix = "GLProgram_"
texture_prefix = "GLTexture_"

texture_prefixes = [
    texture_prefix + "Create_",
    texture_prefix + "CreateProtected_",
    texture_prefix + "Update_",
    texture_prefix + "UpdateStrips_",
]

texture_ops = [
    "create",
    "create_protected",
    "update",
    "update_strips",
]

WORKING_DIR = os.path.dirname(__file__)
DEFAULT_INPUT_FILE_NAME = os.path.join(
    WORKING_DIR, "debug_gpu_raw_profiles.csv")
DEFAULT_OUTPUT_FILE_NAME = ""
DEFAULT_STATS_FILE_NAME = ""
DEFAULT_JSON_FILE_NAME = ""

params = {
    "input": DEFAULT_INPUT_FILE_NAME,
    "output_file": DEFAULT_OUTPUT_FILE_NAME,
    "stats_file": DEFAULT_STATS_FILE_NAME,
    "json_file": DEFAULT_JSON_FILE_NAME,
    "merge_overwrite": False,
}

# Data
raw_data = {}

draw_profiles_data = {}
texture_profiles_data = {}

draw_stats = []
texture_stats = []


def parse():
    global params
    parser = argparse.ArgumentParser(
        description="Script to process raw GPU profiles.")
    parser.add_argument("-i", "--input", help="Path to input raw GPU profiles CSV file.",
                        default=DEFAULT_INPUT_FILE_NAME, required=False)
    parser.add_argument("-o", "--output_file", help="Path to output processed GPU profiles CSV file.",
                        default=DEFAULT_OUTPUT_FILE_NAME, required=False)
    parser.add_argument("-s", "--stats_file", help="Path to output GPU statistical data to a CSV file.",
                        default=DEFAULT_STATS_FILE_NAME, required=False)
    parser.add_argument("-j", "--json_file", help="Path to DIL stats JSON file to merge with.",
                        default=DEFAULT_JSON_FILE_NAME, required=False)
    parser.add_argument("--merge_overwrite", help="If this option and '--json_file' are set, the script will overwrite the existing JSON file with the merged contents. Otherwise, the merged contents will be written to '[json_file_merged.json]'",
                        required=False, default=False, action="store_true")

    args = parser.parse_args()
    params["input"] = args.input
    params["output_file"] = args.output_file
    params["stats_file"] = args.stats_file
    params["json_file"] = args.json_file
    params["merge_overwrite"] = args.merge_overwrite


def read_file():
    global raw_data

    csv_lines = []

    with open(params["input"], "r") as f:
        csv_reader = csv.reader(f, delimiter=",")
        for row in csv_reader:
            csv_lines.append(row)

    # Validate the expected column names
    columns = csv_lines[0]
    if len(columns) != len(expected_column_names):
        print(
            f"Unexpected number of columns (Expected: {len(expected_column_names)}, Actual: {len(columns)})")
        return False
    elif columns != expected_column_names:
        print("Column names do not match expected column names (Expected: {}, Actual: {}".format(
            ", ".join(expected_column_names), ", ".join(columns)))
        return False

    for i in range(1, len(csv_lines)):
        row = csv_lines[i]
        timehandle = row[0]
        label = row[1]
        cpu = row[2]
        gpu = row[3]

        raw_data.setdefault(timehandle, []).append(
            {"label": label, "cpu": cpu, "gpu": gpu})


def process_draw(timehandle, label, cpu_time, gpu_time):
    global draw_profiles_data
    d = draw_profiles_data.setdefault(label, {})
    d.setdefault("cpu_times", []).append(cpu_time)
    d.setdefault("gpu_times", []).append(gpu_time)
    d.setdefault("timehandles", []).append(timehandle)


def process_texture(timehandle, label, resource_op, cpu_time, gpu_time):
    global texture_profiles_data
    d = texture_profiles_data.setdefault(label, {}).setdefault(resource_op, {})
    d.setdefault("cpu_times", []).append(cpu_time)
    d.setdefault("gpu_times", []).append(gpu_time)
    d.setdefault("timehandles", []).append(timehandle)


def process():
    for timehandle, profiles in raw_data.items():
        for p in profiles:
            label = p["label"]
            cpu_time = int(p["cpu"])
            gpu_time = int(p["gpu"])
            if label.startswith(program_prefix):
                # Remove the prefix
                label = label[len(program_prefix):]
                process_draw(timehandle, label, cpu_time, gpu_time)
            elif label.startswith(tuple(texture_prefixes)):
                # Remove the prefix and check what type of texture operation it is
                match_index = -1
                for i in range(0, len(texture_prefixes)):
                    if label.startswith(texture_prefixes[i]):
                        match_index = i
                        break

                process_texture(timehandle, label,
                                texture_ops[match_index], cpu_time, gpu_time)


def compute(stats, times):
    stats["min"] = min(times)
    stats["max"] = max(times)
    stats["median"] = statistics.median(times)
    stats["mean"] = statistics.mean(times)
    stats["stddev"] = statistics.stdev(times) if len(times) > 1 else 0


def compute_stats():
    global draw_stats
    global texture_stats

    for label, profiles in draw_profiles_data.items():
        cpu_times = profiles["cpu_times"]
        gpu_times = profiles["gpu_times"]
        stats = {"label": label, "cpu": {}, "gpu": {}}
        compute(stats["cpu"], cpu_times)
        compute(stats["gpu"], gpu_times)

        draw_stats.append(stats)

    for label, profiles in texture_profiles_data.items():
        stats = {"label": label}
        for i in range(0, len(texture_ops)):
            if texture_ops[i] in profiles:
                cpu_times = profiles[texture_ops[i]]["cpu_times"]
                gpu_times = profiles[texture_ops[i]]["gpu_times"]
                stats[texture_ops[i]] = {"cpu": {}, "gpu": {}}
                compute(stats[texture_ops[i]]["cpu"], cpu_times)
                compute(stats[texture_ops[i]]["gpu"], gpu_times)

        texture_stats.append(stats)


def write_stats():
    with open(params["stats_file"], "w", newline="") as f:
        fieldnames = [
            "label",
            "type",
            "operation",
            "cpu_min",
            "cpu_max",
            "cpu_median",
            "cpu_mean",
            "cpu_stddev",
            "gpu_min",
            "gpu_max",
            "gpu_median",
            "gpu_mean",
            "gpu_stddev",
        ]
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()

        # Write draw stats
        for stat in draw_stats:
            label = stat["label"]
            cpu_min = stat["cpu"]["min"]
            cpu_max = stat["cpu"]["max"]
            cpu_median = stat["cpu"]["median"]
            cpu_mean = stat["cpu"]["mean"]
            cpu_stddev = stat["cpu"]["stddev"]

            gpu_min = stat["gpu"]["min"]
            gpu_max = stat["gpu"]["max"]
            gpu_median = stat["gpu"]["median"]
            gpu_mean = stat["gpu"]["mean"]
            gpu_stddev = stat["gpu"]["stddev"]

            row = {
                "label": label,
                "type": "program",
                "operation": "draw",
                "cpu_min": f"{cpu_min:.0f}",
                "cpu_max": f"{cpu_max:.0f}",
                "cpu_median": f"{cpu_median:.0f}",
                "cpu_mean": f"{cpu_mean:.0f}",
                "cpu_stddev": f"{cpu_stddev:.0f}",
                "gpu_min": f"{gpu_min:.0f}",
                "gpu_max": f"{gpu_max:.0f}",
                "gpu_median": f"{gpu_median:.0f}",
                "gpu_mean": f"{gpu_mean:.0f}",
                "gpu_stddev": f"{gpu_stddev:.0f}",
            }

            w.writerow(row)

        for stat in texture_stats:
            label = stat["label"]
            for i in range(0, len(texture_ops)):
                op = texture_ops[i]

                if op not in stat:
                    continue

                cpu_min = stat[op]["cpu"]["min"]
                cpu_max = stat[op]["cpu"]["max"]
                cpu_median = stat[op]["cpu"]["median"]
                cpu_mean = stat[op]["cpu"]["mean"]
                cpu_stddev = stat[op]["cpu"]["stddev"]

                gpu_min = stat[op]["gpu"]["min"]
                gpu_max = stat[op]["gpu"]["max"]
                gpu_median = stat[op]["gpu"]["median"]
                gpu_mean = stat[op]["gpu"]["mean"]
                gpu_stddev = stat[op]["gpu"]["stddev"]

                row = {
                    "label": label,
                    "type": "texture",
                    "operation": texture_ops[i],
                    "cpu_min": f"{cpu_min:.0f}",
                    "cpu_max": f"{cpu_max:.0f}",
                    "cpu_median": f"{cpu_median:.0f}",
                    "cpu_mean": f"{cpu_mean:.0f}",
                    "cpu_stddev": f"{cpu_stddev:.0f}",
                    "gpu_min": f"{gpu_min:.0f}",
                    "gpu_max": f"{gpu_max:.0f}",
                    "gpu_median": f"{gpu_median:.0f}",
                    "gpu_mean": f"{gpu_mean:.0f}",
                    "gpu_stddev": f"{gpu_stddev:.0f}",
                }

            w.writerow(row)


def write_output():
    with open(params["output_file"], "w", newline="") as f:
        fieldnames = [
            "timehandle",
            "label",
            "type",
            "operation",
            "cpu_time",
            "gpu_time",
        ]
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()

        # Write draw stats
        for label, profiles in draw_profiles_data.items():
            for i in range(0, len(profiles["cpu_times"])):
                th = profiles["timehandles"][i]
                cpu = profiles["cpu_times"][i]
                gpu = profiles["gpu_times"][i]
                row = {
                    "timehandle": th,
                    "label": label,
                    "type": "program",
                    "operation": "draw",
                    "cpu_time": cpu,
                    "gpu_time": gpu,
                }

                w.writerow(row)

        # Write texture stats
        for label, profiles in texture_profiles_data.items():
            for i in range(0, len(texture_ops)):
                if texture_ops[i] in profiles:
                    for j in range(0, len(profiles[texture_ops[i]]["cpu_times"])):
                        th = profiles[texture_ops[i]]["timehandles"][j]
                        cpu = profiles[texture_ops[i]]["cpu_times"][j]
                        gpu = profiles[texture_ops[i]]["gpu_times"][j]

                        row = {
                            "timehandle": th,
                            "label": label,
                            "type": "texture",
                            "operation": texture_ops[i],
                            "cpu_time": cpu,
                            "gpu_time": gpu,
                        }

                        w.writerow(row)


def merge_with_json_stats():
    with open(params["json_file"], "r") as json_file:
        data = json.load(json_file)

    field_names = data["FieldNames"]
    all_stats = data["stats"]
    timehandle_stat_index = field_names.index("TIMEHANDLE")

    # Get the indices of the stat row keyed by timehandle in the JSON
    timehandle_map = {str(x[timehandle_stat_index]): stat_index for stat_index, x in enumerate(data["stats"])}

    # Populate the program and texture operation indices from the JSON so that we can merge to the correct fields
    gl_program_indices = {field: field_names.index(
        field) for field in field_names if field.startswith(program_prefix)}

    # Update stats for GL programs
    for field, field_index in gl_program_indices.items():
        # Sanitize the field name
        sfield = field[len(program_prefix):]

        # Check if we have GPU stats for the program, continue otherwise
        if sfield not in draw_profiles_data:
            continue

        # Get the GPU stats
        profiles = draw_profiles_data[sfield]

        for i, timehandle in enumerate(profiles["timehandles"]):
            # Check existence of timehandle
            if timehandle not in timehandle_map:
                print(
                    f"JSON MERGE - {field}: Timehandle '{timehandle}' not found in JSON.")
                continue

            # Update the stats row with a value for this GLProgram
            stats_row = all_stats[timehandle_map[timehandle]]
            stats_row[field_index] = profiles["gpu_times"][i]

    # Update stats for GL textures
    gl_texture_indices = [{field: field_names.index(
        field) for field in field_names if field.startswith(x)} for x in texture_prefixes]

    for i in range(0, len(texture_ops)):
        indices = gl_texture_indices[i]
        for field, field_index in indices.items():
            sfield = field[len(texture_prefixes[i]):]

            if sfield not in texture_profiles_data:
                continue

            profiles = texture_profiles_data[sfield][texture_ops[i]]
            for i, timehandle in enumerate(profiles["timehandles"]):
                if timehandle not in timehandle_map:
                    print(
                        f"JSON MERGE - {field}: Timehandle '{timehandle}' not found in JSON.")
                    continue

                stats_row = all_stats[timehandle_map[timehandle]]
                stats_row[field_index] = profiles["gpu_times"][i]

    split_filename = os.path.splitext(params["json_file"])
    merged_output_filename = "{}_merged{}".format(
        split_filename[0], split_filename[1])
    if params["merge_overwrite"]:
        merged_output_filename = params["json_file"]

    with open(os.path.join(WORKING_DIR, merged_output_filename), "w") as f:
        json.dump(data, f)


def main():
    parse()
    read_file()
    process()

    if params["output_file"]:
        write_output()

    if params["stats_file"]:
        compute_stats()
        write_stats()

    if params["json_file"]:
        merge_with_json_stats()


if __name__ == "__main__":
    main()
