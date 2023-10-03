#!/usr/bin/python3
#
# This processes the JSON stats output and converts to csv file for processing in excel (using the statsTemplate code)
#
# You can add extra calculated columns using the `-x str` option where str is in this form (basically a JSON string)
#  -x '{"custom": ["LcevcEndTime", "LcevcReceivedTime"]}'
# This creates a new column (called custom) calculated by LcevcEndTime - LcevcReceivedTime
# This can then be selected as normal, or will just be added to the end of all the columns
#
# This is getting complicated I wonder if it would be worth stuffing the JSON data into an SQLlite db
# then we can use SQL to select the data out of a table, which would make the custom selection
# simple an SQL statement (a simple string). If we are lucky we should be able to use constants and other
# cleaver things for calculated columns.
#
import argparse
import json
import sys


# dictionary of calculatedFieldName, and array of field0 and field1
# simple calculation field0 - field1
calculated_columns = {"LcevcDuration": {"fields": ["LcevcEndTime", "LcevcStartTime"], "type": "double", "operation": "subtract"},
                      "LcevcGapDuration": {"fields": ["LcevcStartTime", "last.LcevcEndTime"], "type": "double", "operation": "subtract"},
                      "DecodeDuration": {"fields": ["DecodeEndTime", "DecodeStartTime"], "type": "double", "operation": "subtract"},
                      "DeinterlaceDuration": {"fields": ["DeinterlaceEndTime", "DeinterlaceStartTime"], "type": "double", "operation": "subtract"},
                      "DecodeCallbackDuration": {"fields": ["DecodeCallbackEndTime", "DecodeCallbackStartTime"], "type": "double", "operation": "subtract"},
                      "DecodeGapDuration": {"fields": ["DecodeStartTime", "last.DecodeEndTime"], "type": "double", "operation": "subtract"},
                      "LcevcToDecodeGapDuration": {"fields": ["DecodeStartTime", "LcevcEndTime"], "type": "double", "operation": "subtract"},
                      "DeinterlaceGapDuration": {"fields": ["DeinterlaceStartTime", "last.DeinterlaceEndTime"], "type": "double", "operation": "subtract"},
                      "DecodeTotalDuration": {"fields": ["DecodeDuration", "DeinterlaceDuration"], "type": "double", "operation": "add"},
                      "DecodeRequiredByGapDuration": {"fields": ["DecodeRequireByTime", "last.DecodeRequireByTime"], "type": "double", "operation": "subtract"},
                      "RenderPrepareDuration": {"fields": ["RenderPrepareEndTime", "RenderPrepareStartTime"], "type": "double", "operation": "subtract"},
                      "RenderSwapDuration": {"fields": ["RenderSwapEndTime", "RenderSwapStartTime"], "type": "double", "operation": "subtract"},
                      "RenderDuration": {"fields": ["RenderPrepareDuration", "RenderSwapDuration"], "type": "double", "operation": "add"},
                      "RenderDesiredGapDuration": {"fields": ["RenderDesiredStartTime", "last.RenderDesiredStartTime"], "type": "double", "operation": "subtract"},
                      "RenderCallbackDuration": {"fields": ["RenderCallbackEndTime", "RenderCallbackStartTime"], "type": "double", "operation": "subtract"},
                      "RenderGapDuration": {"fields": ["DecodeRequireByTime", "last.DecodeRequireByTime"], "type": "double", "operation": "subtract"},
                      "RenderResizeCallbackDuration": {"fields": ["RenderResizeCallbackEndTime", "RenderResizeCallbackStartTime"], "type": "double", "operation": "subtract"}
                      }


def get_field_index(field_name, all_field_names):
    if field_name in all_field_names:
        return all_field_names.index(field_name)
    return -1


def add_extra_calculated_columns(extra_columns):
    if extra_columns != "":
        extra_calculated = json.loads(extra_columns)
        for f in extra_calculated:
            if (len(extra_calculated[f]["fields"]) == 2):
                calculated_columns[f] = extra_calculated[f]


