_PRINT_PROPERTIES = "--print-properties"


def pytest_addoption(parser):
    parser.addoption(_PRINT_PROPERTIES, default="failed",
                     choices=["never", "always", "failed"],
                     help="Whether or not to print properties recorded by tests")


def pytest_configure(config):
    config.pluginmanager.register(Reporter(config))


class Reporter(object):
    def __init__(self, config):
        self._config = config

    def pytest_report_teststatus(self, report):
        if report.when == "call":
            wasxfail = hasattr(report, "wasxfail") and report.wasxfail

            if report.passed:
                letter = "P"
            elif report.skipped:
                letter = "x" if wasxfail else "S"
            elif report.failed:
                letter = "F"

            return report.outcome, letter, report.outcome.upper()

    def pytest_runtest_logreport(self, report):
        if report.when == "call" or (report.when == "setup" and report.failed):
            print(f" {report.nodeid} ", end='')

            if report.failed:
                if hasattr(report.longrepr, "reprcrash"):
                    print(report.longrepr.reprcrash.message)

                print(report.longrepr)
            else:
                print()

            if self._should_print_properties(report.failed) and report.user_properties:
                self._print_property(report.user_properties)

                if report.capstdout:
                    print(f"\tstdout = {report.capstdout.rstrip()}")

                if report.capstderr:
                    print(f"\tstderr = {report.capstderr.rstrip()}")

    def _should_print_properties(self, did_fail):
        print_case = self._config.getoption(_PRINT_PROPERTIES)

        return print_case == "always" or (print_case == "failed" and did_fail)

    def _print_property(self, value, indent_level=0):
        indent = indent_level * '\t'
        next_level = indent_level + 1

        if isinstance(value, list):
            print()

            for item in value:
                self._print_property(item, indent_level=next_level)
        elif isinstance(value, dict):
            print()

            for key, item in value.items():
                self._print_property((key, item), indent_level=next_level)
        elif isinstance(value, tuple):
            if len(value) >= 2:
                key = value[0]
                remainder = value[1] if len(value) == 2 else list(value[1:])

                print(f"{indent}{key} =", end='')
                self._print_property(remainder, indent_level=next_level)
            elif len(value) == 1:
                self._print_property(value[0], indent_level=indent_level)
        else:
            print(f" {value}")
