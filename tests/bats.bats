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
        $MESON_EXE_WRAPPER "$J4DD_EXECUTABLE" "$@"
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
    [ $# -eq 0 ] && fail

    local term_mode="$1"
    shift

    PATH="$HELPERS:$PATH" J4DD_UNIT_TEST_STATUS_FILE="$TMP" XDG_DATA_HOME="${TEST_FILES}bats/" XDG_DATA_DIRS="${TEST_FILES}empty/" J4DD_UNIT_TEST_ARGS="--help:--:<><>'\$\$::!?" run_j4dd --dmenu "${HELPERS}/dmenu_selected_imitator.sh" --term-mode "$term_mode" "$@"
    echo 1 | cmp - "$TMP"
}

check_term_availability() {
    [ $# -ne 1 ] && [ $# -ne 2 ] && fail

    local separator="${2--e}"

    if ! type "$1" > /dev/null 2>&1; then
        skip "Can't proceed with terminal mode test! --term $1 is not available."
    fi
    if [ "$separator" ]; then
        if ! "$1" "$separator" true; then
            skip "Can't proceed with terminal mode test! Terminal emulator $1 cannot be executed in the current environment."
        fi
    else
        if ! "$1" true; then
            skip "Can't proceed with terminal mode test! Terminal emulator $1 cannot be executed in the current environment."
        fi
    fi
}

# The following term mode tests are run twice per each mode. First test uses the
# dedicated --term-mode and the second test uses custom term mode with the value
# of --term taken from the manpage.

# --term arguments of custom term mode should be in sync with the manpage!

@test "Test >default< term mode" {
    check_term_availability i3-sensible-terminal

    test_term_mode default
    test_term_mode custom --term "i3-sensible-terminal -e {script}"
}

@test "Test >xterm< term mode" {
    check_term_availability xterm

    test_term_mode xterm
    test_term_mode custom --term "xterm -title {name} -e {cmdline@}"
}

@test "Test >alacritty< term mode" {
    check_term_availability alacritty

    test_term_mode alacritty
    test_term_mode custom --term "alacritty -T {name} -e {cmdline@}"
}

@test "Test >kitty< term mode" {
    check_term_availability kitty ""

    test_term_mode kitty
    test_term_mode custom --term "kitty -T {name} {cmdline@}"
}

@test "Test >terminator< term mode" {
    skip "See https://github.com/gnome-terminator/terminator/issues/923"
    check_term_availability terminator -x

    test_term_mode terminator
    test_term_mode custom --term "terminator -T {name} -x {cmdline@}"
}

@test "Test >gnome-terminal< term mode" {
    check_term_availability gnome-terminal --

    test_term_mode gnome-terminal
    test_term_mode custom --term "gnome-terminal --title {name} -- {cmdline@}"

    # Extra check for deprecated mode.
    test_term_mode custom --term "gnome-terminal --title {name} -e {cmdline*}"
}

test_custom_term_mode() {
    XDG_DATA_HOME="${TEST_FILES}empty/" XDG_DATA_DIRS="" run_j4dd --dmenu "${HELPERS}/dmenu_noselect_imitator.sh" --term-mode custom "$@"
}

@test "Test edge cases of >custom< term mode" {
    test_custom_term_mode --term "  {cmdline} " && false
    test_custom_term_mode --term "  {nname} " && false
    test_custom_term_mode --term "  {} " && false
    test_custom_term_mode --term "\\n" && false
    test_custom_term_mode --term "\\x" && false
    # Test normal behavior too.
    test_custom_term_mode --term "\\ "
    test_custom_term_mode --term " {nam{e}" && false
    test_custom_term_mode --term " {nam{e}}" && false
    test_custom_term_mode --term " {nam{cmdline*}}" && false
    test_custom_term_mode --term " {nam{cmdline@}}" && false
    test_custom_term_mode --term " {nam{nam{name}}}" && false
    test_custom_term_mode --term "command {{name}}" && false
    test_custom_term_mode --term "command {{name}" && false
    test_custom_term_mode --term "command -e={cmdline@}" && false

    # Test must end in `true` because of weird Bash return logic.
    true
}

run_base_tests() {
    XDG_DATA_HOME="${TEST_FILES}bats/desktop-file-samples/rank-0" XDG_DATA_DIRS="${TEST_FILES}bats/desktop-file-samples/rank-1:${TEST_FILES}bats/desktop-file-samples/rank-2" J4DD_UNIT_TEST_STATUS_FILE="$TMP" run_j4dd --dmenu "${HELPERS}/dmenu_noselect_output_imitator.sh" "$@"

    [ -f "$TMP" ]
}

@test "Test normal behavior of j4-dmenu-desktop" {
    export LC_MESSAGES="C"
    run_base_tests
    sort -o "$TMP" "$TMP"
    diff "$TMP" - <<EOF
Eagle
GNU Image Manipulation Program
Htop
Image Editor
Process Viewer
Rank 0 collision
EOF

    XDG_CURRENT_DESKTOP="kde" run_base_tests -x
    sort -o "$TMP" "$TMP"
    diff "$TMP" - <<EOF
Eagle
GNU Image Manipulation Program
Image Editor
Rank 0 collision
EOF

    XDG_CURRENT_DESKTOP="i3" run_base_tests -x
    sort -o "$TMP" "$TMP"
    diff "$TMP" - <<EOF
Eagle
GNU Image Manipulation Program
Htop
Image Editor
Process Viewer
Rank 0 collision
EOF
}

@test "Test correct handling of %F arguments to desktop apps" {
    PATH="$HELPERS:$PATH" J4DD_UNIT_TEST_STATUS_FILE="$TMP" XDG_DATA_HOME="${TEST_FILES}bats/args/" XDG_DATA_DIRS="${TEST_FILES}empty/" J4DD_UNIT_TEST_ARGS="arg1:arg2:arg3:--arg4" run_j4dd --dmenu "${HELPERS}/dmenu_selected_imitator.sh arg1 arg2 arg3 --arg4"
    echo 1 | cmp - "$TMP"

    PATH="$HELPERS:$PATH" J4DD_UNIT_TEST_STATUS_FILE="$TMP" XDG_DATA_HOME="${TEST_FILES}bats/args/" XDG_DATA_DIRS="${TEST_FILES}empty/" J4DD_UNIT_TEST_ARGS="arg1:arg2:arg3:--arg4" run_j4dd --dmenu "${HELPERS}/dmenu_selected_imitator.sh arg1 arg2  arg3   --arg4"
    echo 1 | cmp - "$TMP"
}
