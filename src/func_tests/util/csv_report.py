

def values_to_string(values: list):
    return ','.join([str(v) for v in values])


class CsvReport:
    def __init__(self, path: str):
        self._path = path
        self._last_header_str = None
        self._lines = []

    def add_values(self, values: list):
        self._lines.append(values_to_string(values))

    def add_header(self, header: list):
        header_str = values_to_string(header)
        if self._last_header_str != header_str:
            self.add_values(header)
            self._last_header_str = header_str

    def add(self, items: list):
        header = [i[0] for i in items]
        values = [i[1] for i in items]
        self.add_header(header)
        self.add_values(values)

    def dump(self):
        if self._path:
            with open(self._path, 'wt') as csv_file:
                csv_file.write('\n'.join(self._lines))
