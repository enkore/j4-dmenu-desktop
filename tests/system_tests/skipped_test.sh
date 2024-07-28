#!/bin/sh
cat >&2 <<EOF
Couldn't find pytest executable. See Meson's configure output for more information.
EOF
exit 77
