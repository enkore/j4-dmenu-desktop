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
        XDG_DATA_HOME="" XDG_DATA_DIRS=.:a:b run_j4dd --dmenu "${HELPERS}dmenu_noselect_imitator.sh"
    )
}
