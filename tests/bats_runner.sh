#!/bin/sh

# This shell script is compatible with dash shell.

# The length of the bash script is due to extensive descriptive messages, rather
# than complex logic.

set +o nounset +o errexit

failed_to_execute_j4dd() {
    printf "Couldn't execute j4-dmenu-desktop (path '%s'" "$J4DD_EXECUTABLE"
    if [ "$MESON_EXE_WRAPPER" ]; then
        printf ", MESON_EXE_WRAPPER '%s'" "$MESON_EXE_WRAPPER"
    fi
    printf ")!\n"
    cat >&2 <<EOF

If you are cross compiling and using Meson, make sure than an exe_wrapper is
set.

If you're using CMake, be aware that the CTest test target does NOT build
j4-dmenu-desktop as a dependency of the test, you have to build j4-dmenu-desktop
and **then** run tests.

If you're running bats-runner.sh manually, make sure that j4dd_exe_path you have
supplied is correct.
EOF
    # return code 77 means skipped test
    exit 77
}

BATS_EXECUTABLE=bats

while getopts hb: o; do
    case $o in
    h)
        cat <<EOF
Usage:
  bats-runner.sh -h
  bats-runner.sh [-b bats_exe_path] j4dd_exe_path [bats_arguments...]
Helper script for running Bats testsuite.

This script makes sure that the testsuite is correctly initialized and that
j4-dmenu-desktop can be executed. If Bats cannot be initialized properly, this
script exits with return code 77 to signify a skipped test to the build system.

This script can be run either standalone or by CMake or Meson. Both buildsystems
supply this script with correct arguments by default, so running these tests
via the buildsystem is more practical.

If you want to run checks on locally installed j4-dmenu-desktop, you can run
this script as follows:

    ./bats-runner.sh /usr/bin/j4-dmenu-desktop bats.bats

Options:
  -h             Print this help message
  -b PATH        Path to bats executable. 'bats' by default.

Environment:
  MESON_EXE_WRAPPER  Executable wrapper to be used when trying to run
                     crosscompiled j4-dmenu-desktop (see Meson's test() function
                     documentation)
EOF
        exit 0
        ;;
    b)
        BATS_EXECUTABLE="$OPTARG"
        ;;
    *)
        echo "Invalid option! See \`bats_runner.sh -h\` for more information." >&2
        exit 1
        ;;
    esac
done

shift $((OPTIND - 1))

case $# in
0)
    echo "Not enough arguments! Path to j4-dmenu-desktop executable must be"\
        "supplied. See \`bats_runner.sh -h\` for more information." >&2
    exit 1
    ;;
1)
    echo "WARNING: No arguments supplied to bat! You should at least give"\
        "bats path to the test file. See \`bats_runner.sh -h\` for more"\
        "information." >&2
    ;;
esac

J4DD_EXECUTABLE="$1"
shift

# $MESON_EXE_WRAPPER should be supplied by Meson in its test target
if [ "$MESON_EXE_WRAPPER" ]; then
  "$MESON_EXE_WRAPPER" "$J4DD_EXECUTABLE" --help > /dev/null 2>&1 || failed_to_execute_j4dd
else
  "$J4DD_EXECUTABLE" --help > /dev/null 2>&1 || failed_to_execute_j4dd
fi

case "$J4DD_EXECUTABLE" in
/*)
    ;;
*)
    echo "WARNING: j4dd_exe_path is relative! Please use an absolute path"\
        "instead." >&2
esac

if ! { export -p | grep -q "TERM"; }; then
    echo "WARNING: \$TERM is unset or empty! Bats will not work without it."\
        "Overriding to TERM=dumb..." >&2
    export TERM=dumb
fi

export J4DD_EXECUTABLE
exec "$BATS_EXECUTABLE" "$@"
