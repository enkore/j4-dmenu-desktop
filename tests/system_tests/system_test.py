"""System test collection for j4-dmenu-desktop.

These tests execute j4-dmenu-desktop with predetermined arguments and environment
variables and observe its result.
"""

from __future__ import annotations

import functools
import os.path
import pathlib
import shlex
import shutil
import subprocess

import pytest

import j4dd_run_helper

test_files = pathlib.Path(__file__).parent.parent.absolute() / "test_files/pytest"
helpers = pathlib.Path(__file__).parent.absolute() / "helper_scripts"
empty_dir = pathlib.Path(__file__).parent.parent.absolute() / "test_files/empty"

quirk_modes = ("wine", "multispace")


def mkfifo(name: pathlib.Path | str) -> None:
    try:
        os.remove(name)
    except FileNotFoundError:
        pass
    os.mkfifo(name)


@pytest.fixture(scope="session")
def j4dd_path(pytestconfig):
    """Fixture for returning path to j4-dmenu-desktop.

    See conftest.py for more info.
    """
    return os.path.abspath(pytestconfig.getoption("j4dd_executable"))


@pytest.fixture(scope="session")
def run_j4dd(j4dd_path):
    """Session fixture providing convenient j4dd_run_helper.run_j4dd wrapper."""
    return functools.partial(j4dd_run_helper.run_j4dd, j4dd_path)


@pytest.fixture
def chdir_test_files():  # noqa: D103
    oldpwd = os.getcwd()
    os.chdir(test_files)
    yield
    os.chdir(oldpwd)


def test_help(run_j4dd):
    """Test --help flag."""
    run_j4dd({}, "--help")


def test_version(run_j4dd):
    """Test --version flag."""
    run_j4dd({}, "--version")


def test_SearchPath_checks(run_j4dd, chdir_test_files):  # noqa: N802
    """Regression test faulty SearchPath checks."""
    run_j4dd(
        {"XDG_DATA_HOME": str(empty_dir), "XDG_DATA_DIRS": ".:a:b"},
        "--dmenu",
        str(helpers / "dmenu_noselect_imitator.sh"),
    )


class DisabledTermError(Exception):  # noqa: D101
    pass


