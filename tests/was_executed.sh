#!/bin/sh
if [ -z "$J4DD_UNIT_TEST_STATUS_FILE" ]; then
    echo "\$J4DD_UNIT_TEST_STATUS_FILE is not set!" >&2
    exit 1
fi

trap 'printf 0\\n > $J4DD_UNIT_TEST_STATUS_FILE' EXIT

# This env variable is delimited by :. Its items therefore can not contain :.
# Escaping : is not implemented.
if [ "$J4DD_UNIT_TEST_ARGS" ]; then
    ARGS="$(printf %s: "$@")"
    ARGS="${ARGS%:}"

    [ "$ARGS" != "$J4DD_UNIT_TEST_ARGS" ] && exit 1
fi

trap '' EXIT
printf 1\\n > "$J4DD_UNIT_TEST_STATUS_FILE"
