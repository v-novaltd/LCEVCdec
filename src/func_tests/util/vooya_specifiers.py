import re

def resolution_from_filename(filename: str):
    input_specifiers = filename.lower().split('_')
    resolution_regexp = re.compile('([0-9]+)x([0-9]+)', re.IGNORECASE)
    resolution = next(
        map(lambda match: match.groups(),
            filter(lambda match: match is not None,
                   map(resolution_regexp.match, input_specifiers))),
        None)
    if resolution is None:
        raise RuntimeError('Can\'t parse resolution from filename')
    width, height = tuple([int(r) for r in resolution])
    return width, height

def bitdepth_from_filename(filename: str):
    input_specifiers = filename.lower().split('_')
    bitdepth_regexp = re.compile('(8bit|10bit|12bit|16bit)', re.IGNORECASE)
    return next(
        map(lambda match: int(match.groups()[0][:-3]),
            filter(lambda match: match is not None,
                   map(bitdepth_regexp.match, input_specifiers))),
        None)

def yuv_format_from_filename(filename: str):
    input_specifiers = filename.lower().split('_')
    format_regexp = re.compile('(420p|422p|444p)', re.IGNORECASE)
    return next(
        map(lambda match: match.groups()[0],
            filter(lambda match: match is not None,
                   map(format_regexp.match, input_specifiers))),
        '420p')
