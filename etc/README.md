This directory contains completions.

ZSH and fish completions are hand written, the bash completion is generated
using [complgen](https://github.com/adaszko/complgen). Contributions with a
better bash completion are welcome.

Moving to an automatic completion generation system would be nice. Now, when a
flag is changed, `j4-dmenu-desktop`'s `--help` message must be changed, its
manpage must be changed, ZSH, fish and complgen completions must be changed. The
aforementioned complgen is close, but it isn't feature complete (yet).
