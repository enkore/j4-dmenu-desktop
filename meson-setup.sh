#!/bin/sh

# See BUILDING.md for more info.

# This shell script is compatible with dash shell.

set -o nounset -o errexit

if ! type meson >/dev/null; then
    echo "Meson must be installed!" 1>&2
    exit 1
fi

DRYRUN=""
build_type="release"

while getopts dhb: o; do
    case $o in
    h)
        cat <<EOF
Usage: meson-setup.sh [-h] [-d] [-b release | debug | sanitize] DIR
Helper script to initialize Meson builddir.

Note that the sanitized build style will NOT utilize system-installed
dependencies; instead, they will be fetched and built manually.

  -h   Print this help message
  -d   Dry run - print commands that would have been executed
  -b   Specify build style; set to release when not specified
EOF
        exit 0
        ;;
    d)
        DRYRUN="echo"
        ;;
    b)
        build_type=$OPTARG
        ;;
    *)
        echo "Invalid option!" 1>&2
        exit 1
        ;;
    esac
done

shift $((OPTIND - 1))

if [ $# -eq 0 ]; then
    echo "Build directory must be specified!" 1>&2
    exit 1
fi
if [ $# -ge 2 ]; then
    printf "Too many arguments specified! If you want to append your own " 1>&2
    printf "flags to 'meson setup', invoke it directly. Run this script " 1>&2
    printf "with the -d flag to do a dry-run showing the default native " 1>&2
    printf "files used (you can then append your own flags to the " 1>&2
    printf "'meson setup' invocation).\n" 1>&2
    exit 1
fi

case $build_type in
release)
    exec $DRYRUN meson setup --native-file meson-native/release.ini "$1"
    ;;
debug)
    exec $DRYRUN meson setup --native-file meson-native/debug.ini "$1"
    ;;
sanitize)
    exec $DRYRUN meson setup --native-file meson-native/sanitize.ini "$1"
    ;;
*)
    echo "Unknown build style $build_type!" 1>&2
    exit 1
    ;;
esac
