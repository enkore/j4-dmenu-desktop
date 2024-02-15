This directory contains completions.

ZSH and fish completions are hand written, the bash completion is generated
using [complgen](https://github.com/adaszko/complgen). Contributions with a
better bash completion are welcome.

Moving to an automatic completion generation system would be nice. Now, when a
flag is changed or added, a lot of things depend on it. See
[contributing](../CONTRIBUTING.md#commandline-arguments). The aforementioned
complgen is close, but it isn't feature complete (yet). If it improves or if
some other tool or framework will be able to replace it, completions could be
delegated to it.