def get_all_ordered_field_names(all_field_names):
    # Its better to pick a good order for the CSV file here, rather than use the one provided in the json file
    # Not specifying the GLProgram stats will ensure they are always at the end
    ordered_fields = ["FrameCount", "FrameCountForCC", "CC", "PTS", "TIMEHANDLE",
                      "LcevcReceivedTime", "LcevcStartTime", "LcevcEndTime", "LcevcToDecodeGapDuration",
                      "DeinterlaceStartTime", "DeinterlaceEndTime", "DecodeStartTime", "DecodeEndTime",
                      "RenderPrepareStartTime", "RenderPrepareEndTime", "RenderSwapStartTime", "RenderSwapEndTime",
                      "DecodeRequireByTime", "DecodeRequiredByGapDuration", "RenderDesiredStartTime", "RenderDesiredGapDuration",
                      "LcevcIsValid", "DeinterlaceResult", "DecodeResult", "DecodeSkipped", "RenderResult", "RenderSkipped",
                      "LcevcDuration", "DeinterlaceDuration", "DecodeDuration", "DecodeTotalDuration", "RenderDuration",
                      "LcevcGapDuration", "DeinterlaceGapDuration", "DecodeGapDuration", "RenderGapDuration",
                      "DeinterlaceWaitDuration", "DecodeWaitDuration", "RenderWaitDuration", "RenderDelayDuration",
                      "LcevcQueueDuration", "LcevcQueueProcessedDuration", "LcevcWaitDuration",
                      "LcevcRawQueueDepth", "LcevcProcessedQueueDepth", "LcevcChromaResiduals",
                      "LcevcSize", "LcevcParseDuration", "LcevcNALSize", "LcevcNALProcessDuration", "LcevcNALAddDuration",
                      "DeinterlaceFnDuration",
                      "DecodeLcevcWaitDuration", "DecodePrepareDuration",
                      "DecodeBaseDuration", "DecodeUpscaleDuration", "DecodeHighDuration", "DecodeSFilterDuration",
                      "DecodeGLUploadDuration", "DecodeGLConvertDuration", "DecodeGLPipelineDuration", "DecodeGLColorInfoDuration", "DecodeUploadLoq0Duration", "DecodeUploadLoq1Duration", "DecodeUploadTextureDuration",
                      "DecodeOutputDuration", "DecodeApplyDuration", "DecodeFrameDuration",
                      "DecodeFnStartDuration", "DecodeFnPrepareDuration", "DecodeFnInternalDuration", "DecodeFnFinishDuration",
                      "DecodeCallbackStartTime", "DecodeCallbackEndTime", "DecodeCallbackDuration", "DecodeCallbackWaitDuration", "DecodeCallbackQueueDepth",
                      "RenderCount", "RenderPrepareInitialiseDuration", "RenderFrameDuration",
                      "RenderGLPrepareDuration", "RenderGLConvertDuration", "RenderGLSetupDuration", "RenderPrepareDuration", "RenderSwapDuration",
                      "RenderCallbackStartTime", "RenderCallbackEndTime", "RenderCallbackDuration", "RenderCallbackWaitDuration", "RenderCallbackQueueDepth",
                      "RenderResizeCallbackStartTime", "RenderResizeCallbackEndTime", "RenderResizeCallbackDuration", "RenderResizeCallbackWaitDuration", "RenderResizeCallbackQueueDepth",
                      "DitherStrength", "GpuDecode", "Loq0Active", "Loq1Active", "Loq0Reset", "Loq1Reset", "Loq0Scale", "Loq1Scale", "Loq0Temporal", "Loq1Temporal",
                      "IDRFrame", "LcevcKeyFrame", "BaseKeyFrame", "DumpTime",
                      "LcevcWidth", "LcevcHeight", "LcevcBitdepth", "LcevcOriginX", "LcevcOriginY", "LcevcInternalWidth", "LcevcInternalHeight",
                      "LcevcLimitedRange", "LcevcColorPrimaries", "LcevcMatrixCoefficients", "LcevcColorTransfer",
                      "InputWidth", "InputHeight", "InputBitdepth", "InputImageType", "InputColorFormat",
                      "InputLimitedRange", "InputColorPrimaries", "InputMatrixCoefficients", "InputColorTransfer",
                      "OutputWidth", "OutputHeight", "OutputBitdepth", "OutputImageType", "OutputColorFormat",
                      "OutputLimitedRange", "OutputColorPrimaries", "OutputMatrixCoefficients", "OutputColorTransfer",
                      "RenderWidth", "RenderHeight", "RenderBitdepth", "RenderImageType", "RenderColorFormat",
                      "PeakMemory"]

    all_ordered_field_names = []

    # first add all the ordered names
    for f in ordered_fields:
        if f in all_field_names:
            all_ordered_field_names.append(f)
        elif "GLProgram_" not in f or "GLTexture_" not in f:  # not all programs are loaded on all platforms
            print(f"Ordered field <{f}> is not a valid stats field")
    # now add any missing ones
    for f in all_field_names:
        if f not in all_ordered_field_names:
            all_ordered_field_names.append(f)
            print(f"Field <{f}> is missing from the ordered list")

    if len(all_ordered_field_names) != len(all_field_names):
        print(
            f"The ordered_fields list contains names that are not available any more, found {len(all_ordered_field_names)} expected {len(all_field_names)}")
        return []
    return all_ordered_field_names


