# The test suite accepts two "arguments": path to the j4-dmenu-desktop
# executable (J4DD_EXECUTABLE) and a Meson exe wrapper (MESON_EXE_WRAPPER).
#
# Only the J4DD_EXECUTABLE argument is mandatory.
#
# These environmental variables should be handled by the bats_runner.sh script.
setup_file() {
    if [ -z "$J4DD_EXECUTABLE" ]; then
        echo -ne "\$J4DD_EXECUTABLE is not set! Please use the bats_runner.sh"\
            "wrapper script to run bats tests."
        false
    fi

    export TEST_FILES="${BATS_TEST_DIRNAME}/test_files/"
    export HELPERS="${BATS_TEST_DIRNAME}/bats_helpers/"
}

run_j4dd() {
    if [ -z "$MESON_EXE_WRAPPER" ]; then
        "$J4DD_EXECUTABLE" "$@"
    else
        "$MESON_EXE_WRAPPER" "$J4DD_EXECUTABLE" "$@"
    fi
}

# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

@test "--help succeedes" {
    run_j4dd --help
}

@test "--version succeedes" {
    run_j4dd --version
}

@test "Regression test faulty SearchPath checks" {
    (
        cd "${TEST_FILES}bats/"
        XDG_DATA_HOME="${TEST_FILES}empty/" XDG_DATA_DIRS=.:a:b run_j4dd --dmenu "${HELPERS}dmenu_noselect_imitator.sh"
    )
}

setup() {
    TMP="$(mktemp j4dd-tests-XXXXXXXXXX -p "$BATS_TMPDIR")"
}

teardown() {
    rm -f "$TMP"
}

test_term_mode() {
    [ $# -ne 1 ] && fail

    PATH="$HELPERS:$PATH" J4DD_UNIT_TEST_STATUS_FILE="$TMP" XDG_DATA_HOME="${TEST_FILES}bats/" XDG_DATA_DIRS="${TEST_FILES}empty/" J4DD_UNIT_TEST_ARGS="--help:--:<><>'\$\$::!?" run_j4dd --dmenu "${HELPERS}/dmenu_selected_imitator.sh" --term-mode "$1"
    echo 1 | cmp - "$TMP"
}

@test "Test >default< term mode" {
    if ! type i3-sensible-terminal > /dev/null 2>&1; then
        skip "Can't proceed with terminal mode test! --term i3-sensible-terminal is not available."
    fi

    test_term_mode default
}

@test "Test >xterm< term mode" {
    if ! type xterm > /dev/null 2>&1; then
        skip "Can't proceed with terminal mode test! --term xterm is not available."
    fi

    test_term_mode xterm
}

@test "Test >alacritty< term mode" {
    if ! type alacritty > /dev/null 2>&1; then
        skip "Can't proceed with terminal mode test! --term alacritty is not available."
    fi

    test_term_mode alacritty
}

@test "Test >kitty< term mode" {
    if ! type kitty > /dev/null 2>&1; then
        skip "Can't proceed with terminal mode test! --term kitty is not available."
    fi

    test_term_mode kitty
}

@test "Test >terminator< term mode" {
    skip "See https://github.com/gnome-terminator/terminator/issues/923"
    if ! type terminator > /dev/null 2>&1; then
        skip "Can't proceed with terminal mode test! --term terminator is not available."
    fi

    test_term_mode terminator
}

@test "Test >gnome-terminal< term mode" {
    if ! type gnome-terminal > /dev/null 2>&1; then
        skip "Can't proceed with terminal mode test! --term gnome-terminal is not available."
    fi

    test_term_mode gnome-terminal
}
