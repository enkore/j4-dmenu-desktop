import pathlib  # noqa: D100
import sys

import pytest

import j4dd_run_helper


def pytest_addoption(parser):  # noqa: D103
    parser.addoption(
        "--j4dd-executable", action="store", type=pathlib.Path, required=True
    )


def pytest_sessionstart(session):  # noqa: D103
    try:
        j4dd_run_helper.run_j4dd(
            pathlib.Path(session.config.option.j4dd_executable), {}, "--help"
        )
    except j4dd_run_helper.J4ddRunError as exc:
        print(
            "Cannot proceed with tests! j4-dmenu-desktop is not executable in "
            "the current environment. If you are cross compiling under Meson, "
            "please make sure that exe_wrapper is properly set.\n",
            file=sys.stderr,
        )
        print(exc, file=sys.stderr)
        pytest.exit("Exitted.", returncode=77)