def check_term_emulator_availability(
    terminal_emulator: str, separator: str = "-e"
) -> None:
    """Check whether a terminal emulator is usable for tests."""
    term_path = shutil.which(terminal_emulator)
    if term_path is None:
        raise DisabledTermError(f"'{terminal_emulator}' couldn't be located in $PATH!")
    args = [term_path, separator, "true"] if separator else [term_path, "true"]
    result = subprocess.run(
        args,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
        raise DisabledTermError(
            f"`{shlex.join(args)}` returned with exit status {result.returncode}!"
        )


def check_term_mode(
    term_mode: str,
    run_j4dd,
    tmp_path: pathlib.Path,
    args: list[str],
):
    """Run j4-dmenu-desktop with specific terminal emulator and test results."""
    path_env = os.getenv("PATH")
    term_mode_result_path = tmp_path / f"{term_mode}_term_mode_was_executed.sh_result"
    mkfifo(term_mode_result_path)
    async_result = run_j4dd(
        {
            "PATH": f"{helpers}:{path_env}",
            "J4DD_UNIT_TEST_STATUS_FILE": str(term_mode_result_path),
            "XDG_DATA_HOME": str(test_files),
            "XDG_DATA_DIRS": str(empty_dir),
            "J4DD_UNIT_TEST_ARGS": "--help:--:<><>'$$::!?",
        },
        "--dmenu",
        str(helpers / "dmenu_selected_imitator.sh"),
        "--term-mode",
        term_mode,
        "--strict",
        *args,
        asynchronous=True,
    )
    try:
        with open(term_mode_result_path, "r") as fifo:
            # This blocks.
            fifo_message = fifo.read()
    finally:
        async_result.wait()
    assert fifo_message == "1\n"


terminal_emulators = [
    (
        "default",
        "i3-sensible-terminal",
        "-e",
        "i3-sensible-terminal -e {script}",
    ),
    ("xterm", "xterm", "-e", "xterm -title {name} -e {cmdline@}"),
    ("alacritty", "alacritty", "-e", "alacritty -T {name} -e {cmdline@}"),
    ("kitty", "kitty", "", "kitty -T {name} {cmdline@}"),
    (
        "terminator",
        "terminator",
        "-x",
        "terminator -T {name} -x {cmdline@}",
    ),
    (
        "gnome-terminal",
        "gnome-terminal",
        "--",
        "gnome-terminal --title {name} -- {cmdline@}",
    ),
]


@pytest.fixture(scope="session")
def get_available_term_emulators():
    """Test whether all terminal emulators are available.

    Some terminal emulators are used more than once, this fixture makes sure
    that terminal emulator availability is checked only once.

    Returns:
        A dict with key being terminal emulator executable string and value
        being an exception onject if terminal emulator isn't available or None
        if it is.
    """
    result = {}
    for term_emulator_data in terminal_emulators:
        term_emulator = term_emulator_data[1]
        result[term_emulator] = None
        try:
            check_term_emulator_availability(term_emulator, term_emulator_data[2])
        except DisabledTermError as exc:
            result[term_emulator] = exc

    return result


@pytest.mark.parametrize(
    "mode,terminal_emulator,program_flag,custom_test",
    terminal_emulators,
    ids=(emulator[0] for emulator in terminal_emulators),
)
def test_terminal_emulator(
    mode: str,
    terminal_emulator: str,
    program_flag: str,
    custom_test: str,
    run_j4dd,
    tmp_path,
    get_available_term_emulators,
):
    """Test all supported terminal emulators."""
    if mode == "terminator":
        pytest.skip("See https://github.com/gnome-terminator/terminator/issues/923")

    is_available = get_available_term_emulators[terminal_emulator]
    if is_available is not None:
        pytest.skip(str(is_available))

    check_term_mode(mode, run_j4dd, tmp_path, [])
    check_term_mode("custom", run_j4dd, tmp_path, ["--term", custom_test])


def test_gnome_terminal_deprecated_example(
    run_j4dd, tmp_path, get_available_term_emulators
):
    """Check deprecated custom example of gnome-terminal.

    This example is given in j4-dmenu-desktop's manpage to demonstrate
    {cmdline*} --term-mode custom functionality.
    """
    is_available = get_available_term_emulators["gnome-terminal"]
    if is_available is not None:
        pytest.skip(str(is_available))

    check_term_mode(
        "custom",
        run_j4dd,
        tmp_path,
        ["--term", "gnome-terminal --title {name} -e {cmdline*}"],
    )


@pytest.fixture(scope="session")
def check_custom_term(run_j4dd):
    """Helper function to test custom term mode placeholders."""
    return functools.partial(
        run_j4dd,
        {
            "XDG_DATA_HOME": str(empty_dir),
            "XDG_DATA_DIRS": str(empty_dir),
        },
        "--dmenu",
        str(helpers / "dmenu_noselect_imitator.sh"),
        "--term-mode",
        "custom",
        "--term",
    )


def test_custom_term_mode_edge_cases(check_custom_term):
    """Test edge cases of --term-mode custom."""
    check_custom_term("  {cmdline} ", shouldfail=True)
    check_custom_term("  {nname} ", shouldfail=True)
    check_custom_term("  {} ", shouldfail=True)
    check_custom_term("\\n", shouldfail=True)
    check_custom_term("\\x", shouldfail=True)

    # Test normal behavior too.
    check_custom_term("\\ ")

    check_custom_term(" {nam{e}", shouldfail=True)
    check_custom_term(" {nam{e}}", shouldfail=True)
    check_custom_term(" {nam{cmdline*}}", shouldfail=True)
    check_custom_term(" {nam{cmdline@}}", shouldfail=True)
    check_custom_term(" {nam{nam{name}}}", shouldfail=True)
    check_custom_term("command {{name}}", shouldfail=True)
    check_custom_term("command {{name}", shouldfail=True)
    check_custom_term("command -e={cmdline@}", shouldfail=True)


def run_base_tests_and_get_output(
    run_j4dd, tmp_file: pathlib.Path, env: dict[str, str], *args: str
) -> list[str]:
    mkfifo(tmp_file)
    """Helper function to run j4-dmenu-desktop and return prompt entries."""
    async_result = run_j4dd(
        env,
        "--dmenu",
        str(helpers / "dmenu_noselect_output_imitator.sh"),
        *args,
        asynchronous=True,
    )
    try:
        with open(tmp_file, "r") as fifo:
            return sorted(line.rstrip() for line in fifo)
    finally:
        async_result.wait()


@pytest.fixture
def run_base_tests(run_j4dd):
    """Helper function to run normal behavior tests of j4-dmenu-desktop."""
    return functools.partial(
        run_base_tests_and_get_output,
        run_j4dd,
    )


def splitsort(input: str) -> list[str]:
    """Take a string, strip it, split it into lines and sort those lines."""
    result = input.strip().splitlines()
    result.sort()
    return result


def test_normal_behavior(run_base_tests, tmp_path):
    """Test basic general functionality of j4-dmenu-desktop.

    This test test whether correct desktop app names are provided to the dmenu
    wrapper, whether desktop file collisions are handled correctly and it tests
    the --use-xdg-de flag.
    """
    tmp_file = tmp_path / "normal-behavior-dmenu-input"
    env = {
        "XDG_DATA_HOME": str(test_files / "desktop-file-samples/rank-0"),
        "XDG_DATA_DIRS": f"{test_files / 'desktop-file-samples/rank-1'}"
        f":{test_files / 'desktop-file-samples/rank-2'}",
        "J4DD_UNIT_TEST_STATUS_FILE": str(tmp_file),
        "LC_MESSAGES": "C",
    }

    dmenu_output = run_base_tests(tmp_file, env)
    assert dmenu_output == splitsort(
        """
Eagle
GNU Image Manipulation Program
Htop
Image Editor
Process Viewer
Rank 0 collision
    """
    )

    env["XDG_CURRENT_DESKTOP"] = "kde"
    dmenu_output = run_base_tests(tmp_file, env, "-x")
    assert dmenu_output == splitsort(
        """
Eagle
GNU Image Manipulation Program
Image Editor
Rank 0 collision
        """
    )

    env["XDG_CURRENT_DESKTOP"] = "i3"
    dmenu_output = run_base_tests(tmp_file, env, "-x")
    assert dmenu_output == splitsort(
        """
Eagle
GNU Image Manipulation Program
Htop
Image Editor
Process Viewer
Rank 0 collision
        """
    )


def test_halding_of_file_field_codes(run_j4dd, tmp_path):
    """Test correct handling of %F arguments to desktop apps."""
    tmp_file = tmp_path / "field-codes"
    mkfifo(tmp_file)
    path = os.getenv("PATH")
    env = {
        "PATH": f"{helpers}:{path}",
        "J4DD_UNIT_TEST_STATUS_FILE": str(tmp_file),
        "XDG_DATA_HOME": str(test_files / "args"),
        "XDG_DATA_DIRS": str(empty_dir),
        "J4DD_UNIT_TEST_ARGS": "arg1:arg2:arg3:--arg4",
    }

    async_result = run_j4dd(
        env,
        "--dmenu",
        str(helpers / "dmenu_selected_imitator.sh") + " arg1 arg2 arg3 --arg4",
        asynchronous=True,
    )
    try:
        with open(tmp_file, "r") as fifo:
            fifo_message = fifo.read()
    finally:
        async_result.wait()
    assert fifo_message == "1\n"

    async_result = run_j4dd(
        env,
        "--dmenu",
        str(helpers / "dmenu_selected_imitator.sh") + " arg1 arg2  arg3   --arg4",
        asynchronous=True,
    )
    try:
        with open(tmp_file, "r") as fifo:
            fifo_message = fifo.read()
    finally:
        async_result.wait()
    assert fifo_message == "1\n"

def test_quirks_mutual_exclusivity(run_j4dd):
    """Test --desktop-file-quirks and --strict-parsing conflict."""
    run_j4dd({}, "--desktop-file-quirks", ",".join(quirk_modes), "--strict-parsing", shouldfail=True)


def test_quirks_alldisabled(run_j4dd):
    """Test that --desktop-file-quirks with everything disabled fails."""
    run_j4dd({}, "--desktop-file-quirks", ",".join(f"no{mode}" for mode in quirk_modes), shouldfail=True)


def test_quirks_nomode(run_j4dd):
    """Test that --desktop-file-quirks supports disabling."""
    path_env = os.getenv("PATH")
    run_j4dd(
        {
            "PATH": f"{helpers}:{path_env}",
            "XDG_DATA_HOME": str(test_files / "quirks"),
            "XDG_DATA_DIRS": str(empty_dir),
        },
        "--dmenu",
        str(helpers / "dmenu_noselect_imitator.sh"),
        "--desktop-file-quirks",
        "nomultispace",
    )

def test_quirks_chaining(run_j4dd):
    """Test that --desktop-file-quirks specified multiple times works."""
    path_env = os.getenv("PATH")
    run_j4dd(
        {
            "PATH": f"{helpers}:{path_env}",
            "XDG_DATA_HOME": str(test_files / "quirks"),
            "XDG_DATA_DIRS": str(empty_dir),
        },
        "--dmenu",
        str(helpers / "dmenu_noselect_imitator.sh"),
        "--desktop-file-quirks",
        "nomultispace",
        "--desktop-file-quirks",
        "nowine",
    )
