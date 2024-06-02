#!/bin/sh

# See BUILDING.md for more info.

# This shell script is compatible with dash shell.

set +o nounset +o errexit

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

if [ $# -ne 1 ]; then
    echo "Build directory must be specified!" 1>&2
    exit 1
fi

case $build_type in
release)
    exec $DRYRUN meson setup --buildtype=release -Db_lto=true "$1"
    ;;
debug)
    exec $DRYRUN meson setup -Dsplit-source=true --unity=off "$1"
    ;;
sanitize)
    # See https://github.com/catchorg/Catch2/issues/2811 for explanation of forcefallback
    exec $DRYRUN meson setup -Dsplit-source=true -Db_sanitize=address,undefined -Dcpp_debugstl=true -Db_lundef=false --wrap-mode forcefallback --unity=off "$1"
    ;;
*)
    echo "Unknown build style $build_type!" 1>&2
    exit 1
    ;;
esac
