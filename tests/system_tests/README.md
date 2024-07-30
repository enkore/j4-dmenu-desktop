# System tests for j4-dmenu-desktop

[![Ruff](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/ruff/main/assets/badge/v2.json)](https://github.com/astral-sh/ruff)

These tests execute `j4-dmenu-desktop` with predetermined environment and
examine it's output.

## Dependencies
Dependencies can be found in the `requirements.txt` file. They can be installed
with

```
python3 -m pip install -r requirements.txt
```

Usage of [Python virtual
environments](https://docs.python.org/3/library/venv.html) is recommended.

System tests depend on [pytest](https://docs.pytest.org/en/stable/) testing
framework.

System tests have been verified to work on Python 3.7.4 with pytest version
3.10.1. System tests may work with older versions of these programs too.

### Terminal emulators
The tests can optionally test j4-dmenu-desktop integration with various terminal
emulators. If a terminal emulator is available and it is executable in the
current environment (most terminal emulators do not work without a display
server running), the test for it is enabled, otherwise it's skipped. Tested
terminal emulators include: `i3-sensible-terminal` (a terminal emulator wrapper
provided by `i3`), `xterm`, `alacritty`, `kitty`, `terminator`, `gnome-terminal`.

## Running tests
The simplest way to execute system test is through the build system. To run
system tests in Meson, you can run

```
meson test pytest-system-test
```

You must run this command from a Meson build directory. In CMake, you can run

```
ctest -R pytest-system-test
```

> [!NOTE]
> Running system tests when cross compiling is currently supported in Meson
> only.

You must run this command from a CMake build directory. Note that
`j4-dmenu-desktop` **must be built** (with `make j4-dmenu-desktop`) beforehand!
Tests will fail otherwise[^1].

You can run tests manually with

```
pytest tests/system_tests/system_test.py --j4dd-executable /usr/bin/j4-dmenu-desktop
```

Here the system installed `j4-dmenu-desktop` executable located at
`/usr/bin/j4-dmenu-desktop` is used. When executing `pytest` manually, you can
pass the `-v` flag one or more times to increase verbosity, control the handling
of temporary files and use other standard `pytest` flags.

## Development
I, meator, use [ruff](https://github.com/astral-sh/ruff) to lint and format code
in this repository. Customized rules for `ruff` can be found in
`pyproject.toml`. I also use [mypy](https://github.com/python/mypy) for static
typing (although without the `--strict` option, which doesn't make sense with
`pytest`'s test function and fixture system).

Tests themselves are located in `system_test.py`. The file ends in `_test.py` to
make it compatible [with test
discovery](https://docs.pytest.org/en/stable/explanation/goodpractices.html#test-discovery)
(although that is not necessary when explicitly specifying the test file).

`j4dd_run_helper.py` includes helper code to run `j4-dmenu-desktop` with the set
environment variables and arguments. It also prints verbose error messages.

The rest of the files should be self-explanatory.

[^1]: This is true for CMake only, Meson manages build dependencies for tests
      correctly.
