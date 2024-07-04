#!/bin/sh
if [ -z "$J4DD_UNIT_TEST_STATUS_FILE" ]; then
    echo "\$J4DD_UNIT_TEST_STATUS_FILE is not set!" >&2
    exit 1
fi

cat > "$J4DD_UNIT_TEST_STATUS_FILE"
exec echo