def get_calculated_column(col_name, all_field_names):
    cols = col_name.split(".")
    f = cols[-1]
    # field not available, return out of range index
    if f not in all_field_names:
        return len(all_field_names) + 1
    idx = all_field_names.index(f)
    # if the field_name starts with `last.` then use the
    # previous rows data, indicated with a -ve index
    if len(cols) > 1 and cols[0] == "last":
        idx = -idx
    return idx


def get_column_value(all_rows, row, col_idx):
    # ensure index is in range
    idx = abs(col_idx)
    if idx > len(all_rows[0]):
        return 0  # return 0 for columns that don't exist
    # -ve index indicated the previous row's data
    if col_idx < 0:
        return all_rows[row - 1][idx]
    else:
        return all_rows[row][idx]


def get_calculation_function(cc_info):
    def subtract(v1, v2):
        return v1 - v2

    def add(v1, v2):
        return v1 + v2

    def multiply(v1, v2):
        return v1 * v2

    def divide(v1, v2):
        return v1 / v2

    # default is subtraction
    operation = "subtract"
    if "operation" in cc_info:
        operation = cc_info["operation"]

    if operation == "add":
        return add
    elif operation == "multiply":
        return multiply
    elif operation == "divide":
        return divide
    return subtract


def add_calculated_columns(all_field_names, calculated_columns, all_rows):
    for k in calculated_columns:
        cc_info = calculated_columns[k]
        i1 = get_calculated_column(cc_info["fields"][0], all_field_names)
        i2 = get_calculated_column(cc_info["fields"][1], all_field_names)
        calc = get_calculation_function(cc_info)
        index = all_field_names.index(k)
        for j in range(len(all_rows)):
            # need to use the previous row, but there is not one
            if (i1 < 0 or i2 < 0) and j == 0:
                res = "N/A"
            else:
                v1 = get_column_value(all_rows, j, i1)
                v2 = get_column_value(all_rows, j, i2)
                res = round(calc(v1, v2), 3)
            all_rows[j].insert(index, res)
            # print(f"calculating {i} using <{calculated_columns[i]}> index {index} {len(all_rows[j])} {i1} {i2} res {res}")
    return all_rows


def do_details(data):
    headers = {
        "TestInfo": ["UTC Run", "URI", "GL Decode", "Pre-Calculate", "Dithering", "Perseus Surface FP Setting", "Pipeline Format", "use LOQ0", "use LOQ1", "supports protected mode", "passthrough mode", "stable frame count", "predicted average method", "Residual Type"],
        "Version": ["DILVersion", "DPIVersion"],
        "JSONConfig": ["initial", "final"],
        "CPUInfo": ["CPU"],
        "BaseDecoderInfo": ["HWDecodingEnabled", "UsingHWDecoding", "Decoder"],
        "GLInfo": ["Version", "GLSLVersion", "Renderer", "Vendor", "UsingES2"],
    }
    kStr = ""
    vStr = ""
    for i in headers:
        for j in headers[i]:
            if (len(kStr) > 0):
                kStr += ","
                vStr += ","
            kStr += i + "." + j
            vStr += str(data[i][j]).replace(",", "_")
    print(kStr)
    print(vStr)


