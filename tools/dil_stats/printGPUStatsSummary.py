import os
import json
import csv
import argparse
import pandas as pd

GPU_STAT_NAMES = [
    "GPUProgram",
    "GPUTextureCreate",
    "GPUTextureCreateProtected",
    "GPUTextureUpdate",
    "GPUTextureUpdateStrips",
]

WORKING_DIR = os.path.dirname(__file__)
DEFAULT_INPUT_FILE_NAME = os.path.join(
    WORKING_DIR, "Dec_Ref_Playground_Stats.json")

stats_filename = DEFAULT_INPUT_FILE_NAME
output_csv_filename = None

stats_json = {}
stats = []

empty_stats = set(GPU_STAT_NAMES)
summary = {}


def parse_args():
    global stats_filename
    global output_csv_filename
    parser = argparse.ArgumentParser(
        description="Script to process raw GPU profiles.")
    parser.add_argument("-i", "--input", help="Path to input JSON file.",
                        default=DEFAULT_INPUT_FILE_NAME, required=False)
    parser.add_argument("-o", "--output", help="Output CSV filename to write stats to. If not specified, results will be written in human-readable format to stdout.",
                        required=False)

    args = parser.parse_args()
    stats_filename = args.input

    if args.output:
        output_csv_filename = args.output


def load_stats(stats_file):
    global stats_json
    global stats
    with open(stats_filename, "r") as f:
        stats_json = json.load(f)

    stats = stats_json["stats"]


def process():
    global summary
    global empty_stats
    gpu_stats = {k: [x for x in stats if x[1] == k] for k in GPU_STAT_NAMES}
    summary = {k: {"cpu": {}, "gpu": {}} for k in GPU_STAT_NAMES}

    dataframes = {k: None for k in GPU_STAT_NAMES}
    agg_list = ["min", "median", "max", "mean", "count"]

    for key in gpu_stats.keys():
        values = gpu_stats[key]
        dataframe_data = [[x[3], x[4], x[5]] for x in values]
        dataframes[key] = pd.DataFrame(
            dataframe_data, columns=["name", "cpu", "gpu"])
        df_grouped = dataframes[key].groupby("name")

        for processor in ["cpu", "gpu"]:
            for name, group in df_grouped:
                v = group[processor].agg(agg_list)
                s = {
                    "min": v["min"] * 1e-6,
                    "median": v["median"] * 1e-6,
                    "max": v["max"] * 1e-6,
                    "mean": v["mean"] * 1e-6,
                    "count": int(v["count"]),
                }

                summary[key][processor][name] = s
                if key in empty_stats:
                    empty_stats.remove(key)


def output_csv(filename):
    non_empty_stats = [x for x in GPU_STAT_NAMES if x not in empty_stats]

    header = ["stat", "resource_name", "cpu_min", "cpu_median", "cpu_max",
              "cpu_mean", "gpu_min", "gpu_median", "gpu_max", "gpu_mean"]
    rows = []
    for stat in non_empty_stats:
        for resource in summary[stat]["cpu"].keys():
            cpu_val = summary[stat]["cpu"][resource]
            gpu_val = summary[stat]["gpu"][resource]
            cpu_min = "{:.6f}".format(cpu_val["min"])
            cpu_median = "{:.6f}".format(cpu_val["median"])
            cpu_max = "{:.6f}".format(cpu_val["max"])
            cpu_mean = "{:.6f}".format(cpu_val["mean"])
            gpu_min = "{:.6f}".format(gpu_val["min"])
            gpu_median = "{:.6f}".format(gpu_val["median"])
            gpu_max = "{:.6f}".format(gpu_val["max"])
            gpu_mean = "{:.6f}".format(gpu_val["mean"])

            rows.append([stat, resource, cpu_min, cpu_median, cpu_max,
                        cpu_mean, gpu_min, gpu_median, gpu_max, gpu_mean])

    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)


def print_summary():
    for stat in GPU_STAT_NAMES:
        if stat in empty_stats:
            continue

        print(stat)
        for resource in summary[stat]["cpu"].keys():
            cpu_val = summary[stat]["cpu"][resource]
            gpu_val = summary[stat]["gpu"][resource]
            print("{}\tCPU - Min: {:.6f}, Max: {:.6f}, Mean: {:.6f}\tGPU - Min: {:.6f}, Max: {:.6f}, Mean: {:.6f}".format(
                resource, cpu_val["min"], cpu_val["max"], cpu_val["mean"], gpu_val["min"], gpu_val["max"], gpu_val["mean"]))

        print()


def main():
    parse_args()
    load_stats(stats_filename)
    process()

    if output_csv_filename is not None:
        output_csv(output_csv_filename)
    else:
        print_summary()


if __name__ == "__main__":
    main()
