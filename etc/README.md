This directory contains completions.

All three completions (Bash, Fish and ZSH) are hand written. The Bash completion
is operational, but it may contain slight errors. Contributions with completion
fixes and enhancements are welcome.

Moving to an automatic completion generation system would be nice. Now, when a
flag is changed or added, a lot of things depend on it. See
[contributing](../CONTRIBUTING.md#commandline-arguments).
[Complgen](https://github.com/adaszko/complgen) is close, but it isn't feature
complete (yet). If it improves or if some other tool or framework will be able
to replace it, completions could be delegated to it.

The complgen file in this directory is unused and can be out of date. It is kept
in the repository because it might be useful in the future if complgen would
be used for completion generation.