def line_to_csv(field_keys, field_types, stat_line, count, done_header):
    rStr = ""
    if done_header == 0:
        print(",".join("%s" % x for x in field_types))
        print(",".join("%s" % x for x in field_keys))
        done_header = 1
    for i in field_keys:
        if (len(rStr) > 0):
            rStr += ","
        rStr = rStr + str(stat_line[field_keys[i]])
    print(rStr)
    return done_header


def convert_to_csv(brief, field_keys, field_types, stats, data, skip_start, skip_end):
    # pull out the details
    if not brief:
        do_details(data)
    # now process each line
    done_header = 1 if brief else 0
    skip_end = skip_end if skip_end > skip_start else len(stats)
    for i in range(skip_start, skip_end):
        done_header = line_to_csv(
            field_keys, field_types, stats[i], i, done_header)


def append_calculated_columns(all_field_names, all_field_types):
    cc_list = list(calculated_columns.keys())
    all_field_names.extend(cc_list)
    # assume all calculated columns are double's
    for f in cc_list:
        cc_info = calculated_columns[f]
        type = "double"
        if "type" in cc_info.keys():
            type = cc_info["type"]
        all_field_types.append(type)


def parse_json_from_device(file, brief, order_column, skip_start, skip_end, columns):
    try:
        # parse the JSON
        json_file = open(file)
        data = json.load(json_file)
        # collect the field names
        all_field_names = data["FieldNames"]
        all_field_types = data["FieldTypes"]
        # Add the extra calculated columns
        append_calculated_columns(all_field_names, all_field_types)
        duplicates = [
            x for x in all_field_names if all_field_names.count(x) > 1]
        if len(duplicates) > 0:
            print(
                f"The following fields are duplicated between calculated and all fields <{duplicates}>")
            return
        if len(columns) == 0:
            # now we want a list, combining the ordered_fields and all_field_names
            check_list = get_all_ordered_field_names(all_field_names)
        else:
            check_list = columns

        field_keys = {}
        field_types = []
        # make a dictionary, key is fieldname, value is index in data array.
        for f in check_list:
            if f in all_field_names:
                field_keys[f] = all_field_names.index(f)
                field_types.append(all_field_types[field_keys[f]])
            else:
                print(f"Field <{f}> is not a valid field")
        if len(field_keys) != len(check_list):
            print(
                f"Failed to match all the desired columns, found {len(field_keys)} expected {len(check_list)}, <{check_list}>")
            return
        # collect the stats array, sort it by the FrameCount
        # can no longer guarantee that the stats file is ordered correctly
        sort_idx = -1 if order_column == "" else \
            get_field_index(order_column, all_field_names)
        all_rows = data["stats"]
        # sort the array if we have a valid column to sort on
        if sort_idx <= len(all_rows):
            all_rows = sorted(all_rows, key=lambda x: x[sort_idx])
        stats = add_calculated_columns(
            all_field_names, calculated_columns, all_rows)
        convert_to_csv(brief, field_keys, field_types,
                       stats, data, skip_start, skip_end)
        return 0
    except Exception as e:
        print("Bad file", file, e)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Main for invoking on an individual component.
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


def main():
    parser = argparse.ArgumentParser(description="Analyse DIL Stats JSON file")
    parser.add_argument('-b', '--brief', action='store_true',
                        help="Just the frame stats and nothing else")
    parser.add_argument('-c', '--columns', default="",
                        help="Column names and order to collect")
    parser.add_argument('-e', '--end', required=False,
                        type=int, default=0, help="last frame number")
    parser.add_argument('-f', '--file', required=True, help="JSON stats file")
    parser.add_argument('-n', '--number-of-frames', required=False,
                        type=int, default=0, help="number of frames to process")
    parser.add_argument('-o', '--order',
                        default="FrameCount", help="Order by column, empty string is no ordering, default is FrameCount")
    parser.add_argument('-s', '--start', required=False,
                        type=int, default=0, help="start frame number")
    parser.add_argument('-x', '--extra-calculated',
                        default="", help="Extra calculated columns")
    args = parser.parse_args()
    add_extra_calculated_columns(args.extra_calculated)
    columns = [x.strip()
               for x in args.columns.split(',') if not x.strip() == '']
    end = args.start + args.number_of_frames if args.end == 0 else args.end
    return parse_json_from_device(args.file, args.brief, args.order, args.start, end, columns)


if __name__ == "__main__":
    sys.exit(main())
